# Bangladesh-Specific Language Helpers (v1.1)

This document describes the Bangladesh-specific linguistic tools, domain dictionaries, number formatting, calendar calculations, and style converters provided by **Amar Bangla Kotha** (`libabk`).

---

## 1. Domain Dictionary Packs

To support professionals, writers, and administrative workers in Bangladesh, Amar Bangla Kotha provides three dedicated domain vocabulary packs that can be toggled on/off independently.

### Pack Summary & Provenance

| Domain Pack | File Path | Total Words | Scope & Provenance |
|---|---|---|---|
| **Bangladesh Places & Landmarks** | [`data/domains/bangladesh_places.tsv`](../data/domains/bangladesh_places.tsv) | ~180 entries | All 8 divisions, 64 administrative districts, major upazilas, prominent rivers, national landmarks, and key infrastructure (e.g. Padma Bridge, Metro Rail, Rooppur). Authored in-house based on official administrative records of the Government of Bangladesh. |
| **Islamic Terms & Greetings** | [`data/domains/islamic_terms.tsv`](../data/domains/islamic_terms.tsv) | ~300 entries | Common daily greetings (*আসসালামু আলাইকুম*, *ওয়ালাইকুম আসসালাম*), theological terms, Islamic calendar months, prayers, and cultural expressions. Curated in-house. |
| **Legal, Medical & Banking** | [`data/domains/legal_medical_banking.tsv`](../data/domains/legal_medical_banking.tsv) | ~400 entries | Professional terminology used across courts, hospitals, and financial institutions in Bangladesh (e.g. *অ্যাকাউন্ট*, *চালান*, *জামিন*, *প্রেসক্রিপশন*, *খতিয়ান*, *রিট*). Curated in-house from standard statutory and public service glossaries. |

### Enabling & Disabling Packs
Users can enable or disable domain packs via the Settings GUI or programmatic APIs:
```cpp
// C++ API
engine.set_domain_pack_enabled(abk::DomainPackType::BANGLADESH_PLACES, true);
engine.set_domain_pack_enabled(abk::DomainPackType::ISLAMIC_TERMS, false);

// C API
abk_engine_set_domain_pack_enabled(engine, 0 /* BANGLADESH_PLACES */, true);
```

---

## 2. Currency Shortcut & Phrase Expansions

- **Currency Shortcut**: Typing `tk` or `taka` instantly expands or suggests the official Bangladeshi Taka currency sign: **৳** (`U+09F3`).
- **Standard Greetings**:
  - `salam` / `assalamu` $\rightarrow$ `আসসালামু আলাইকুম`
  - `inshallah` / `inshaallah` $\rightarrow$ `ইনশাআল্লাহ`
  - `alhamdulillah` $\rightarrow$ `আলহামদুলিল্লাহ`
  - `bismillah` $\rightarrow$ `বিসমিল্লাহ`
  - `subhanallah` $\rightarrow$ `সুবহানাল্লাহ`
  - `mashallah` $\rightarrow$ `মাশাআল্লাহ`
  - `jazakallah` $\rightarrow$ `জাজাকাল্লাহ`
  - `allahhafez` $\rightarrow$ `আল্লাহ হাফেজ`

---

## 3. South Asian Digit Grouping (Lakh / Crore Style)

Unlike the Western grouping convention of 3-3-3 digits (`1,250,000`), Bangladesh and South Asia use the traditional 3-2-2-2 grouping format:
- `1250` $\rightarrow$ `১,২৫০` (or `1,250`)
- `1250000` $\rightarrow$ `১২,৫০,০০০` (or `12,50,000`)
- `10000000` $\rightarrow$ `১,০০,০০,০০০` (or `1,00,00,000`)

### CLI & API Usage
```bash
./bin/abk_cli --number 1250000
# Output:
# Lakh/Crore (Bengali): ১২,৫০,০০০
# Lakh/Crore (ASCII):   12,50,000
# In Bengali Words:    বারো লক্ষ পঞ্চাশ হাজার
```

---

## 4. Number to Words (সংখ্যা থেকে কথায় রূপান্তর)

The engine converts integers to full grammatically correct Bengali words across scales:
- Supports 0 through 99 with standard Bengali orthography.
- Scale units: শত (hundred), হাজার (thousand), লক্ষ (lakh: $10^5$), কোটি (crore: $10^7$).
- Negative numbers prefixed with `ঋণাত্মক`.

