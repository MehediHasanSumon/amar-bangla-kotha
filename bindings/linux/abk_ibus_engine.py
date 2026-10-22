#!/usr/bin/env python3
"""
Amar Bangla Kotha (আমার বাংলা কথা) - IBus Engine Daemon
Connects Linux IBus to C++ libabk engine via ctypes
"""

import sys
import os
import ctypes
from ctypes import c_void_p, c_char_p, c_size_t, c_int, c_double, Structure, POINTER

import gi
gi.require_version('IBus', '1.0')
from gi.repository import IBus, GLib

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

def load_abk_library():
    possible_paths = [
        os.path.expanduser("~/.local/lib/libabk.so"),
        "/usr/local/lib/libabk.so",
        "/usr/lib/libabk.so",
        os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "lib", "libabk.so")
    ]
    for p in possible_paths:
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
                lib.abk_load_dictionary.restype = c_int
                lib.abk_load_dictionary.argtypes = [c_void_p, c_char_p]
                lib.abk_normalize_nfc.restype = c_int
                lib.abk_normalize_nfc.argtypes = [c_char_p, c_char_p, c_size_t]
                lib.abk_pop_logical_unit.restype = c_int
                lib.abk_pop_logical_unit.argtypes = [c_char_p, c_char_p, c_size_t]
                lib.abk_grapheme_count.restype = c_size_t
                lib.abk_grapheme_count.argtypes = [c_char_p]
                lib.abk_suggestion_select_next.restype = c_int
                lib.abk_suggestion_select_next.argtypes = [c_void_p]
                lib.abk_suggestion_select_prev.restype = c_int
                lib.abk_suggestion_select_prev.argtypes = [c_void_p]
                lib.abk_suggestion_select_index.restype = c_int
                lib.abk_suggestion_select_index.argtypes = [c_void_p, c_int]
                lib.abk_suggestion_get_selected_index.restype = c_int
                lib.abk_suggestion_get_selected_index.argtypes = [c_void_p]
                lib.abk_suggestion_reset.restype = None
                lib.abk_suggestion_reset.argtypes = [c_void_p]
                lib.abk_engine_set_secure_mode.restype = None
                lib.abk_engine_set_secure_mode.argtypes = [c_void_p, c_int]
                lib.abk_engine_is_secure_mode.restype = c_int
                lib.abk_engine_is_secure_mode.argtypes = [c_void_p]
                lib.abk_engine_set_clipboard_mode.restype = None
                lib.abk_engine_set_clipboard_mode.argtypes = [c_void_p, c_int]
                lib.abk_engine_is_clipboard_mode.restype = c_int
                lib.abk_engine_is_clipboard_mode.argtypes = [c_void_p]
                lib.abk_engine_record_commit.restype = None
                lib.abk_engine_record_commit.argtypes = [c_void_p, c_char_p, c_char_p, c_int]
                lib.abk_engine_can_undo_commit.restype = c_int
                lib.abk_engine_can_undo_commit.argtypes = [c_void_p]
                lib.abk_engine_get_undo_info.restype = c_int
                lib.abk_engine_get_undo_info.argtypes = [c_void_p, c_char_p, c_size_t, c_char_p, c_size_t]
                lib.abk_engine_clear_undo.restype = None
                lib.abk_engine_clear_undo.argtypes = [c_void_p]
                lib.abk_token_should_bypass.restype = c_int
                lib.abk_token_should_bypass.argtypes = [c_void_p, c_char_p]
                lib.abk_pop_grapheme_cluster.restype = c_int
                lib.abk_pop_grapheme_cluster.argtypes = [c_char_p, c_char_p, c_size_t]
                return lib
            except Exception as e:
                print(f"Error loading {p}: {e}", file=sys.stderr)
    raise RuntimeError("Could not find libabk.so. Please run 'make install-user' first.")

abk_lib = load_abk_library()

DEBUG_LOGGING = os.environ.get("ABK_DEBUG", "1") == "1"

def log_event(event_type: str, details: str):
    if DEBUG_LOGGING:
        import time
        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        sys.stderr.write(f"[ABK_EVENT_LOG {ts}] {event_type} | {details}\n")
        sys.stderr.flush()

def normalize_nfc(text: str) -> str:
    if not text:
        return ""
    raw = text.encode('utf-8')
    buf = ctypes.create_string_buffer(len(raw) * 4 + 32)
    ret = abk_lib.abk_normalize_nfc(raw, buf, len(buf))
    if ret >= 0:
        return buf.value.decode('utf-8')
    return text

