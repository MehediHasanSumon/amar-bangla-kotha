#!/usr/bin/env python3
"""
Amar Bangla Kotha (আমার বাংলা কথা) - Standalone Production Floating Toolbar
- Floating overlay (Always on Top) that never sinks below other applications
- Live synchronization with IBus and GNOME input sources (Super + Space switching)
- Automatic system-wide typing activation when 'Avro' is selected
- Restores default English ('xkb:us::eng') when closed or switched to English
- Streamlined Dropdown: strictly 'Avro' and 'English'
- Action cluster: [⌨] Layout Viewer, [⚙] Settings, [ⓘ] Info dropdown (Hide to System Tray, Quit)
- System Tray integration with AyatanaAppIndicator3 / Gtk.StatusIcon
"""

import sys
import os
import json
import subprocess
import ctypes
from ctypes import c_void_p, c_char_p, c_size_t, c_int, c_double, Structure, POINTER

# Cheat sheet text
ONE_PAGE_CHEATSHEET = (
    "• Vowels / স্বরবর্ণ (Case-Sensitive): o (অ/inherent), a (আ/া), i (ই/ি), I/ee (ঈ/ী), "
    "u/oo (উ/ু), U (ঊ/ূ), rri (ঋ/ৃ), e (এ/ে), OI (ঐ/ৈ), O (ও/ো), OU (ঔ/ৌ)\n"
    "  ↳ koi → কই | kOI → কৈ | kou → কউ | kOU → কৌ (both appear in suggestions)\n"
    "• Backtick (`) Separator / ব্যাকটিক: ek`i → একই (vs eki → একি), ek`Ta → একটা, am`ra → আমরা, "
    "a` → া, i` → ি, I` → ী, t`` → ৎ, .` → ., :` → :, $` → $\n"
    "• Consonants / ব্যঞ্জনবর্ণ: k (ক), kh (খ), g (গ), gh (ঘ), Ng (ঙ) | c (চ), ch (ছ), j (জ), jh (ঝ), NG (ঞ)\n"
    "  T (ট), Th (ঠ), D (ড), Dh (ঢ), N (ণ) | t (ত), th (থ), d (দ), dh (ধ), n (ন)\n"
    "  p (প), ph/f (ফ), b (ব), bh/v (ভ), m (ম) | z (য), r (র), l (ল), sh/S (শ), Sh (ষ), s (স), h (হ)\n"
    "  R (ড়), Rh (ঢ়), y/Y (য়), J (জ়), ng (ং), : (ঃ), ^ (ঁ), kkh/kx (ক্ষ), gg/jNG (জ্ঞ)\n"
    "  ↳ Word-start y → ইয় (yahoo → ইয়াহু); capital Y forces য়; post-vowel a → য়া (samia → সামিয়া)\n"
    "• Phalas, Reph & Hasanta / ফলা ও রেফ: w (ব-ফলা: bishwo → বিশ্ব), y/Z (য-ফলা: bybohar → ব্যবহার, "
    "aZDmin → অ্যাডমিন, rZ → র‍্য), r (র-ফলা: protidin → প্রতিদিন), m (ম-ফলা: podma → পদ্মা), "
    "rr before C (রেফ: orrko → অর্ক), ,, (হসন্ত: ্)\n"
    "• Smart Punctuation / বিরামচিহ্ন: . → । (kept as . in 3.5, URLs, emails, paths, Dr., e.g.), "
    "$ → ৳ ($500 → ৳৫০০; kept as $ in $PATH, ${HOME}, $var), 0-9 → ০-৯ (configurable)\n"
    "• Autocorrect & Preview / অটোকারেক্ট ও সাজেশন: amra → আমরা, software → সফটওয়্যার\n"
    "• Toggle Hotkey: Press F12 or Super+Space anytime to toggle Bangla ⇄ English."
)

if "--cheatsheet" in sys.argv:
    print("Amar Bangla Kotha (আমার বাংলা কথা) — One-Page Phonetic Cheat Sheet\n")
    print(ONE_PAGE_CHEATSHEET)
    sys.exit(0)

# Force X11 backend (XWayland) so that GNOME Mutter permanently honors
# _NET_WM_STATE_ABOVE and _NET_WM_STATE_SKIP_TASKBAR over all applications
os.environ["GDK_BACKEND"] = "x11"

# Import PyGObject / GTK 3 & IBus
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
gi.require_version('GdkPixbuf', '2.0')
from gi.repository import Gtk, Gdk, GLib, GdkPixbuf

# Set decoupled program name so GNOME Shell Dash never attaches a dock launcher
GLib.set_prgname("abk-floating-service")
GLib.set_application_name("Amar Bangla Kotha Service")

