# Backspace Mechanics & Unicode Decomposition Specification

**Document Version:** 1.0.0  
**Engine:** Amar Bangla Kotha (`libabk`)  
**Scope:** Linux (IBus, Fcitx5), macOS (InputMethodKit), Windows (TSF)

---

## 1. Executive Summary & Architectural Decision

In Bengali desktop computing and phonetic IMEs, text deletion via the `Backspace` key presents a fundamental linguistic dilemma:
**Should pressing Backspace delete an entire visual Grapheme Cluster (orthographic syllable), or should it delete the single logical Unicode code point (or logical sub-unit)?**

### Decision: Logical Unit Deletion (`pop_logical_unit`) by Default

Amar Bangla Kotha adopts **Logical Unit Deletion** as the default behavior for backspacing within compositions and committed text, while providing full engine support for **Grapheme Cluster Deletion** (`pop_grapheme_cluster`):

| Approach | Definition | Example: `কি` (U+0995 U+09BF) | Example: `ক্ত` (U+0995 U+09CD U+09A4) | User Experience |
|:---|:---|:---|:---|:---|
| **Logical Unit (`pop_logical_unit`)** *(Default)* | Deletes one logical Unicode sign or consonant from the end of the byte stream. | `কি` &rarr; `ক` | `ক্ত` &rarr; `ক্` &rarr; `ক` | **Predictable & Incremental:** Mirrors Avro Keyboard, Microsoft Bengali IME, and Linux ibus-m17n. Allows fixing typos without retyping the entire syllable. |
| **Grapheme Cluster (`pop_grapheme_cluster`)** | Deletes the entire visual typographic cluster in one keystroke according to UAX #29. | `কি` &rarr; `""` | `ক্ত` &rarr; `""` | **Destructive:** Deletes entire consonant conjuncts and multi-code-point syllables in one keystroke. |

---

## 2. Bengali Script Linguistic & Typographic Realities

### 2.1 The "Left-Sided" Vowel Signs (Visual vs. Logical Ordering)

In Bengali orthography, three vowel signs (*Kar* signs) render **visually to the left** of the consonant they modify:
- **Hrosho-I Kar:** `ি` (U+09BF `BENGALI VOWEL SIGN I`)
- **E-Kar:** `ে` (U+09C7 `BENGALI VOWEL SIGN E`)
- **Oi-Kar:** `ৈ` (U+09C8 `BENGALI VOWEL SIGN AI`)

Additionally, two-part vowel signs split around the consonant:
- **O-Kar:** `ো` (U+09CB) &mdash; visually surrounds consonant left and right (`ে` + `া`).
- **Ou-Kar:** `ৌ` (U+09CC) &mdash; visually surrounds consonant left and right (`ে` + `ৗ`).

#### Logical Sequence vs. Display
Despite visual placement on the left, Unicode strictly enforces **phonetic/logical ordering**:
$$\text{Consonant} + \text{Vowel Sign}$$
For example, `কি` is encoded as:
1. `ক` (U+0995 `BENGALI LETTER KA`)
2. `ি` (U+09BF `BENGALI VOWEL SIGN I`)

When a user types `কি` and presses `Backspace`:
- **Correct Logical Popping:** Removes `ি` (U+09BF), leaving `ক` (U+0995). The visual character on the left disappears while the base consonant remains.
- **Incorrect/Naive Visual Popping:** If an engine attempted to delete from the visual left, it would delete `ক` and leave an illegal standalone vowel sign `ি`.

`libabk`'s `utf8::pop_logical_unit` correctly recognizes all Bengali Kar signs (`া`, `ি`, `ী`, `ু`, `ূ`, `ৃ`, `ৄ`, `ে`, `ৈ`, `ো`, `ৌ`, `ৢ`, `ৣ`) as dependent marks attached to the trailing end of the logical stream and removes them before the base consonant.

### 2.2 Conjuncts (Yuktakshar) and Hasanta (Virama)

Bengali conjuncts are formed by joining two or more consonants with a Hasanta (`্` U+09CD `BENGALI SIGN VIRAMA`):
$$\text{Consonant}_1 + \text{Hasanta} + \text{Consonant}_2$$

Example: `ক্ত` (K-TA)
1. `ক` (U+0995)
2. `্` (U+09CD)
3. `ত` (U+09A4)

#### Progressive Deletion Flow
When backspacing through `ক্ত`:
1. **1st Backspace:** Pops `ত` (U+09A4) &rarr; String becomes `ক্` (Ka + Hasanta).
2. **2nd Backspace:** Pops `্` (U+09CD) &rarr; String becomes `ক` (Ka).
3. **3rd Backspace:** Pops `ক` (U+0995) &rarr; String becomes empty.

This granular progression enables precise editing of complex ligatures (e.g., `ক্ষ্ম`, `ম্প্র`, `ত্থ`).

### 2.3 Nukta Consonants (`ড়`, `ঢ়`, `য়`)

In Bengali Unicode:
- `ড়` (U+09DC `BENGALI LETTER RRA`): Precomposed form.
- `ঢ়` (U+09DD `BENGALI LETTER RHA`): Precomposed form.
- `য়` (U+09DF `BENGALI LETTER YYA`): Precomposed form.

Under Unicode NFC normalization (enforced by `libabk`), decomposed sequences like `ড` + `়` (U+09A1 + U+09BC) are automatically normalized into the precomposed character `ড়` (U+09DC). Therefore, pressing Backspace deletes `ড়` in a single step, preventing orphaned Nukta marks.

