# Comprehensive Codebase Audit: Amar Bangla Kotha (Core Engine)

**Audit Date:** October 2026  
**Auditor:** Senior C++ Engineer & Computational Linguist  
**Target:** Production-Grade Cross-Platform Bangla Phonetic Keyboard Engine (`libabk`)  
**Scope:** Architecture, Greedy Parser, Unicode Handling, Preedit/Cursor, Memory/Concurrency, and Ranked Risk Analysis.

---

## 1. Architecture Map

The repository is structured into distinct functional layers:

```
┌────────────────────────────────────────────────────────────────────────┐
│                     Platform Frontends & UI                            │
│  - Linux IBus Engine: bindings/linux/abk_ibus_engine.py (Python/GI)   │
│  - Desktop GUI Tool: gui/app.py (PyQt6 / Tkinter style overlay)        │
│  - CLI Test Harness: cli/main.cpp                                      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ C ABI (abk_engine_*, abk_get_suggestions)
┌───────────────────────────────────▼────────────────────────────────────┐
│                    C Interface Layer (src/c_api.cpp)                   │
│  - Marshals C++ structures to heap-allocated C structs                │
│  - Manages engine lifecycle (create/destroy)                           │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│              AmarBanglaKothaEngine (src/engine.cpp)                    │
│  - Pipeline coordinator: transliteration + dictionary + suggestions   │
│  - Candidate score calculation and deduplication                      │
└───────┬───────────────────────────┬───────────────────────────┬────────┘
        │                           │                           │
┌───────▼─────────────┐   ┌─────────▼──────────┐   ┌───────────▼─────────┐
│   Transliterator    │   │    Dictionary      │   │     Autocorrect     │
│(src/transliterator) │   │ (src/dictionary)   │   │  (src/autocorrect)  │
│ - Rule-based mapper │   │ - In-memory Trie   │   │ - Damerau-Levensh.  │
│ - Hardcoded vectors │   │ - Static corpus    │   │ - Phonetic penalty  │
│ - Greedy 1-pass     │   │ - File loading     │   │ - Trie branch prune │
└───────┬─────────────┘   └─────────┬──────────┘   └───────────┬─────────┘
        │                           │                          │
        └───────────────────────────┼──────────────────────────┘
                                    │
                    ┌───────────────▼───────────────┐
                    │      Trie (src/trie.cpp)      │
                    │  - char32_t codepoint trie    │
                    │  - Node: std::unordered_map   │
                    └───────────────┬───────────────┘
                                    │
                    ┌───────────────▼───────────────┐
                    │ UTF-8 Utility (src/utf8_util) │
                    │  - Codepoint encode/decode    │
                    │  - Bengali unicode char check │
                    └───────────────────────────────┘
```

### Component Details
1. **Parser (`Transliterator`)**: Single-pass, left-to-right deterministic scanner that substitutes matched Latin prefixes with Bengali strings.
2. **Rule Tables**: Hardcoded static `std::vector<Rule>` initialized inside the constructor in `src/transliterator.cpp`.
3. **Dictionary (`Dictionary` & `Trie`)**: Word frequency store backed by a heap-allocated pointer Trie (`std::unordered_map<char32_t, unique_ptr<TrieNode>>`). Contains ~244 embedded words plus external corpus loader.
4. **Autocorrect (`Autocorrect`)**: Modified Damerau-Levenshtein distance calculation on Trie nodes with reduced penalty (0.5) for phonetically similar Bengali characters.
5. **Preedit & Commit Logic**: Managed in `bindings/linux/abk_ibus_engine.py`. Tracks an ASCII `buffer`, requests candidate suggestions on each keystroke, renders underlined preedit via `IBus.Text`, and commits on Space/Enter/Number selection.

---

## 2. Greedy / Longest-Match Parsing Inventory

The current engine relies entirely on sequential longest-match / greedy prefix matching. Below are the exact points of failure in `src/transliterator.cpp`:

### The Root Cause of the "koi" Bug
- **Code Reference:** `src/transliterator.cpp`, Lines 163–181 (`kar_rules`) and Lines 295–324 (`Vowel handling`).
- **Mechanism:**
  1. The user types `"koi"`.
  2. At index 0, `'k'` matches `consonant_rules` (line 117), emitting `"ক"`. Flag `last_was_consonant` is set to `true`.
  3. At index 1, because `last_was_consonant == true`, the parser checks `kar_rules` (line 299):
     ```cpp
     if (match_and_replace(input, i, kar_rules, kar_out, kar_len))
     ```
  4. In `kar_rules`, the pattern `"oi"` -> `ৈ` (line 165) matches 2 characters (`'o'` and `'i'`).
  5. The parser consumes `"oi"` and outputs `"ৈ"`, producing **"কৈ"**.
  6. The much more frequent Bengali word **"কই"** (consisting of consonant 'ক' + inherent vowel 'o' + independent vowel 'ই') is **structurally impossible** to generate because:
     - The parser never evaluates alternative segmentations (e.g., `'k'` + `'o'` followed by independent `'i'`).
     - `kar_rules` greedily eats `"oi"`.
     - The parser has no backtracking, no lattice generation, and no beam search.