try:
    gi.require_version('IBus', '1.0')
    from gi.repository import IBus
    HAS_IBUS = True
except Exception:
    HAS_IBUS = False

try:
    gi.require_version('AyatanaAppIndicator3', '0.1')
    from gi.repository import AyatanaAppIndicator3
    HAS_INDICATOR = True
except Exception:
    HAS_INDICATOR = False

# 1. C Types mapping for libabk
class AbkCandidate(Structure):
    _fields_ = [
        ("word", c_char_p),
        ("score", c_double),
        ("candidate_type", c_int),
        ("explanation", c_char_p)
    ]

class AbkSuggestionList(Structure):
    _fields_ = [
        ("raw_input", c_char_p),
        ("primary_transliteration", c_char_p),
        ("candidate_count", c_size_t),
        ("candidates", POINTER(AbkCandidate))
    ]

def load_abk():
    """Multi-platform dynamic library loader supporting Linux, Windows, and macOS."""
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    search_dirs = [
        os.path.join(base_dir, "lib"),
        os.path.join(base_dir, "bin"),
        os.path.join(base_dir, "build"),
        base_dir,
        os.path.expanduser("~/.local/lib"),
        "/usr/local/lib",
        "/usr/lib",
        "/usr/lib/x86_64-linux-gnu",
        "/opt/homebrew/lib"
    ]

    if sys.platform == "win32":
        lib_names = ["abk.dll", "libabk.dll"]
        search_dirs.append(r"C:\Program Files\Amar Bangla Kotha\bin")
    elif sys.platform == "darwin":
        lib_names = ["libabk.dylib"]
    else:
        lib_names = ["libabk.so"]

    for d in search_dirs:
        for name in lib_names:
            p = os.path.join(d, name)
            if os.path.exists(p):
                try:
                    lib = ctypes.CDLL(p)
                    lib.abk_engine_create.restype = c_void_p
                    lib.abk_engine_create.argtypes = []
                    lib.abk_engine_destroy.restype = None
                    lib.abk_engine_destroy.argtypes = [c_void_p]
                    lib.abk_get_suggestions.restype = POINTER(AbkSuggestionList)
                    lib.abk_get_suggestions.argtypes = [c_void_p, c_char_p]
                    lib.abk_free_suggestions.restype = None
                    lib.abk_free_suggestions.argtypes = [POINTER(AbkSuggestionList)]
                    lib.abk_learn_word.restype = c_int
                    lib.abk_learn_word.argtypes = [c_void_p, c_char_p, ctypes.c_uint32]
                    lib.abk_load_dictionary.restype = c_int
                    lib.abk_load_dictionary.argtypes = [c_void_p, c_char_p]
                    lib.abk_get_version.restype = c_char_p
                    return lib
                except Exception as e:
                    print(f"Notice: Failed to bind {p}: {e}", file=sys.stderr)
    return None

ABK = load_abk()

def copy_to_clipboard(text: str, gtk_clipboard=None) -> bool:
    """Multi-platform clipboard copy supporting Wayland, X11, macOS, and Windows."""
    if not text:
        return False
    success = False

    if gtk_clipboard:
        try:
            gtk_clipboard.set_text(text, -1)
            gtk_clipboard.store()
            success = True
        except Exception:
            pass

    if sys.platform == 'darwin':
        try:
            p = subprocess.Popen(['pbcopy'], stdin=subprocess.PIPE)
            p.communicate(text.encode('utf-8'))
            success = True
        except Exception:
            pass
    elif sys.platform == 'win32':
        try:
            p = subprocess.Popen(['clip'], stdin=subprocess.PIPE, shell=True)
            p.communicate(text.encode('utf-16le'))
            success = True
        except Exception:
            pass
    else:  # Linux / Unix
        if os.environ.get('WAYLAND_DISPLAY'):
            try:
                p = subprocess.Popen(['wl-copy'], stdin=subprocess.PIPE)
                p.communicate(text.encode('utf-8'))
                success = True
            except Exception:
                pass
        if not success:
            for tool, args in [('xclip', ['-selection', 'clipboard']), ('xsel', ['--clipboard', '--input'])]:
                try:
                    p = subprocess.Popen([tool] + args, stdin=subprocess.PIPE)
                    p.communicate(text.encode('utf-8'))
                    success = True
                    break
                except Exception:
                    pass

    return success

# State persistence helper
STATE_FILE = os.path.expanduser("~/.config/amar-bangla-kotha/gui_state.json")

def load_gui_state():
    state = {
        "theme": "dark",
        "is_bangla_mode": True,
        "selected_mode": "Avro",
        "display_dropdown": "Avro",
        "x": -1,
        "y": 40
    }
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                saved = json.load(f)
                state.update(saved)
        except Exception:
            pass

    if state.get("is_bangla_mode", True):
        state["selected_mode"] = "Avro"
        state["display_dropdown"] = "Avro"
    else:
        state["selected_mode"] = "English"
        state["display_dropdown"] = "English"

    return state

