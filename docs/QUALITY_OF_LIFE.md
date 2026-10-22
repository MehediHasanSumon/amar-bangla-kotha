# Workstream 8: Quality of Life (QoL) Specification

## 1. Executive Summary

Workstream 8 delivers essential user experience enhancements to **Amar Bangla Kotha (`libabk`) v1.1**, designed to streamline day-to-day writing, provide instant help, and respect user privacy at every layer:

1. **Quick Convert Tool**: Instant bidirectional transliteration for bulk and selected text (Banglish $\leftrightarrow$ Unicode Bangla) preserving code, URLs, and emails.
2. **Emoji & Symbol Suggestions**: Contextual emoji suggestions (e.g., `smile` $\rightarrow$ 😊, `bhalobasha` $\rightarrow$ ❤️, `fire` $\rightarrow$ 🔥, `tk` $\rightarrow$ ৳) ranked appropriately without dislodging primary lexical candidates.
3. **Smart Punctuation**: Typographical enhancements:
   - Full-stop `.` $\rightarrow$ Dari `।` (with intelligent bypass for decimals `3.14`, web domains `google.com`, emails, and abbreviations `e.g.`, `ডা.`).
   - Triple dot `...` $\rightarrow$ True ellipsis `…` (`U+2026`).
   - Paired straight quotes `"` $\rightarrow$ Curly quotes `“` and `”`.
4. **Live Keymap Cheat Sheet**: Fully searchable, real-time interactive cheat sheet integrated into the GUI and documented in [`docs/KEYMAP.md`](file:///home/sumon/Desktop/Keyboard/docs/KEYMAP.md).
5. **Local Feedback Button ("This suggestion is wrong")**: Allows users to save erroneous suggestions locally to `~/.config/amar-bangla-kotha/feedback.jsonl` for inclusion in GitHub bug reports.
6. **Opt-in Local Correction Log**: Privacy-first typing correction tracker:
   - **Default: OFF**.
   - **Local only**: Never transmits any network packets.
   - **Strict Zero Password Logging**: Complete auto-bypass in password/PIN/secure fields.
   - **Size-capped**: 1,000 entries max rolling window with atomic disk sync and instant clear button.

---

## 2. Quick Convert (Banglish $\leftrightarrow$ Unicode Bangla)

### C++ Engine API
- [`QualityOfLife::convert_banglish_to_bangla(text, dict)`](file:///home/sumon/Desktop/Keyboard/include/abk/quality_of_life.h#L64-L70):
  - Tokenizes paragraphs while respecting non-transliterated technical tokens (`URL`, `EMAIL`, `FILE_PATH`, `CODE_TOKEN`).
  - Separates punctuation prefixes `(` and suffixes `?`, `,`, `.`.
  - Transliterates Banglish phonetic tokens into canonical Unicode Bengali (NFC).
- [`QualityOfLife::convert_bangla_to_banglish(text)`](file:///home/sumon/Desktop/Keyboard/include/abk/quality_of_life.h#L71-L74):
  - Inverts Bengali Unicode codepoints into readable phonetic Latin text.
- C API:
  - `abk_convert_banglish_to_bangla(engine, in, out_buf, out_cap)`
  - `abk_convert_bangla_to_banglish(in, out_buf, out_cap)`

### Test Vectors
| Direction | Input | Output | Verification |
|---|---|---|---|
| Banglish $\rightarrow$ Bangla | `ami banglay gan gai` | `আমি বাংলায় গান গাই` | Passed |
| Code & URL Bypass | `visit https://bangla.org sumon@test.com` | `visit https://bangla.org sumon@test.com` | Passed |
| Bangla $\rightarrow$ Banglish | `আমি বাংলায় কথা বলি` | `ami banglay kotha boli` | Passed |

---

## 3. Smart Punctuation Rules

Applied via [`QualityOfLife::apply_smart_punctuation(text)`](file:///home/sumon/Desktop/Keyboard/include/abk/quality_of_life.h#L75-L82):

1. **Ellipsis Replacement**:
   `...` $\rightarrow$ `…` (`U+2026`)
2. **Paired Quotes**:
   `"quote"` $\rightarrow$ `“quote”` (`U+201C` and `U+201D`)
3. **Period to Bengali Dari**:
   `.` $\rightarrow$ `।` (`U+0964`), with **strict bypass filters**:
   - Preceded and followed by digits: `3.14159` $\rightarrow$ Unchanged.
   - Inside web domains: `example.com` $\rightarrow$ Unchanged.
   - Following common abbreviations: `e.g.`, `i.e.`, `etc.`, `vs.`, `dr.`, `mr.`, `ডা.`, `ড.` $\rightarrow$ Unchanged.

---

## 4. Emoji & Symbol Suggestions

Integrated directly into `AmarBanglaKothaEngine::process()`:
- **Symbolic Emoticons** (e.g. `:)`, `:D`, `<3`): Scored at `9500.0` to take rank #1 when typed intentionally.
- **Word Triggers** (e.g. `smile`, `bhalobasha`, `fire`, `ful`, `tara`, `tk`): Scored at `6200.0` so that primary lexical words (`ভালোবাসা`, `আগুন`, `ফুল`) retain rank #1 for typing flow, while the emoji appears seamlessly within the candidate list.

---

## 5. Local Feedback & Bug Reporting

Users can flag erroneous candidates directly in the Settings GUI or via C API:
- **Storage**: `~/.config/amar-bangla-kotha/feedback.jsonl`
- **Schema**:
```json
{
  "timestamp": "2026-10-23T01:05:00Z",
  "raw_input": "kotha",
  "received": "কোথা",
  "expected": "কথা",
  "os": "Linux 6.8.0-45-generic (x86_64)",
  "app": "gedit"
}
```
- Completely offline, privacy-first, and user-initiated.

---

## 6. Opt-In User Correction Logger

### Privacy Architecture
- **Opt-In Only**: Default is disabled (`enabled = false`).
- **Zero-Logging in Secure Mode**: When `set_secure_mode(true)` is activated by password/PIN fields, the logger drops events instantly.
- **Size Capped**: Max 1,000 entries; oldest entries purged automatically.
- **Full Transparency**: GUI tab allows viewing every entry and provides a 1-click **"Clear Correction Log"** button.
