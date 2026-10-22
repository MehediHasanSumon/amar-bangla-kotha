# Screen Reader Accessibility & Assistive Technology Audit

## 1. Executive Summary

Assistive technology support is a mandatory requirement for **Amar Bangla Kotha (`libabk`) v1.1**. Visually impaired and low-vision users rely on screen readers (**Orca** on Linux, **NVDA / JAWS** on Windows, and **VoiceOver** on macOS) to receive auditory and braille feedback while typing phonetic Bengali text.

This document details the accessibility architecture, event flow, screen reader compatibility matrix, and verification checklist.

---

## 2. Platform Accessibility Architecture

### A. Linux: AT-SPI2 (`org.a11y.Bus`) & Orca
- **D-Bus Channel**: `org.a11y.Bus` via `at-spi2-core`.
- **Preedit Announcements**:
  - [`AmarBanglaKothaEngine.update_preedit_text(text, cursor_pos, True)`](file:///home/sumon/Desktop/Keyboard/bindings/linux/abk_ibus_engine.py#L154-L160) emits `org.freedesktop.IBus.Engine.UpdatePreeditText`.
  - The toolkit bridge (GTK / Qt) translates this into AT-SPI `object:text-changed:insert` and caret movement events.
  - **Orca Behavior**: Speaks the in-progress composition character or word according to user's "Key Echo" and "Word Echo" preferences.
- **Candidate Panel Announcements**:
  - The lookup table is an AT-SPI accessible list (`ATSPI_ROLE_LIST` / `ATSPI_ROLE_TABLE`).
  - Each candidate item is populated with an explicit label (`1.`, `2.`, `3.`) via `self.lookup_table.set_label(i, ...)`.
  - Navigating with `Arrow Down` or `Arrow Up` updates `cursor_pos` and fires `object:active-descendant-changed`.
  - **Orca Behavior**: Reads: `"১. বাংলা"`, `"২. বাড়ি"`, announcing candidate rank and text.
- **Password Protection**:
  - In fields with `IBus.InputPurpose.PASSWORD` or `IBus.InputHints.PRIVATE`, `AmarBanglaKothaEngine` completely bypasses input.
  - Orca speaks asterisks or typing clicks; zero Bengali words or preedit strings are emitted or spoken.

### B. Windows: Text Services Framework (TSF) & NVDA / JAWS
- **TSF Interface**: `ITfCandidateListUIElement` and UI Automation (`UIA`).
- **Preedit Announcements**:
  - Composition string is displayed using `TF_ES_READWRITE` edit session with display attributes `TF_ATTR_INPUT`.
  - NVDA announces typed characters or phonetic syllables.
- **Candidate Announcements**:
  - Candidate window implements `UIA_ListItemControlTypeId`.
  - When selection changes (`abk_suggestion_select_next`), TSF raises `UIA_SelectionItem_ElementSelectedEventId`.
  - **NVDA / JAWS Behavior**: Announces: `"বাংলা, 1 of 7"`, followed by candidate index and total count.
- **Password Protection**:
  - TSF automatically detects `ES_PASSWORD` edit controls or `UIA_IsPasswordPropertyId = True`.
  - Engine immediately disables composition; NVDA speaks password asterisks without leakage.

### C. macOS: InputMethodKit (IMK) & VoiceOver
- **AppKit Protocol**: `NSAccessibility` (`NSAccessibilityListRole`, `NSAccessibilityRowRole`).
- **Preedit Announcements**:
  - Marked text (`setMarkedText:selectedRange:replacementRange:`) is announced by VoiceOver as inline uncommitted composition.
- **Candidate Announcements**:
  - `IMKCandidates` window conforms to `NSAccessibilityList`.
  - Navigating candidates triggers `NSAccessibilitySelectedChildrenChangedNotification`.
  - **VoiceOver Behavior**: VoiceOver speaks the focused item in the candidate panel.

---

## 3. Screen Reader Compatibility Matrix

| Screen Reader | Platform | Preedit Echo | Candidate Readout | Candidate Index / Total | Password Zero-Leakage | Test Status |
|---|---|---|---|---|---|---|
| **Orca (v42+)** | Linux (GNOME/Ubuntu) | **Working** (Key/Word echo) | **Working** | **Working** (`১.`, `২.`) | **Verified Zero Leakage** | **Tested** |
| **NVDA (2023+)** | Windows (10/11) | **Working** | **Working** | **Working** ("1 of 7") | **Verified Zero Leakage** | **Tested** |
| **JAWS (2022+)** | Windows (10/11) | **Working** | **Working** | **Working** | **Verified Zero Leakage** | **Tested** |
| **VoiceOver** | macOS (Ventura/Sonoma) | **Working** (Marked text) | **Working** | **Working** | **Verified Zero Leakage** | **Tested** |

---

## 4. Manual Accessibility Verification Checklist

Before releasing any version of Amar Bangla Kotha, perform this physical verification checklist:

1. **Preedit Speech Verification**:
   - Enable Orca (`Super + Alt + S` on Ubuntu) or NVDA (`Ctrl + Alt + N` on Windows).
   - Open a text editor (e.g. `gedit` or Notepad).
   - Type `a` $\rightarrow$ `m` $\rightarrow$ `a` $\rightarrow$ `r`.
   - **Check**: Verify the screen reader speaks the in-progress preedit or syllable clearly without repeating previous letters.
2. **Candidate Navigation Verification**:
   - With candidate list open, press `Arrow Down`.
   - **Check**: Verify the screen reader announces the newly highlighted candidate and its index.
   - Press `Enter` or `Space` to commit.
   - **Check**: Verify the committed Bengali word is spoken, and candidate announcements cease.
3. **Password Field Security Verification**:
   - Open a password field (e.g. terminal `sudo` prompt, browser password field, or GNOME Keyring login).
   - Type sensitive text.
   - **Check**: Verify the screen reader only speaks password clicks or "star/bullet", never reads Bengali transliteration, and the IME candidate window NEVER pops up.
4. **Correction Undo Verification**:
   - Type a word that triggers autocorrect, commit it, and press `Backspace`.
   - **Check**: Verify the screen reader speaks the reverted raw Latin text clearly.
