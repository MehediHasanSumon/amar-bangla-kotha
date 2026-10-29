# Provenance and Licensing of Datasets and Rules

This document records the provenance, authorship, and licensing terms of every rule table, corpus, vocabulary file, and external dataset used in **Amar Bangla Kotha** (`libabk`), in accordance with project safety and copyright policies.

---

## 1. Rule Tables & Unicode Normalization Policy

### `data/keymap_phonetic.toml` (Primary Phonetic Keymap & Phonotactics)
- **Origin**: Clean-room authored from scratch specifically for **Amar Bangla Kotha** (`libabk`).
- **De-Facto Key Convention & Licensing Decision**: This file implements the de-facto Avro-compatible Latin-to-Bangla phonetic typing convention so existing Bengali phonetic typists feel immediately at home. **Zero text, rule tables, phonotactic tables, documentation, or autocorrect entries were copied or adapted from OmicronLab Avro Keyboard, OpenBangla Keyboard, or any third-party source.** All TOML schema definitions, 8-state lattice context predicates, phonotactic compatibility sets, and beam priority weights were independently engineered in-house.
- **Unicode Internal Form & NFC Policy (UAX #15)**:
  - Only standard Bengali Unicode code points (`U+0980..U+09EF` plus `U+200C`/`U+200D` joiners) are emitted. Assamese `ৰ` (`U+09F0`) and `ৱ` (`U+09F1`) are strictly prohibited across rules and runtime output.
  - All internal strings, rule outputs, dictionary entries, and candidate outputs are normalized via `abk::utf8::normalize_nfc()` to **Canonical Unicode NFC (UAX #15)**:
    - Two-part vowel signs are canonically composed: `ো` (`U+09CB`), `ৌ` (`U+09CC`).
    - Nukta consonants remain canonically decomposed per the Unicode Composition Exclusion Table (Singleton/Script-Specific Exclusions):
      - `ড়` = `ড` + `়` (`U+09A1 U+09BC`, canonically equivalent to `U+09DC`)
      - `ঢ়` = `ঢ` + `়` (`U+09A2 U+09BC`, canonically equivalent to `U+09DD`)
      - `য়` = `য` + `়` (`U+09AF U+09BC`, canonically equivalent to `U+09DF`)
      - `জ়` = `জ` + `়` (`U+099C U+09BC`)
    - Ra-phala uses `C + ্ (U+09CD) + র (U+09B0)` (e.g. `প্র = প + ্ + র`), Reph uses `র (U+09B0) + ্ (U+09CD) + C` (e.g. `র্ক = র + ্ + ক`), and Ra + Ya-phala uses `র (U+09B0) + ZWJ (U+200D) + ্ (U+09CD) + য (U+09AF)` (`র‍্য`).
- **Licensing**: Native project license (GPLv3 / project-owned).

### `data/phonetic_rules.tsv` (Legacy TSV Rule Table)
- **Origin**: Authored specifically for Amar Bangla Kotha by the engineering team (retained for backward `.tsv` loader compatibility).
- **Description**: Tab-delimited phonetic mapping rules covering independent vowels, dependent vowel signs (kars), consonants, juktakkhor conjuncts, ZWJ/ZWNJ ligatures, and punctuation overrides.
- **Licensing**: Native project license (GPLv3 / project-owned).
- **Third-Party Policy Check**: Does **NOT** copy or import proprietary or external files from OpenBangla Keyboard or OmicronLab Avro.

### `data/autocorrect_default.tsv` (Default User-Editable Autocorrect Table)
- **Origin**: Clean-room authored from scratch specifically for **Amar Bangla Kotha** (`libabk`).
- **Description**: 65 verified tab-separated replacement entries across three categories (`conjunct_exception`, `loanword`, `shortcut`) covering everyday Bangla words where automatic conjunct formation would misfire without a backtick (e.g. `amra -> আমরা`, `ekta -> একটা`, `aglano -> আগলানো`) and common English-spelled loanwords (e.g. `software -> সফটওয়্যার`, `doctor -> ডাক্তার`, `school -> স্কুল`, `internet -> ইন্টারনেট`, `university -> ইউনিভার্সিটি`).
- **Third-Party Policy Check**: **Zero entries were copied, translated, or adapted from OmicronLab Avro `autodict`, OpenBangla Keyboard autocorrect JSON/TOML files, or any external IME distribution.** All entries are normalized to Canonical Unicode NFC (`abk::utf8::normalize_nfc`).
- **Licensing**: Native project license (GPLv3 / project-owned).

### `docs/KEYMAP.md` & In-App Cheat Sheets (`cli/main.cpp`, `gui/app.py`, `tools/abk_settings.py`)
- **Origin**: Written completely from scratch in English and Bangla by the Amar Bangla Kotha team.
- **Description**: Bilingual user-facing typing guide and one-page quick cheat sheet documenting vowels, backtick escapes, case-sensitive consonants, phalas, reph, conjuncts, smart punctuation/currency guards, autocorrect, and suggestion window controls.
- **Third-Party Policy Check**: **Zero prose, layout tables, or example lists were copied from Avro Keyboard PDF manuals or third-party guides.**
- **Licensing**: Native project license (GPLv3 / project-owned).

---

## 2. Dictionaries & Vocabulary Corpora

### `data/bangla_wordlist.txt`
- **Origin**: Curated core Bengali frequency corpus of 1,519 high-frequency foundational words.
- **Description**: Contains Bengali words paired with integer frequency scores derived from publicly available non-copyrighted Bengali texts and classic linguistic frequency distributions.
- **Licensing**: Permissive / Public Domain / Project-owned.
- **Status**: Clean. No proprietary data.

### `data/bangla_bigrams.tsv`
- **Origin**: Authored and curated specifically for Amar Bangla Kotha by the engineering team.
- **Description**: High-frequency natural Bengali collocations and bigram language model pairs ($w_1 \rightarrow w_2$) with frequency weights.
- **Licensing**: Native project license (GPLv3 / project-owned).
- **Status**: Clean. No copyrighted or third-party datasets imported.

---

## 3. Public Datasets (Available for Import)

The test harness provides an automated importer script [`scripts/import_dataset.py`](file:///home/sumon/Desktop/Keyboard/scripts/import_dataset.py) with explicit license validation:

1. **Google Dakshina Dataset**:
   - **Source**: Google Research / GitHub (`google-research/dakshina`)
   - **License**: Apache License 2.0 (Permissive)
   - **Usage**: Permitted for evaluation, benchmarking, and testing.

2. **AI4Bharat Aksharantar Dataset**:
   - **Source**: AI4Bharat / HuggingFace (`ai4bharat/aksharantar`)
   - **License**: Creative Commons Attribution 4.0 International (CC-BY 4.0)
   - **Usage**: Permitted with attribution.

---

## 4. Linguistic Verification ("NEEDS NATIVE REVIEW")

The following test items in `tests/data/golden.tsv`, `tests/data/avro_style.tsv`, and `tests/data/conjuncts_avro_style.tsv` are documented for native linguistic review rather than silently assumed:

### 4.1 Individual Spelling & Guide Vector Items
| Input | Strict Character Output | Dictionary / Guide Variant | Reason for Review |
|---|---|---|---|
| `bhalo` | ভাল | ভালো | Both variants are common in modern standard Bangla (চলতি রীতি); pure inherent `'o'` yields `ভাল`, dictionary yields `ভালো` (`bhalO` strictly yields `ভালো`). |
| `gaaN` | গায়াণ | গান | Strict Avro rules map post-vocalic `a` to `য়া` and capital `N` to `ণ`. |
| `shundor` | শুন্দর | সুন্দর | Sibilant ambiguity (`স` vs `শ`). |
| `byakti` | ব্যাক্তি | ব্যক্তি | Guide lists `byakti -> ব্যক্তি`; strict character keys are `byokti`/`bykti` (`a` after `y` adds `া`), while dictionary-backed mode resolves `byakti -> ব্যক্তি`. |
| `aZromeTik` | অ্যারমেটিক | অ্যারোমেটিক | Guide lists lowercase `o` in `ro`; strict Section 1 rule requires capital `O` (`aZrOmeTik`) for `ো` unless handled by dictionary/autocorrect. |
| `lokkhmi` | লক্ষ্মি | লক্ষ্মী | Guide lists lowercase `i`; strict Section 1 rule requires `I` or `ee` (`lokkhmI`/`lokkhmee`) for `ী` unless handled by dictionary/autocorrect. |
| `` ag`lano `` | আগলান | আগলানো | Guide Section 6 lists lowercase final `o` (whereas Section 5 notes `aglanO`); strict Section 1 rule requires capital `O` (`` ag`lanO ``) for `ো` unless handled by dictionary/autocorrect. |

### 4.2 Code-Point Generated Conjunct Validation Table (`tests/data/conjuncts_avro_style.tsv`)
- **Origin**: Clean-room generated strictly from Unicode Bengali code-point composition (`C1 + U+09CD + C2 [+ U+09CD + C3]`), never copied from reference PDF glyph extractions (which contain corrupted Assamese `ৰ্` and broken ligatures).
- **Status**: All **251 conjunct clusters** in `tests/data/conjuncts_avro_style.tsv` carry explicit `NEEDS_NATIVE_REVIEW` status so a native speaker can sign off on orthographically rare clusters (e.g. `ক্ব`, `গ্ব`, `ষ্ব`, `ল্ভ`) versus everyday clusters.

---

## 5. Legacy Font Mappings (Workstream 6)

### `src/legacy_converter.cpp` & `tests/data/legacy_vectors.tsv`
- **Origin**: Standard public domain layout specification for SutonnyMJ / Bijoy ANSI TrueType typography in Bangladesh and reference implementation patterns from `Mad-FOX/bijoy2unicode` (MPL 2.0).
- **Description**: Mapping tables translating CP1252/ANSI character indices to Unicode UTF-8 Bengali codepoints, accompanied by orthographic pre-kar and reph reordering algorithms.
- **Licensing**: Permissive / MPL-2.0 compatible.
- **Verification**: Hand-verified against 22 real-world Bengali vectors covering complex conjuncts, pre-kars, compound vowels, and reph ordering.

---

## 6. Bangladesh-Specific Domain Packs & Helpers (Workstream 7)

### `data/domains/bangladesh_places.tsv`
- **Origin**: Authored in-house based on official public administrative geographic records of the Government of Bangladesh.
- **Description**: Contains all 8 divisions, 64 districts, and prominent upazilas/landmarks (~180 entries) with frequency annotations.
- **Licensing**: Project-owned / Public Domain factual geographic data.

### `data/domains/islamic_terms.tsv`
- **Origin**: Curated in-house by native speakers.
- **Description**: Daily greetings, theological expressions, Islamic calendar months, and prayer terminology (~300 entries).
- **Licensing**: Project-owned / Public Domain religious terminology.

### `data/domains/legal_medical_banking.tsv`
- **Origin**: Curated in-house from standard public service laws, judicial acts, medical terms, and banking glossaries used in Bangladesh (~400 entries).
- **Licensing**: Project-owned / Public Domain statutory and technical terminology.

### `src/bangladesh_helpers.cpp`
- **Origin**: Authored specifically for Amar Bangla Kotha.
- **Description**: Algorithmic number to words conversion (0 to hundreds of crores), South Asian Lakh/Crore digit grouping, revised Bangla Academy Bangabda calendar calculations (2019 revision), and Sadhu-Cholito morphological rules.
- **Licensing**: Project native license.

---

## 7. Quality of Life & Correction Privacy (Workstream 8)

### `src/quality_of_life.cpp` & `include/abk/quality_of_life.h`
- **Origin**: Authored in-house for Amar Bangla Kotha.
- **Description**: Bidirectional Banglish $\leftrightarrow$ Bangla converters, smart typographical punctuation (`.` $\rightarrow$ `।`, `...` $\rightarrow$ `…`, `"` $\rightarrow$ `“”`), and emoji suggestion table based on Unicode 15.0 emoji standards.
- **Privacy Architecture**: Local JSON Lines feedback storage (`feedback.jsonl`) and size-capped user correction logger (`correction_log.jsonl`) with guaranteed zero logging in password fields and opt-in by default.
- **Licensing**: Project native license.
