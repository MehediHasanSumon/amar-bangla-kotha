#include "abk/engine.h"
#include "abk/utf8_util.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <sys/resource.h>
#include <unistd.h>

struct TestCase {
    std::string input;
    std::string expected;
    std::string category;
};

struct CategoryStat {
    int total = 0;
    int top1 = 0;
    int top3 = 0;
};

static long get_memory_rss_kb() {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::stringstream ss(line);
            std::string key;
            long rss_kb = 0;
            ss >> key >> rss_kb;
            return rss_kb;
        }
    }
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss; // On Linux, this is KB
    }
    return 0;
}

int main(int argc, char* argv[]) {
    std::string golden_path = "tests/data/golden.tsv";
    std::string dict_path = "data/bangla_wordlist.txt";

    if (argc >= 2) golden_path = argv[1];
    if (argc >= 3) dict_path = argv[2];

    std::cout << "=========================================================\n";
    std::cout << "   Amar Bangla Kotha - Production Benchmark Runner       \n";
    std::cout << "=========================================================\n";
    std::cout << "Golden Dataset: " << golden_path << "\n";
    std::cout << "Dictionary:     " << dict_path << "\n";

    // 1. Load Golden Dataset
    std::ifstream infile(golden_path);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open " << golden_path << "\n";
        return 1;
    }

    std::vector<TestCase> test_cases;
    std::string line;
    bool is_header = true;
    while (std::getline(infile, line)) {
        std::string trimmed = abk::utf8::trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        if (is_header && trimmed.find("input\t") == 0) {
            is_header = false;
            continue;
        }
        is_header = false;

        std::stringstream ss(trimmed);
        std::string input, expected, category;
        if (std::getline(ss, input, '\t') &&
            std::getline(ss, expected, '\t') &&
            std::getline(ss, category, '\t')) {
            test_cases.push_back({input, expected, category});
        }
    }

    std::cout << "Loaded " << test_cases.size() << " test cases.\n\n";

    // 2. Initialize Engine and Measure Startup
    auto start_load = std::chrono::high_resolution_clock::now();
    abk::AmarBanglaKothaEngine engine;
    bool loaded = engine.load_dictionary_file(dict_path);
    auto end_load = std::chrono::high_resolution_clock::now();
    double load_ms = std::chrono::duration<double, std::milli>(end_load - start_load).count();

    std::cout << "Engine Dictionary Load Time: " << std::fixed << std::setprecision(2) << load_ms << " ms "
              << "(Words in dictionary: " << engine.get_dictionary().word_count() << ", Status: "
              << (loaded ? "SUCCESS" : "BUILTIN_ONLY") << ")\n";

    long rss_after_init = get_memory_rss_kb();
    std::cout << "Resident Memory (RSS):        " << rss_after_init << " KB ("
              << (rss_after_init / 1024.0) << " MB)\n\n";

    // 3. Run Benchmark
    int total_words = 0;
    int overall_top1 = 0;
    int overall_top3 = 0;
    std::map<std::string, CategoryStat> category_stats;
    std::vector<double> keystroke_latencies_us;
    std::vector<std::string> failed_top1_details;

    // Warm up
    for (int i = 0; i < 5; ++i) {
        engine.process("ami");
    }

    for (const auto& tc : test_cases) {
        total_words++;
        category_stats[tc.category].total++;

        // Measure keystroke-by-keystroke latency (simulating realistic interactive typing)
        for (size_t len = 1; len <= tc.input.length(); ++len) {
            std::string sub = tc.input.substr(0, len);
            auto t0 = std::chrono::high_resolution_clock::now();
            auto res = engine.process(sub);
            auto t1 = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            keystroke_latencies_us.push_back(us);
        }

        // Full word evaluation
        auto result = engine.process(tc.input);

        bool matched_top1 = false;
        bool matched_top3 = false;

        if (!result.candidates.empty() && result.candidates[0].word == tc.expected) {
            matched_top1 = true;
        }

        for (size_t i = 0; i < result.candidates.size() && i < 3; ++i) {
            if (result.candidates[i].word == tc.expected) {
                matched_top3 = true;
                break;
            }
        }

        if (matched_top1) {
            overall_top1++;
            category_stats[tc.category].top1++;
        } else {
            std::string top_cand = result.candidates.empty() ? "<none>" : result.candidates[0].word;
            failed_top1_details.push_back("[" + tc.category + "] Input: '" + tc.input + "' | Expected: '" +
                                          tc.expected + "' | Got: '" + top_cand + "'");
        }

        if (matched_top3) {
            overall_top3++;
            category_stats[tc.category].top3++;
        }
    }

    // 4. Calculate Latency Percentiles
    std::sort(keystroke_latencies_us.begin(), keystroke_latencies_us.end());
    size_t n_keystrokes = keystroke_latencies_us.size();
    double p50 = 0.0, p95 = 0.0, p99 = 0.0, avg_lat = 0.0;

    if (n_keystrokes > 0) {
        p50 = keystroke_latencies_us[static_cast<size_t>(n_keystrokes * 0.50)];
        p95 = keystroke_latencies_us[static_cast<size_t>(n_keystrokes * 0.95)];
        p99 = keystroke_latencies_us[static_cast<size_t>(n_keystrokes * 0.99)];
        double sum = 0.0;
        for (double d : keystroke_latencies_us) sum += d;
        avg_lat = sum / n_keystrokes;
    }

    // 5. Output Results
    std::cout << "---------------------------------------------------------\n";
    std::cout << " Category Breakdown\n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << std::left << std::setw(24) << "Category"
              << std::setw(8)  << "Total"
              << std::setw(12) << "Top-1 Acc"
              << std::setw(12) << "Top-3 Acc" << "\n";
    std::cout << "---------------------------------------------------------\n";

    for (const auto& pair : category_stats) {
        const std::string& cat = pair.first;
        const auto& stat = pair.second;
        double t1_pct = (stat.total > 0) ? (100.0 * stat.top1 / stat.total) : 0.0;
        double t3_pct = (stat.total > 0) ? (100.0 * stat.top3 / stat.total) : 0.0;

        std::cout << std::left << std::setw(24) << cat
                  << std::setw(8)  << stat.total
                  << std::setw(6)  << stat.top1 << " (" << std::fixed << std::setprecision(1) << std::setw(4) << t1_pct << "%)"
                  << "  " << std::setw(3) << stat.top3 << " (" << std::fixed << std::setprecision(1) << std::setw(4) << t3_pct << "%)\n";
    }

    std::cout << "---------------------------------------------------------\n";
    double overall_t1_pct = (total_words > 0) ? (100.0 * overall_top1 / total_words) : 0.0;
    double overall_t3_pct = (total_words > 0) ? (100.0 * overall_top3 / total_words) : 0.0;

    std::cout << std::left << std::setw(24) << "OVERALL TOTAL"
              << std::setw(8)  << total_words
              << std::setw(6)  << overall_top1 << " (" << std::fixed << std::setprecision(1) << std::setw(4) << overall_t1_pct << "%)"
              << "  " << std::setw(3) << overall_top3 << " (" << std::fixed << std::setprecision(1) << std::setw(4) << overall_t3_pct << "%)\n";
    std::cout << "---------------------------------------------------------\n\n";

    std::cout << "Performance & Latency (over " << n_keystrokes << " keystroke evaluations):\n";
    std::cout << "  • Latency Average: " << std::fixed << std::setprecision(2) << avg_lat << " µs (" << (avg_lat / 1000.0) << " ms)\n";
    std::cout << "  • Latency p50:     " << std::fixed << std::setprecision(2) << p50 << " µs (" << (p50 / 1000.0) << " ms)\n";
    std::cout << "  • Latency p95:     " << std::fixed << std::setprecision(2) << p95 << " µs (" << (p95 / 1000.0) << " ms)\n";
    std::cout << "  • Latency p99:     " << std::fixed << std::setprecision(2) << p99 << " µs (" << (p99 / 1000.0) << " ms)\n";
    std::cout << "  • Final RSS:       " << get_memory_rss_kb() << " KB (" << (get_memory_rss_kb() / 1024.0) << " MB)\n\n";

    // 6. Enforce Performance Budgets (Phase 6 requirement)
    std::cout << "---------------------------------------------------------\n";
    std::cout << " Performance Budget Enforcement (CI Thresholds)\n";
    std::cout << "---------------------------------------------------------\n";
    bool budget_passed = true;

    // Budget 1: Latency p99 < 5.0 ms (5000 µs)
    std::cout << "  • Keystroke Latency (p99): " << std::fixed << std::setprecision(2) << p99 << " µs (Limit: 5000.00 µs) -> ";
    if (p99 <= 5000.0) {
        std::cout << "[PASS]\n";
    } else {
        std::cout << "[FAIL - REGRESSION!]\n";
        budget_passed = false;
    }

    // Budget 2: Startup & load time < 300 ms
    std::cout << "  • Startup & Load Time:     " << std::fixed << std::setprecision(2) << load_ms << " ms (Limit: 300.00 ms) -> ";
    if (load_ms <= 300.0) {
        std::cout << "[PASS]\n";
    } else {
        std::cout << "[FAIL - REGRESSION!]\n";
        budget_passed = false;
    }

    // Budget 3: Memory RSS < 100 MB (102400 KB)
    long final_rss = get_memory_rss_kb();
    std::cout << "  • Resident Memory (RSS):   " << (final_rss / 1024.0) << " MB (Limit: 100.00 MB) -> ";
    if (final_rss <= 102400) {
        std::cout << "[PASS]\n";
    } else {
        std::cout << "[FAIL - REGRESSION!]\n";
        budget_passed = false;
    }
    std::cout << "---------------------------------------------------------\n\n";

    if (!failed_top1_details.empty()) {
        std::cout << "Top-1 Mismatch Highlights (First 15 sample failures):\n";
        for (size_t i = 0; i < failed_top1_details.size() && i < 15; ++i) {
            std::cout << "  " << failed_top1_details[i] << "\n";
        }
        if (failed_top1_details.size() > 15) {
            std::cout << "  ... and " << (failed_top1_details.size() - 15) << " more failures.\n";
        }
    }

    if (!budget_passed) {
        std::cerr << "ERROR: Performance budget exceeded! Failing CI build.\n";
        return 1;
    }

    return 0;
}