### Other Greedy Match Conflicts
1. **`modifier_rules` (lines 15–24):**
   - `"t`"`, `"T`"`, `"th`"` map to Khanda-Ta (`ৎ`). If user types `"th`"`, it matches before `"t"` or `"th"`.
   - `"ng"` maps to Anusvara (`ং`) (e.g. `bangla` -> `বাংলা`). However, words requiring `ঙ্গ` (ngg), `ঙ্ক` (ngk), or `ঙ` (Ng) often collide or require awkward casing overrides.
2. **`juktakkhor_rules` (lines 27–95):**
   - Contains explicit, hardcoded full words like `dhonnobad` -> `ধন্যবাদ` and `dhonno` -> `ধন্য`.
   - Ad-hoc entries exist because the grammar cannot naturally handle geminates and ya-phala without greedily mis-parsing them.
3. **`initial_vowel_rules` (lines 143–160):**
   - `"oi"` -> `ঐ`, `"ou"` -> `ঔ`, `"aa"` -> `আ`, `"ee"` -> `ঈ`, `"oo"` -> `ঊ`, `"ii"` -> `ঈ`, `"uu"` -> `ঊ`.
   - If user types `"oinik"`, `"oi"` becomes `ঐ`. But if a user wants `অই` (rare) or another combination, longest-match forbids it.
4. **Inherent "o" vs O-kar "ো" (lines 306–316):**
   - Terminal check: `if (kar_len == 1 && input[i] == 'o')`.
   - Consonant chain heuristic: if `consonant_chain_count >= 2`, inherent 'o' is suppressed (`shobdo` -> `শব্দ`). Otherwise, it appends O-kar (`bhalo` -> `ভালো`).
   - This hardcoded heuristic fails for words like `koto` -> `কত` (incorrectly becomes `কতো`), `mon` -> `মন` (becomes `মোন` if typed `mono`), etc.

---

## 3. Unicode Handling & Normalization

### Normalization Forms (NFC / NFD / NFKC)
- **Current State:** **No Unicode normalization is implemented anywhere.**
- Strings are treated as opaque byte sequences and converted to `char32_t` via raw UTF-8 bit shifting in `src/utf8_util.cpp`.
- **Risks:**
  - **Decomposed vs Composed Forms:**
    - Bengali `ড়` (U+09DC) in NFC is 1 codepoint. In NFD, it is `ড` (U+09A1) + Nukta (U+09BC) (2 codepoints).
    - Bengali `ঢ়` (U+09DD) vs `ঢ` (U+09A2) + Nukta (U+09BC).
    - Bengali `য়` (U+09DF) vs `য` (U+09AF) + Nukta (U+09BC).
    - The classification function `is_bengali_consonant` (lines 135–137) only checks `0x09DC`, `0x09DD`, `0x09DF`. If text is fed in NFD (common on macOS), the engine completely fails to recognize them as consonants!
  - **Khanda-Ta:** U+09CE (`ৎ`) vs `ত` + Hasanta (`\u09A4\u09CD`). These must be normalized for dictionary lookups.
  - **Reph and Ya-phala:** Reph is encoded as `র` (U+09B0) + Virama (U+09CD), but without ZWJ (U+200D), some OpenType renderers fail to distinguish Reph from Ya-phala ligatures (e.g. `র‍্য` vs `র্য`).

---

## 4. Preedit, Backspace, and Pre-Base Vowel Signs

### The Pre-base Glyph Dilemma
In the Bengali script:
- `ি` (Hrasva I-kar, U+09BF)
- `ে` (E-kar, U+09C7)
- `ৈ` (Oi-kar, U+09C8)

These vowel signs are **pre-base graphemes**: in text rendering, they visually appear to the **left** of the consonant cluster (e.g. `কি` displays as `[ি][ক]`), but logically in Unicode memory and phonetic typing, they are positioned **after** the consonant (`U+0995` followed by `U+09BF`).

### Audit of Current Behavior
- **Backspace Handling (`bindings/linux/abk_ibus_engine.py` line 168):**
  ```python
  if keyval == IBus.KEY_BackSpace:
      if self.buffer:
          self.buffer = self.buffer[:-1]
          self.update_preedit_and_lookup()
          return True
  ```
  - The engine pops one character from the Latin ASCII `buffer` and re-runs the full transliterator pipeline.
  - While this prevents splitting multi-byte UTF-8 sequences, it causes issues:
    1. Cursor positioning within the preedit text does not match logical grapheme clusters.
    2. Arrow key navigation inside the preedit string is unhandled.
    3. If the user edits mid-word, the parser recalculates from position 0 without maintaining cursor anchor.

