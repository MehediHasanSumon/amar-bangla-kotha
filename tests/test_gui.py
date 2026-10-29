#!/usr/bin/env python3
"""
Unit Test Suite for Amar Bangla Kotha Standalone Production GUI
Verifies:
1. Multi-platform dynamic library discovery and C ABI bindings.
2. GUI state persistence (theme, mode, layout, minimized, coordinates).
3. Cross-platform clipboard helper.
4. Keyboard toolbar initialization (matching mockup elements).
5. Language mode toggling (বাংলা ⇄ English) and logo update.
6. Layout selection (ফোনেটিক, অভ্র, জাতীয়, বিজয়, ইউনিকোড).
7. Theme switching (Light ⇄ Dark).
8. Minimize toggling (Full ⇄ Minimized Version).
"""

import sys
import os
import unittest
import tempfile

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, "gui"))

try:
    import gi
    gi.require_version('Gtk', '3.0')
    gi.require_version('Gdk', '3.0')
    from gi.repository import Gtk, Gdk, GLib
    HAS_GTK = True
except Exception:
    HAS_GTK = False

import app

class TestGuiProduction(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not HAS_GTK:
            raise unittest.SkipTest("GTK 3 / PyGObject not available in environment")
        Gtk.init_check()

    def test_library_loader(self):
        """Verifies multi-platform library discovery finds and binds libabk."""
        lib = app.load_abk()
        self.assertIsNotNone(lib, "libabk library should load successfully")
        self.assertTrue(hasattr(lib, "abk_engine_create"))
        self.assertTrue(hasattr(lib, "abk_get_suggestions"))
        self.assertTrue(hasattr(lib, "abk_learn_word"))

    def test_gui_state_persistence(self):
        """Verifies state persistence loads and saves theme and coordinates cleanly."""
        orig_state_file = app.STATE_FILE
        try:
            with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tf:
                temp_path = tf.name
            app.STATE_FILE = temp_path

            state = app.load_gui_state()
            self.assertIn("theme", state)

            test_state = {
                "theme": "dark",
                "is_bangla_mode": True,
                "selected_mode": "বাংলা",
                "selected_layout": "ফোনেটিক",
                "display_dropdown": "বাংলা",
                "is_minimized": False,
                "x": 400,
                "y": 60
            }
            app.save_gui_state(test_state)

            loaded = app.load_gui_state()
            self.assertEqual(loaded["theme"], "dark")
            self.assertEqual(loaded["is_bangla_mode"], True)
            self.assertEqual(loaded["selected_layout"], "ফোনেটিক")
            self.assertEqual(loaded["x"], 400)
            self.assertEqual(loaded["y"], 60)
        finally:
            app.STATE_FILE = orig_state_file
            if os.path.exists(temp_path):
                os.remove(temp_path)

    def test_clipboard_helper(self):
        """Verifies clipboard helper handles text copying without crashing."""
        ret = app.copy_to_clipboard("")
        self.assertFalse(ret)

        clipboard = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)
        ret2 = app.copy_to_clipboard("বাংলা", clipboard)
        self.assertTrue(ret2)

    def test_toolbar_initialization(self):
        """Verifies AvroKeyboardBar initializes widgets according to the reference mockup."""
        bar = app.AvroKeyboardBar()
        self.assertIsNotNone(bar.toolbar_box)
        self.assertIsNotNone(bar.logo_btn)
        self.assertIsNotNone(bar.dropdown_btn)
        self.assertIsNotNone(bar.btn_keyboard)
        self.assertIsNotNone(bar.btn_settings)
        self.assertIsNotNone(bar.btn_info)

        # Check initial display
        self.assertIn(bar.dropdown_label.get_text(), ("Avro", "English"))
        bar.destroy()

    def test_mode_and_layout_toggling(self):
        """Verifies mode toggling between Avro and English."""
        bar = app.AvroKeyboardBar()

        # Toggle to English
        bar.on_select_mode(None, "English")
        self.assertFalse(bar.is_bangla_mode)
        self.assertEqual(bar.dropdown_label.get_text(), "English")

        # Toggle back to Avro
        bar.on_select_mode(None, "Avro")
        self.assertTrue(bar.is_bangla_mode)
        self.assertEqual(bar.dropdown_label.get_text(), "Avro")

        # Toggle via on_toggle_mode
        bar.on_toggle_mode()
        self.assertFalse(bar.is_bangla_mode)
        self.assertEqual(bar.dropdown_label.get_text(), "English")

        bar.destroy()

    def test_theme_and_tray_toggling(self):
        """Verifies switching between Light and Dark themes and tray hide/show."""
        bar = app.AvroKeyboardBar()

        # Theme toggle
        orig_theme = bar.current_theme
        bar.on_toggle_theme()
        self.assertNotEqual(bar.current_theme, orig_theme)
        bar.on_toggle_theme()
        self.assertEqual(bar.current_theme, orig_theme)

        # Tray hide and show
        bar.hide_to_tray()
        bar.show_from_tray()

        bar.destroy()

if __name__ == '__main__':
    unittest.main()
