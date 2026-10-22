# Amar Bangla Kotha (আমার বাংলা কথা) - Testing & Verification Guide

This document details the automated verification test harness, regression benchmarks, platform-specific manual verification checklists, and diagnostic event logs for **Workstream 1: Suggestion Panel Navigation & Focus Fix**.

---

## 1. Automated Verification Harness

The engine test suite validates all phonetic transformations, Trie dictionary operations, autocorrect phonetic distance formulas, candidate ranking, and the central `SuggestionState` state machine.

### Running Automated Tests
```bash
# Unit and integration test suite
make test

# Full build verification (smoke, benchmark, fuzz)
make smoke-test
make benchmark
make fuzz-test
```

### Test Coverage Highlights for Workstream 1
- **`select_next_candidate()` & `select_prev_candidate()`**: Circular wrap-around at 0 and $N-1$ boundaries.
- **Direct Selection & Clamping (`select_candidate(idx)`)**: Clamps out-of-range indexes safely to $[0, N-1]$.
- **Selection Stability**: Retains the previously selected word when candidate lists expand or shrink across keystrokes.
- **C ABI Binding**: Verifies `abk_suggestion_select_next`, `abk_suggestion_select_prev`, `abk_suggestion_select_index`, `abk_suggestion_get_selected_index`, `abk_suggestion_get_selected_word`, `abk_suggestion_is_visible`, and `abk_suggestion_reset`.

---

## 2. Platform Manual Verification Checklists

### A. Linux (IBus Engine)
- **Path**: `bindings/linux/abk_ibus_engine.py`
- **Setup**: Run `./scripts/install.sh --user` and restart IBus (`ibus restart`). Select "Bengali (Amar Bangla Kotha)".
- **Checklist**:
  1. Open a text editor (e.g., `gedit`, terminal, or browser).
  2. Type `koi` — verify preedit shows `কই` with candidate popup.
  3. Press **Down Arrow** ($\downarrow$): Verify highlight moves from 1st to 2nd candidate and text cursor inside editor does **not** move.
  4. Press **Up Arrow** ($\uparrow$): Verify highlight moves back up and wraps around when moving past top item.
  5. Press **Tab** / **Shift+Tab**: Verify candidate selection moves forward and backward.
  6. **Mouse Click**: Click candidate #2 (e.g., `কৈ`) with the mouse. Verify `কৈ` is immediately inserted with a trailing space and preedit finishes cleanly.
  7. Press **Space**: Verify selected candidate is committed with space.
  8. Press **Enter**: Verify selected candidate is committed without trailing space.
  9. Press **Escape**: Verify composition is cancelled and candidate popup closes immediately.

### B. Linux (Fcitx5 Addon)
- **Path**: `bindings/linux/fcitx5/`
- **Setup**: Build addon with `cmake -B build -S bindings/linux/fcitx5 && cmake --build build && cmake --install build`. Restart Fcitx5 (`fcitx5 -r -d`).
- **Checklist**:
  1. Open a Qt or GTK application. Switch to Amar Bangla Kotha.
  2. Type `k` — verify candidate list displays matching words.
  3. Press **Down Arrow** / **Up Arrow** / **Tab**: Verify `CommonCandidateList` updates its cursor without moving the application caret.
  4. **Mouse Click**: Click any candidate in the Fcitx5 candidate popup. Verify `AbkCandidateWord::select()` fires and commits the candidate word.

### C. macOS (InputMethodKit)
- **Path**: `bindings/macos/`
- **Target**: `AmarBanglaKotha.app` in `/Library/Input Methods/` or `~/Library/Input Methods/`.
- **Checklist**:
  1. Open Safari, TextEdit, or Notes.
  2. Type `ami` — verify candidate panel appears below marked text.
  3. Check panel style: Verify panel has `NSWindowStyleMaskNonactivatingPanel` and does **not** steal focus from TextEdit/Safari.
  4. Press **Down Arrow** (keyCode 125) and **Up Arrow** (keyCode 126): Verify candidate highlight moves and `handleEvent:` returns `YES` (consumed).
  5. Press **Tab** (keyCode 48): Verify forward candidate navigation.
  6. **Mouse Click**: Click candidate item in `IMKCandidates`. Verify `candidateSelected:` receives the string and inserts into client.
  7. Press **Return** or **Space**: Verify currently highlighted candidate commits.

### D. Windows (TSF - Text Services Framework)
- **Path**: `bindings/windows/`
- **Target**: `abk_tsf.dll` registered via `regsvr32 abk_tsf.dll`.
- **Checklist**:
  1. Open Notepad or Microsoft Word. Switch IME to Amar Bangla Kotha.
  2. Type `kotha` — verify composition underline and floating candidate list window.
  3. Window properties: Verify candidate window is created with `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`.
  4. Clicking candidate panel: Verify `WM_MOUSEACTIVATE` returns `MA_NOACTIVATE` so Notepad never loses focus; candidate under mouse cursor commits cleanly.
  5. Press **VK_DOWN** / **VK_UP** / **VK_TAB**: Verify both `OnTestKeyDown` and `OnKeyDown` return `*pfEaten = TRUE` and move candidate index without moving Notepad caret.

---

## 3. Diagnostic Event Logs: Before vs After

Diagnostic event logging can be enabled via `abk_set_debug_logging(engine, 1)` or running `abk_cli --debug`.

### Scenario: User types "k", presses Down Arrow, then presses Space

#### BEFORE (Symptom: Panel unresponsive, host caret moved, selection reset to 0)
```text
[HOST_APP] KeyDown DownArrow received
[ENGINE]   OnKeyDown(VK_DOWN): pfEaten=FALSE (unhandled)
[HOST_APP] Action: Move document cursor down 1 line (Preedit broken or cancelled)
[ENGINE]   focusOut / deactivate received: Clear candidates!
[USER]     Clicks mouse on candidate panel -> Panel stole focus, host cancelled composition!
```

#### AFTER (Fixed: Single Source of Truth `SuggestionState`, keys consumed, non-activating window)
```text
[ABK_DEBUG] [QUERY] input="k" primary="ক" candidates=10
[ABK_DEBUG] [EVENT] [KEY_DOWN] consumed=1 idx=1 note="navigate next candidate"
[ABK_DEBUG] [EVENT] [KEY_DOWN] consumed=1 idx=2 note="navigate next candidate"
[ABK_DEBUG] [EVENT] [KEY_UP]   consumed=1 idx=1 note="navigate prev candidate"
[ABK_DEBUG] [EVENT] [KEY_SPACE] consumed=1 idx=1 note="কথা"
[ABK_DEBUG] [COMMIT] committed="কথা " preedit_cleared=true
```

### Scenario: User clicks Candidate with Mouse

#### AFTER (Fixed: Mouse Click Handling)
```text
[ABK_DEBUG] [QUERY] input="koi" primary="কই" candidates=5
[ABK_DEBUG] [EVENT] [CLICK] consumed=1 idx=1 note="কৈ"
[ABK_DEBUG] [COMMIT] committed="কৈ " preedit_cleared=true
```