---

## 5. Concurrency, Memory Ownership, and Performance

### Thread-Safety Audit
- `AmarBanglaKothaEngine`, `Dictionary`, `Trie`, and `Autocorrect` contain **no synchronization primitives** (`std::mutex`, `std::atomic`, or reader-writer locks).
- If one thread calls `process()` while another thread calls `learn_word()` or `load_dictionary_file()`, memory corruption and data races will occur immediately.

### Memory Ownership & Allocation Overhead
- **Trie Representation (`include/abk/trie.h`):**
  ```cpp
  struct TrieNode {
      std::unordered_map<char32_t, std::unique_ptr<TrieNode>> children;
      bool is_terminal = false;
      uint32_t frequency = 0;
      std::string word;
  };
  ```
  - Every TrieNode contains an `unordered_map` allocating a hash bucket table on the heap, plus a `std::string` and a `unique_ptr`.
  - For a production dictionary of 150,000 words (~600,000 nodes), this architecture consumes **~60 to 120 MB of RAM** and causes massive heap fragmentation.
  - Memory-mapped Double-Array Trie (DAT) or DAWG (Directed Acyclic Word Graph) would reduce this to **< 5 MB** with zero allocation overhead.
- **Prefix Search Bottleneck (`src/trie.cpp` line 80):**
  - `collect_prefix_words` recursively collects *all* descendant words in the subtree into a `vector`.
  - It then executes `std::sort` on the *entire* list before truncating to `limit = 10`.
  - For single-character prefixes (e.g. 'ক'), this causes an allocation of tens of thousands of elements and a slow quicksort on every single keystroke.
- **C ABI Memory Leaks (`src/c_api.cpp`):**
  - `abk_get_suggestions` calls `malloc` for `abk_suggestion_list_t`, `raw_input`, `primary_transliteration`, `candidates` array, and individual candidate strings.
  - If a client language fails to call `abk_free_suggestions` (or an exception intervenes), heap memory leaks permanently.

---

## 6. Ranked List of Defects and Risks (P0 / P1 / P2)

| Severity | ID | Component | Description |
|---|---|---|---|
| **P0** | DEF-01 | `Transliterator` | **Greedy longest-match cannot disambiguate diphthongs vs inherent vowels.** "koi" unconditionally produces "কৈ" instead of the common word "কই". |
| **P0** | DEF-02 | `Engine` | **Candidate ranking hardcodes primary transliteration at score 9000-10000+.** Even when the primary output is not a real Bengali word or has negligible frequency, it completely suppresses high-frequency dictionary alternatives on Space bar. |
| **P0** | DEF-03 | `Unicode` | **Zero Unicode normalization.** Decomposed characters (NFD) like `ড` + Nukta fail consonant detection and dictionary lookup compared to composed `ড়` (NFC). |
| **P0** | DEF-04 | `Concurrency` | **Engine and Dictionary are not thread-safe.** Concurrent keystroke processing and background dictionary updates cause data races. |
| **P1** | DEF-05 | `Trie` | **Excessive heap allocations & inefficient prefix search.** `std::unordered_map` per node yields high memory footprint (~70MB+ on large vocabularies) and sorts full descendant subtrees on every keystroke. |
| **P1** | DEF-06 | `Transliterator` | **Hardcoded rule vectors in C++ source.** Linguistic rules cannot be updated, localized, or tuned without recompiling the shared library. |
| **P1** | DEF-07 | `Autocorrect` | **Handcrafted phonetic confusion costs.** Only a few hardcoded codepoints get cost 0.5; missing common substitutions (ত/থ, ব/ভ, ঢ়/ড়). |
| **P1** | DEF-08 | `IBus / Platform` | **No grapheme cluster awareness during preedit cursor navigation.** Visual cursor position desynchronizes from logical codepoints around pre-base vowel signs (`ি`, `ে`, `ৈ`). |
| **P2** | DEF-09 | `Unicode` | **Lack of ZWJ/ZWNJ generation.** Words requiring explicit Ya-phala after Ra (র‍্য) or preventing ligature formation cannot be typed. |
| **P2** | DEF-10 | `Engine` | **Punctuation & numbers.** Bengali Dari (`।`) conversion and numeral transliteration (১, ২, ৩) are absent. URLs and decimals (e.g. `3.5`) are treated as plain text splits. |
| **P2** | DEF-11 | `C API` | **Fine-grained manual malloc/free.** Fragile ownership semantics between C++ std::string and C ctypes wrapper. |
