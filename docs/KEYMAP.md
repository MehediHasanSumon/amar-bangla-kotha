# Amar Bangla Kotha - Phonetic Keymap Reference

**Amar Bangla Kotha** uses a modern, context-sensitive phonetic input schema inspired by conventional Avro Phonetic standards, extended with an unambiguous beam-search lattice decoder and explicit phonetic overrides.

---

## 1. Vowels & Vowel Signs (স্বরবর্ণ ও কার)

Vowels behave context-sensitively:
- **At word start or after another vowel**: Produces an **independent vowel** (স্বতন্ত্র স্বরবর্ণ).
- **Immediately following a consonant**: Produces a **dependent vowel sign** (কার - Kar).

| Latin Input | Independent Vowel (মুক্ত স্বরবর্ণ) | Example | Dependent Kar (কার) | Example | Notes |
|:---:|:---:|:---|:---:|:---|:---|
| `a` | **অ** / **আ** | `ami` → আমি, `onek` → অনেক | **া** | `kotha` → কথা | Context-aware |
| `aa` / `A` | **আ** | `aam` → আম | **া** | `kaaj` → কাজ | Long 'A' |
| `i` | **ই** | `iti` → ইতি | **ি** | `koti` → কোটি | Hrasva-I |
| `I` / `ee` | **ঈ** | `Id` → ঈদ | **ী** | `tIr` → তীর | Dirgha-I (Uppercase override) |
| `u` | **উ** | `upor` → উপর | **ু** | `tumi` → তুমি | Hrasva-U |
| `U` / `oo` | **ঊ** | `Usha` → ঊষা | **ূ** | `bhUt` → ভূত | Dirgha-U (Uppercase override) |
| `rri` | **ঋ** | `rritu` → ঋতু | **ৃ** | `krrishna` → কৃষ্ণ | Ri-kar |
| `e` | **এ** | `ektu` → একটু | **ে** | `desh` → দেশ | E-kar |
| `OI` | **ঐ** | `Oikya` → ঐক্য | **ৈ** | `tOiri` → তৈরি | Oi-kar (Uppercase override) |
| `o` | **ও** | `onek` → অনেক / `o` → ও | **ো** | `koro` → করো | O-kar |
| `OU` | **ঔ** | `OUshodh` → ঔষধ | **ৌ** | `nOUka` → নৌকা | Ou-kar (Uppercase override) |

---

## 2. Diphthongs & The `koi` vs `kOI` Disambiguation

In legacy phonetic engines with 1-pass greedy longest-match, typing `koi` greedily converted `oi` to `ৈ` (`কৈ`), which broke the common Bengali word **কই** (meaning *"where"*).

In Amar Bangla Kotha, our lattice beam-search parser explores multiple parallel hypotheses and weights them against unigram frequency:

| Input | Output | Linguistic Reading | Mechanism |
|:---|:---|:---|:---|
| `koi` | **কই** | Consonant `ক` + Independent `ই` | Top-1 default candidate based on linguistic corpus frequency |
| `kOI` | **কৈ** | Consonant `ক` + Dependent Kar `ৈ` | Explicit uppercase override for Oi-kar |
| `kOi` | **কৈ** | Consonant `ক` + Dependent Kar `ৈ` | Capitalized diphthong variation |
| `khai` | **খাই** | `খা` + `ই` | Natural diphthong split |
| `jai` | **যাই** | `যা` + `ই` | Natural diphthong split |
| `gei` | **গেই** | `গে` + `ই` | Natural diphthong split |
| `bhai` | **ভাই** | `ভা` + `ই` | Natural diphthong split |

---

## 3. Consonants (ব্যঞ্জনবর্ণ)

### Velar (ক-বর্গ)
| Latin | Output | Example |
|:---|:---|:---|
| `k` | **ক** | `kotha` → কথা |
| `kh` | **খ** | `khobor` → খবর |
| `g` | **গ** | `gaan` → গান |
| `gh` | **ঘ** | `ghor` → ঘর |
| `Ng` / `ng` | **ঙ** / **ং** | `rongo` → রঙ্গ |

### Palatal (চ-বর্গ)
| Latin | Output | Example |
|:---|:---|:---|
| `c` / `ch` | **চ** | `chokh` → চোখ |
| `Ch` | **ছ** | `Chobi` → ছবি |
| `j` | **জ** | `jol` → জল |
| `jh` | **ঝ** | `jhor` → ঝড় |
| `NG` | **ঞ** | `munchi` → মুঞ্ছি |

### Retroflex (ট-বর্গ)
| Latin | Output | Example |
|:---|:---|:---|
| `T` | **ট** | `Taka` → টাকা |
| `Th` | **ঠ** | `Thik` → ঠিক |
| `D` | **ড** | `Daak` → ডাক |
| `Dh` | **ঢ** | `Dhal` → ঢাল |
| `N` | **ণ** | `karoN` → কারণ |