def pop_logical_unit(text: str) -> str:
    if not text:
        return ""
    raw = text.encode('utf-8')
    buf = ctypes.create_string_buffer(len(raw) + 16)
    ret = abk_lib.abk_pop_logical_unit(raw, buf, len(buf))
    if ret >= 0:
        return buf.value.decode('utf-8')
    return text[:-1]

# 2. IBus Engine Implementation
class AmarBanglaKothaEngine(IBus.Engine):
    __gtype_name__ = 'AmarBanglaKothaEngine'

    def __init__(self):
        super().__init__()
        self.engine_handle = abk_lib.abk_engine_create()
        self.buffer = ""
        self.current_primary = ""
        self.current_candidates = []
        self.lookup_table = IBus.LookupTable.new(9, 0, True, True)
        self.is_secure_mode = False

        dict_paths = [
            os.path.expanduser("~/.local/share/amar-bangla-kotha/bangla_wordlist.txt"),
            "/usr/local/share/amar-bangla-kotha/bangla_wordlist.txt",
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "bangla_wordlist.txt")
        ]
        for dp in dict_paths:
            if os.path.exists(dp):
                abk_lib.abk_load_dictionary(self.engine_handle, dp.encode('utf-8'))
                break

    def do_destroy(self):
        if self.engine_handle:
            abk_lib.abk_engine_destroy(self.engine_handle)
            self.engine_handle = None

    def update_preedit_display(self, display_text=None):
        if not display_text:
            display_text = self.current_primary or self.buffer
        text = IBus.Text.new_from_string(display_text)
        text.append_attribute(IBus.AttrType.UNDERLINE, IBus.AttrUnderline.SINGLE, 0, len(display_text))
        self.update_preedit_text(text, len(display_text), True)

    def update_preedit_and_lookup(self):
        if not self.buffer:
            self.current_primary = ""
            self.current_candidates = []
            abk_lib.abk_suggestion_reset(self.engine_handle)
            self.hide_lookup_table()
            self.hide_preedit_text()
            return

        res_ptr = abk_lib.abk_get_suggestions(self.engine_handle, self.buffer.encode('utf-8'))
        if not res_ptr:
            return

        res = res_ptr.contents
        self.current_primary = res.primary_transliteration.decode('utf-8') if res.primary_transliteration else ""
        self.current_candidates = []

        self.lookup_table.clear()
        for i in range(res.candidate_count):
            w = res.candidates[i].word.decode('utf-8')
            self.current_candidates.append(w)
            self.lookup_table.append_candidate(IBus.Text.new_from_string(w))

        abk_lib.abk_free_suggestions(res_ptr)

        # Sync selected index from engine (single source of truth)
        sel_idx = abk_lib.abk_suggestion_get_selected_index(self.engine_handle)
        if 0 <= sel_idx < len(self.current_candidates):
            self.lookup_table.set_cursor_pos(sel_idx)

        # Update pre-edit string (live preview underlined)
        display_text = self.current_candidates[sel_idx] if (0 <= sel_idx < len(self.current_candidates)) else (self.current_primary or self.buffer)
        self.update_preedit_display(display_text)

        # Show candidate window if candidates exist
        if self.current_candidates:
            self.update_lookup_table(self.lookup_table, True)
        else:
            self.hide_lookup_table()

    def do_candidate_clicked(self, index, button, state):
        log_event("CLICK_RECEIVED", f"Candidate clicked at index: {index}, button: {button}")
        if 0 <= index < len(self.current_candidates):
            abk_lib.abk_suggestion_select_index(self.engine_handle, index)
            self.lookup_table.set_cursor_pos(index)
            target = self.current_candidates[index]
            self.commit_word(target, append_space=False)

    def commit_word(self, word=None, append_space=False):
        if not word:
            sel_idx = self.lookup_table.get_cursor_pos()
            if 0 <= sel_idx < len(self.current_candidates):
                word = self.current_candidates[sel_idx]
            else:
                word = self.current_primary or self.buffer

        log_event("COMMIT_TEXT", f"Word: '{word}', append_space={append_space}")

        raw_typed = self.buffer

        # 1. Reset state and hide composition UI FIRST
        self.buffer = ""
        self.current_primary = ""
        self.current_candidates = []
        abk_lib.abk_suggestion_reset(self.engine_handle)
        self.hide_lookup_table()
        self.hide_preedit_text()

        # 2. Append trailing space if requested and ensure NFC canonical form
        clean_word = normalize_nfc(word)
        text_to_commit = clean_word + (" " if append_space else "")

        # 3. Workstream 3: Record commit for Undo on Backspace
        if raw_typed and text_to_commit and self.engine_handle:
            abk_lib.abk_engine_record_commit(
                self.engine_handle,
                raw_typed.encode('utf-8'),
                text_to_commit.encode('utf-8'),
                0
            )

        # 4. Commit text cleanly in a single call
        if text_to_commit:
            self.commit_text(IBus.Text.new_from_string(text_to_commit))

    def do_set_content_type(self, purpose, hints):
        # Workstream 2: Auto-detect password / PIN / sensitive fields
        is_pwd = False
        try:
            # IBus.InputPurpose.PASSWORD = 2, PIN = 3
            if hasattr(IBus, 'InputPurpose'):
                if purpose in (IBus.InputPurpose.PASSWORD, IBus.InputPurpose.PIN):
                    is_pwd = True
            if hasattr(IBus, 'InputHints'):
                if hints & IBus.InputHints.PRIVATE:
                    is_pwd = True
        except Exception:
            pass

        self.is_secure_mode = is_pwd
        if self.engine_handle:
            abk_lib.abk_engine_set_secure_mode(self.engine_handle, 1 if is_pwd else 0)

        if is_pwd:
            log_event("SECURE_FIELD_DETECTED", "Password / sensitive field active - auto bypass enabled")
            self.buffer = ""
            self.current_primary = ""
            self.current_candidates = []
            self.hide_lookup_table()
            self.hide_preedit_text()

    def do_process_key_event(self, keyval, keycode, state):
        # Workstream 2: If inside a password or sensitive field, auto-bypass completely
        if self.is_secure_mode:
            return False

        # Ignore key release
        if state & IBus.ModifierType.RELEASE_MASK:
            return False

        # Ignore Ctrl/Alt/Super shortcuts (pass through to OS)
        if state & (IBus.ModifierType.CONTROL_MASK | IBus.ModifierType.MOD1_MASK | IBus.ModifierType.SUPER_MASK):
            return False

        # Workstream 1: Navigation with Arrow Down / Key_KP_Down / Tab
        if keyval in (IBus.KEY_Down, IBus.KEY_KP_Down, IBus.KEY_Tab):
            if self.buffer and self.current_candidates:
                abk_lib.abk_suggestion_select_next(self.engine_handle)
                self.lookup_table.cursor_down()
                sel_idx = self.lookup_table.get_cursor_pos()
                log_event("KEY_CONSUMED", f"Arrow Down/Tab -> selected index: {sel_idx} ('{self.current_candidates[sel_idx]}')")
                self.update_lookup_table(self.lookup_table, True)
                self.update_preedit_display(self.current_candidates[sel_idx])
                return True
            log_event("KEY_IGNORED", f"Arrow Down/Tab ignored (buffer={len(self.buffer)})")
            return False

        # Workstream 1: Navigation with Arrow Up / Key_KP_Up / Shift+Tab
        if keyval in (IBus.KEY_Up, IBus.KEY_KP_Up, IBus.KEY_ISO_Left_Tab):
            if self.buffer and self.current_candidates:
                abk_lib.abk_suggestion_select_prev(self.engine_handle)
                self.lookup_table.cursor_up()
                sel_idx = self.lookup_table.get_cursor_pos()
                log_event("KEY_CONSUMED", f"Arrow Up/Shift-Tab -> selected index: {sel_idx} ('{self.current_candidates[sel_idx]}')")
                self.update_lookup_table(self.lookup_table, True)
                self.update_preedit_display(self.current_candidates[sel_idx])
                return True
            log_event("KEY_IGNORED", f"Arrow Up/Shift-Tab ignored (buffer={len(self.buffer)})")
            return False

        # Page Up / Page Down
        if keyval in (IBus.KEY_Page_Up, IBus.KEY_KP_Page_Up):
            if self.buffer and self.current_candidates:
                self.lookup_table.page_up()
                sel_idx = self.lookup_table.get_cursor_pos()
                abk_lib.abk_suggestion_select_index(self.engine_handle, sel_idx)
                self.update_lookup_table(self.lookup_table, True)
                self.update_preedit_display(self.current_candidates[sel_idx])
                return True
        if keyval in (IBus.KEY_Page_Down, IBus.KEY_KP_Page_Down):
            if self.buffer and self.current_candidates:
                self.lookup_table.page_down()
                sel_idx = self.lookup_table.get_cursor_pos()
                abk_lib.abk_suggestion_select_index(self.engine_handle, sel_idx)
                self.update_lookup_table(self.lookup_table, True)
                self.update_preedit_display(self.current_candidates[sel_idx])
                return True

        # 1. Space: commit candidate with trailing space
        if keyval == IBus.KEY_space:
            if self.buffer:
                sel_idx = self.lookup_table.get_cursor_pos()
                target = self.current_candidates[sel_idx] if (0 <= sel_idx < len(self.current_candidates)) else (self.current_primary or self.buffer)
                self.commit_word(target, append_space=True)
                return True
            return False

        # 2. Enter / Return: commit candidate without extra space
        if keyval in (IBus.KEY_Return, IBus.KEY_KP_Enter):
            if self.buffer:
                sel_idx = self.lookup_table.get_cursor_pos()
                target = self.current_candidates[sel_idx] if (0 <= sel_idx < len(self.current_candidates)) else (self.current_primary or self.buffer)
                self.commit_word(target, append_space=False)
                return True
            return False

        # 3. Backspace
        if keyval == IBus.KEY_BackSpace:
            if self.buffer:
                self.buffer = self.buffer[:-1]
                self.update_preedit_and_lookup()
                return True
            elif self.engine_handle and abk_lib.abk_engine_can_undo_commit(self.engine_handle):
                comm_buf = ctypes.create_string_buffer(512)
                raw_buf = ctypes.create_string_buffer(512)
                ret = abk_lib.abk_engine_get_undo_info(self.engine_handle, comm_buf, 512, raw_buf, 512)
                if ret:
                    comm_str = comm_buf.value.decode('utf-8')
                    raw_str = raw_buf.value.decode('utf-8')
                    log_event("UNDO_COMMIT", f"Reverting committed '{comm_str}' to raw '{raw_str}'")
                    try:
                        self.delete_surrounding_text(-len(comm_str), len(comm_str))
                    except Exception as e:
                        log_event("UNDO_ERROR", f"delete_surrounding_text failed: {e}")
                    self.buffer = raw_str
                    self.update_preedit_and_lookup()
                    return True
            return False

        # 4. Escape: cancel composition
        if keyval == IBus.KEY_Escape:
            if self.buffer:
                self.buffer = ""
                self.current_primary = ""
                self.current_candidates = []
                self.hide_lookup_table()
                self.hide_preedit_text()
                return True
            return False

        # 5. Number selection (1-9) for candidates
        if self.buffer and IBus.KEY_1 <= keyval <= IBus.KEY_9:
            idx = keyval - IBus.KEY_1
            if idx < len(self.current_candidates):
                self.commit_word(self.current_candidates[idx], append_space=True)
                return True

        # 6. Latin characters & special phonetic symbols
        char = chr(keyval) if keyval < 128 else None
        if char and (char.isalnum() or char in "^:`"):
            self.buffer += char
            self.update_preedit_and_lookup()
            return True

        # 7. Punctuation (commit pending word with punctuation)
        if self.buffer and char and char in ".,!?;:-()":
            target = self.current_candidates[0] if self.current_candidates else (self.current_primary or self.buffer)
            self.commit_word(target + char, append_space=False)
            return True

        return False

    def do_focus_out(self):
        if self.buffer:
            target = self.current_candidates[0] if self.current_candidates else (self.current_primary or self.buffer)
            self.commit_word(target, append_space=False)
        if self.engine_handle:
            abk_lib.abk_engine_clear_undo(self.engine_handle)

