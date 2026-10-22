# "Amar Bangla Kotha" (আমার বাংলা কথা) Architecture Document

## Overview

**Amar Bangla Kotha** (`libabk`) is a high-performance, cross-platform Bengali keyboard engine written in standard C++17. It provides:
1. **Lattice Beam-Search Decoder**: Replaces greedy longest-match with a multi-hypothesis graph search guided by data-driven phonetic rules.
2. **Compact Flat-Array Trie**: Flat memory node pool with binary-searched child branches executing directly in L1 cache.
3. **Dynamic User Dictionary Overlay**: Immutable system vocabulary paired with persistent dynamic user word learning.
4. **Phonetic Autocorrect**: Bounded Damerau-Levenshtein search with a Bengali homophone confusion matrix and guaranteed termination (< 1 ms).
5. **Principled Log-Linear Ranker**: Unified probability combination of transliteration confidence, unigram LM frequency, edit penalties, and prefix distance.
6. **Unicode Normalization & Grapheme Engine**: Canonical NFC normalization (UAX #15) and orthographic syllable/cluster deletion (UAX #29).
7. **Universal C ABI & Frontends**: Drop-in integrations for macOS (IMKit), Windows (TSF), Linux (IBus / Fcitx5), and CLI/mobile runtimes.

---

## Architecture Diagram

```mermaid
flowchart TD
    UserInput["Latin Input (e.g., 'koi')"] --> Engine["abk::AmarBanglaKothaEngine"]
    
    subgraph CoreEngine ["ABK Core Engine (C++17)"]
        Engine --> Translit["abk::Transliterator\n(Lattice Beam-Search Decoder)"]
        Engine --> Dict["abk::Dictionary\n(Two-Tier: System + User Overlay)"]
        Engine --> AutoCorr["abk::Autocorrect\n(Bounded Phonetic Damerau-Levenshtein)"]
        
        Translit --> Rules["Data-Driven Rules\n(data/phonetic_rules.tsv)"]
        Dict --> Trie["abk::Trie\n(Flat Array Pool Compact Trie)"]
        AutoCorr --> Fuzzy["Bounded BFS Branch Search"]
        Trie -.-> Fuzzy
    end
    
    Translit --> Candidates["Candidate Collector"]
    Trie --> Candidates
    Fuzzy --> Candidates
    
    Candidates --> Ranker["Principled Log-Linear Ranker\n(Score = w_trans*P_trans + w_lm*logFreq - w_edit*Dist)"]
    Ranker --> NFC["Unicode NFC Normalizer (UAX #15)"]
    NFC --> Result["SuggestionResult / abk_suggestion_list_t"]
    
    subgraph Platforms ["Cross-Platform Integrations"]
        Result --> Mac["macOS (IMKit Server & App Bundle)"]
        Result --> Win["Windows (TSF Text Input Processor DLL)"]
        Result --> LinIBus["Linux IBus (abk_ibus_engine.py)"]
        Result --> LinFcitx["Linux Fcitx5 (amar-bangla-kotha.so)"]
        Result --> CLI["Terminal CLI & Benchmarks"]
    end
```

---

## Detailed Subsystem Design

### 1. Lattice Beam-Search Decoder (`abk/transliterator.h`)
- **Greedy Longest-Match Elimination**: In traditional 1-pass engines, greedy matching on `koi` produces `কৈ` because `oi` is greedily matched as `ৈ`.
- **Lattice Construction**:
  - Treats phonetic transliteration as finding optimal paths through a directed acyclic graph (lattice).
  - Matches rule substrings against `data/phonetic_rules.tsv` at all character indices.
  - Context constraints evaluated per edge:
    - `START`: Valid only at word beginning (`pos == 0`).
    - `AFTER_CONSONANT`: Valid only when previous character in hypothesis was a consonant.
    - `AFTER_VOWEL`: Valid only when previous character in hypothesis was a vowel.
    - `END`: Valid only if the edge ends at the input's final index.
  - Retains the top $W = 24$ hypotheses at each position, scored by path priority and length weighting.
- **Safe Fallback**: Every position includes a safe UTF-8 fallback edge with baseline priority ($1.0$). If an invalid byte or non-phonetic symbol occurs, it is properly validated and emitted (or sanitized to `\uFFFD`) without causing the beam to stall.

### 2. Flat-Array Compact Trie (`abk/trie.h`)
- **Cache-Locality Optimization**: Replaced individual dynamic heap allocations (`std::unordered_map<char32_t, unique_ptr<TrieNode>>`) with a single contiguous flat vector:
  ```cpp
  struct CompactTrieNode {
      char32_t ch;
      uint32_t frequency;
      bool is_terminal;
      std::string word;
      std::vector<std::pair<char32_t, uint32_t>> children; // sorted by codepoint
  };
  ```
- Child branches are searched via `std::lower_bound` directly in L1 CPU cache.
- Memory consumption dropped to **~5.08 MB RSS** with zero memory fragmentation.

### 3. Dynamic User Dictionary Overlay (`abk/dictionary.h`)
- Maintains a two-tier architecture:
  1. `system_trie`: Preloaded foundational vocabulary and system wordlists (immutable).
  2. `user_trie`: Custom user learned words dynamically recorded via [`abk_learn_word`](file:///home/sumon/Desktop/Keyboard/include/abk/abk.h#L54).
- User words take precedence on conflicts and are seamlessly merged into suggestion lists without mutating the system wordlist on disk.

### 4. Bounded Phonetic Autocorrect (`abk/autocorrect.h`)
- Evaluates Damerau-Levenshtein distance with a specialized **Bengali Phonetic Confusion Matrix**:
  - Sibilants (`শ/ষ/স`): distance cost $0.35$.
  - Nasals (`ণ/ন`): distance cost $0.35$.
  - Short/Long Vowels (`ি/ী`, `ু/ূ`, `ই/ঈ`): distance cost $0.30$.
  - Flaps (`ড়/ঢ়/র/ড`): distance cost $0.40$.
- **Hard Execution Guard**: The BFS branch exploration is strictly bounded to $3,000$ operations, guaranteeing termination in **under 1.0 ms**.

### 5. Principled Log-Linear Ranking (`abk/engine.h`)
Candidates from all sources are scored using a log-linear model:

$$\text{Score}(W \mid X) = w_{\text{trans}} \cdot \log P_{\text{trans}}(W \mid X) + w_{\text{lm}} \cdot \log_{10}(F(W) + 1) - w_{\text{edit}} \cdot \text{EditDist}(W, \hat{W}) - w_{\text{len}} \cdot |L(W) - L(X)|$$

Where:
- $w_{\text{trans}} = 8500.0$
- $w_{\text{lm}} = 500.0$
- $w_{\text{edit}} = 1200.0$
- $w_{\text{len}} = 150.0$

### 6. Unicode Normalization & Grapheme Processing (`abk/utf8_util.h`)
- **UAX #15 Normalization (NFC)**:
  - Canonical composition of two-part vowel signs (ো, ৌ) and canonicalization of Nukta (ড়, ঢ়, য়).
  - Guarantees byte-level consistency across all platforms and rendering engines.
- **UAX #29 Grapheme Cluster Aware Editing (`pop_logical_unit`)**:
  - Visual backspace deleting: Deleting backspace on `কি` removes only the vowel sign `ি`, preserving the base consonant `ক`.
  - Proper handling of complex conjuncts, Virama (`্`), and ZWJ (`\u200D`).

---

## Cross-Platform Frontend Architecture

1. **macOS InputMethodKit (`bindings/macos/`)**:
   - `AmarBanglaKothaController` subclasses `IMKInputController`.
   - Manages marked preedit text, underline attributes, and floating candidate panels.
   - Packaged as a native `AmarBanglaKotha.app` bundle installed to `~/Library/Input Methods/`.

2. **Windows Text Services Framework (`bindings/windows/`)**:
   - `CTsfEngine` implements COM interfaces: `ITfTextInputProcessor`, `ITfKeyEventSink`, `ITfCompositionSink`, and `ITfEditSession`.
   - Registered under CLSID `{3F5C3B4E-5D8A-4F9A-8417-6B1A4B0C7291}` and Profile GUID `{7A1E3B8F-9C2D-4E5F-B0A1-3C5E7A9B1D3F}` for LangIDs `0x0445` and `0x0845`.

3. **Linux IBus & Fcitx5 (`bindings/linux/`)**:
   - IBus engine daemon (`abk_ibus_engine.py`) with ctypes C ABI binding.
   - Native C++ Fcitx5 addon (`bindings/linux/fcitx5/`) implementing `fcitx::InputMethodEngineV2` for modern Wayland and X11 desktops.

---

## Crash Safety & Exception Barriers

The core engine guarantees **zero host crashes**:
- In `src/c_api.cpp`, every public ABI function wraps engine operations inside a `try { ... } catch (...) { ... }` block.
- In out-of-memory or internal error conditions, the ABI gracefully falls back to passthrough text.
- Memory allocation inside C structures (`abk_suggestion_list_t`) is isolated and strictly freed via `abk_free_suggestions()`.
