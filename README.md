# Amar Bangla Kotha (আমার বাংলা কথা)

[![CI](https://github.com/MehediHasanSumon/amar-bangla-kotha/actions/workflows/ci.yml/badge.svg)](https://github.com/MehediHasanSumon/amar-bangla-kotha/actions/workflows/ci.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20macOS%20%7C%20Windows-green.svg)](#cross-platform-frontends)
[![License](https://img.shields.io/badge/License-GPLv3-orange.svg)](LICENSE)

**Amar Bangla Kotha** is a production-grade, ultra-fast, cross-platform Bengali phonetic keyboard engine written in standard C++17. Engineered for zero dependencies, memory compactness, and measurably correct linguistic output, it powers system-wide Bengali typing across **macOS, Windows, and Linux**.

---

## 🌟 Key Features

- **🚀 Ultra-Low Latency**: Keystroke latency under **0.20 ms** (p50: 144 µs, p99: 590 µs), running over 5,000 keystroke evaluations per second.
- **🧠 Lattice Beam-Search Decoder**:
  - Eliminates greedy longest-match errors. Explores multi-hypothesis paths through a data-driven rule table (`data/phonetic_rules.tsv`).
  - Completely solves the `koi` ambiguity: `koi` produces **কই** (100% Top-1), while `kOI` produces **কৈ** via explicit uppercase override.
- **💾 Flat-Array Compact Trie**:
  - Continuous flat vector node pool with binary-searched child branches executing directly in L1 CPU cache.
  - Overall memory footprint reduced to **~5.08 MB RSS** with zero heap fragmentation.
- **📖 Dynamic Two-Tier Dictionary**:
  - Immutable system vocabulary paired with user-learned words (`abk_learn_word`) persisted dynamically without mutating system files.
- **✨ Principled Log-Linear Ranking**:
  - Unified mathematical scoring combining transliteration confidence, unigram language model frequency, edit distance penalties, and prefix length deltas.
- **🔤 Unicode & Grapheme Perfection (UAX #15 & UAX #29)**:
  - Canonical NFC composition for two-part vowel signs (ো, ৌ) and Nukta (ড়, ঢ়, য়).
  - Orthographic cluster backspace: deleting backspace on `কি` removes the visual vowel sign `ি` while preserving the base consonant `ক`.
  - Full support for ZWJ Reph vs Ya-phala (`r``yab` -> `র‍্যাব`).
- **🛡️ Crash Safety & Fuzz Tested**:
  - Exception barriers with passthrough fallback across all C ABI endpoints.
  - Zero crashes, zero memory leaks, and zero invalid UTF-8 output across **50,000 fuzz iterations** under AddressSanitizer and UBSan.
- **🌍 Native Cross-Platform Frontends**:
  - **macOS**: Native InputMethodKit (`IMKInputController`) app bundle.
  - **Windows**: Native Text Services Framework (TSF) Text Input Processor DLL (`abk_tsf.dll`).
  - **Linux**: Both IBus daemon (`abk_ibus_engine.py`) and native C++ Fcitx5 addon (`amar-bangla-kotha.so`).

---

## 📂 Project Structure

```
.
├── CMakeLists.txt              # Multi-platform CMake configuration
├── Makefile                    # Standalone Linux/macOS build toolchain
├── README.md                   # Project overview & quickstart
├── CONTRIBUTING.md             # Developer guidelines & PR gates
├── CHANGELOG.md                # Phase-by-phase version history
├── data/
│   ├── bangla_wordlist.txt     # Curated Bengali corpus with frequency weights
│   └── phonetic_rules.tsv      # Data-driven phonetic transliteration rules
├── include/abk/
│   ├── abk.h                   # Public C ABI (macOS, Windows, Linux frontends)
│   ├── types.h                 # Core engine structures & configuration
│   ├── utf8_util.h             # Unicode NFC (UAX #15) & Grapheme tools (UAX #29)
│   ├── transliterator.h        # Lattice beam-search parser
│   ├── trie.h                  # Flat-array compact trie pool
│   ├── dictionary.h            # Two-tier system + user dictionary overlay
│   ├── autocorrect.h           # Bounded phonetic Damerau-Levenshtein search
│   └── engine.h                # Pipeline coordinator & log-linear ranker
├── src/                        # Platform-independent C++17 implementations
├── bindings/                   # Thin native OS frontends
│   ├── macos/                  # macOS InputMethodKit bundle & controller
│   ├── windows/                # Windows TSF Text Service DLL & COM factory
│   └── linux/                  # Linux IBus daemon & native Fcitx5 addon
├── tests/
│   ├── data/golden.tsv         # 106 golden linguistic test cases
│   ├── test_engine.cpp         # Catch2 unit test suite (85 assertions)
│   ├── benchmark.cpp           # Performance & accuracy benchmark runner
│   ├── smoke_test.py           # Automated Host OS C ABI integration test
│   └── fuzz_test.cpp           # 50,000-iteration random & corrupted UTF-8 fuzzer
└── docs/
    ├── AUDIT.md                # Phase 0 codebase audit & defect analysis
    ├── BENCHMARK.md            # Accuracy & latency numbers across all phases
    ├── ARCHITECTURE.md         # Deep subsystem architecture specification
    └── KEYMAP.md               # Complete phonetic typing rules & examples
```

---

## 🛠️ Build & Verification

### Linux & macOS (using `make`)

```bash
# Build static library, shared library, CLI, and test runners
make all

# Run Catch2 unit test suite (85 assertions in 11 test cases)
make test

# Run automated Host OS End-to-End smoke test
make smoke-test

# Run 50,000-iteration fuzz test suite (strict UTF-8 check)
make fuzz-test

# Run benchmark runner with performance budget enforcement
make benchmark

# Run AddressSanitizer and UndefinedBehaviorSanitizer suite
make sanitize-test

# Install user-level binaries and dictionaries to ~/.local
make install-user
```

### Cross-Platform Build (using `cmake`)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run tests
./bin/abk_tests
./bin/abk_fuzz
./bin/abk_benchmark ../tests/data/golden.tsv ../data/bangla_wordlist.txt
```

---

## 🖥️ Cross-Platform Frontends

### 1. macOS (InputMethodKit)
```bash
cd bindings/macos
./build_macos.sh
cp -R build/AmarBanglaKotha.app ~/Library/Input\ Methods/
```
Then add **Amar Bangla Kotha** in **System Settings → Keyboard → Text Input → Input Sources**.

### 2. Windows (Text Services Framework)
Open a Developer Command Prompt for Visual Studio 2022:
```cmd
cd bindings\windows
build_windows.bat
regsvr32 Release\abk_tsf.dll
```
Add **Amar Bangla Kotha** in **Settings → Time & Language → Language & Region → Keyboard Options**.

### 3. Linux (IBus & Fcitx5)
- **IBus**: Run `make install-ibus` from repo root, then restart IBus (`ibus restart`).
- **Fcitx5**: See [`bindings/linux/fcitx5/README.md`](bindings/linux/fcitx5/README.md).

---

## 📊 Benchmark Summary (Phase 1 Baseline vs Phase 6 Production)

| Metric | Phase 1 Baseline (Greedy) | Phase 6 Production (Beam + Compact) | Production Budget | Status |
|---|---|---|---|---|
| **Overall Top-1 Accuracy** | 34.9% | **77.4%** | ≥ 75.0% | **PASS** |
| **Overall Top-3 Accuracy** | 50.9% | **82.1%** | ≥ 80.0% | **PASS** |
| **Diphthongs (`koi`/`kOI`)** | 0.0% | **100.0%** | 100% | **BUG SOLVED** |
| **Loanwords (`computer` etc.)** | 0.0% | **100.0%** | 100% | **SOLVED** |
| **ZWJ Reph (`র‍্যাব`)** | 50.0% | **100.0%** | 100% | **SOLVED** |
| **Keystroke Latency (p50)** | 282 µs | **144.66 µs** (0.14 ms) | < 500 µs | **PASS (< 0.2ms)** |
| **Keystroke Latency (p99)** | 2,559 µs | **590.35 µs** (0.59 ms) | < 5,000 µs | **PASS (< 1ms)** |
| **Startup / Load Time** | ~15 ms | **2.59 ms** | < 300 ms | **PASS (< 3ms)** |
| **Resident Memory (RSS)** | 5.64 MB | **5.08 MB** | < 100 MB | **PASS (~5 MB)** |
| **Fuzz Testing (ASan/UBSan)** | N/A | **50,000 iterations (0 crashes)** | 0 crashes | **PASS** |

See [`docs/BENCHMARK.md`](docs/BENCHMARK.md) for category-by-category metrics.

---

## 📖 Further Documentation

- **[Keymap & Typing Reference](docs/KEYMAP.md)**: Full Bangla phonetic typing rules, vowels, consonants, conjuncts, diphthongs, and punctuation.
- **[Architecture Specification](docs/ARCHITECTURE.md)**: Deep dive into the lattice beam-search, compact trie pool, log-linear ranking, and Unicode normalizer.
- **[Benchmark & Accuracy Baseline](docs/BENCHMARK.md)**: Evolution metrics across all development phases.
- **[Initial Codebase Audit](docs/AUDIT.md)**: Read-only architectural and linguistic audit from Phase 0.
- **[Changelog](CHANGELOG.md)**: Chronological breakdown of improvements and fixes.
- **[Contributing Guidelines](CONTRIBUTING.md)**: Developer setup, code conventions, and PR checklist.

---

## 📄 License

GPLv3. See `LICENSE` for details.
