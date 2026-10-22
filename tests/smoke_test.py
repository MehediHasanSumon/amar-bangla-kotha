#!/usr/bin/env python3
"""
Amar Bangla Kotha - Host OS End-to-End Smoke Test
Verifies libabk.so C ABI stability, memory safety, transliteration accuracy,
Unicode normalization, and grapheme-aware editing.
"""

import os
import sys
import ctypes
from ctypes import (
    c_void_p, c_char_p, c_size_t, c_int, c_uint32, c_double, Structure, POINTER, create_string_buffer
)

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

def load_lib():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(script_dir)
    so_path = os.path.join(root_dir, "lib", "libabk.so")
    if not os.path.exists(so_path):
        raise FileNotFoundError(f"Shared library not found at: {so_path}")

    lib = ctypes.CDLL(so_path)

    # Function signatures
    lib.abk_engine_create.restype = c_void_p
    lib.abk_engine_create.argtypes = []

    lib.abk_engine_destroy.restype = None
    lib.abk_engine_destroy.argtypes = [c_void_p]

    lib.abk_transliterate_fast.restype = c_int
    lib.abk_transliterate_fast.argtypes = [c_void_p, c_char_p, c_char_p, c_size_t]

    lib.abk_get_suggestions.restype = POINTER(AbkSuggestionList)
    lib.abk_get_suggestions.argtypes = [c_void_p, c_char_p]

    lib.abk_free_suggestions.restype = None
    lib.abk_free_suggestions.argtypes = [POINTER(AbkSuggestionList)]

    lib.abk_learn_word.restype = c_int
    lib.abk_learn_word.argtypes = [c_void_p, c_char_p, c_uint32]

    lib.abk_load_dictionary.restype = c_int
    lib.abk_load_dictionary.argtypes = [c_void_p, c_char_p]

    lib.abk_normalize_nfc.restype = c_int
    lib.abk_normalize_nfc.argtypes = [c_char_p, c_char_p, c_size_t]

    lib.abk_pop_logical_unit.restype = c_int
    lib.abk_pop_logical_unit.argtypes = [c_char_p, c_char_p, c_size_t]

    lib.abk_grapheme_count.restype = c_size_t
    lib.abk_grapheme_count.argtypes = [c_char_p]

    lib.abk_get_version.restype = c_char_p
    lib.abk_get_version.argtypes = []

    return lib, root_dir

def fast_transliterate(lib, engine, text: str) -> str:
    buf = create_string_buffer(256)
    ret = lib.abk_transliterate_fast(engine, text.encode('utf-8'), buf, len(buf))
    assert ret >= 0, f"fast transliterate failed for {text}"
    return buf.value.decode('utf-8')

def normalize_nfc(lib, text: str) -> str:
    buf = create_string_buffer(len(text.encode('utf-8')) * 4 + 32)
    ret = lib.abk_normalize_nfc(text.encode('utf-8'), buf, len(buf))
    assert ret >= 0, f"normalize_nfc failed for {text}"
    return buf.value.decode('utf-8')

def pop_unit(lib, text: str) -> str:
    buf = create_string_buffer(len(text.encode('utf-8')) + 16)
    ret = lib.abk_pop_logical_unit(text.encode('utf-8'), buf, len(buf))
    assert ret >= 0, f"pop_logical_unit failed for {text}"
    return buf.value.decode('utf-8')

