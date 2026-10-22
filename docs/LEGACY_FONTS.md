# Legacy Font Converter (Bijoy / SutonnyMJ ANSI ⇄ Unicode)

This document describes the legacy font conversion engine implemented in **Amar Bangla Kotha** (`libabk`).

---

## 1. Background & The Legacy ANSI Problem

For over two decades in Bangladesh, desktop publishing (DTP), newspapers, legal documents, and government offices relied heavily on ANSI-encoded TrueType fonts (primarily **SutonnyMJ**, **SutonnyBanglaOMJ**, and **Bijoy** 2000/Classic/Bayanno).

### Fundamental Architectural Differences
- **Visual vs. Logical Ordering**: In legacy typewriter-style ANSI fonts, pre-base vowels (e.g., হ্রস্ব ই-কার `w`, এ-কার `†`/`‡`, ঐ-কার `ˆ`/`‰`) were typed **before** the consonant visually. Modern Unicode requires logical phonetic order (Base Consonant + Vowel Kar).
- **Compound Vowels**: ও-কার (`ো`) was typed as an এ-কার before the consonant and an আকার after the consonant.
- **Reph (`র্`) Placement**: In SutonnyMJ, Reph was typed **after** the consonant cluster via symbol `©`. In Unicode, Reph is strictly ordered **before** the consonant cluster as `র` + `্` (`U+09B0 U+09CD`).
- **Dedicated Conjunct Glyphs**: Hundreds of complex conjuncts (যুক্তাক্ষর) were placed into arbitrary extended ASCII / ANSI byte codes (e.g. `³` for ক্ত, `¶` for ক্ষ, `Á` for জ্ঞ).

---

## 2. Supported Fonts & Encodings

| Font Family | Encoding Scheme | Status |
|---|---|---|
| **SutonnyMJ** | Windows ANSI / CP1252 | **Fully Supported (100% Tested)** |
| **SutonnyBanglaOMJ** | Windows ANSI | **Fully Supported** |
| **Bijoy 2000 / Classic / Bayanno (ANSI)** | Windows ANSI | **Fully Supported** |
| **Amar Bangla / Probhat ANSI** | Windows ANSI | **Supported** |
| **Boishakhi / Non-Standard Custom DTP** | Custom Offset | *Unsupported* (requires custom offset remapping) |

---

## 3. Implementation & Conversion Pipeline

The conversion engine in [`include/abk/legacy_converter.h`](../include/abk/legacy_converter.h) and [`src/legacy_converter.cpp`](../src/legacy_converter.cpp) implements a 7-stage orthographic pipeline:

```
[Raw ANSI Input]
       ↓
[1. CP1252 / UTF-8 Sanitizer] (decodes single-byte or multi-byte clipboard representations)
       ↓
[2. Multi-Char Glyph Substitution] (longest-match mapping for conjuncts, vowels, kars)
       ↓
[3. Pre-Kar Logical Reordering] (shifts ি, ে, ৈ past consonant and conjunct clusters)
       ↓
[4. Compound Vowel Composition] (combines visual split pairs: ে + া ➔ ো, ে + ৗ ➔ ৌ)
       ↓
[5. Reph Shifting] (shifts trailing র্ to precede the target consonant cluster)
       ↓
[6. Orthographic Cleanup] (collapses double halants ্+্ ➔ ্, normalizes colons)
       ↓
[7. Canonical NFC Normalization] (UAX #15 enforcement)
       ↓
[Standard Unicode UTF-8]
```

### Reverse Conversion (Unicode ➔ Bijoy ANSI)
The engine also supports bidirectional reverse conversion via `convert_unicode_to_ansi`:
1. Decomposes compound vowels into split visual markers (`ো` $\rightarrow$ `ে` + `া`).
2. Moves pre-kars to precede consonants visually.
3. Moves Reph to trail consonants.
4. Maps Unicode characters and conjuncts to standard Bijoy ANSI codes.

---

## 4. Test Vectors & Accuracy Benchmark

The engine is benchmarked against hand-verified test vectors in [`tests/data/legacy_vectors.tsv`](../tests/data/legacy_vectors.tsv):

| Category | Sample ANSI Input | Expected Unicode Output | Result |
|---|---|---|---|
| **Classic Sentence** | `Dfq cv‡k av‡bi kx‡l †ewóZ cvwb‡Z fvmgvb RvZxq dyj kvcjv|` | উভয় পাশে ধানের শীষে বেষ্টিত পানিতে ভাসমান জাতীয় ফুল শাপলা। | **PASS** |
| **National Anthem** | `Avgvi †mvbvi evsjv, Avwg †Zvgvq fv‡jvevwm|` | আমার সোনার বাংলা, আমি তোমায় ভালোবাসি। | **PASS** |
| **Reph Basic** | `Kg©` | কর্ম | **PASS** |
| **Reph Conjunct** | `ea©gvb` | বর্ধমান | **PASS** |
| **Reph with Kar** | `g~wZ©` / `a‡g©i` | মূর্তি / ধর্মের | **PASS** |
| **Compound ও-কার** | `†mvbvi` / `‡Kv_vq` | সোনার / কোথায় | **PASS** |
| **Compound ঔ-কার** | `†gŠjfx` | মৌলভী | **PASS** |
| **Complex Conjunct** | `hy³ivóª` / `weÁvb` / `e¨e¯’v` | যুক্তরাষ্ট্র / বিজ্ঞান / ব্যবস্থা | **PASS** |
| **Numerals & Dates** | `12345` / `2024 mv‡j` | ১২৩৪৫ / ২০২৪ সালে | **PASS** |
| **Dari / Punctuation** | `RvZxq|` | জাতীয়। | **PASS** |

**Benchmark Accuracy**: **22 / 22 (100.0%)** across all test vectors.

---

## 5. Usage & Tools

### 1. Command Line Interface (CLI)
```bash
# Convert ANSI string
./bin/abk_cli --convert-ansi "Avgvi †mvbvi evsjv"

# Convert file
./bin/abk_cli --convert-ansi legacy_document.txt > unicode_output.txt

# Reverse convert Unicode to Bijoy
./bin/abk_cli --convert-unicode "আমার সোনার বাংলা"
```

### 2. Clipboard & Paste Tool
```bash
# Convert current clipboard text once
python3 tools/abk_legacy_clipboard.py --once

# Background watcher: automatically converts any copied Bijoy text into Unicode
python3 tools/abk_legacy_clipboard.py --watch

# Launch desktop graphical converter
python3 tools/abk_legacy_clipboard.py --gui
```

### 3. C & C++ API
```cpp
// C++ API
#include "abk/legacy_converter.h"
std::string unicode = abk::LegacyConverter::convert_ansi_to_unicode("Avgvi †mvbvi evsjv");
bool is_legacy = abk::LegacyConverter::is_likely_legacy_ansi(text);

// C API
#include "abk/abk.h"
char out_buf[1024];
abk_ansi_to_unicode("Avgvi †mvbvi evsjv", out_buf, sizeof(out_buf));
```
