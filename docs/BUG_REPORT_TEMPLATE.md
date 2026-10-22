# Bug Report Guidelines & Self-Test Diagnostic

When reporting bugs or transliteration inconsistencies in **Amar Bangla Kotha**, providing system diagnostics ensures rapid reproduction and resolution.

---

## 1. How to Generate Self-Test Diagnostics

Run the engine diagnostic self-test command from your terminal:

```bash
# Linux / macOS:
abk_cli --selftest

# Windows:
abk_cli.exe --selftest
```

The output will automatically format as a GitHub-compatible Markdown block detailing:
- Core engine build version & compiler
- Operating system kernel & machine architecture
- Startup & load latency against the 300 ms budget
- Resident memory (RSS) against the 100 MB budget
- P99 keystroke latency against the 5 ms budget
- Golden benchmark Top-1 and Top-3 accuracy
- Subsystem health checks (Token bypass, Backspace logical unit, Bigram prediction, Shortcut expansion)

---

## 2. Issue Submission Template

Copy and paste the template below into your GitHub issue:

```markdown
### 1. Description of the Problem
<!-- Brief summary of what happened vs what was expected. -->

### 2. Steps to Reproduce
1. In application: [e.g. Chrome, Word, VS Code, Slack, Terminal]
2. Type Latin phonetic text: [e.g. "kOI"]
3. Action: [e.g. Pressed Space / Tab / Backspace]
4. Observed result: [e.g. "কই" instead of "কৈ"]

### 3. Application & System Environment
- **Operating System**: [Windows 11 / macOS Sonoma / Ubuntu 24.04]
- **Frontend**: [TSF / InputMethodKit / IBus / Fcitx5]
- **Host Application**: [e.g. Chrome, LibreOffice, gedit]
- **Engine Version**: 1.0.0

### 4. Engine Self-Test Output
<!-- Paste output of "abk_cli --selftest" here -->
```