### 2.4 Khanda-Ta (`ৎ`), Anusvara (`ং`), Visarga (`ঃ`), Chandrabindu (`ঁ`)

- `ৎ` (U+09CE `BENGALI LETTER KHANDA TA`): Independent code point, popped as 1 logical unit.
- `ং` (U+0982 `BENGALI SIGN ANUSVARA`): Dependent mark, popped as 1 logical unit.
- `ঃ` (U+0983 `BENGALI SIGN VISARGA`): Dependent mark, popped as 1 logical unit.
- `ঁ` (U+0981 `BENGALI SIGN CANDRABINDU`): Dependent mark, popped as 1 logical unit.

### 2.5 Zero-Width Characters (ZWJ & ZWNJ)

- **ZWJ (`U+200D`):** Used in explicit conjunct overrides, such as Ya-phala (`র‍্য` = `র` + `্` + `ZWJ` + `য`).
- **ZWNJ (`U+200C`):** Used to prevent conjunct formation (e.g., `ক্` + `ZWNJ` + `ত` &rarr; `ক্` এবং `ত` পৃথক থাকে).

When popping a logical unit, if the trailing character is preceded by a ZWJ/ZWNJ attached to a Virama, the engine removes the dependent sign cleanly without leaving invisible formatting artifacts.

---

## 3. Undo on Backspace Specification

### 3.1 Motivation
Phonetic IMEs frequently perform auto-completion or auto-commit when the user presses `Space`, `Enter`, or punctuation. Occasionally, the engine may autocorrect a slang word or proper noun to an unintended dictionary word.
Without Undo on Backspace, the user must backspace through every Bengali character of the wrong word and retype it in English mode.

### 3.2 State Machine
The core engine maintains an `UndoRecord` struct:

```cpp
struct UndoRecord {
    std::string raw_input;        // What the user typed in Latin (e.g. "kotha")
    std::string committed_text;   // What was committed to host app (e.g. "কথা ")
    bool was_autocorrected;       // Whether autocorrect changed the spelling
    bool can_undo;                // Valid only immediately after commit
};
```

#### Lifecycle Rules
1. **Commit Event:**
   When a candidate is committed to the host application (via Space, Enter, or click), `engine.record_commit(raw_input, committed_text)` stores the snapshot and sets `can_undo = true`.
2. **Immediate Backspace Event:**
   If Backspace is received while `buffer.empty()` and `engine.can_undo_commit()` is true:
   - The engine retrieves the committed text length and raw input string via `abk_engine_get_undo_info()`.
   - The frontend issues a `delete_surrounding_text` (or equivalent host API) to erase the committed word.
   - The frontend restores `buffer = raw_input`.
   - The engine sets `can_undo = false`.
   - The composition preedit is reopened with the original Latin text and suggestion list.
3. **Second Backspace Event:**
   The user is now in standard preedit mode; pressing Backspace deletes characters normally from the Latin buffer (`raw_input.pop_back()`).
4. **Invalidation (Undo Chain Break):**
   The undo record is immediately invalidated (`clear_undo()`) upon:
   - Any typing key (alphanumeric, symbol, space) while buffer is empty.
   - Cursor movement or arrow navigation in the host document.
   - Focus change (`focus_out`, window deactivate).
   - Explicit escape (`Escape` key).
   - Entering secure/password fields.

---

## 4. Token Classification & Non-Bengali Bypass

To prevent erratic transliteration while typing code, URLs, or mixed-language text, `TokenClassifier` categorizes all input tokens:

```
                  +--------------------------------+
                  |         Raw User Input         |
                  +--------------------------------+
                                   |
         +-------------------------+-------------------------+
         |                         |                         |
  [Starts with ',']      [URL / Email / Path]         [Code Token]
         |                         |                         |
         v                         v                         v
Explicit Escape            Bypass Completely         Bypass Completely
(Transliterate)           (Keep Raw Latin)          (Keep Raw Latin)
                                                             |
                 +-------------------------------------------+
                 |
         [English Word]
                 |
      +----------+----------+
      |                     |
[Banglish Stop-Word?]   [In English Vocab?]
      |                     |
      v                     v
Never Bypass             Bypass
(Transliterate)         (Keep Raw Latin)
```

### 4.1 Non-Bypass Guarantees for Banglish (0% False-Positive Rate)
High-frequency Banglish words share identical character spellings with common English abbreviations and words (e.g., `ami`, `tumi`, `kotha`, `bhalo`, `dhonnobad`, `kemon`, `jani`, `shesh`, `desh`, `amader`).
The `banglish_stop_words` filter explicitly intercepts these tokens prior to vocabulary matching, ensuring zero false-positive English bypasses.

---

## 5. Automated Verification Checklist

- [x] Unit test: `pop_logical_unit` removes `ি` from `কি` leaving `ক`.
- [x] Unit test: `pop_logical_unit` removes `ে` from `কে` leaving `ক`.
- [x] Unit test: `pop_logical_unit` removes `ৈ` from `কৈ` leaving `ক`.
- [x] Unit test: Progressive deletion of conjunct `ক্ত` (`ক্ত` &rarr; `ক্` &rarr; `ক` &rarr; `""`).
- [x] Unit test: `pop_grapheme_cluster` removes entire cluster `কি` in one call.
- [x] State machine test: Undo restores raw Latin on first backspace; second backspace pops Latin character.
- [x] Token classifier: URLs, emails, file paths, camelCase, snake_case bypass transliteration.
- [x] Banglish protection: `ami`, `tumi`, `kotha` are NEVER bypassed as English.