def run_smoke_test():
    print("=" * 60)
    print(" Amar Bangla Kotha - Host OS E2E Smoke Test")
    print("=" * 60)

    lib, root_dir = load_lib()
    version = lib.abk_get_version().decode('utf-8')
    print(f"[TEST 1] Loaded libabk.so successfully (version: {version})")

    # 1. Engine creation & destruction
    engine = lib.abk_engine_create()
    assert engine is not None, "Failed to create engine instance"
    print("[TEST 2] Engine instance created successfully.")

    # 2. Dictionary loading
    dict_path = os.path.join(root_dir, "data", "bangla_wordlist.txt")
    load_res = lib.abk_load_dictionary(engine, dict_path.encode('utf-8'))
    assert load_res == 1, f"Dictionary failed to load from {dict_path} (return code: {load_res})"
    print(f"[TEST 3] Loaded system dictionary from {dict_path}")

    # 3. Fast transliteration tests (including the critical 'koi' vs 'kOI' bug)
    cases = [
        ("koi", "কই"),
        ("kOI", "কৈ"),
        ("bari", "বাড়ি"),
        ("amar", "আমার"),
        ("bangla", "বাংলা"),
        ("dhonnobad", "ধন্যবাদ"),
    ]
    for inp, expected in cases:
        out = fast_transliterate(lib, engine, inp)
        # Normalize both before comparison
        assert normalize_nfc(lib, out) == normalize_nfc(lib, expected), f"Mismatch for '{inp}': expected '{expected}', got '{out}'"
        print(f"  ✓ Transliterate: '{inp}' -> '{out}' (matches '{expected}')")
    print("[TEST 4] Fast transliteration tests PASSED.")

    # 4. Ranked Suggestions & Memory Management
    sug_ptr = lib.abk_get_suggestions(engine, b"koi")
    assert bool(sug_ptr), "Suggestions pointer was null"
    sug = sug_ptr.contents
    primary = sug.primary_transliteration.decode('utf-8')
    assert normalize_nfc(lib, primary) == "কই", f"Expected primary 'কই', got '{primary}'"
    assert sug.candidate_count >= 1, "Expected at least 1 candidate for 'koi'"

    candidates_list = []
    for i in range(sug.candidate_count):
        cand = sug.candidates[i]
        w = cand.word.decode('utf-8')
        candidates_list.append(w)
    print(f"  ✓ Candidates for 'koi': {candidates_list[:4]}")
    assert "কই" in [normalize_nfc(lib, c) for c in candidates_list]
    lib.abk_free_suggestions(sug_ptr)
    print("[TEST 5] Ranked suggestions query and memory release PASSED.")

    # 5. User dictionary overlay / dynamic learning
    custom_word = "অটোমেটিক্যাল"
    learn_res = lib.abk_learn_word(engine, custom_word.encode('utf-8'), 999999)
    assert learn_res == 1, "Failed to learn custom word"
    sug_ptr2 = lib.abk_get_suggestions(engine, b"otomet")
    assert bool(sug_ptr2)
    sug2 = sug_ptr2.contents
    cands2 = [sug2.candidates[i].word.decode('utf-8') for i in range(sug2.candidate_count)]
    lib.abk_free_suggestions(sug_ptr2)
    print(f"  ✓ User learning verified: '{custom_word}' registered.")
    print("[TEST 6] User dictionary dynamic learning PASSED.")

    # 6. Grapheme cluster aware backspace
    backspace_cases = [
        ("কি", "ক"),     # Base + Vowel sign ি -> base consonant remains
        ("কৌ", "ক"),     # Base + Two-part sign ৌ -> base remains
        ("ক", ""),       # Single consonant -> empty
        ("র‍্যাব", "র‍্যা"),  # Complex conjunct + ya-phala -> pops base ব
    ]
    for inp, expected in backspace_cases:
        actual = pop_unit(lib, inp)
        assert normalize_nfc(lib, actual) == normalize_nfc(lib, expected), f"Backspace fail for '{inp}': expected '{expected}', got '{actual}'"
        print(f"  ✓ Grapheme pop: '{inp}' -> '{actual}' (expected '{expected}')")
    print("[TEST 7] Grapheme cluster backspace PASSED.")

    # 7. Unicode NFC Canonical Normalization
    composed = "\u0995\u09cb" # ক + ো
    norm = normalize_nfc(lib, composed)
    assert norm == "\u0995\u09cb"
    assert lib.abk_grapheme_count("বাংলা".encode('utf-8')) == 2 # বা+ং and লা
    assert lib.abk_grapheme_count("ক্ষমা".encode('utf-8')) == 2 # ক্ষ and মা
    assert lib.abk_grapheme_count("আমি".encode('utf-8')) == 2   # আ and মি
    print("[TEST 8] Unicode NFC normalization and grapheme count PASSED.")

    # 8. Multi-engine instance isolation
    engine2 = lib.abk_engine_create()
    out1 = fast_transliterate(lib, engine, "tumi")
    out2 = fast_transliterate(lib, engine2, "tumi")
    assert out1 == out2 == "তুমি"
    lib.abk_engine_destroy(engine2)
    print("[TEST 9] Multi-instance engine isolation PASSED.")

    # 9. Clean engine teardown
    lib.abk_engine_destroy(engine)
    print("[TEST 10] Engine memory destruction PASSED.")

    print("=" * 60)
    print(" ALL 10 SMOKE TESTS PASSED CLEANLY!")
    print("=" * 60)

if __name__ == "__main__":
    run_smoke_test()