### Hand-Verified Benchmark Examples
| Number | Bengali Words Output |
|---|---|
| `0` | শূন্য |
| `1` | এক |
| `21` | একুশ |
| `99` | নিরানব্বই |
| `105` | একশত পাঁচ |
| `1250` | এক হাজার দুইশত পঞ্চাশ |
| `1500000` | পনেরো লক্ষ |
| `75000000` | সাত কোটি পঞ্চাশ লক্ষ |
| `-50` | ঋণাত্মক পঞ্চাশ |

---

## 5. Bangabda Calendar (বঙ্গাব্দ) & Date Helpers

Implements the official revised Bengali calendar defined by the **Bangla Academy** (effective in Bangladesh since 2019):
- **Pohela Boishakh (পহেলা বৈশাখ)** is fixed at **April 14**.
- Months 1 to 6 (*বৈশাখ*, *জ্যৈষ্ঠ*, *আষাঢ়*, *শ্রাবণ*, *ভাদ্র*, *আশ্বিন*): each has **31 days**.
- Months 7 to 10 (*কার্তিক*, *অগ্রহায়ণ*, *পৌষ*, *মাঘ*): each has **30 days**.
- Month 11 (*ফাল্গুন*): **29 days** (or **30 days** in a Gregorian leap year).
- Month 12 (*চৈত্র*): **30 days**.
- Bangabda Year = Gregorian Year - 593 (for dates on/after April 14), and Gregorian Year - 594 (for dates before April 14).

### Bengali Ordinals
- 1: ১লা, 2: ২রা, 3: ৩রা, 4: ৪ঠা
- 5 to 18: ৫ই, ৬ই, ... ১৮ই
- 19 to 31: ১৯শে, ২০শে, ... ৩১শে

### Verified Date Examples
- `2026-04-14` $\rightarrow$ **১লা বৈশাখ ১৪৩৩ বঙ্গাব্দ**
- `2026-05-14` $\rightarrow$ **৩১শে বৈশাখ ১৪৩৩ বঙ্গাব্দ**
- `2026-05-15` $\rightarrow$ **১লা জ্যৈষ্ঠ ১৪৩৩ বঙ্গাব্দ**
- `2026-02-21` $\rightarrow$ **৮ই ফাল্গুন ১৪৩২ বঙ্গাব্দ**

---

## 6. Sadhu $\leftrightarrow$ Cholito Conversion (সাধু $\leftrightarrow$ চলিত)

Provides rule-based morphological conversion for standard literary forms (verbs and pronouns) while preserving punctuation:

### 1. Pronoun Mappings
| Sadhu Form | Cholito Form |
|---|---|
| তাহার / তাহারা / তাহাদের / তাহাকে | তার / তারা / তাদের / তাকে |
| যাহার / যাহারা / যাহাদের / যাহাকে / যাহা | যার / যারা / যাদের / যাকে / যা |
| ইহা / ইহাতে / ইহাদের / ইহাকে | এটা / এতে / এদের / একে |
| উহা / উহাতে / উহাদের / উহাকে | ওটা / ওতে / ওদের / ওকে |
| কাহার / কাহারা / কাহাকে | কার / কারা / কাকে |
| সহিত | সাথে |

### 2. Regular Verb Suffix Transformations
- **Present Continuous**: `-িতেছে` $\rightarrow$ `-চ্ছে` (*করিতেছে* $\rightarrow$ *করছে*, *চলিতেছে* $\rightarrow$ *চলছে*)
- **Past Continuous**: `-িতেছিল` $\rightarrow$ `-চ্ছিল` (*করিতেছিল* $\rightarrow$ *করছিল*)
- **Future**: `-িব` $\rightarrow$ `-ব` (*করিব* $\rightarrow$ *করব*, *বলিব* $\rightarrow$ *বলব*), `-িবেন` $\rightarrow$ `-বেন` (*করিবেন* $\rightarrow$ *করবেন*)
- **Past Perfect**: `-িয়াছিল` $\rightarrow$ `-েছিল` (*বলিয়াছিল* $\rightarrow$ *বলেছিল*, *করিয়াছিল* $\rightarrow$ *করেছিল*)
- **Conjunctive**: `-িয়া` $\rightarrow$ `-ে` (*করিয়া* $\rightarrow$ *করে*, *বলিয়া* $\rightarrow$ *বলে*)
- **Infinitive**: `-িবার` $\rightarrow$ `-ার` (*করিবার* $\rightarrow$ *করার*)

### CLI Usage
```bash
./bin/abk_cli --sadhu "তাহারা ভাত খাইতেছে এবং পানি পান করিতেছে।"
# Output: তারা ভাত খাচ্ছে এবং পানি পান করছে।

./bin/abk_cli --cholito "তারা ভাত খাচ্ছে।"
# Output: তাহারা ভাত খাইতেছে।
```
