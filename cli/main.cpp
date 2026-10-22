#include "abk/engine.h"
#include "abk/utf8_util.h"
#include "abk/legacy_converter.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/utsname.h>
#include <sys/resource.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

static const char* candidate_type_to_str(abk::CandidateType type) {
    switch (type) {
        case abk::CandidateType::DICTIONARY_EXACT: return "Dict Exact";
        case abk::CandidateType::EXACT_TRANSLITERATION: return "Transliteration";
        case abk::CandidateType::PREFIX_COMPLETION: return "Prefix Match";
        case abk::CandidateType::AUTOCORRECT: return "Autocorrect";
        case abk::CandidateType::PHONETIC_ALTERNATIVE: return "Phonetic Alt";
        case abk::CandidateType::SHORTCUT_EXPANSION: return "Shortcut";
        case abk::CandidateType::NEXT_WORD_PREDICTION: return "Next Word";
        default: return "Candidate";
    }
}

static long get_current_rss_kb() {
#if defined(__linux__)
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
        return usage.ru_maxrss;
    }
#elif defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss / 1024;
    }
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / 1024;
    }
#endif
    return 0;
}

static std::string get_os_info() {
#if defined(__linux__) || defined(__APPLE__)
    struct utsname un;
    if (uname(&un) == 0) {
        return std::string(un.sysname) + " " + un.release + " (" + un.machine + ")";
    }
#elif defined(_WIN32)
    return "Microsoft Windows";
#endif
    return "Unknown OS";
}

static std::string get_compiler_info() {
#if defined(__clang__)
    return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__) + "." + std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "Unknown Compiler";
#endif
}

struct GoldenCase {
    std::string input;
    std::string expected;
    std::string category;
};

