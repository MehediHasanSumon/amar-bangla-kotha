# Linux Wayland Compatibility and Architecture

## 1. Wayland Input Architecture Overview

Unlike legacy X11, Wayland compositors prohibit individual client applications from querying global key states, positioning arbitrary top-level overlay windows, or grabbing keyboard focus outside their surface hierarchy.

For an Input Method Engine (IME) like **Amar Bangla Kotha**, input management is delegated through Wayland-native protocols:
- **`zwp_text_input_v3`**: Protocol spoken between client applications (GTK4, Qt6, Chromium, Firefox) and the Wayland compositor to coordinate cursor rectangles, preedit text, surrounding text, and committed strings.
- **`zwp_input_method_v2`**: Protocol spoken between the compositor and input method servers (IBus, Fcitx5) to receive raw key events, manage lookup tables, and inject text.
- **IBus / Fcitx5 Daemon Layer**: `libabk` binds to Linux via [`bindings/linux/abk_ibus_engine.py`](file:///home/sumon/Desktop/Keyboard/bindings/linux/abk_ibus_engine.py) (IBus component) or Fcitx5. The daemon bridges directly into the compositor's Wayland socket.

```
+----------------------------------------------------------------+
|                       Wayland Compositor                       |
|         (GNOME Mutter / KDE KWin / wlroots / Hyprland)         |
|  - Manages Wayland surfaces & renders Candidate Popup Window   |
+-------------------+------------------------+-------------------+
                    |                        |
         zwp_text_input_v3        zwp_input_method_v2 / D-Bus
                    |                        |
                    v                        v
        +-----------------------+  +--------------------------+
        |   Client Application  |  |    IBus / Fcitx5 Daemon  |
        | (Firefox, gedit, etc.)|  | (abk_ibus_engine.py)     |
        +-----------------------+  +-------------+------------+
                                                 |
                                                 v
                                       +-------------------+
                                       |  C++ Core libabk  |
                                       +-------------------+
```

---

## 2. Compatibility Matrix by Compositor

| Compositor / Desktop | Protocol | Preedit Display | Candidate Popup Window | Password Auto-Bypass | Overall Status | Notes & Workarounds |
|---|---|---|---|---|---|---|
| **GNOME 40+ (Mutter)** | Native IBus bridge + `zwp_text_input_v3` | **Working** (inline underlined) | **Working** (rendered by GNOME Shell at cursor) | **Verified** | **100% Native** | Default desktop in Ubuntu & Fedora. Seamless out of the box. |
| **KDE Plasma 5.24+ / 6 (KWin)** | `zwp_input_method_v2` via IBus / Fcitx5 | **Working** | **Working** (KWin cursor tracking) | **Verified** | **100% Native** | Requires selecting "IBus" in *System Settings $\rightarrow$ Virtual Keyboard*. |
| **Sway / wlroots** | `input-method-v2` / `layer-shell` | **Working** | **Working** | **Verified** | **Working** | Candidate popup rendered via `wlr-layer-shell`. Requires `IBUS_ENABLE_SYNC_MODE=1`. |
| **Hyprland** | `text-input-v3` | **Working** | **Working** | **Verified** | **Working** | Requires `QT_IM_MODULE=ibus` / `GTK_IM_MODULE=ibus` and `XMODIFIERS=@im=ibus`. |
| **XWayland (Legacy Apps)** | XIM / GTK_IM_MODULE | **Working** | **Working** | **Verified** | **Working** | X11 fallback bridges automatically through `ibus-x11`. |

---

## 3. Detailed Compositor Analyses & Quirks

### A. GNOME Shell (Mutter)
- **Status**: First-class citizen.
- **Mechanism**: GNOME Shell has direct internal integration with `ibus-daemon`. Mutter receives text input requests from clients, dispatches keystrokes to `abk_ibus_engine.py`, and renders the candidate window directly as a Clutter actor pinned to the text cursor.
- **Result**: Zero flickering, perfect high-DPI scaling, and zero Wayland permission errors.

### B. KDE Plasma (KWin 5 & 6)
- **Status**: Fully supported.
- **Configuration**:
  ```bash
  # In ~/.xprofile or environment:
  export GTK_IM_MODULE=ibus
  export QT_IM_MODULE=ibus
  export XMODIFIERS=@im=ibus
  ```
- **KWin Quirks**: In KWin 5, the virtual keyboard setting must be explicitly enabled in System Settings; otherwise KWin does not start the input-method protocol pipe.

### C. wlroots / Sway
- **Status**: Supported with `layer-shell`.
- **Candidate Window**: Unlike GNOME, wlroots relies on the input method UI (`ibus-ui-gtk3` or custom layer surface) to draw the window. It positions correctly relative to cursor coordinates emitted by `zwp_text_input_v3`.

### D. Hyprland
- **Status**: Supported in Hyprland v0.30+.
- **Configuration**:
  ```ini
  # In ~/.config/hypr/hyprland.conf:
  exec-once = ibus-daemon -drxR
  env = GTK_IM_MODULE,ibus
  env = QT_IM_MODULE,ibus
  env = XMODIFIERS,@im=ibus
  ```

---

## 4. Electron & Browser Wayland Flag Matrix

Certain Electron and Chromium apps running on Wayland disable Wayland IME protocols by default. The following flags ensure native Wayland typing:

| Application | Recommended Execution Flags | Issue Without Flags |
|---|---|---|
| **Google Chrome / Chromium** | `--enable-features=UseOzonePlatform --ozone-platform=wayland --enable-wayland-ime` | Falls back to XWayland; candidate window may offset. |
| **VS Code** | `code --enable-features=UseOzonePlatform --ozone-platform=wayland --enable-wayland-ime` | Works under XWayland, but native Wayland improves latency. |
| **Discord / Slack** | `--enable-features=UseOzonePlatform --ozone-platform=wayland` | Fixes blurred text and restores cursor tracking. |
| **Firefox Wayland** | Default native (`MOZ_ENABLE_WAYLAND=1`) | Works out of the box on Ubuntu/Fedora. |

---

## 5. Fallback Mode: Clipboard Paste

In environments where Wayland completely restricts text input (such as full-screen games, Wine, or sandboxed Flatpaks without IPC portals):
- `libabk` provides **Clipboard-Paste Fallback Mode** (`abk_engine_set_clipboard_mode(handle, 1)`).
- When enabled, candidates are copied to the Wayland system clipboard (`wl-copy`) and injected via `Ctrl+V` key simulation.