def save_gui_state(state):
    try:
        os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
        with open(STATE_FILE, "w", encoding="utf-8") as f:
            json.dump(state, f, indent=2)
    except Exception:
        pass

# CSS Themes for Exact Match to the Mockup
CSS_THEMES = """
* {
    font-family: 'Hind Siliguri', 'Kalpurush', 'Noto Sans Bengali', 'SolaimanLipi', 'Ubuntu', 'Segoe UI', sans-serif;
    outline: none;
}

window.abk-window {
    background-color: transparent;
}

/* Main Toolbar Frame */
.abk-toolbar {
    border-radius: 13px;
    padding: 5px 8px;
    transition: all 180ms ease;
}

/* Light Theme (Default, matching mockup) */
.toolbar-light {
    background-color: #FFFFFF;
    border: 1px solid rgba(0, 0, 0, 0.09);
    box-shadow: 0 10px 24px rgba(0, 0, 0, 0.15), 0 2px 6px rgba(0, 0, 0, 0.06);
}

/* Dark Theme (matching mockup dark theme option) */
.toolbar-dark {
    background-color: #21262D;
    border: 1px solid rgba(255, 255, 255, 0.14);
    box-shadow: 0 10px 24px rgba(0, 0, 0, 0.45), 0 2px 6px rgba(0, 0, 0, 0.25);
}

/* Logo Badge Button */
.logo-btn {
    background: transparent;
    border: none;
    padding: 0;
    margin: 0;
    border-radius: 6px;
    box-shadow: none;
}
.logo-btn:hover {
    opacity: 0.92;
}

/* Dividers */
.v-divider {
    background-color: rgba(0, 0, 0, 0.12);
    min-width: 1px;
    margin: 4px 6px;
}
.toolbar-dark .v-divider {
    background-color: rgba(255, 255, 255, 0.16);
}

/* Dropdown Pill Button */
.mode-dropdown-btn {
    background: transparent;
    border: none;
    border-radius: 6px;
    padding: 4px 8px;
    box-shadow: none;
    transition: background-color 140ms ease;
}
.mode-dropdown-btn:hover {
    background-color: rgba(0, 0, 0, 0.05);
}
.toolbar-dark .mode-dropdown-btn:hover {
    background-color: rgba(255, 255, 255, 0.08);
}

.mode-dropdown-label {
    font-size: 13.5px;
    font-weight: 600;
    color: #0F172A;
}
.toolbar-dark .mode-dropdown-label {
    color: #F8FAFC;
}

.mode-dropdown-arrow {
    font-size: 11px;
    color: #64748B;
    margin-left: 4px;
}
.toolbar-dark .mode-dropdown-arrow {
    color: #94A3B8;
}

/* Action Buttons Cluster */
.action-cluster {
}

.action-btn {
    background: transparent;
    border: none;
    border-radius: 6px;
    padding: 4px 6px;
    min-width: 26px;
    min-height: 26px;
    font-size: 13.5px;
    color: #334155;
    box-shadow: none;
    transition: all 120ms ease;
}
.action-btn:hover {
    background-color: rgba(0, 0, 0, 0.06);
    color: #0F172A;
}
.toolbar-dark .action-btn {
    color: #E2E8F0;
}
.toolbar-dark .action-btn:hover {
    background-color: rgba(255, 255, 255, 0.10);
    color: #FFFFFF;
}

/* Active Button State (bright blue square) */
.action-btn-active {
    background-color: #2563EB;
    color: #FFFFFF;
    border-radius: 6px;
}

/* Floating Popup Menus (Matching mockup theme cards) */
menu, .menu-light {
    background-color: #FFFFFF;
    border: 1px solid rgba(0, 0, 0, 0.08);
    border-radius: 12px;
    padding: 6px;
    box-shadow: 0 12px 28px rgba(0, 0, 0, 0.18), 0 2px 8px rgba(0, 0, 0, 0.08);
}
.menu-dark {
    background-color: #21262D;
    border: 1px solid rgba(255, 255, 255, 0.14);
    border-radius: 12px;
    padding: 6px;
    box-shadow: 0 12px 28px rgba(0, 0, 0, 0.50), 0 2px 8px rgba(0, 0, 0, 0.25);
}

.menu-light menuitem {
    padding: 6px 14px;
    border-radius: 6px;
    font-size: 13.5px;
    font-weight: 500;
    color: #1E293B;
}
.menu-dark menuitem {
    padding: 6px 14px;
    border-radius: 6px;
    font-size: 13.5px;
    font-weight: 500;
    color: #F8FAFC;
}

.menu-light menuitem:hover {
    background-color: rgba(0, 0, 0, 0.06);
}
.menu-dark menuitem:hover {
    background-color: rgba(255, 255, 255, 0.08);
}

/* Selected item styling in menus */
.menu-light .menu-item-selected {
    background-color: #DBEAFE;
    color: #1E3A8A;
    font-weight: 600;
}
.menu-dark .menu-item-selected {
    background-color: #2563EB;
    color: #FFFFFF;
    font-weight: 600;
}

separator {
    background-color: rgba(0, 0, 0, 0.08);
    margin: 4px 6px;
    min-height: 1px;
}
.menu-dark separator {
    background-color: rgba(255, 255, 255, 0.12);
}
"""

