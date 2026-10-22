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