static int run_selftest(int argc, char* argv[]) {
    std::string golden_path = "tests/data/golden.tsv";
    std::string dict_path = "data/bangla_wordlist.txt";
    std::string bigram_path = "data/bangla_bigrams.tsv";

    if (argc >= 3) golden_path = argv[2];
    if (argc >= 4) dict_path = argv[3];

    // Fallback search paths for installed binary
    const std::vector<std::string> golden_candidates = {
        golden_path,
        "tests/data/golden.tsv",
        "/usr/share/amar-bangla-kotha/golden.tsv",
        "/usr/local/share/amar-bangla-kotha/golden.tsv"
    };
    for (const auto& p : golden_candidates) {
        std::ifstream f(p);
        if (f.is_open()) {
            golden_path = p;
            break;
        }
    }

    const std::vector<std::string> dict_candidates = {
        dict_path,
        "data/bangla_wordlist.txt",
        "/usr/share/amar-bangla-kotha/bangla_wordlist.txt",
        "/usr/local/share/amar-bangla-kotha/bangla_wordlist.txt"
    };
    for (const auto& p : dict_candidates) {
        std::ifstream f(p);
        if (f.is_open()) {
            dict_path = p;
            break;
        }
    }

    // 1. Initialize Engine
    abk::EngineConfig config;
    config.dictionary_file_path = dict_path;
    config.bigram_model_file_path = bigram_path;
    config.enable_autocorrect = true;
    config.enable_prefix_completion = true;
    config.enable_next_word_prediction = true;

    auto start_load = std::chrono::high_resolution_clock::now();
    abk::AmarBanglaKothaEngine engine(config);
    auto end_load = std::chrono::high_resolution_clock::now();
    double load_ms = std::chrono::duration<double, std::milli>(end_load - start_load).count();

    // 2. Load Golden Test Cases
    std::vector<GoldenCase> golden_cases;
    std::ifstream infile(golden_path);
    if (infile.is_open()) {
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
            std::string inp, exp, cat;
            if (std::getline(ss, inp, '\t') && std::getline(ss, exp, '\t') && std::getline(ss, cat, '\t')) {
                golden_cases.push_back({inp, exp, cat});
            }
        }
    }

    // 3. Evaluate Golden Set
    int top1_count = 0;
    int top3_count = 0;
    std::vector<double> latencies_us;

    // Warm-up
    for (int i = 0; i < 5; ++i) engine.process("ami");

    for (const auto& tc : golden_cases) {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = engine.process(tc.input);
        auto t1 = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        latencies_us.push_back(us);

        bool in_top1 = (!res.candidates.empty() && res.candidates[0].word == tc.expected);
        bool in_top3 = false;
        for (size_t i = 0; i < res.candidates.size() && i < 3; ++i) {
            if (res.candidates[i].word == tc.expected) {
                in_top3 = true;
                break;
            }
        }
        if (in_top1) top1_count++;
        if (in_top3) top3_count++;
    }

    // Sort latencies
    std::sort(latencies_us.begin(), latencies_us.end());
    double avg_lat = 0;
    for (double d : latencies_us) avg_lat += d;
    if (!latencies_us.empty()) avg_lat /= latencies_us.size();
    double p50 = latencies_us.empty() ? 0 : latencies_us[latencies_us.size() * 50 / 100];
    double p95 = latencies_us.empty() ? 0 : latencies_us[latencies_us.size() * 95 / 100];
    double p99 = latencies_us.empty() ? 0 : latencies_us[latencies_us.size() * 99 / 100];
    long rss_kb = get_current_rss_kb();
    double rss_mb = rss_kb / 1024.0;

    // 4. Subsystem verification
    // Subsystem A: Token classification
    bool token_test_pass = (engine.should_bypass_token("https://google.com") == true &&
                            engine.should_bypass_token("user@domain.com") == true &&
                            engine.should_bypass_token("myVariableName") == true &&
                            engine.should_bypass_token("ami") == false &&
                            engine.should_bypass_token("kotha") == false);

    // Subsystem B: Backspace logical unit
    bool backspace_pass = (abk::utf8::pop_logical_unit("কি") == "ক" &&
                           abk::utf8::pop_logical_unit("কে") == "ক" &&
                           abk::utf8::pop_logical_unit("ক্ত") == "ক্");

    // Subsystem C: Bigram Next-Word Prediction
    engine.record_commit("amar", "আমার ", false);
    auto next_preds = engine.get_next_word_predictions(3);
    bool bigram_pass = (!next_preds.empty() && next_preds[0].word == "নাম");

    // Subsystem D: Personal Shortcut
    engine.add_personal_shortcut("thx", "ধন্যবাদ");
    auto sc_res = engine.process("thx");
    bool shortcut_pass = (!sc_res.candidates.empty() && sc_res.candidates[0].word == "ধন্যবাদ");

    // 5. Budget Verdicts
    bool p99_budget_pass = (p99 < 5000.0); // 5 ms limit
    bool startup_budget_pass = (load_ms < 300.0); // 300 ms limit
    bool rss_budget_pass = (rss_mb < 100.0); // 100 MB limit
    bool golden_pass = (golden_cases.empty() || (static_cast<double>(top1_count) / golden_cases.size() >= 0.70));

    bool overall_pass = p99_budget_pass && startup_budget_pass && rss_budget_pass &&
                        token_test_pass && backspace_pass && bigram_pass && shortcut_pass && golden_pass;

    // 6. Print Markdown Report (Copy-pasteable into Bug Reports)
    std::cout << "\n```markdown\n";
    std::cout << "### Amar Bangla Kotha Engine Self-Test Report\n";
    std::cout << "- **Engine Version**: " << "Amar Bangla Kotha v1.0.0 (C++ Core Engine)\n";
    std::cout << "- **Operating System**: " << get_os_info() << "\n";
    std::cout << "- **Compiler**: " << get_compiler_info() << " (C++17)\n";
    std::cout << "- **Built-in Words**: " << engine.get_dictionary().word_count() << "\n";
    std::cout << "- **Startup / Load Time**: " << std::fixed << std::setprecision(2) << load_ms << " ms "
              << (startup_budget_pass ? "[PASS]" : "[FAIL]") << "\n";
    std::cout << "- **Resident Memory (RSS)**: " << rss_mb << " MB "
              << (rss_budget_pass ? "[PASS]" : "[FAIL]") << "\n";
    std::cout << "\n#### Golden Dataset Benchmark:\n";
    std::cout << "- **Total Cases**: " << golden_cases.size() << "\n";
    if (!golden_cases.empty()) {
        double top1_pct = (100.0 * top1_count) / golden_cases.size();
        double top3_pct = (100.0 * top3_count) / golden_cases.size();
        std::cout << "- **Top-1 Accuracy**: " << top1_count << "/" << golden_cases.size()
                  << " (" << std::setprecision(1) << top1_pct << "%)\n";
        std::cout << "- **Top-3 Accuracy**: " << top3_count << "/" << golden_cases.size()
                  << " (" << std::setprecision(1) << top3_pct << "%)\n";
    }
    std::cout << "\n#### Latency Profile:\n";
    std::cout << "- **Latency (avg)**: " << std::setprecision(2) << avg_lat << " µs\n";
    std::cout << "- **Latency (p50)**: " << p50 << " µs\n";
    std::cout << "- **Latency (p95)**: " << p95 << " µs\n";
    std::cout << "- **Latency (p99)**: " << p99 << " µs "
              << (p99_budget_pass ? "[PASS]" : "[FAIL]") << "\n";
    std::cout << "\n#### Core Subsystems:\n";
    std::cout << "- **Token Classifier & Bypass**: " << (token_test_pass ? "PASS" : "FAIL") << "\n";
    std::cout << "- **Backspace Unit & Kar Popping**: " << (backspace_pass ? "PASS" : "FAIL") << "\n";
    std::cout << "- **Next-Word Prediction (Bigram)**: " << (bigram_pass ? "PASS" : "FAIL") << "\n";
    std::cout << "- **Shortcut & Snippet Expansion**: " << (shortcut_pass ? "PASS" : "FAIL") << "\n";
    std::cout << "\n#### Overall Status:\n";
    std::cout << "- **Self-Test Verdict**: " << (overall_pass ? "**ALL CHECKS PASSED (HEALTHY)**" : "**FAIL (REGRESSION DETECTED)**") << "\n";
    std::cout << "```\n\n";

    return overall_pass ? 0 : 1;
}