### Dental (ত-বর্গ)
| Latin | Output | Example |
|:---|:---|:---|
| `t` | **ত** | `tumi` → তুমি |
| `th` | **থ** | `thamo` → থামো |
| `d` | **দ** | `din` → দিন |
| `dh` | **ধ** | `dhon` → ধন |
| `n` | **ন** | `naam` → নাম |

### Labial (প-বর্গ)
| Latin | Output | Example |
|:---|:---|:---|
| `p` | **প** | `pani` → পানি |
| `ph` / `f` | **ফ** | `phul` → ফুল |
| `b` | **ব** | `boi` → বই |
| `bh` / `v` | **ভ** | `bhalo` → ভালো |
| `m` | **ম** | `maa` → মা |

### Liquids, Flaps & Semivowels
| Latin | Output | Example |
|:---|:---|:---|
| `r` | **র** | `raat` → রাত |
| `l` | **ল** | `laal` → লাল |
| `R` | **ড়** | `baRi` / `bari` → বাড়ি |
| `Rh` | **ঢ়** | `aaRha` → আঢ়া |
| `y` | **য়** | `shomoy` → সময় |
| `w` | **ওয়** | `hawa` → হাওয়া |

### Sibilants & Aspirate (শ, ষ, স, হ)
| Latin | Output | Example | Notes |
|:---|:---|:---|:---|
| `sh` | **শ** | `shohor` → শহর | Talobyo-Sha |
| `Sh` / `S` | **ষ** | `koshTo` → কষ্ট | Murdhonyo-Sha |
| `s` | **স** | `shokol` → সকল | Donto-Sha |
| `h` | **হ** | `haat` → হাত | Ho |

---

## 4. Conjuncts (যুক্তাক্ষর - Juktakkhor)

Consonants joined together automatically form traditional conjuncts via Unicode Virama (`্`):

| Pattern | Conjunct | Composition | Example |
|:---|:---:|:---|:---|
| `kkh` / `x` | **ক্ষ** | ক + ্ + ষ | `khoma` / `kxoma` → ক্ষমা |
| `ggy` / `jng` | **জ্ঞ** | জ + ্ + ঞ | `gyan` → জ্ঞান |
| `nt` | **ন্ত** | ন + ্ + ত | `shanto` → শান্ত |
| `nT` | **ন্ট** | ন + ্ + ট | `ghonTa` → ঘণ্টা |
| `nd` | **ন্দ** | ন + ্ + দ | `anondo` → আনন্দ |
| `mp` | **ম্প** | ম + ্ + প | `shomporko` → সম্পর্ক |
| `st` | **স্ত** | স + ্ + ত | `rasta` → রাস্তা |
| `shch` | **শ্চ** | শ + ্ + চ | `nishchoy` → নিশ্চয় |
| `bhy` | **ভ্য** | ভ + ্ + য | `shovyo` → সভ্য |

---

## 5. Special Modifiers & ZWJ / ZWNJ Handling

| Keystroke | Name | Unicode Codepoint | Example | Function |
|:---:|:---|:---:|:---|:---|
| `t``` | **খণ্ডের ত (ৎ)** | `U+09CE` | `hot``hat` → হঠাৎ | Explicit Khanda-Ta mark |
| `:` | **বিসর্গ (ঃ)** | `U+0983` | `du:kho` → দুঃখ | Visarga |
| `^` | **চন্দ্রবিন্দু (ঁ)** | `U+0981` | `c^ad` → চাঁদ | Chandrabindu |
| `ng` | **অনুস্বার (ং)** | `U+0982` | `bangla` → বাংলা | Anusvara |
| `r``` | **ZWJ Reph Barrier** | `U+200D` | `r``yab` → **র‍্যাব** | Generates `র + ্ + ZWJ + য-ফলা` instead of `র্য` |
| `,,` | **হসন্ত (্)** | `U+09CD` | `h,,` → হ্ | Forces explicit virama |

---

## 6. Numerals & Punctuation

- **Numerals**: `0 1 2 3 4 5 6 7 8 9` map contextually to Bengali numerals `০ ১ ২ ৩ ৪ ৫ ৬ ৭ ৮ ৯`.
- **Decimal Numbers**: Decimal numbers like `3.5` or `0.05` are preserved with standard period (`3.5`).
- **Bengali Dari (।)**:
  - Typing `.` at the end of a Bengali sentence emits Bengali Dari (`।`, `U+09F7`).
  - Typing `..` explicitly produces a Dari (`।`).
  - Typing `...` produces standard ellipsis (`...`).
- **Internet Patterns**: URLs (`http://`, `https://`, `www.`) and email addresses (`name@domain.com`) are automatically detected and preserved in Latin script without transliteration.
