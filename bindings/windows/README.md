# Amar Bangla Kotha (আমার বাংলা কথা) - Windows TSF Frontend

This directory contains the Windows Text Services Framework (TSF) Text Input Processor (TIP) implementation for **Amar Bangla Kotha**. It allows system-wide phonetic typing across all modern Windows applications (Microsoft Word, Notepad, Edge, Chrome, Visual Studio, etc.) without third-party hooking tools.

---

## Architecture Overview

```
  Windows Application (e.g., Word, Chrome)
                │
                ▼ (ITfTextInputProcessor / ITfKeyEventSink)
       [abk_tsf.dll (TSF TIP)]
                │
                ▼ (abk_get_suggestions / abk_pop_logical_unit)
       [libabk Core Engine (C++17)]
                ├── Lattice Beam-Search Transliterator
                ├── Compact Trie Dictionary & LM
                └── Bounded Autocorrect
```

- **`CTsfEngine`**:
  - Implements `ITfTextInputProcessor`, `ITfKeyEventSink`, `ITfCompositionSink`, and `ITfEditSession`.
  - Captures keystrokes, triggers synchronous/asynchronous edit sessions, creates composition ranges (`ITfComposition`), displays underline preedit text, and commits the finalized Bengali string.
- **`dllmain.cpp`**:
  - Exposes COM DLL entry points (`DllGetClassObject`, `DllCanUnloadNow`, `DllRegisterServer`, `DllUnregisterServer`).
  - Registers the COM class GUID: `{3F5C3B4E-5D8A-4F9A-8417-6B1A4B0C7291}`.
  - Registers the TSF Profile GUID: `{7A1E3B8F-9C2D-4E5F-B0A1-3C5E7A9B1D3F}` for both Bengali (India) `0x0445` and Bengali (Bangladesh) `0x0845`.

---

## Build Requirements

- Windows 10 / 11 (64-bit)
- Visual Studio 2019 or Visual Studio 2022 with C++ Desktop Development workload
- CMake 3.15+

---

## Compilation

Open a **Developer Command Prompt for VS 2022** and execute:

```cmd
cd bindings\windows
build_windows.bat
```

Or manually using CMake:

```cmd
mkdir build
cd build
cmake .. -A x64
cmake --build . --config Release
```

The resulting library will be `Release\abk_tsf.dll`.

---

## Installation & Registration

1. Copy the dictionary data:
   Create the directory `%APPDATA%\AmarBanglaKotha` and copy `data\bangla_wordlist.txt` into it:
   ```cmd
   mkdir "%APPDATA%\AmarBanglaKotha"
   copy "..\..\data\bangla_wordlist.txt" "%APPDATA%\AmarBanglaKotha\"
   copy "..\..\data\phonetic_rules.tsv" "%APPDATA%\AmarBanglaKotha\"
   ```

2. Register the DLL using an Administrator command prompt:
   ```cmd
   regsvr32 Release\abk_tsf.dll
   ```
   You will see a dialog stating: `DllRegisterServer in Release\abk_tsf.dll succeeded.`

3. Enable the keyboard:
   - Go to **Windows Settings** → **Time & Language** → **Language & Region**.
   - Under **Preferred Languages**, add **Bangla (Bangladesh)** or **Bangla (India)**.
   - Under Keyboard options, add **Amar Bangla Kotha (আমার বাংলা কথা)**.
   - Switch layouts using `Win + Space` or `Alt + Shift`.

---

## Uninstallation

To unregister the service:
```cmd
regsvr32 /u Release\abk_tsf.dll
```
