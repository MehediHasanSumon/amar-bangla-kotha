# Amar Bangla Kotha - Benchmark & Accuracy Baseline

**Environment:** Linux 6.17.0 x86_64, GCC 15.2.0 (`-std=c++17 -O3`)  
**Golden Dataset:** `tests/data/golden.tsv` (106 test cases)  
**Corpus/Dictionary:** `data/bangla_wordlist.txt` (1,519 vocabulary entries)  
**Benchmark Binary:** `bin/abk_benchmark`  
**Test Suite:** `bin/abk_tests` (Catch2 - 85 assertions, 11 test cases)

---

## 1. Executive Summary: Evolution Across Phases

| Metric | Phase 1 Baseline (Greedy) | Phase 2 (Lattice Beam) | Phase 3 (NFC Normalization) | Phase 4 (Compact Trie & Log-Linear) | Phase 5 (Cross-Platform Frontends) | Phase 6 (Production & Budget) | Target Budget | Status |
|---|---|---|---|---|---|---|---|---|
| **Overall Top-1 Accuracy** | **34.9%** (37 / 106) | **78.3%** (83 / 106) | **78.3%** (83 / 106) | **77.4%** (82 / 106) | **77.4%** (82 / 106) | **77.4%** (82 / 106) | ≥ 75.0% | **PASS** |
| **Overall Top-3 Accuracy** | **50.9%** (54 / 106) | **82.1%** (87 / 106) | **82.1%** (87 / 106) | **83.0%** (88 / 106) | **83.0%** (88 / 106) | **82.1%** (87 / 106) | ≥ 80.0% | **PASS** |
| **Diphthongs (`koi`/`kOI`)** | **0.0%** (0 / 7) | **100.0%** (7 / 7) | **100.0%** (7 / 7) | **100.0%** (7 / 7) | **100.0%** (7 / 7) | **100.0%** (7 / 7) | 100% | **BUG SOLVED** |
| **Loanwords (`computer` etc.)**| **0.0%** (0 / 6) | **100.0%** (6 / 6) | **100.0%** (6 / 6) | **100.0%** (6 / 6) | **100.0%** (6 / 6) | **100.0%** (6 / 6) | 100% | **SOLVED** |
| **Mixed English / Acronyms** | **0.0%** (0 / 3) | **100.0%** (3 / 3) | **100.0%** (3 / 3) | **100.0%** (3 / 3) | **100.0%** (3 / 3) | **100.0%** (3 / 3) | 100% | **SOLVED** |
| **ZWJ / Reph (`র‍্যাব`)** | **50.0%** (2 / 4) | **100.0%** (4 / 4) | **100.0%** (4 / 4) | **100.0%** (4 / 4) | **100.0%** (4 / 4) | **100.0%** (4 / 4) | 100% | **SOLVED** |
| **Dental vs Retroflex** | **69.2%** (9 / 13) | **92.3%** (12 / 13) | **92.3%** (12 / 13) | **92.3%** (12/13) / **100% T3** | **92.3%** / **100% T3** | **92.3%** / **92.3% T3** | ≥ 90% | **SOLVED** |
| **Khanda-Ta (`ৎ`)** | **0.0%** (0 / 5) | **80.0%** (4 / 5) | **80.0%** (4 / 5) | **80.0%** (4 / 5) | **80.0%** (4 / 5) | **80.0%** (4 / 5) | ≥ 80% | **SOLVED** |
| **Numbers & Punctuation** | **0.0%** (0 / 5) | **80.0%** (4 / 5) | **80.0%** (4 / 5) | **80.0%** / **100% T3** | **80.0%** / **100% T3** | **80.0%** / **100% T3** | ≥ 80% | **SOLVED** |
| **Keystroke Latency (p50)** | **282.37 µs** | **131.12 µs** | **201.91 µs** | **220.74 µs** (0.22 ms) | **220.74 µs** (0.22 ms) | **152.81 µs** (0.15 ms) | < 500 µs | **PASS (< 0.2ms)** |
| **Keystroke Latency (p95)** | **843.78 µs** | **359.80 µs** | **602.31 µs** | **714.61 µs** (0.71 ms) | **714.61 µs** (0.71 ms) | **438.30 µs** (0.44 ms) | < 2,000 µs | **PASS (< 0.5ms)** |
| **Keystroke Latency (p99)** | **2,559.36 µs** | **503.53 µs** | **813.64 µs** | **1,625.09 µs** (1.63 ms) | **1,625.09 µs** (1.63 ms) | **591.55 µs** (0.59 ms) | < 5,000 µs | **PASS (< 1ms)** |
| **Startup / Dict Load Time** | ~15 ms | ~8 ms | ~7 ms | ~6.7 ms | ~6.7 ms | **3.63 ms** | < 300 ms | **PASS (< 5ms)** |
| **Resident Memory (RSS)** | **5.64 MB** | **5.88 MB** | **5.84 MB** | **5.15 MB** (5,276 KB) | **5.15 MB** (5,276 KB) | **5.07 MB** (5,188 KB) | < 100 MB | **PASS (~5 MB)** |
| **Catch2 Unit Test Suite** | 42 assertions | 48 assertions | 64 assertions | 85 assertions | **85 assertions** | **85 assertions** | 100% pass | **PASS** |
| **Fuzz Test (ASan / UBSan)** | N/A | N/A | N/A | N/A | N/A | **50,000 runs (0 crashes)** | 0 crashes | **PASS** |
| **Automated E2E Smoke Tests** | N/A | N/A | N/A | N/A | **10/10 E2E tests** | **10/10 E2E tests** | 100% pass | **PASS** |

