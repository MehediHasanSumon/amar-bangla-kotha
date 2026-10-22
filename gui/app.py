#!/usr/bin/env python3
"""
Amar Bangla Kotha (আমার বাংলা কথা) - Standalone Avro-Style GUI Toolbar
Built with GTK 3 & Modern C++ libabk Engine
"""

import sys
import os
import subprocess
import ctypes
from ctypes import c_void_p, c_char_p, c_size_t, c_int, c_double, Structure, POINTER

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
from gi.repository import Gtk, Gdk, GLib, Pango

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
    paths = [
        os.path.expanduser("~/.local/lib/libabk.so"),
        "/usr/local/lib/libabk.so",
        os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "lib", "libabk.so")
    ]
    for p in paths:
        if os.path.exists(p):
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
    raise RuntimeError("libabk.so not found. Run 'make install-user' first.")

ABK = load_abk()

CSS = """
window.abk-bar {
    background: rgba(26, 27, 38, 0.95);
    border: 1px solid #414868;
    border-radius: 12px;
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
}

.brand-label {
    font-weight: bold;
    font-size: 13px;
    color: #7AA2F7;
    padding: 0 4px;
}

.toggle-btn-bn {
    background: linear-gradient(135deg, #00BA88, #008763);
    color: #FFFFFF;
    font-weight: bold;
    border-radius: 8px;
    border: none;
    padding: 4px 12px;
}

.toggle-btn-en {
    background: linear-gradient(135deg, #3B82F6, #1D4ED8);
    color: #FFFFFF;
    font-weight: bold;
    border-radius: 8px;
    border: none;
    padding: 4px 12px;
}

.input-entry {
    background: #24283B;
    color: #C0CAF5;
    border: 1px solid #414868;
    border-radius: 8px;
    padding: 4px 10px;
    font-size: 14px;
}

.input-entry:focus {
    border-color: #7AA2F7;
}

.preview-label {
    color: #9ECE6A;
    font-weight: bold;
    font-size: 15px;
    padding: 0 6px;
}

.cand-btn {
    background: #292E42;
    color: #7DCFFF;
    border: 1px solid #3B4261;
    border-radius: 6px;
    padding: 2px 8px;
    font-size: 13px;
}

.cand-btn:hover {
    background: #3B4261;
    color: #FFFFFF;
}

.menu-btn {
    background: transparent;
    color: #A9B1D6;
    border: none;
    padding: 4px 8px;
    border-radius: 6px;
}

.menu-btn:hover {
    background: #414868;
    color: #FFFFFF;
}
"""

