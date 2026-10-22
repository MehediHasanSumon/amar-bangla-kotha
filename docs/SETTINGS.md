# Settings GUI, First-Run Tutorial & Diagnostic Guide

This document details the configuration system, bilingual graphical interface ([`tools/abk_settings.py`](../tools/abk_settings.py)), interactive onboarding tutorial, and self-test diagnostics.

---

## 1. Overview & Architecture

Amar Bangla Kotha stores user settings in a platform-independent JSON file:
- **Linux/macOS**: `~/.config/amar-bangla-kotha/config.json`
- **Windows**: `%APPDATA%\AmarBanglaKotha\config.json`

The Settings GUI is lightweight, cross-platform (built using standard library Python Tkinter with zero third-party dependencies), and fully bilingual in **Bangla (বাংলা)** and **English**.

### Launching the Settings GUI
```bash
# On Linux
abk-settings
# Or directly from source:
python3 tools/abk_settings.py

# Launch directly into First-Run Tutorial:
python3 tools/abk_settings.py --tutorial
```

---

## 2. Configuration Options & Schema

Example configuration file (`config.json`):
```json
{
  "language_ui": "bn",
  "toggle_hotkey": "F12",
  "number_mode": "bengali",
  "candidate_count": 5,
  "theme": "light",
  "bypass_terminal": true,
  "autocorrect_enabled": true,
  "learning_enabled": true
}
```

### Settings Breakdown
1. **Language (ভাষা)**:
   - Toggle between **English** and **বাংলা**. Instantly translates all labels, tooltips, and buttons.
2. **Toggle Hotkey (ভাষা পরিবর্তনের শর্টকাট)**:
   - Configurable trigger key (default: `F12`, with support for `Ctrl+Space`, `Shift+Space`).
3. **Number Mode (সংখ্যার মোড)**:
   - `English (0-9)`: Keystrokes produce standard ASCII digits.
   - `Bengali (০-৯)`: Keystrokes automatically produce Bengali numerals.
4. **Candidate Count (পরামর্শের সংখ্যা)**:
   - Slider from 3 to 9 candidates displayed in the candidate panel.
5. **Theme (থিম)**:
   - `Light (উজ্জ্বল)`: Standard high-contrast system theme.
   - `Dark (ডার্ক)`: Modern low-strain dark mode.
6. **Per-App Profile (অ্যাপ প্রোফাইল)**:
   - Bypass Bangla transliteration automatically in terminal emulators, bash, and IDE code buffers.

---

## 3. Personal Dictionary & Learning Management

Located under the **Dictionary (অভিধান)** tab:
- **Personal Words & Frequency**: View custom words learned by the engine during typing.
- **Custom Shortcuts (সংক্ষেপণ)**: Add or edit quick expansions (e.g. `amr` $\rightarrow$ `আমার`, `tk` $\rightarrow$ `৳`).
- **Export / Import**: Save or load personal wordlists to/from `.tsv` or `.txt` format for backup and device synchronization.
- **Reset Learning (রিসেট)**: Clears all learned proper nouns, dynamic bigrams, and autocorrect history after user confirmation, restoring factory defaults without deleting static corpora.

---

## 4. Interactive First-Run Tutorial

The First-Run Tutorial guides new users through the phonetic transliteration rules and override conventions in an interactive sandbox.

| Lesson | Focus | English Input | Bengali Output | Key Concept |
|---|---|---|---|---|
| **Lesson 1** | Basic Phonetics | `ami banglay gan gai` | আমি বাংলায় গান গাই | Natural phonetic spelling |
| **Lesson 2** | Retroflex Overrides | `Ta` / `Da` / `Na` | ট / ড / ণ | Uppercase indicates retroflex consonants |
| **Lesson 3** | Diphthongs | `koi` vs `kOI` | কই vs কৈ | Uppercase `OI` produces `ঐ` / `ৈ` |
| **Lesson 4** | Hasanta & Raw Pass-through | `k,,` / `Esc` | ক্ / raw input | Escape cancels preedit without committing |

Users can type directly into the tutorial sandbox with live feedback and candidate display powered by `libabk.so`.

---

## 5. Diagnostic Self-Test (`abk_cli --selftest`)

To ensure engine integrity and facilitate bug reports, the CLI includes an automated self-test suite:
```bash
./bin/abk_cli --selftest
```

### Self-Test Capabilities
1. Evaluates the full golden test dataset ([`tests/data/golden.tsv`](../tests/data/golden.tsv)).
2. Measures Top-1 and Top-3 transliteration accuracy across all linguistic categories (Basic, Retroflex, Conjunct, Diphthong, Punctuation, Ambiguous).
3. Performs real-time latency profiling (p50, p95, p99 keystroke latency).
4. Monitors memory usage (Resident Set Size).
5. Outputs a Markdown report formatted for GitHub issue submissions.

### Submitting a Bug Report
When encountering an issue:
1. Run `./bin/abk_cli --selftest` and copy the output.
2. Open a new issue using [`.github/ISSUE_TEMPLATE/bug_report.md`](../.github/ISSUE_TEMPLATE/bug_report.md).
3. Paste the self-test report and specify the operating system and target application.