def main():
    IBus.init()
    bus = IBus.Bus()
    if not bus.is_connected():
        print("Error: Could not connect to IBus daemon.", file=sys.stderr)
        sys.exit(1)

    factory = IBus.Factory.new(bus.get_connection())
    factory.add_engine('amar-bangla-kotha', AmarBanglaKothaEngine)

    if '--ibus' in sys.argv:
        bus.request_name('org.freedesktop.IBus.AmarBanglaKotha', 0)
    else:
        component = IBus.Component.new(
            'org.freedesktop.IBus.AmarBanglaKotha',
            'Amar Bangla Kotha (আমার বাংলা কথা)',
            '1.0.0',
            'GPL',
            'Amar Bangla Kotha Team',
            'https://github.com/sumon/Keyboard',
            sys.argv[0] + ' --ibus',
            'amar-bangla-kotha'
        )
        engine_desc = IBus.EngineDesc.new(
            'amar-bangla-kotha',
            'Amar Bangla Kotha (আমার বাংলা কথা)',
            'Phonetic Bengali Keyboard Engine',
            'bn',
            'GPL',
            'Amar Bangla Kotha Team',
            'input-keyboard',
            'us'
        )
        component.add_engine(engine_desc)
        bus.register_component(component)

    GLib.MainLoop().run()

if __name__ == '__main__':
    main()