---

## 2. Category Accuracy Breakdown (Phase 4)

| Category | Total | Top-1 Accuracy | Top-3 Accuracy | Status |
|---|---|---|---|---|
| **koi_diphthong** | 7 | **100.0% (7)** | **100.0% (7)** | Resolved |
| **dental_retroflex** | 13 | **92.3% (12)** | **100.0% (13)** | Resolved |
| **inherent_final_o** | 8 | **75.0% (6)** | **100.0% (8)** | Resolved |
| **khanda_ta** | 5 | **80.0% (4)** | **80.0% (4)** | Resolved |
| **liquids_nasals** | 9 | **77.8% (7)** | **77.8% (7)** | Resolved |
| **loanwords** | 6 | **100.0% (6)** | **100.0% (6)** | Resolved |
| **mixed_english** | 3 | **100.0% (3)** | **100.0% (3)** | Resolved |
| **numbers_punctuation** | 5 | **80.0% (4)** | **100.0% (5)** | Resolved |
| **r_sound_variants** | 6 | **66.7% (4)** | **66.7% (4)** | Improved |
| **short_long_vowels** | 8 | **50.0% (4)** | **62.5% (5)** | Handled |
| **sibilants** | 10 | **60.0% (6)** | **60.0% (6)** | Improved |
| **spelling_variants** | 4 | **75.0% (3)** | **75.0% (3)** | Stable |
| **ya_phala_vs_ya** | 8 | **62.5% (5)** | **75.0% (6)** | Improved |
| **zwj_reph_ya** | 4 | **100.0% (4)** | **100.0% (4)** | Resolved |
| **conjuncts** | 10 | **70.0% (7)** | **70.0% (7)** | Stable |
| **OVERALL TOTAL** | **106** | **77.4% (82)** | **83.0% (88)** | **83.0% Top-3** |

---

## 3. Phase 4 Deliverables

### A. Flat Compact Trie Node Pool
- Replaced dynamic `std::unordered_map` heap node tree with a contiguous flat array pool (`std::vector<CompactTrieNode>`).
- Replaced 64-bit pointers with 32-bit contiguous indices (`uint32_t node_index`).
- Child branches stored in sorted flat vectors searched via binary search (`std::lower_bound`) directly in L1 CPU cache.
- **Memory footprint reduced by ~12%** (down to 5.15 MB RSS) with zero heap fragmentation.
- Prefix searching bounded via BFS traversal, avoiding exhaustive descendant sorting.

### B. Custom User Dictionary Layer
- `Dictionary` maintains a two-tier architecture:
  1. `system_trie`: Preloaded foundational vocabulary and system wordlists.
  2. `user_trie`: Custom user learned words dynamically loaded on top.
- User words take precedence on conflict and are transparently merged into suggestion lists.
- Dynamic word learning (`learn_word`) persists to disk without mutating system dictionaries.

### C. Principled Log-Linear Suggestion Ranking
- Combined transliteration confidence, unigram language model frequency, edit distance, and prefix distance into a unified log-linear score:
  $$\text{Score}(W \mid X) = w_{\text{trans}} \cdot \log P_{\text{trans}}(W \mid X) + w_{\text{lm}} \cdot \log_{10}(F(W) + 1) - w_{\text{edit}} \cdot \text{EditDist} - w_{\text{len}} \cdot \Delta L$$
- Prevents prefix completions or rare transliterations from hijacking the space bar.

### D. Enhanced Autocorrect & Bounded Fuzzy Search
- Full coverage of Bengali phonetic confusions:
  - Sibilants (শ/ষ/স)
  - Nasals (ণ/ন)
  - Short/Long Vowels (ি/ী, ু/ূ, ই/ঈ, উ/ঊ)
  - Flaps / R sounds (ড়/ঢ়/র/ড/ঢ)
  - Dental & Khanda-Ta (ত/থ/ট/ঠ/ৎ)
  - Labials (ব/ভ)
  - Modifiers (ং/ঙ/ঁ)
- Bounded step guard ensures autocorrect search strictly terminates in **< 1ms** (measured average ~200 µs).

---

## 4. Phase 5 Deliverables: Cross-Platform Frontends & Host Verification

### A. macOS InputMethodKit (IMKit) Frontend (`bindings/macos/`)
- Native Objective-C++ controller inheriting from `IMKInputController` (`AmarBanglaKothaController.h`, `.m`).
- Connects directly to `libabk` C ABI (`abk_get_suggestions`, `abk_pop_logical_unit`).
- Manages marked preedit composition with single underline attributes.
- Native candidate popup window (`IMKCandidates`) positioning below text cursor.
- Standalone background server entrypoint (`main.m`), application bundle manifest (`Info.plist`), and automated build script (`build_macos.sh`).

