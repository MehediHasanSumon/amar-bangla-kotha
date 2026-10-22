#include "abk/engine.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

static const char* candidate_type_to_str(abk::CandidateType type) {
    switch (type) {
        case abk::CandidateType::DICTIONARY_EXACT: return "Dict Exact";
        case abk::CandidateType::EXACT_TRANSLITERATION: return "Transliteration";
        case abk::CandidateType::PREFIX_COMPLETION: return "Prefix Match";
        case abk::CandidateType::AUTOCORRECT: return "Autocorrect";
        case abk::CandidateType::PHONETIC_ALTERNATIVE: return "Phonetic Alt";
        default: return "Candidate";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "==========================================================\n";
    std::cout << "  Amar Bangla Kotha (আমার বাংলা কথা) - C++ Core Engine   \n";
    std::cout << "  Cross-Platform Keyboard Engine (Linux, macOS, Windows) \n";
    std::cout << "==========================================================\n";
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

    if (argc > 1) {
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
                  << "  (" << latency_us << " µs)\n";

        if (result.candidates.empty()) {
            std::cout << "  Candidates: [None]\n\n";
        } else {
            std::cout << "  Candidates:\n";
            for (size_t i = 0; i < result.candidates.size(); i++) {
                const auto& cand = result.candidates[i];
                std::cout << "    [" << (i + 1) << "] "
                          << std::left << std::setw(18) << cand.word
                          << " (Score: " << std::fixed << std::setprecision(1) << cand.score << ")"
                          << " [" << candidate_type_to_str(cand.type) << "]\n";
            }
            std::cout << "\n";
        }
    }

    return 0;
}
