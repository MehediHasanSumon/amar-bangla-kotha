# Changelog: Amar Bangla Kotha (আমার বাংলা কথা)

All notable changes to the **Amar Bangla Kotha** project are documented in this file.

---

## [1.0.0] - Production Release - 2026-10-22

### Summary of Completed Engineering Phases

#### Phase 0: Codebase Audit & Architectural Mapping
- Performed an exhaustive read-only audit of the original codebase (`docs/AUDIT.md`).
- Identified core architectural bottlenecks: 1-pass greedy longest-match parser, lack of Unicode NFC canonicalization, absence of visual-vs-logical vowel sign handling, and memory fragmentation.
- Formulated ranked P0/P1/P2 defect matrix.

#### Phase 1: Benchmark Harness & Golden Baseline
- Integrated Catch2 v2.13.10 unit test runner (`tests/catch2/catch.hpp`, `tests/catch_main.cpp`).
- Created a comprehensive 106-case golden evaluation dataset (`tests/data/golden.tsv`) covering 15 phonetic and orthographic categories:
  - Diphthongs (`koi`/`kOI`), Dental/Retroflex (`ত`/`ট`, `থ`/`ঠ`, `দ`/`ড`, `ধ`/`ঢ`, `ন`/`ণ`), Sibilants (`স`/`শ`/`ষ`), ZWJ Reph (`র‍্যাব`), Khanda-Ta (`ৎ`), Conjuncts, Numbers, and Punctuation.
- Built automated benchmark runner (`tests/benchmark.cpp`, `bin/abk_benchmark`) reporting Top-1/Top-3 accuracy, keystroke latency percentiles (p50/p95/p99), and resident memory footprint (RSS).
- Established baseline numbers in `docs/BENCHMARK.md` (Top-1: 34.9%, Top-3: 50.9%, RSS: 5.64 MB).

#### Phase 2: Core Parser Rewrite (Lattice Beam Search)
- Replaced greedy longest-match with a directed acyclic graph (lattice) beam-search parser (`include/abk/transliterator.h`, `src/transliterator.cpp`).
- Extracted hardcoded rules into an external data-driven rule table (`data/phonetic_rules.tsv`) supporting context rules (`START`, `AFTER_CONSONANT`, `AFTER_VOWEL`, `END`, `ANY`).
- **Fixed the infamous `koi` bug**:
  - `koi` produces **কই** (100% Top-1, weighted by unigram corpus frequency).
  - `kOI` explicitly produces **কৈ** via uppercase override convention.
- Top-1 accuracy surged from **34.9% to 78.3%**; Top-3 accuracy reached **82.1%**.

#### Phase 3: Unicode Normalization & Grapheme Editing
- Implemented Unicode UAX #15 Canonical Composition (NFC) in `src/utf8_util.cpp`:
  - Normalized multi-part vowel signs (`ো`, `ৌ`) and canonicalized Nukta combinations (`ড়`, `ঢ়`, `য়`).
- Implemented ZWJ and ZWNJ handling for Ya-phala vs Reph (`r``yab` -> `র‍্যাব` with `\u200D`).
- Implemented orthographic cluster-aware backspace deletion (`abk_pop_logical_unit`):
  - Backspacing on `কি` removes the visual vowel sign `ি`, preserving the base consonant `ক`.
  - Backspacing on `কৌ` removes two-part kar `ৌ`, leaving `ক`.
- Expanded C ABI in `include/abk/abk.h` and `src/c_api.cpp` with `abk_normalize_nfc`, `abk_pop_logical_unit`, and `abk_grapheme_count`.

#### Phase 4: Compact Trie Pool & Principled Log-Linear Ranking
- Replaced pointer-heavy heap node trees (`std::unordered_map`) with a contiguous flat array node pool (`std::vector<CompactTrieNode>`).
- Implemented binary-search child lookup in L1 CPU cache (`std::lower_bound`), reducing memory to **5.08 MB RSS**.
- Built a two-tier user dictionary overlay (`system_trie` + `user_trie`), enabling persistent dynamic learning (`abk_learn_word`) without mutating system wordlists.
- Implemented principled log-linear candidate scoring:
  $$\text{Score} = w_{\text{trans}} \log P_{\text{trans}} + w_{\text{lm}} \log P_{\text{LM}} - w_{\text{edit}} \text{Dist} - w_{\text{len}} \Delta L$$
- Enhanced phonetically-aware autocorrect with a Bengali homophone confusion matrix and bounded execution (< 1.0 ms).

#### Phase 5: Cross-Platform Frontends & Host Smoke Tests
- Developed native **macOS InputMethodKit (IMKit)** frontend (`bindings/macos/`):
  - `AmarBanglaKothaController` subclassing `IMKInputController`, preedit underline styling, floating candidate panel, `build_macos.sh`.
- Developed native **Windows Text Services Framework (TSF)** TIP (`bindings/windows/`):
  - `CTsfEngine` implementing COM interfaces (`ITfTextInputProcessor`, `ITfKeyEventSink`, `ITfCompositionSink`, `ITfEditSession`), DLL registration (`abk_tsf.dll`).
- Upgraded **Linux IBus Engine** (`bindings/linux/abk_ibus_engine.py`) with NFC and grapheme backspacing.
- Developed native **Linux Fcitx5 C++ Addon** (`bindings/linux/fcitx5/`).
- Created automated 10-point Host OS End-to-End smoke test harness (`tests/smoke_test.py`), integrated into Makefile (`make smoke-test`).

#### Phase 6: Cross-Platform Production Readiness & Fuzzing
- Developed automated 50,000-iteration fuzz testing harness (`tests/fuzz_test.cpp`):
  - Tested corrupted UTF-8 sequences, out-of-range surrogates, random keystrokes, and rapid teardown cycles.
  - Verified 0 crashes, 0 memory leaks, and 100% valid UTF-8 compliance.
- Added defensive exception barriers and passthrough fallbacks across C ABI (`src/c_api.cpp`).
- Enforced hard performance budgets in `tests/benchmark.cpp`:
  - Latency p99: **590.35 µs** (budget: < 5000 µs) -> **PASS**.
  - Startup & Load: **2.59 ms** (budget: < 300 ms) -> **PASS**.
  - Memory: **5.08 MB** (budget: < 100 MB) -> **PASS**.
- Built multi-platform CI matrix workflow (`.github/workflows/ci.yml`) covering Linux (GCC & Clang), macOS (Intel & Apple Silicon), and Windows (MSVC 2022).

#### Phase 7: Documentation & Handoff
- Authored complete Phonetic Keymap Reference (`docs/KEYMAP.md`).
- Authored deep Architecture Specification (`docs/ARCHITECTURE.md`).
- Authored Contributing Guidelines (`CONTRIBUTING.md`).
- Updated Project `README.md` with multi-platform instructions and architecture diagrams.