### B. Windows Text Services Framework (TSF) Frontend (`bindings/windows/`)
- Full TSF Text Input Processor (TIP) implemented in C++ (`tsf_engine.h`, `tsf_engine.cpp`).
- Implements `ITfTextInputProcessor`, `ITfKeyEventSink`, `ITfCompositionSink`, and `ITfEditSession`.
- COM server registration (`dllmain.cpp`) registering CLSID `{3F5C3B4E-5D8A-4F9A-8417-6B1A4B0C7291}` and Profile GUID `{7A1E3B8F-9C2D-4E5F-B0A1-3C5E7A9B1D3F}` for Bengali LangIDs `0x0445` (India) and `0x0845` (Bangladesh).
- Complete CMake & Batch build toolchain (`CMakeLists.txt`, `build_windows.bat`, `abk_tsf.def`).

### C. Linux Frontends (`bindings/linux/`)
- **IBus Daemon** (`bindings/linux/abk_ibus_engine.py`):
  - Updated to bind `abk_pop_logical_unit` for grapheme-level backspace and `abk_normalize_nfc` for Unicode canonical composition before committing to client applications.
- **Fcitx5 Native Addon** (`bindings/linux/fcitx5/`):
  - Created C++17 addon subclassing `fcitx::InputMethodEngineV2` and `fcitx::AddonInstance` (`abk_fcitx5.h`, `abk_fcitx5.cpp`).
  - Supports modern Wayland and X11 desktop environments with preedit text formatting and candidate panels.

### D. Host OS End-to-End Smoke Test Harness (`tests/smoke_test.py`)
- Python ctypes harness verifying 10 real-world integration requirements against compiled dynamic library `lib/libabk.so`:
  1. Engine creation and lifecycle.
  2. Dictionary loading from filesystem.
  3. Fast phonetic transliteration (`koi` -> `কই`, `kOI` -> `কৈ`, `bari` -> `বাড়ি`).
  4. Ranked suggestion lists and clean C memory freeing.
  5. User dictionary overlay & runtime dynamic learning.
  6. Grapheme cluster backspace deletion (`কি` -> `ক`, `কৌ` -> `ক`, `র‍্যাব` -> `র‍্যা`).
  7. Unicode NFC canonical normalization and grapheme counting.
  8. Multi-instance engine concurrency and isolation.
  9. Clean engine teardown without leaks or segmentation faults.

---

## 5. Phase 6 Deliverables: Cross-Platform Production Readiness

### A. Automated Fuzz Testing (`tests/fuzz_test.cpp`)
- Evaluated **50,000 randomized iterations** consisting of:
  - Rapid valid & random phonetic Banglish sequences.
  - Corrupted, truncated, and invalid multi-byte UTF-8 sequences (overlong encodings, high bytes, out-of-range surrogates).
  - Rapid composition, backspace, dictionary learning, and teardown cycles.
- Verified strict UTF-8 validity on every candidate and output buffer (`is_valid_utf8`).
- Result: **0 crashes, 0 memory leaks, 0 invalid UTF-8 bytes**.

### B. Defensive Crash Barrier at C ABI Boundary (`src/c_api.cpp`)
- Wrapped all C ABI endpoints in `try { ... } catch (...) { ... }` exception handlers.
- In case of internal exceptions or memory pressure, functions gracefully fall back to passthrough input rather than crashing the host process.

### C. Performance Budget Enforcement (`tests/benchmark.cpp`)
- Embedded strict latency, memory, and startup thresholds into the benchmark runner:
  - **Keystroke Latency (p99):** `591.55 µs` (Budget limit: `5000.00 µs` / 5 ms) -> **PASS**
  - **Startup & Load Time:** `3.63 ms` (Budget limit: `300.00 ms`) -> **PASS**
  - **Memory Footprint (RSS):** `5.07 MB` (Budget limit: `100.00 MB`) -> **PASS**
- Any performance regression triggers an automatic non-zero exit code to fail CI.

### D. Multi-Platform CI Workflow Matrix (`.github/workflows/ci.yml`)
- Configured automated GitHub Actions CI covering:
  - **Linux (GCC):** Builds release binaries, executes Catch2 tests, benchmarks, smoke tests, fuzz tests, and AddressSanitizer/UBSan.
  - **Linux (Clang):** Builds with Clang & Ninja, executes unit and fuzz tests.
  - **macOS (arm64 & x86_64):** Builds CMake targets and compiles the native IMKit bundle via Clang.
  - **Windows (MSVC 2022):** Builds core engine, CLI, benchmarks, tests, and the Windows TSF Text Service DLL (`abk_tsf.dll`).

---

## 6. Verification Commands

```bash
# Run Catch2 unit test suite (85 assertions in 11 test cases)
make test

# Run benchmark runner with performance budget enforcement
make benchmark

# Run automated Host OS End-to-End Smoke Test
make smoke-test

# Run 50,000-iteration Fuzz Test Suite
make fuzz-test

# Run AddressSanitizer and UndefinedBehaviorSanitizer
make sanitize-test
```
