# Contributing to Amar Bangla Kotha (আমার বাংলা কথা)

Thank you for your interest in contributing to **Amar Bangla Kotha**! This project is a production-quality, cross-platform Bengali phonetic keyboard engine engineered for correctness, speed, and safety.

---

## 1. Core Engineering Principles

Before contributing code, please keep the following non-negotiable architectural rules in mind:

1. **Measurable Correctness First**: We do not guess whether an engine change improves transliteration. Every change must be verified against our golden benchmark dataset (`tests/data/golden.tsv`).
2. **Zero Heavy External Dependencies**: The core engine must remain in clean standard C++17. Do not introduce Boost, ICU, or third-party frameworks into `include/abk` or `src/`.
3. **Platform Independence**:
   - `include/abk/` and `src/` must NEVER include OS headers (`<windows.h>`, `<Cocoa/Cocoa.h>`, `<sys/...>`, etc.).
   - Platform-specific code belongs strictly inside `bindings/` (e.g. `bindings/macos/`, `bindings/windows/`, `bindings/linux/`).
4. **Crash Safety & Defense in Depth**: The engine must never crash the host application. All public C ABI calls in `src/c_api.cpp` must be enclosed in `try { ... } catch (...)` exception barriers with safe passthrough fallbacks.
5. **Strict Performance Budgets**:
   - Keystroke Latency (p99): strictly under **5.0 ms** (measured baseline is ~0.59 ms).
   - Startup & Load Time: strictly under **300.0 ms** (measured baseline is ~3 ms).
   - Resident Memory (RSS): strictly under **100.0 MB** (measured baseline is ~5.08 MB).
   - Any PR exceeding these thresholds will fail CI.

---

## 2. Setting Up the Development Environment

### Prerequisites
- **Linux**: GCC 9+ or Clang 10+, `make`, `cmake`, `python3`
- **macOS**: Xcode Command Line Tools (`clang`), `make`, `cmake`, `python3`
- **Windows**: Visual Studio 2019/2022 with C++ desktop development, `cmake`

### Building the Project

```bash
# Clone the repository
git clone https://github.com/sumon/Keyboard.git
cd Keyboard

# Build all libraries, CLI, and test runners
make all

# Or using CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## 3. Testing & Verification

Every pull request must pass the full test suite and maintain our benchmark scores:

```bash
# 1. Run Catch2 Unit Tests (85 assertions)
make test

# 2. Run Host OS End-to-End Smoke Test (10 verification scenarios)
make smoke-test

# 3. Run Fuzz Testing Suite (50,000 iterations under AddressSanitizer/UBSan)
make fuzz-test

# 4. Run Full Sanitizer Verification
make sanitize-test

# 5. Run Performance Benchmark against Golden Dataset
make benchmark
```

---

## 4. Modifying Phonetic Rules

Phonetic transliteration rules are entirely data-driven and located in:
👉 [`data/phonetic_rules.tsv`](file:///home/sumon/Desktop/Keyboard/data/phonetic_rules.tsv)

### Rule Format:
```tsv
Pattern    Output    Context    Category    Priority    Description
```

- **Context Options**: `START`, `AFTER_CONSONANT`, `AFTER_VOWEL`, `END`, `ANY`.
- **Category Options**: `OVERRIDE`, `VOWEL`, `CONSONANT`, `CONJUNCT`, `DIPHTHONG`, `MODIFIER`, `PUNCTUATION`, `OTHER`.
- **Priority Scaling**: Digraphs/conjuncts should have higher priority ($180 - 450$) than single characters ($80 - 120$).

When adding or altering a rule:
1. Add corresponding test cases to [`tests/data/golden.tsv`](file:///home/sumon/Desktop/Keyboard/tests/data/golden.tsv).
2. Run `make benchmark` to verify that Top-1 and Top-3 accuracy improve and that no existing categories regress.

---

## 5. Pull Request Checklist

- [ ] Code compiles cleanly with zero compiler warnings (`-Wall -Wextra`).
- [ ] No platform-specific headers added to core engine headers or source files.
- [ ] All existing Catch2 unit tests pass (`make test`).
- [ ] Smoke tests pass without errors (`make smoke-test`).
- [ ] Fuzz test completes 50,000 iterations without crashes or invalid UTF-8 (`make fuzz-test`).
- [ ] Performance benchmark verifies compliance with CI performance budgets (`make benchmark`).
- [ ] Documentation updated if public API or keymaps are modified.
