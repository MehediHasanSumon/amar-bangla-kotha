# Amar Bangla Kotha — Phonetic Keymap & Typing Guide
# আমার বাংলা কথা — ফোনেটিক কীম্যাপ ও টাইপিং নির্দেশিকা

**Amar Bangla Kotha (`libabk`)** implements a case-sensitive, lattice-decoded Bangla phonetic typing system compatible with the familiar Avro-style phonetic key convention, written clean-room from scratch.

**আমার বাংলা কথা (`libabk`)** একটি কেস-সেনসিটিভ ল্যাটিস-ডিকোডেড বাংলা ফোনেটিক টাইপিং ইঞ্জিন, যা পরিচিত অভ্র-ধাঁচের কী কনভেনশনের সাথে সামঞ্জস্যপূর্ণভাবে সম্পূর্ণ নিজস্ব কোড ও ডেটা দিয়ে তৈরি।

---

## Quick One-Page Cheat Sheet (এক নজরে সংক্ষিপ্ত তালিকা)

### 1. Vowels / স্বরবর্ণ ও কার (Case-Sensitive / বড়-ছোট হাতের অক্ষর আলাদা)
- **At word start or after a vowel** → Independent vowel (**স্বতন্ত্র স্বরবর্ণ**).
- **Right after a consonant** → Dependent vowel sign (**কার**).

| Key(s) | স্বরবর্ণ (Independent) | কার (Sign) | Example (উদাহরণ) | বাংলা ফলাফল |
|:---:|:---:|:---:|:---|:---|
| `o` | **অ** | *(inherent / অদৃশ্য)* | `omor`, `onek`, `kolom` | অমর, অনেক, কলম |
| `a` | **আ** | **া** | `amar`, `kotha` | আমার, কথা |
| `i` | **ই** | **ি** | `iti`, `din` | ইতি, দিন |
| `I` / `ee` | **ঈ** | **ী** | `Igol`, `eegol`, `kI`, `kee` | ঈগল, কী |
| `u` / `oo` | **উ** | **ু** | `ujan`, `bujhi`, `boojhi` | উজান, বুঝি |
| `U` | **ঊ** | **ূ** | `Unocollish`, `dUr` | ঊনচল্লিশ, দূর |
| `rri` | **ঋ** | **ৃ** | `rriju`, `grriho` | ঋজু, গৃহ |
| `e` | **এ** | **ে** | `emon`, `keno` | এমন, কেন |
| `OI` | **ঐ** | **ৈ** | `OIrabot`, `kOI` | ঐরাবত, কৈ |
| `O` | **ও** | **ো** | `OtoprOto`, `glOb` | ওতপ্রোত, গ্লোব |
| `OU` | **ঔ** | **ৌ** | `OUpodeshik`, `bOU`, `kOU` | ঔপদেশিক, বৌ, কৌ |

> **Important (`koi` vs `kOI` / গুরুত্বপূর্ণ পার্থক্য):**
> Uppercase letters are distinct keys — input is **never** case-folded.
> - `koi` = `k` + inherent `o` + independent `i` → **কই** (while `কৈ` also appears in suggestions).
> - `kOI` = `k` + `OI` → **কৈ** (while `কই` also appears in suggestions).
> - `kou` → **কউ**, whereas `kOU` → **কৌ**.

---

### 2. Backtick Separator (`` ` ``) / ব্যাকটিক বিভাজক (অ্যাকসেন্ট কী)

The backtick key (`` ` ``) acts as an explicit structural separator:
ব্যাকটিক কী (`` ` ``) বর্ণের জোড়া ভাঙতে বা নির্দিষ্ট রূপ জোর করতে ব্যবহৃত হয়:

