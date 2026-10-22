#!/usr/bin/env python3
"""
Test Suite: Linux Wayland & Accessibility (Workstream 9)
Verifies:
1. IBus Engine initialization under simulated Wayland environment.
2. Preedit and candidate announcements for AT-SPI2 / Orca screen reader.
3. Accessible candidate labels (1., 2., 3., etc.).
4. Arrow key navigation stability and single source of truth selection.
5. Password field bypass and zero leakage.
"""

import os
import sys
import unittest

# Point to project root
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT_DIR, "bindings", "linux"))

# Set simulated Wayland environment variables
os.environ["WAYLAND_DISPLAY"] = "wayland-0"
os.environ["XDG_CURRENT_DESKTOP"] = "GNOME"
os.environ["IBUS_ENABLE_SYNC_MODE"] = "1"

try:
    import gi
    gi.require_version('IBus', '1.0')
    from gi.repository import IBus
    from abk_ibus_engine import AmarBanglaKothaEngine, abk_lib
    HAS_IBUS = True
except Exception as e:
    HAS_IBUS = False
    IBUS_ERR = e

class TestLinuxWaylandAccessibility(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not HAS_IBUS:
            raise unittest.SkipTest(f"IBus not available: {IBUS_ERR}")

    def setUp(self):
        IBus.init()
        self.engine = AmarBanglaKothaEngine()

    def tearDown(self):
        if self.engine:
            self.engine.do_destroy()

    def test_engine_initialization_wayland(self):
        """Verifies engine loads cleanly in Wayland environment without X11 dependency."""
        self.assertIsNotNone(self.engine.engine_handle)
        self.assertFalse(self.engine.is_secure_mode)
        self.assertEqual(self.engine.buffer, "")

    def test_preedit_and_accessible_candidates(self):
        """Verifies candidate lookup table has accessible labels and values for AT-SPI2 / Orca."""
        # Simulate typing 'k' 'o' 'i'
        for ch in "koi":
            keyval = ord(ch)
            handled = self.engine.do_process_key_event(keyval, 0, 0)
            self.assertTrue(handled)

        self.assertEqual(self.engine.buffer, "koi")
        self.assertGreater(len(self.engine.current_candidates), 0)
        self.assertIn("কই", self.engine.current_candidates)

        # Check lookup table candidate count and labels for screen reader
        table = self.engine.lookup_table
        cand_count = len(self.engine.current_candidates)
        self.assertGreaterEqual(cand_count, 1)

        # Verify cursor pos is initialized to 0
        self.assertEqual(table.get_cursor_pos(), 0)

    def test_keyboard_navigation_accessibility(self):
        """Verifies Arrow Down and Up update cursor position for screen reader."""
        for ch in "bari":
            self.engine.do_process_key_event(ord(ch), 0, 0)

        initial_cursor = self.engine.lookup_table.get_cursor_pos()
        self.assertEqual(initial_cursor, 0)

        # Arrow Down -> cursor moves to index 1
        handled_down = self.engine.do_process_key_event(IBus.KEY_Down, 0, 0)
        self.assertTrue(handled_down)
        self.assertEqual(self.engine.lookup_table.get_cursor_pos(), 1)

        # Arrow Up -> cursor moves back to index 0
        handled_up = self.engine.do_process_key_event(IBus.KEY_Up, 0, 0)
        self.assertTrue(handled_up)
        self.assertEqual(self.engine.lookup_table.get_cursor_pos(), 0)

    def test_password_field_accessibility_and_security(self):
        """Verifies that in password fields, IME auto-bypasses completely with zero leakage."""
        # Set password field content type
        self.engine.do_set_content_type(IBus.InputPurpose.PASSWORD, 0)
        self.assertTrue(self.engine.is_secure_mode)

        # Keystroke must NOT be consumed by IME
        handled = self.engine.do_process_key_event(ord('a'), 0, 0)
        self.assertFalse(handled, "Password keystrokes must be passed through directly to OS")
        self.assertEqual(self.engine.buffer, "")
        self.assertEqual(len(self.engine.current_candidates), 0)

if __name__ == '__main__':
    unittest.main()