int main(int argc, char* argv[]) {
    // Check for --selftest flag
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--selftest" || arg == "-s" || arg == "--test") {
            return run_selftest(argc, argv);
        }
        if (arg == "--convert-ansi" || arg == "-ca") {
            std::string input;
            if (i + 1 < argc) {
                std::string target = argv[i + 1];
                std::ifstream f(target);
                if (f.is_open()) {
                    std::stringstream ss;
                    ss << f.rdbuf();
                    input = ss.str();
                } else {
                    input = target;
                }
            } else {
                std::stringstream ss;
                ss << std::cin.rdbuf();
                input = ss.str();
            }
            std::cout << abk::LegacyConverter::convert_ansi_to_unicode(input) << "\n";
            return 0;
        }
        if (arg == "--convert-unicode" || arg == "-cu") {
            std::string input;
            if (i + 1 < argc) {
                std::string target = argv[i + 1];
                std::ifstream f(target);
                if (f.is_open()) {
                    std::stringstream ss;
                    ss << f.rdbuf();
                    input = ss.str();
                } else {
                    input = target;
                }
            } else {
                std::stringstream ss;
                ss << std::cin.rdbuf();
                input = ss.str();
            }
            std::cout << abk::LegacyConverter::convert_unicode_to_ansi(input) << "\n";
            return 0;
        }
    }

    std::cout << "==========================================================\n";
    std::cout << "  Amar Bangla Kotha (আমার বাংলা কথা) - C++ Core Engine   \n";
    std::cout << "  Cross-Platform Keyboard Engine (Linux, macOS, Windows) \n";
    std::cout << "==========================================================\n";
    std::cout << "Options:\n";
    std::cout << "  --selftest    - Run automated self-test & print bug report\n";
    std::cout << "Commands:\n";
    std::cout << "  :dict <file>  - Load external wordlist dictionary\n";
    std::cout << "  :learn <word> - Learn/save a new Bengali word\n";
    std::cout << "  :bench        - Run engine benchmark on common phrases\n";
    std::cout << "  :quit or :q   - Exit playground\n";
    std::cout << "----------------------------------------------------------\n";

    abk::EngineConfig config;
    config.max_suggestions = 7;
    config.enable_autocorrect = true;
    config.enable_prefix_completion = true;

    if (argc > 1 && argv[1][0] != '-') {
        config.dictionary_file_path = argv[1];
    }

    abk::AmarBanglaKothaEngine engine(config);
    std::cout << "Initialized with " << engine.get_dictionary().word_count() << " built-in words.\n\n";

    std::string line;
    while (true) {
        std::cout << "ABK > ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line.empty()) continue;

        if (line == ":q" || line == ":quit" || line == "exit") {
            std::cout << "Shubho biday! (Goodbye!)\n";
            break;
        }

        if (line.rfind(":dict ", 0) == 0) {
            std::string path = line.substr(6);
            if (engine.load_dictionary_file(path)) {
                std::cout << "Successfully loaded dictionary from: " << path << "\n";
                std::cout << "Total vocabulary words: " << engine.get_dictionary().word_count() << "\n\n";
            } else {
                std::cout << "Error: Could not open dictionary file: " << path << "\n\n";
            }
            continue;
        }

        if (line.rfind(":ansi ", 0) == 0) {
            std::string text = line.substr(6);
            std::cout << "Unicode: " << abk::LegacyConverter::convert_ansi_to_unicode(text) << "\n\n";
            continue;
        }

        if (line.rfind(":unicode ", 0) == 0) {
            std::string text = line.substr(9);
            std::cout << "Bijoy ANSI: " << abk::LegacyConverter::convert_unicode_to_ansi(text) << "\n\n";
            continue;
        }

        if (line.rfind(":learn ", 0) == 0) {
            std::string word = line.substr(7);
            engine.learn_word(word, 500);
            std::cout << "Learned word: '" << word << "' (added to engine vocabulary)\n\n";
            continue;
        }

        if (line == ":bench") {
            const std::vector<std::string> bench_samples = {
                "ami", "amar", "bangla", "kotha", "dhonnobad",
                "manush", "shundor", "bhalo", "bangladesh", "dhaka"
            };
            std::cout << "\nRunning Transliteration & Suggestion Benchmark (10,000 iterations)...\n";
            auto start = std::chrono::high_resolution_clock::now();
            size_t total_ops = 10000;
            for (size_t i = 0; i < total_ops; i++) {
                engine.process(bench_samples[i % bench_samples.size()]);
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            double avg_us = static_cast<double>(duration_us) / static_cast<double>(total_ops);
            std::cout << "Total time: " << duration_us / 1000.0 << " ms\n";
            std::cout << "Average processing latency per keystroke: " << avg_us << " microseconds ("
                      << static_cast<int>(1000000.0 / avg_us) << " ops/sec)\n\n";
            continue;
        }

        // Process user input
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine.process(line);
        auto end = std::chrono::high_resolution_clock::now();
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        std::cout << "\n  Input   : " << result.raw_input << "\n";
        std::cout << "  Primary : " << result.primary_transliteration
                  << " (" << latency_us << " µs)\n";
        std::cout << "  Suggestions (" << result.candidates.size() << "):\n";
        for (size_t i = 0; i < result.candidates.size(); i++) {
            const auto& cand = result.candidates[i];
            std::cout << "    [" << (i + 1) << "] " << cand.word
                      << "  (score: " << static_cast<int>(cand.score)
                      << ", " << candidate_type_to_str(cand.type) << ")\n";
        }
        std::cout << "\n";
    }

    return 0;
}
