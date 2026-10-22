# Provenance and Licensing of Datasets and Rules

This document records the provenance, authorship, and licensing terms of every rule table, corpus, vocabulary file, and external dataset used in **Amar Bangla Kotha** (`libabk`), in accordance with project safety and copyright policies.

---

## 1. Rule Tables

### `data/phonetic_rules.tsv`
- **Origin**: Authored specifically for Amar Bangla Kotha by the engineering team.
- **Description**: Tab-delimited phonetic mapping rules covering independent vowels, dependent vowel signs (kars), consonants, juktakkhor conjuncts, ZWJ/ZWNJ ligatures, and punctuation overrides.
- **Licensing**: Native project license (GPLv3 / project-owned).
- **Third-Party Policy Check**: Does **NOT** copy or import proprietary or external files from OpenBangla Keyboard or OmicronLab Avro. All priority weightings, context tokens, and lattice edge definitions were designed and calibrated in-house.

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

The following test items in `tests/data/golden.tsv` or vocabulary variants are documented for native linguistic review rather than assumed:

| Input | Current Output | Alternative Variant | Reason for Review |
|---|---|---|---|
| `bhalo` | ভালো | ভাল | Both variants are common in modern standard Bangla (চলতি রীতি). |
| `gaaN` | গাণ | গান | Phonetic transliteration yields retroflex ণ due to capital `N`; dictionary unigram prefers dental ন. |
| `shundor` | সুন্দর | শুন্দর | Sibilant ambiguity (স vs শ). |

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