class AvroKeyboardBar(Gtk.Window):
    def __init__(self):
        super().__init__(type=Gtk.WindowType.TOPLEVEL)
        self.set_title("Amar Bangla Kotha")

        # 1. Floating Overlay (Always on Top over all apps, never sinks)
        self.set_wmclass("abk-floating-service", "abk-floating-service")
        self.set_keep_above(True)
        self.stick()  # Visible on all workspaces / virtual desktops
        self.set_decorated(False)
        self.set_resizable(False)
        self.set_skip_taskbar_hint(True)
        self.set_skip_pager_hint(True)
        self.set_type_hint(Gdk.WindowTypeHint.UTILITY)
        self.set_accept_focus(False)  # Allows typing in other windows without stealing focus
        self.set_focus_on_map(False)

        # Transparency compositing
        screen = self.get_screen()
        visual = screen.get_rgba_visual()
        if visual and screen.is_composited():
            self.set_visual(visual)
        self.set_app_paintable(True)

        # Style context & CSS Provider
        self.get_style_context().add_class("abk-window")
        self.css_provider = Gtk.CssProvider()
        self.css_provider.load_from_data(CSS_THEMES.encode('utf-8'))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            self.css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        # Load persisted state
        self.gui_state = load_gui_state()
        self.current_theme = self.gui_state.get("theme", "dark")
        self.is_bangla_mode = self.gui_state.get("is_bangla_mode", True)
        self.selected_mode = "Avro" if self.is_bangla_mode else "English"
        self.display_text = self.selected_mode

        # Connect IBus
        self.ibus_bus = None
        if HAS_IBUS:
            try:
                self.ibus_bus = IBus.Bus()
            except Exception:
                self.ibus_bus = None

        # Build UI layout
        self.build_ui()
        self.apply_theme_class()
        self.update_ui_state()

        # Connect window dragging and state events
        self.add_events(
            Gdk.EventMask.BUTTON_PRESS_MASK
            | Gdk.EventMask.BUTTON_RELEASE_MASK
            | Gdk.EventMask.POINTER_MOTION_MASK
        )
        self.connect("button-press-event", self.on_window_button_press)
        self.connect("motion-notify-event", self.on_window_motion_notify)
        self.connect("button-release-event", self.on_window_button_release)
        self.connect("key-press-event", self.on_window_key_press)
        self.connect("window-state-event", self.on_window_state_event)
        self.connect("focus-out-event", self.on_focus_out_event)
        self.connect("map", self.on_window_map)
        self.is_dragging = False

        # Initialize System Tray Indicator
        self.indicator = None
        self.init_tray_indicator()

        # Sync initial engine to OS typing
        if self.is_bangla_mode:
            self.set_system_engine("amar-bangla-kotha")
        else:
            self.set_system_engine("xkb:us::eng")

        # Periodic timer (200ms) to detect Super+Space and maintain Always-on-Top
        GLib.timeout_add(200, self.sync_engine_from_system)

        # Restore window position
        saved_x = self.gui_state.get("x", -1)
        saved_y = self.gui_state.get("y", 40)
        if saved_x > 0:
            self.move(saved_x, saved_y)
        else:
            self.set_position(Gtk.WindowPosition.CENTER)

    def on_window_map(self, widget):
        self.set_keep_above(True)
        self.stick()

    def on_window_state_event(self, widget, event):
        if not (event.new_window_state & Gdk.WindowState.ABOVE):
            self.set_keep_above(True)
        self.stick()

    def on_focus_out_event(self, widget, event):
        self.set_keep_above(True)
        self.stick()
        return False

    def get_logo_path(self, is_bangla: bool) -> str:
        base_dir = os.path.dirname(os.path.abspath(__file__))
        fname = "logo_bangla.svg" if is_bangla else "logo_english.svg"
        search_dirs = [
            base_dir,
            os.path.join(base_dir, "gui"),
            os.path.join(base_dir, "..", "gui"),
            os.path.expanduser("~/.local/share/amar-bangla-kotha"),
            "/usr/local/share/amar-bangla-kotha",
        ]
        for d in search_dirs:
            p = os.path.join(d, fname)
            if os.path.exists(p):
                return p
        fallback = os.path.join(base_dir, "icon.svg")
        return fallback if os.path.exists(fallback) else ""

    def build_ui(self):
        # Main outer container
        self.toolbar_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=2)
        self.toolbar_box.get_style_context().add_class("abk-toolbar")
        self.add(self.toolbar_box)

        # 1. Logo Badge (Clickable to toggle mode)
        self.logo_btn = Gtk.Button()
        self.logo_btn.get_style_context().add_class("logo-btn")
        self.logo_btn.set_tooltip_text("Amar Bangla Kotha (Click or F12 to toggle Mode)")
        self.logo_btn.connect("clicked", self.on_logo_clicked)

        self.logo_img = Gtk.Image()
        self.update_logo_image()
        self.logo_btn.add(self.logo_img)
        self.toolbar_box.pack_start(self.logo_btn, False, False, 0)

        # 2. First Divider
        self.divider1 = Gtk.Box()
        self.divider1.get_style_context().add_class("v-divider")
        self.toolbar_box.pack_start(self.divider1, False, False, 0)

        # 3. Mode / Language Dropdown Button
        self.dropdown_btn = Gtk.Button()
        self.dropdown_btn.get_style_context().add_class("mode-dropdown-btn")
        self.dropdown_btn.set_tooltip_text("ইনপুট মোড নির্বাচন (Avro / English)")
        self.dropdown_btn.connect("clicked", self.on_dropdown_clicked)

        dropdown_inner = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=2)
        self.dropdown_label = Gtk.Label(label="Avro")
        self.dropdown_label.get_style_context().add_class("mode-dropdown-label")
        dropdown_inner.pack_start(self.dropdown_label, False, False, 0)

        self.dropdown_arrow = Gtk.Label(label="⌵")
        self.dropdown_arrow.get_style_context().add_class("mode-dropdown-arrow")
        dropdown_inner.pack_start(self.dropdown_arrow, False, False, 0)

        self.dropdown_btn.add(dropdown_inner)
        self.toolbar_box.pack_start(self.dropdown_btn, False, False, 0)

        # 4. Second Divider
        self.divider2 = Gtk.Box()
        self.divider2.get_style_context().add_class("v-divider")
        self.toolbar_box.pack_start(self.divider2, False, False, 0)

        # 5. Action Buttons Cluster ([⌨] [⚙] [ⓘ])
        self.action_cluster = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=2)
        self.action_cluster.get_style_context().add_class("action-cluster")

        # 5a. Keyboard Layout Viewer [ ⌨ ]
        self.btn_keyboard = Gtk.Button(label="⌨")
        self.btn_keyboard.get_style_context().add_class("action-btn")
        self.btn_keyboard.set_tooltip_text("লেআউট ভিউয়ার (Layout Viewer)")
        self.btn_keyboard.connect("clicked", self.on_open_keyboard_viewer)
        self.action_cluster.pack_start(self.btn_keyboard, False, False, 0)

        # 5b. Settings [ ⚙ ]
        self.btn_settings = Gtk.Button(label="⚙")
        self.btn_settings.get_style_context().add_class("action-btn")
        self.btn_settings.set_tooltip_text("সেটিংস (Settings)")
        self.btn_settings.connect("clicked", self.on_open_settings_menu)
        self.action_cluster.pack_start(self.btn_settings, False, False, 0)

        # 5c. Info Icon [ ⓘ ] with Dropdown (Hide on Tray, Quit)
        self.btn_info = Gtk.Button(label="ⓘ")
        self.btn_info.get_style_context().add_class("action-btn")
        self.btn_info.set_tooltip_text("মেনু ও অপশন (Info & Options)")
        self.btn_info.connect("clicked", self.on_open_info_menu)
        self.action_cluster.pack_start(self.btn_info, False, False, 0)

        self.toolbar_box.pack_start(self.action_cluster, False, False, 0)

    def update_logo_image(self):
        svg_path = self.get_logo_path(self.is_bangla_mode)
        if svg_path and os.path.exists(svg_path):
            try:
                pb = GdkPixbuf.Pixbuf.new_from_file_at_scale(svg_path, 28, 28, True)
                self.logo_img.set_from_pixbuf(pb)
                return
            except Exception:
                pass
        self.logo_img.set_from_icon_name("input-keyboard", Gtk.IconSize.DND)

    def apply_theme_class(self):
        ctx = self.toolbar_box.get_style_context()
        if self.current_theme == "dark":
            ctx.remove_class("toolbar-light")
            ctx.add_class("toolbar-dark")
        else:
            ctx.remove_class("toolbar-dark")
            ctx.add_class("toolbar-light")

    def update_ui_state(self):
        # Update logo
        self.update_logo_image()

        # Update label text to strictly Avro or English
        if self.is_bangla_mode:
            self.display_text = "Avro"
            self.selected_mode = "Avro"
        else:
            self.display_text = "English"
            self.selected_mode = "English"

        self.dropdown_label.set_text(self.display_text)
        self.queue_resize()

    def set_system_engine(self, engine_name: str):
        """Switches system keyboard input source instantly."""
        try:
            if self.ibus_bus and self.ibus_bus.is_connected():
                self.ibus_bus.set_global_engine(engine_name)
            else:
                subprocess.run(['ibus', 'engine', engine_name], capture_output=True)
        except Exception:
            pass

    def sync_engine_from_system(self):
        """Keeps window floating above and detects Super+Space keyboard switching."""
        self.set_keep_above(True)
        self.stick()

        try:
            if self.ibus_bus and self.ibus_bus.is_connected():
                engine = self.ibus_bus.get_global_engine()
                if engine:
                    eng_name = engine.get_name()
                    if eng_name == "amar-bangla-kotha" and not self.is_bangla_mode:
                        self.is_bangla_mode = True
                        self.selected_mode = "Avro"
                        self.display_text = "Avro"
                        self.gui_state["is_bangla_mode"] = True
                        self.gui_state["selected_mode"] = "Avro"
                        self.gui_state["display_dropdown"] = "Avro"
                        save_gui_state(self.gui_state)
                        self.update_ui_state()
                    elif eng_name != "amar-bangla-kotha" and self.is_bangla_mode:
                        self.is_bangla_mode = False
                        self.selected_mode = "English"
                        self.display_text = "English"
                        self.gui_state["is_bangla_mode"] = False
                        self.gui_state["selected_mode"] = "English"
                        self.gui_state["display_dropdown"] = "English"
                        save_gui_state(self.gui_state)
                        self.update_ui_state()
        except Exception:
            pass
        return True

    def on_window_button_press(self, widget, event):
        if event.button == 1:
            self.drag_start_x = event.x_root
            self.drag_start_y = event.y_root
            self.win_start_x, self.win_start_y = self.get_position()
            self.is_dragging = True
            try:
                self.begin_move_drag(event.button, int(event.x_root), int(event.y_root), event.time)
            except Exception:
                pass
            return True
        return False

    def on_window_motion_notify(self, widget, event):
        if getattr(self, "is_dragging", False):
            dx = event.x_root - self.drag_start_x
            dy = event.y_root - self.drag_start_y
            self.move(int(self.win_start_x + dx), int(self.win_start_y + dy))
            return True
        return False

    def on_window_button_release(self, widget, event):
        if event.button == 1 and getattr(self, "is_dragging", False):
            self.is_dragging = False
            win_x, win_y = self.get_position()
            self.gui_state["x"] = int(win_x)
            self.gui_state["y"] = int(win_y)
            save_gui_state(self.gui_state)
            return True
        return False

    def on_window_key_press(self, widget, event):
        if event.keyval == Gdk.KEY_F12:
            self.on_toggle_mode()
            return True
        return False

    def on_logo_clicked(self, widget=None):
        self.on_toggle_mode()

    def on_toggle_mode(self):
        self.is_bangla_mode = not self.is_bangla_mode
        self.selected_mode = "Avro" if self.is_bangla_mode else "English"
        self.display_text = "Avro" if self.is_bangla_mode else "English"
        self.gui_state["is_bangla_mode"] = self.is_bangla_mode
        self.gui_state["selected_mode"] = self.selected_mode
        self.gui_state["display_dropdown"] = self.display_text
        save_gui_state(self.gui_state)

        # Switch system typing engine
        if self.is_bangla_mode:
            self.set_system_engine("amar-bangla-kotha")
        else:
            self.set_system_engine("xkb:us::eng")

        self.update_ui_state()

    def on_toggle_theme(self, widget=None):
        self.current_theme = "light" if self.current_theme == "dark" else "dark"
        self.gui_state["theme"] = self.current_theme
        save_gui_state(self.gui_state)
        self.apply_theme_class()

    # --------------------------------------------------------------------------
    # Dropdown Menu: Strictly 'Avro' and 'English'
    # --------------------------------------------------------------------------
    def on_dropdown_clicked(self, widget):
        menu = Gtk.Menu()
        menu.get_style_context().add_class("menu-dark" if self.current_theme == "dark" else "menu-light")

        options = [
            ("Avro", self.is_bangla_mode),
            ("English", not self.is_bangla_mode)
        ]

        for label, is_active in options:
            prefix = "✓ " if is_active else "   "
            item = Gtk.MenuItem(label=f"{prefix}{label}")
            if is_active:
                item.get_style_context().add_class("menu-item-selected")
            item.connect("activate", self.on_select_mode, label)
            menu.append(item)

        menu.show_all()
        menu.popup_at_widget(widget, Gdk.Gravity.SOUTH, Gdk.Gravity.NORTH, None)

    def on_select_mode(self, widget, mode_name: str):
        if mode_name == "Avro":
            self.is_bangla_mode = True
            self.selected_mode = "Avro"
            self.display_text = "Avro"
            self.set_system_engine("amar-bangla-kotha")
        else:
            self.is_bangla_mode = False
            self.selected_mode = "English"
            self.display_text = "English"
            self.set_system_engine("xkb:us::eng")

        self.gui_state["is_bangla_mode"] = self.is_bangla_mode
        self.gui_state["selected_mode"] = self.selected_mode
        self.gui_state["display_dropdown"] = self.display_text
        save_gui_state(self.gui_state)
        self.update_ui_state()

    # --------------------------------------------------------------------------
    # Info Menu [ ⓘ ]: Hide on System Tray & Quit
    # --------------------------------------------------------------------------
    def on_open_info_menu(self, widget):
        widget.get_style_context().add_class("action-btn-active")

        menu = Gtk.Menu()
        menu.get_style_context().add_class("menu-dark" if self.current_theme == "dark" else "menu-light")
        menu.connect("deactivate", lambda _: widget.get_style_context().remove_class("action-btn-active"))

        m_tray = Gtk.MenuItem(label="📌  Hide on System Tray")
        m_tray.connect("activate", lambda _: self.hide_to_tray())
        menu.append(m_tray)

        menu.append(Gtk.SeparatorMenuItem())

        m_quit = Gtk.MenuItem(label="❌  Quit")
        m_quit.connect("activate", lambda _: self.on_quit_app())
        menu.append(m_quit)

        menu.show_all()
        menu.popup_at_widget(widget, Gdk.Gravity.SOUTH, Gdk.Gravity.NORTH, None)

    def init_tray_indicator(self):
        """Initializes system tray icon so user can restore toolbar anytime."""
        if HAS_INDICATOR:
            try:
                self.indicator = AyatanaAppIndicator3.Indicator.new(
                    "amar-bangla-kotha-tray",
                    "input-keyboard",
                    AyatanaAppIndicator3.IndicatorCategory.APPLICATION_STATUS
                )
                self.indicator.set_status(AyatanaAppIndicator3.IndicatorStatus.ACTIVE)

                tray_menu = Gtk.Menu()
                tray_menu.get_style_context().add_class("menu-dark" if self.current_theme == "dark" else "menu-light")

                m_show = Gtk.MenuItem(label="Show Toolbar (টুলবার দেখান)")
                m_show.connect("activate", lambda _: self.show_from_tray())
                tray_menu.append(m_show)

                m_toggle = Gtk.MenuItem(label="Toggle Avro / English (F12)")
                m_toggle.connect("activate", lambda _: self.on_toggle_mode())
                tray_menu.append(m_toggle)

                tray_menu.append(Gtk.SeparatorMenuItem())

                m_quit = Gtk.MenuItem(label="Quit (বন্ধ করুন)")
                m_quit.connect("activate", lambda _: self.on_quit_app())
                tray_menu.append(m_quit)

                tray_menu.show_all()
                self.indicator.set_menu(tray_menu)
            except Exception:
                self.indicator = None

    def hide_to_tray(self):
        self.hide()

    def show_from_tray(self):
        self.show_all()
        self.set_keep_above(True)
        self.stick()
        self.present()

    def on_quit_app(self):
        # Reset system to default English when closing
        self.set_system_engine("xkb:us::eng")
        win_x, win_y = self.get_position()
        self.gui_state["x"] = int(win_x)
        self.gui_state["y"] = int(win_y)
        save_gui_state(self.gui_state)
        Gtk.main_quit()

    # --------------------------------------------------------------------------
    # Settings Menu [ ⚙ ]: General, Hotkeys, Theme, Autocorrect, Dictionary
    # --------------------------------------------------------------------------
    def on_open_settings_menu(self, widget):
        widget.get_style_context().add_class("action-btn-active")

        menu = Gtk.Menu()
        menu.get_style_context().add_class("menu-dark" if self.current_theme == "dark" else "menu-light")
        menu.connect("deactivate", lambda _: widget.get_style_context().remove_class("action-btn-active"))

        items = [
            ("⚙ সেটিংস", self.on_settings_general),
            ("⚙ হটকি", self.on_settings_hotkeys),
            ("🌐 থিম", self.on_toggle_theme),
            ("⚙ অটোকারেক্ট", self.on_settings_autocorrect),
            ("📖 ডিকশনারি", self.on_settings_dictionary),
            (None, None),
            ("ⓘ সম্পর্কে", self.on_open_about)
        ]

        for label, callback in items:
            if label is None:
                menu.append(Gtk.SeparatorMenuItem())
            else:
                item = Gtk.MenuItem(label=label)
                if callback:
                    item.connect("activate", callback)
                menu.append(item)

        menu.show_all()
        menu.popup_at_widget(widget, Gdk.Gravity.SOUTH, Gdk.Gravity.NORTH, None)

    def on_settings_general(self, widget=None):
        settings_script = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools", "abk_settings.py")
        if os.path.exists(settings_script):
            subprocess.Popen([sys.executable, settings_script])
        else:
            self.show_info_dialog("সেটিংস", "Amar Bangla Kotha Settings tools/abk_settings.py e ache.")

    def on_settings_hotkeys(self, widget=None):
        msg = (
            "Amar Bangla Kotha Hotkey Preferences:\n\n"
            "• Super (Win) + Space: System-wide OS input source toggle\n"
            "• F12: Switch between Avro ⇄ English\n"
            "• Backtick (`): Phonetic joiner / separator (e.g. am`ra -> আমরা)"
        )
        self.show_info_dialog("হটকি সেটিংস (Hotkeys)", msg)

    def on_settings_autocorrect(self, widget=None):
        msg = (
            "Amar Bangla Kotha Autocorrect System:\n\n"
            "• Status: Enabled\n"
            "• Rules file: data/autocorrect_default.tsv\n"
            "• Example: 'amra' -> 'আমরা', 'software' -> 'সফটওয়্যার'\n"
            "• Backspacing once undoes any automatic correction."
        )
        self.show_info_dialog("অটোকারেক্ট সেটিংস", msg)

    def on_settings_dictionary(self, widget=None):
        dialog = Gtk.Dialog(title="ডিকশনারি ম্যানেজার (Personal Dictionary)", parent=self, flags=0)
        dialog.add_button(Gtk.STOCK_CLOSE, Gtk.ResponseType.CLOSE)

        box = dialog.get_content_area()
        box.set_spacing(10)
        box.set_margin_start(16)
        box.set_margin_end(16)
        box.set_margin_top(16)

        lbl = Gtk.Label(label="Amar Bangla Kotha Dictionary Manager active.")
        box.pack_start(lbl, False, False, 0)
        dialog.show_all()
        dialog.run()
        dialog.destroy()

    def on_open_keyboard_viewer(self, widget=None):
        dialog = Gtk.Dialog(title="কীম্যাপ ভিউয়ার (Layout Viewer)", parent=self, flags=0)
        dialog.add_button(Gtk.STOCK_CLOSE, Gtk.ResponseType.CLOSE)
        dialog.set_default_size(700, 480)

        box = dialog.get_content_area()
        box.set_spacing(10)
        box.set_margin_start(16)
        box.set_margin_end(16)
        box.set_margin_top(16)
        box.set_margin_bottom(12)

        title = Gtk.Label(label="<b>Amar Bangla Kotha (আমার বাংলা কথা) — ফনেটিক কীম্যাপ ভিউয়ার</b>")
        title.set_use_markup(True)
        box.pack_start(title, False, False, 0)

        lbl = Gtk.Label(label=ONE_PAGE_CHEATSHEET)
        lbl.set_line_wrap(True)
        lbl.set_selectable(True)
        lbl.set_xalign(0.0)
        box.pack_start(lbl, True, True, 0)

        dialog.show_all()
        dialog.run()
        dialog.destroy()

    def on_open_about(self, widget=None):
        msg = (
            "Amar Bangla Kotha (আমার বাংলা কথা)\n"
            "Modern Cross-Platform Phonetic Bengali Input Method\n\n"
            "Version: 1.0.0 (Production)\n"
            "C++ Core Engine with IBus, Fcitx5, Windows TSF & macOS IMKit\n"
            "License: GPL-3.0\n"
            "Website: https://github.com/amar-bangla-kotha"
        )
        self.show_info_dialog("সম্পর্কে (About)", msg)

    def show_info_dialog(self, title: str, message: str):
        dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text=title
        )
        dialog.format_secondary_text(message)
        dialog.run()
        dialog.destroy()

def main():
    app = AvroKeyboardBar()
    app.connect("destroy", lambda _: Gtk.main_quit())
    app.show_all()
    Gtk.main()

if __name__ == '__main__':
    main()
