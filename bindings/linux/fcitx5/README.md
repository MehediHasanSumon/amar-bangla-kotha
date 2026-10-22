# Amar Bangla Kotha (আমার বাংলা কথা) - Fcitx5 Frontend

This directory contains the native C++ Fcitx5 plugin module for **Amar Bangla Kotha**. It allows seamless phonetic Bengali typing on Linux desktops running Fcitx5 under Wayland or X11 (GNOME, KDE Plasma, Sway, Hyprland, etc.).

---

## Architecture Overview

```
 Linux Application (Wayland / X11 via Wayland text-input-v3 or XIM)
                          │
                          ▼
            [Fcitx5 Input Method Framework]
                          │
                          ▼ (InputMethodEngineV2)
       [amar-bangla-kotha.so (Fcitx5 Addon)]
                          │
                          ▼ (C ABI - abk_get_suggestions)
             [libabk Core Engine (C++17)]
```

- **`AmarBanglaKothaEngine`**: Subclasses `fcitx::InputMethodEngineV2` and `fcitx::AddonInstance`. Intercepts key events, performs live preedit updates, displays candidate panels, and emits committed UTF-8 text strings directly to the focused input context.

---

## Build Requirements

- Linux with GCC 8+ or Clang 8+ (C++17)
- `cmake` (3.16+)
- `extra-cmake-modules`
- `fcitx5` development headers (`fcitx5-devel` or `libfcitx5core-dev`)

---

## Compilation & Installation

```bash
cd bindings/linux/fcitx5
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/.local
make
make install
```

After installation:
1. Restart Fcitx5:
   ```bash
   fcitx5 -r -d
   ```
2. Open Fcitx5 Configuration tool (`fcitx5-config-qt`), add **Amar Bangla Kotha** under Bengali input methods.
