# Amar Bangla Kotha (আমার বাংলা কথা) - macOS InputMethodKit Frontend

This directory contains the native macOS Input Method Kit (IMKit) implementation of **Amar Bangla Kotha**, allowing system-wide Bengali phonetic typing across all macOS applications (Safari, Pages, Keynote, Terminal, Slack, Chrome, VS Code, etc.).

---

## Architecture Overview

```
 macOS Application (e.g. Safari, Pages)
            │
            ▼ (NSEvent / Cocoa Text System)
  [IMKServer / AmarBanglaKothaController]
            │
            ▼ (C ABI - abk_get_suggestions, abk_pop_logical_unit)
      [libabk Core Engine (C++17)]
            ├── Lattice Beam-Search Transliterator
            ├── Compact Trie Dictionary & LM
            └── Bounded Autocorrect
```

- **`AmarBanglaKothaController`**: Inherits from `IMKInputController`. It intercepts keypress events, feeds Latin keystrokes into `libabk`, manages marked preedit composition with underline styles, displays a native floating candidate panel, and commits chosen Bengali words directly into the text view.
- **`main.m`**: Boots `IMKServer` with server connection name `AmarBanglaKotha_1_Connection`.
- **`Info.plist`**: Configures the bundle as a background Input Method (`LSUIElement = true`) associated with the Bengali script (`smBengali`).

---

## Prerequisites for Building

- macOS 10.13+ (High Sierra, Mojave, Catalina, Big Sur, Monterey, Ventura, Sonoma, Sequoia)
- Xcode Command Line Tools (`xcode-select --install`)
- Clang with C++17 and Objective-C++ support

---

## Building the Bundle

From the repository root or this directory, run:

```bash
cd bindings/macos
./build_macos.sh
```

This will:
1. Compile the static library `libabk.a`.
2. Generate the bundle `build/AmarBanglaKotha.app`.
3. Copy `Info.plist` and dictionary files (`bangla_wordlist.txt`, `phonetic_rules.tsv`) into `AmarBanglaKotha.app/Contents/Resources`.
4. Compile `AmarBanglaKotha` executable linking against Cocoa and InputMethodKit frameworks.
5. Ad-hoc codesign the bundle.

---

## Installation & Deployment

1. Copy the generated `.app` bundle into your user Input Methods directory:
   ```bash
   cp -R build/AmarBanglaKotha.app ~/Library/Input\ Methods/
   ```

2. Register the input method:
   - Log out and log back in (or restart macOS).
   - Alternatively, kill the IMKit cache:
     ```bash
     killall -9 AmarBanglaKotha 2>/dev/null || true
     ```

3. Enable Amar Bangla Kotha:
   - Open **System Settings** (or System Preferences) → **Keyboard** → **Text Input** → **Input Sources** (Edit).
   - Click **+**, search for **Bengali (Bangla)**.
   - Select **Amar Bangla Kotha** and click **Add**.

---

## Typing Guide

- Standard Phonetic Banglish:
  - `amar` → আমার
  - `bangla` → বাংলা
  - `koi` → কই
  - `kOI` → কৈ
  - `bari` → বাড়ি
  - `r``yab` → র‍্যাব
- Commit Keys:
  - **Space**: Commits top candidate and appends a space.
  - **Enter / Return**: Commits top candidate without trailing space.
  - **1 - 9**: Selects corresponding candidate from the popup window.
  - **Backspace**: Deletes previous keystroke and updates live preedit.
  - **Escape**: Cancels current composition.