| Pattern | Behavior (কাজ) | বাংলা ব্যাখ্যা | Example | Output |
|:---|:---|:---|:---|:---|
| `` `<vowel> `` | Forces independent vowel after a consonant | ব্যঞ্জনের পরেও পূর্ণ স্বরবর্ণ বসায় | `` ek`i `` vs `eki` | **একই** vs একি |
| `` `a `` | Forces plain `আ` after a vowel instead of `য়া` | স্বরবর্ণের পরে `য়া` না করে সরাসরি `আ` বসায় | `` sami`a `` vs `samia` | **সামিআ** vs সামিয়া |
| `` <vowel>` `` | Forces vowel sign (kar) standalone; `` o` `` outputs nothing | ব্যঞ্জন ছাড়াই কার-চিহ্ন বসায় (`` o` `` ফাঁকা থাকে) | `` a` ``, `` i` ``, `` I` `` | **া**, **ি**, **ী** |
| `` C1`C2 `` | Prevents automatic conjunct between two consonants | দুটি ব্যঞ্জনের যুক্তাক্ষর হওয়া থামায় | `` ek`Ta ``, `` am`ra ``, `` ag`lanO `` | **একটা**, **আমরা**, **আগলানো** |
| `` t`` `` | Khanda-Ta (two backticks after `t`) | খণ্ড-ত (`ৎ`) তৈরি করে | `` hot``hat`` ``, `` ut``sob `` | **হঠাৎ**, **উৎসব** |
| `` .` `` / `` :` `` / `` $` `` | Literal ASCII `.`, `:`, `$` | দাঁড়ি/বিসর্গ/টাকা না করে ইংরেজি `.`, `:`, `$` রাখে | `` .` ``, `` :` ``, `` $`500 `` | `.`, `:`, `$৫০০` |

- **Stray Backtick Rule (অতিরিক্ত ব্যাকটিক):** Any lone or trailing `` ` `` that does not trigger a special escape (e.g. standalone `` ` `` or trailing `` ek` ``) is silently consumed as a separator boundary and **never** appears as a literal `` ` `` in Bangla output.

---

### 3. Consonants / ব্যঞ্জনবর্ণ ও বিশেষ চিহ্ন (Case-Sensitive)

#### Velar & Palatal / ক-বর্গ ও চ-বর্গ
| Key | Bangla | Key | Bangla | Key | Bangla | Key | Bangla | Key | Bangla |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| `k` | **ক** | `kh` | **খ** | `g` | **গ** | `gh` | **ঘ** | `Ng` | **ঙ** |
| `c` | **চ** | `ch` | **ছ** | `j` | **জ** | `jh` | **ঝ** | `NG` | **ঞ** |

#### Retroflex & Dental / ট-বর্গ ও ত-বর্গ
| Key | Bangla | Key | Bangla | Key | Bangla | Key | Bangla | Key | Bangla |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| `T` | **ট** | `Th` | **ঠ** | `D` | **ড** | `Dh` | **ঢ** | `N` | **ণ** |
| `t` | **ত** | `th` | **থ** | `d` | **দ** | `dh` | **ধ** | `n` | **ন** |

#### Labial, Approximants, Sibilants & Modifiers / প-বর্গ, অন্তঃস্থ, উষ্ম ও অনুষঙ্গ
| Key | Bangla | Key | Bangla | Key | Bangla | Key | Bangla |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| `p` | **প** | `ph` / `f` | **ফ** | `b` | **ব** | `bh` / `v` | **ভ** |
| `m` | **ম** | `z` | **য** | `r` | **র** | `l` | **ল** |
| `sh` / `S` | **শ** | `Sh` | **ষ** | `s` | **স** | `h` | **হ** |
| `R` | **ড়** | `Rh` | **ঢ়** | `y` / `Y` | **য়** | `J` | **জ়** *(optional setting)* |
| `ng` | **ং** | `:` | **ঃ** | `^` | **ঁ** | `` t`` `` | **ৎ** |

#### Special Consonant Rules / ব্যঞ্জনবর্ণের বিশেষ নিয়ম
1. **Word-initial `y` vs `Y`:** Bangla `য়` never starts a standard word, so lowercase `y` at word start produces **ইয়** (`yahoo` → **ইয়াহু**). Capital `Y` forces **য়** anywhere, even at word start (`Y` → **য়**).
2. **Post-vocalic `a`:** After a vowel or vowel sign, typing `a` produces **য়া** (`samia` → **সামিয়া**, `doa` → **দয়া**; after `O`/`ো` it produces **ওয়া**: `khaOa` → **খাওয়া**). Use `` `a `` to force plain `আ`.
3. **Dedicated Ligature Shortcuts:** `gg` or `jNG` → **জ্ঞ** (`biggo` → **বিজ্ঞ**); `kkh` or `kx` → **ক্ষ** (`okkhor` → **অক্ষর**).

---

### 4. Phalas, Reph & Hasanta / ফলা, রেফ ও হসন্ত

| Name (নাম) | Key(s) | Rule (নিয়ম) | Examples (উদাহরণ) |
|:---|:---:|:---|:---|
| **ব-ফলা (Ba-phala)** | `w` (or `b` in words) | `w` after a consonant (`্ + ব`); dictionary/rules also handle `b` clusters | `bishwo` → **বিশ্ব**, `udwayI` → **উদ্বায়ী**, `ambia` → **আম্বিয়া**, `balb` → **বাল্ব** |
| **য-ফলা (Ya-phala)** | `y` / `Z` | `y` after consonant (`্ + য`); capital `Z` forces য-ফলা anywhere (including after vowels or `র` via ZWJ) | `bybohar` → **ব্যবহার**, `aZDmin` → **অ্যাডমিন**, `rZ` / `` r``yab `` → **র‍্য** / **র‍্যাব** |
| **র-ফলা (Ra-phala)** | `r` | `r` after consonant (`C + ্ + র`) | `prosonno` → **প্রসন্ন**, `protidin` → **প্রতিদিন** |
| **ম-ফলা (Ma-phala)** | `m` | `m` after consonant (`C + ্ + ম`) | `podma` → **পদ্মা**, `lokkhmI` → **লক্ষ্মী** |
| **রেফ (Reph)** | `rr` | `rr` **before** a consonant (`র + ্ + C`); single `r` stays পূর্ণ `র` | `orrko` → **অর্ক**, `arrtonad` → **আর্তনাদ**, `orrthohIn` → **অর্থহীন** (`sorkar` → সরকার) |
| **হসন্ত (Hasanta)** | `,,` | Two commas emit explicit virama (`্`, `U+09CD`) | `m,,` → **ম্**, `h,,` → **হ্** |

---

### 5. Conjuncts / যুক্তাক্ষর (Automatic General Rule)

Adjacent consonants automatically join with `্` (`C1 + ্ + C2`) when phonotactically valid in Bengali:
পাশাপাশি ব্যঞ্জনবর্ণ টাইপ করলে স্বয়ংক্রিয়ভাবে যুক্তাক্ষর তৈরি হয়:

- `okkhor` → **অক্ষর**, `onto` → **অন্ত**, `biggo` → **বিজ্ঞ**, `brahmoN` → **ব্রাহ্মণ**, `sombol` → **সম্বল**, `glOb` → **গ্লোব**, `okTOpas` → **অক্টোপাস**, `raShTro` → **রাষ্ট্র**।
- **When you want separate consonants (যুক্তাক্ষর না চাইলে):**
  - Type `` ` `` between them: `` ek`Ta `` → **একটা**, `` am`ra `` → **আমরা**, `` ag`lanO `` → **আগলানো**।
  - Or rely on **Autocorrect** for everyday words: typing `amra`, `ekta`, `aglano`, `apni`, `korbo` directly gives **আমরা**, **একটা**, **আগলানো**, **আপনি**, **করব**!

---

### 6. Punctuation, Symbols & Smart Guards / বিরামচিহ্ন, সংখ্যা ও সুরক্ষা নিয়ম

| Input | Default Output | Guard / Exception Rules (কখন রূপান্তর হবে না) |
|:---:|:---:|:---|
| `.` | **।** *(দাঁড়ি)* | Stays literal `.` inside **decimals** (`3.5` → `৩.৫`), **URLs** (`https://gov.bd`), **emails** (`user@mail.org`), **file paths** (`/etc/hosts`, `~/doc.txt`, `main.cpp`), **ellipsis** (`...`), and **abbreviations** (`Dr.`, `Mr.`, `e.g.`, `i.e.`, `Ph.D.`, `U.S.A.`). |
| `.`` ` `` | `.` | Explicit literal dot anywhere. |
| `:` / `:`` ` `` | **ঃ** / `:` | `:` emits visarga (`ঃ`); `:`` ` `` emits literal colon (`:`). |
| `$` / `$`` ` `` | **৳** / `$` | Emits Taka (`৳`) standalone or with amounts (`$500` → `৳৫০০`, `500$` → `৫০০৳`), **never** inside shell/code variables (`$PATH`, `${HOME}`, `$(pwd)`, `$var`, `$$`, `$?`, `foo$bar`). `$`` ` `` forces literal `$`. |
| `0`–`9` | **০**–**৯** | Configurable setting: Bangla digits ON (`০-৯`) or OFF (`0-9`). |
| `\|` *(pipe)* | `\|` *(or `.` / `।`)* | Configurable setting: passthrough `\|` (default), literal `.` (Avro *"Use pipe for dot"*), or `।`. |
| `; ? ! ( ) { } [ ] / \ + - * %` | *Unchanged* | Passed through directly as typed. |

---

### 7. Autocorrect & Candidate Window / অটোকারেক্ট ও সাজেশন উইন্ডো

1. **Autocorrect (স্বয়ংক্রিয় সংশোধন):**
   - Handles conjunct exceptions (`amra` → **আমরা**), English-spelling loanwords (`software` → **সফটওয়্যার**, `doctor` → **ডাক্তার**, `school` → **স্কুল**), and user shortcuts.
   - Editable locally: add, edit, delete, import/export TSV, or toggle globally on/off.
   - **Never overrides explicit control keys:** If you type any uppercase key (`A`–`Z`), backtick (`` ` ``), or double-comma (`,,`), autocorrect steps aside and honors your exact phonetic keys.
   - **First Backspace Revert (প্রথম ব্যাকস্পেসে পূর্বাবস্থা):** Pressing `Backspace` immediately after an autocorrect commit reverts to the exact raw text you typed without re-forcing autocorrect.

2. **Suggestion Modes & Navigation (সাজেশন মোড ও কিবোর্ড কন্ট্রোল):**
   - **Candidate Order:** Remembered choice → Autocorrect match → Classic phonetic output → Skeleton-matched dictionary suggestions (collapsing `ত/ট/ৎ`, `শ/ষ/স`, `ই/ঈ`, `উ/ঊ`, `ন/ণ`, `র/ড়/ঢ়` so spelling uncertainties still find the right Bangla word).
   - **3 Modes:** `Character-first` *(default)*, `Dictionary-first`, or `Classic phonetic` *(no popup window)*.
   - **Keys:** `Down` arrow (and optional `Tab` in settings) browses candidates; `1`–`9` selects directly; `Space`/`Enter` commits; `Esc` closes the suggestion window.
   - **Remember My Choice:** Selecting a non-default candidate remembers your preference locally (size-capped & resettable in Settings).
   - **Caret Follow & Pin:** Follows the text cursor by default, or can be pinned to fixed coordinates when an application misreports caret coordinates.

---

## বাংলা সংস্করণ — সংক্ষিপ্ত ব্যবহারবিধি

১. **বড় ও ছোট হাতের অক্ষর আলাদা:** `i` = ই/ি, কিন্তু `I` বা `ee` = ঈ/ী; `o` = অ (ব্যঞ্জনের পরে অদৃশ্য), কিন্তু `O` = ও/ো; `koi` লিখলে **কই** এবং `kOI` লিখলে **কৈ** পাবেন (উভয় বানানই সাজেশন তালিকায় থাকবে)।  
২. **ব্যাকটিক (`` ` ``) ব্যবহার:** দুটি ব্যঞ্জনের যুক্তাক্ষর থামাতে মাঝে `` ` `` দিন (যেমন `` ek`Ta `` → **একটা**), ব্যঞ্জনের পরে পূর্ণ স্বরবর্ণ আনতে `` ` `` দিন (যেমন `` ek`i `` → **একই**), আর খণ্ড-ত লিখতে `` t`` `` চাপুন (**ৎ**)।  
৩. **ফলা ও রেফ:** ব্যঞ্জনের পরে `w` = ব-ফলা (`bishwo` → **বিশ্ব**), `y` বা `Z` = য-ফলা (`bybohar` → **ব্যবহার**, `aZDmin` → **অ্যাডমিন**), `r` = র-ফলা (`protidin` → **প্রতিদিন**), `m` = ম-ফলা (`podma` → **পদ্মা**), আর ব্যঞ্জনের আগে `rr` = রেফ (`orrko` → **অর্ক**)। হসন্তের জন্য দুইবার কমা `,,` চাপুন (**্**)।  
৪. **স্মার্ট বিরামচিহ্ন:** বাক্যের শেষে `.` দিলে দাঁড়ি (**।**) বসে, কিন্তু দশমিক সংখ্যা (`3.5` → `৩.৫`), ওয়েবসাইট, ইমেইল, ফাইল পাথ বা সংক্ষিপ্ত রূপে (`Dr.`, `e.g.`) ডট (`.`) অক্ষুণ্ণ থাকে। টাকার চিহ্নের জন্য `$` চাপুন (`$500` → `৳৫০০`), তবে কোডের ভ্যারিয়েবল (`$PATH`, `${HOME}`) স্বয়ংক্রিয়ভাবে অপরিবর্তিত থাকে।  
৫. **সাজেশন ও অটোকারেক্ট:** `amra` → **আমরা** বা `software` → **সফটওয়্যার** অটোকারেক্টের মাধ্যমে সরাসরি লেখা যায়, আর অটোকারেক্ট হওয়ার পর প্রথমবার `Backspace` চাপলেই আপনার টাইপ করা মূল লেখা ফিরে আসবে।