class AvroKeyboardBar(Gtk.Window):
    def __init__(self):
        super().__init__(type=Gtk.WindowType.TOPLEVEL)
        self.set_title("Amar Bangla Kotha")
        self.set_default_size(720, 48)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_keep_above(True)
        self.set_decorated(False)
        self.set_resizable(False)
        self.set_skip_taskbar_hint(False)

        # Style context
        self.get_style_context().add_class("abk-bar")
        css_provider = Gtk.CssProvider()
        css_provider.load_from_data(CSS.encode('utf-8'))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        # Initialize C++ engine
        self.engine = ABK.abk_engine_create()
        self.is_bangla_mode = True
        self.candidates = []

        # Load dictionary
        dict_paths = [
            os.path.expanduser("~/.local/share/amar-bangla-kotha/bangla_wordlist.txt"),
            "/usr/local/share/amar-bangla-kotha/bangla_wordlist.txt",
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "bangla_wordlist.txt")
        ]
        for dp in dict_paths:
            if os.path.exists(dp):
                ABK.abk_load_dictionary(self.engine, dp.encode('utf-8'))
                break

        # Main Layout Box
        main_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        main_box.set_margin_start(10)
        main_box.set_margin_end(10)
        main_box.set_margin_top(8)
        main_box.set_margin_bottom(8)
        self.add(main_box)

        # 1. Drag handle / Logo
        drag_event_box = Gtk.EventBox()
        brand_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=4)
        brand_lbl = Gtk.Label(label="🇧🇩 আমার বাংলা কথা")
        brand_lbl.get_style_context().add_class("brand-label")
        brand_box.pack_start(brand_lbl, False, False, 0)
        drag_event_box.add(brand_box)
        drag_event_box.connect("button-press-event", self.on_drag_press)
        main_box.pack_start(drag_event_box, False, False, 0)

        # 2. Avro Language Switch Toggle Button
        self.toggle_btn = Gtk.Button()
        self.toggle_btn.connect("clicked", self.on_toggle_mode)
        main_box.pack_start(self.toggle_btn, False, False, 0)

        # 3. Live Transliteration Typing Box
        self.entry = Gtk.Entry()
        self.entry.set_width_chars(20)
        self.entry.get_style_context().add_class("input-entry")
        self.entry.connect("changed", self.on_text_changed)
        self.entry.connect("activate", self.on_entry_activate)
        self.entry.connect("key-press-event", self.on_key_press)
        main_box.pack_start(self.entry, True, True, 0)

        # 4. Live Preview Label
        self.preview_lbl = Gtk.Label(label="")
        self.preview_lbl.get_style_context().add_class("preview-label")
        main_box.pack_start(self.preview_lbl, False, False, 0)

        # 5. Candidate Buttons Container
        self.cand_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=4)
        main_box.pack_start(self.cand_box, False, False, 0)

        # 6. Quick Menu Button (Avro 3-line / gear icon)
        menu_btn = Gtk.Button(label="☰ Menu")
        menu_btn.get_style_context().add_class("menu-btn")
        menu_btn.connect("clicked", self.on_show_menu)
        main_box.pack_start(menu_btn, False, False, 0)

        # 7. Close button
        close_btn = Gtk.Button(label="✕")
        close_btn.get_style_context().add_class("menu-btn")
        close_btn.connect("clicked", lambda _: Gtk.main_quit())
        main_box.pack_start(close_btn, False, False, 0)

        self.update_toggle_btn_ui()

    def update_toggle_btn_ui(self):
        ctx = self.toggle_btn.get_style_context()
        if self.is_bangla_mode:
            self.toggle_btn.set_label("🌐 বাংলা (F12)")
            ctx.remove_class("toggle-btn-en")
            ctx.add_class("toggle-btn-bn")
            self.entry.set_placeholder_text("Bangla likhun (e.g. kotha, dhonnobad)...")
        else:
            self.toggle_btn.set_label("🌐 English (F12)")
            ctx.remove_class("toggle-btn-bn")
            ctx.add_class("toggle-btn-en")
            self.entry.set_placeholder_text("English mode active (press F12 to switch)...")
            self.preview_lbl.set_text("")
            self.clear_candidates()

    def on_toggle_mode(self, widget=None):
        self.is_bangla_mode = not self.is_bangla_mode
        self.update_toggle_btn_ui()
        self.entry.grab_focus()

    def on_drag_press(self, widget, event):
        if event.button == 1:
            self.begin_move_drag(event.button, int(event.x_root), int(event.y_root), event.time)

    def on_text_changed(self, entry):
        text = entry.get_text()
        if not self.is_bangla_mode or not text.strip():
            self.preview_lbl.set_text("")
            self.clear_candidates()
            return

        # Query C++ core engine
        res_ptr = ABK.abk_get_suggestions(self.engine, text.encode('utf-8'))
        if res_ptr:
            res = res_ptr.contents
            primary = res.primary_transliteration.decode('utf-8') if res.primary_transliteration else ""
            self.preview_lbl.set_text(primary)

            self.clear_candidates()
            self.candidates = []
            max_show = min(res.candidate_count, 4)
            for i in range(max_show):
                w = res.candidates[i].word.decode('utf-8')
                self.candidates.append(w)
                btn = Gtk.Button(label=f"{i+1}. {w}")
                btn.get_style_context().add_class("cand-btn")
                btn.connect("clicked", lambda _, word=w: self.commit_word(word))
                self.cand_box.pack_start(btn, False, False, 0)

            self.cand_box.show_all()
            ABK.abk_free_suggestions(res_ptr)

    def clear_candidates(self):
        for child in self.cand_box.get_children():
            self.cand_box.remove(child)
        self.candidates = []

    def commit_word(self, word):
        if not word:
            return

        # Copy to clipboard
        clipboard = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)
        clipboard.set_text(word, -1)
        clipboard.store()

        # Also support wl-copy / xclip on Wayland/X11
        try:
            subprocess.run(["wl-copy", word], check=False)
        except Exception:
            pass

        self.preview_lbl.set_text(f"✓ Copied: {word}")
        self.entry.set_text("")
        self.clear_candidates()
        self.entry.grab_focus()

    def on_entry_activate(self, entry):
        text = entry.get_text()
        if not text:
            return

        if self.is_bangla_mode:
            target = self.candidates[0] if self.candidates else self.preview_lbl.get_text()
            if target.startswith("✓ "):
                target = target[2:]
            self.commit_word(target)
        else:
            self.commit_word(text)

    def on_key_press(self, widget, event):
        # F12 toggles language mode (Avro standard)
        if event.keyval == Gdk.KEY_F12:
            self.on_toggle_mode()
            return True

        # Number keys 1-4 for candidate selection
        if self.candidates and Gdk.KEY_1 <= event.keyval <= Gdk.KEY_4:
            idx = event.keyval - Gdk.KEY_1
            if idx < len(self.candidates):
                self.commit_word(self.candidates[idx])
                return True

        return False

    def on_show_menu(self, widget):
        menu = Gtk.Menu()

        # 1. Switch language
        m_switch = Gtk.MenuItem(label="🔄 Switch Language (F12)")
        m_switch.connect("activate", self.on_toggle_mode)
        menu.append(m_switch)

        # 2. Phonetic guide
        m_guide = Gtk.MenuItem(label="⌨️ Phonetic Layout Guide")
        m_guide.connect("activate", self.show_layout_guide)
        menu.append(m_guide)

        # 3. Learn word
        m_learn = Gtk.MenuItem(label="➕ Add New Word to Dictionary")
        m_learn.connect("activate", self.show_learn_dialog)
        menu.append(m_learn)

        # 4. Separator
        menu.append(Gtk.SeparatorMenuItem())

        # 5. Quit
        m_quit = Gtk.MenuItem(label="❌ Exit")
        m_quit.connect("activate", lambda _: Gtk.main_quit())
        menu.append(m_quit)

        menu.show_all()
        menu.popup_at_widget(widget, Gdk.Gravity.SOUTH, Gdk.Gravity.NORTH, None)

    def show_layout_guide(self, widget):
        dialog = Gtk.Dialog(title="Phonetic Layout Guide", parent=self, flags=0)
        dialog.add_button(Gtk.STOCK_CLOSE, Gtk.ResponseType.CLOSE)
        dialog.set_default_size(480, 360)

        box = dialog.get_content_area()
        box.set_spacing(10)
        box.set_margin_start(16)
        box.set_margin_end(16)
        box.set_margin_top(16)

        title = Gtk.Label(label="<b>Amar Bangla Kotha - Phonetic Key Mapping</b>")
        title.set_use_markup(True)
        box.pack_start(title, False, False, 0)

        guide_text = (
            "• Vowels: a (আ/া), i (ই/ি), ee/I (ঈ/ী), u (উ/ু), oo/U (ঊ/ূ), e (এ/ে), o (ও/ো), oi (ঐ/ৈ), ou (ঔ/ৌ)\n"
            "• Consonants: k (ক), kh (খ), g (গ), gh (ঘ), ng (ং), Ng (ঙ)\n"
            "• Palatals: c/ch (চ), chh (ছ), j (জ), jh (ঝ), NG (ঞ)\n"
            "• Retroflex: T (ট), Th (ঠ), D (ড), Dh (ঢ), N (ণ)\n"
            "• Dentals: t (ত), th (থ), d (দ), dh (ধ), n (ন)\n"
            "• Labials: p (প), ph/f (ফ), b (ব), bh/v (ভ), m (ম)\n"
            "• Sibilants: sh (শ), Sh (ষ), s (স), h (হ)\n"
            "• Semi-vowels & Liquids: r (র), l (ল), z (য), y (য়), R (ড়), Rh (ঢ়)\n"
            "• Conjuncts: kkh (ক্ষ), gy/jny (জ্ঞ), pro (প্র), tyo (ত্য), rrk (র্ক)\n"
            "• Modifiers: t` (ৎ), ^ (ঁ), : (ঃ)"
        )
        lbl = Gtk.Label(label=guide_text)
        lbl.set_line_wrap(True)
        box.pack_start(lbl, True, True, 0)

        dialog.show_all()
        dialog.run()
        dialog.destroy()

    def show_learn_dialog(self, widget):
        dialog = Gtk.Dialog(title="Add Word to Dictionary", parent=self, flags=0)
        dialog.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OK, Gtk.ResponseType.OK)

        box = dialog.get_content_area()
        box.set_spacing(10)
        box.set_margin_start(16)
        box.set_margin_end(16)
        box.set_margin_top(16)

        lbl = Gtk.Label(label="Notun kon Bangla shobdo ti engine vocabulary te add korte chan?")
        box.pack_start(lbl, False, False, 0)

        entry = Gtk.Entry()
        entry.set_placeholder_text("Bangla shobdo likhun (e.g. রোবোটিক্স)...")
        box.pack_start(entry, False, False, 0)

        dialog.show_all()
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            w = entry.get_text().strip()
            if w:
                ABK.abk_learn_word(self.engine, w.encode('utf-8'), 1000)
                self.preview_lbl.set_text(f"✓ Added: {w}")
        dialog.destroy()

def main():
    app = AvroKeyboardBar()
    app.connect("destroy", Gtk.main_quit)
    app.show_all()
    Gtk.main()

if __name__ == '__main__':
    main()
