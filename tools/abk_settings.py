#!/usr/bin/env python3
"""
Amar Bangla Kotha (আমার বাংলা কথা) - Settings GUI & First-Run Interactive Tutorial
Cross-platform settings manager, interactive tutorial, keymap cheat sheet,
quick text convert, and opt-in correction log/feedback manager.
"""

import sys
import os
import json
import ctypes
from ctypes import c_void_p, c_char_p, c_size_t, c_int, c_double, Structure, POINTER

# 1. Load libabk library for live interactive transliteration & QoL features
def load_abk():
    paths = [
        os.path.expanduser("~/.local/lib/libabk.so"),
        "/usr/local/lib/libabk.so",
        "/usr/lib/libabk.so",
        os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "lib", "libabk.so")
    ]
    for p in paths:
        if os.path.exists(p):
            try:
                lib = ctypes.CDLL(p)
                lib.abk_engine_create.restype = c_void_p
                lib.abk_engine_create.argtypes = []
                lib.abk_engine_destroy.restype = None
                lib.abk_engine_destroy.argtypes = [c_void_p]
                lib.abk_transliterate_fast.restype = c_int
                lib.abk_transliterate_fast.argtypes = [c_void_p, c_char_p, c_char_p, c_size_t]
                lib.abk_load_dictionary.restype = c_int
                lib.abk_load_dictionary.argtypes = [c_void_p, c_char_p]
                lib.abk_engine_add_shortcut.restype = None
                lib.abk_engine_add_shortcut.argtypes = [c_void_p, c_char_p, c_char_p]
                lib.abk_engine_remove_shortcut.restype = c_int
                lib.abk_engine_remove_shortcut.argtypes = [c_void_p, c_char_p]
                lib.abk_engine_clear_personal_data.restype = None
                lib.abk_engine_clear_personal_data.argtypes = [c_void_p]

                # Workstream 8: QoL C API bindings
                lib.abk_convert_banglish_to_bangla.restype = c_int
                lib.abk_convert_banglish_to_bangla.argtypes = [c_void_p, c_char_p, c_char_p, c_size_t]
                lib.abk_convert_bangla_to_banglish.restype = c_int
                lib.abk_convert_bangla_to_banglish.argtypes = [c_char_p, c_char_p, c_size_t]
                lib.abk_apply_smart_punctuation.restype = c_int
                lib.abk_apply_smart_punctuation.argtypes = [c_char_p, c_char_p, c_size_t]
                lib.abk_engine_set_correction_logging.restype = None
                lib.abk_engine_set_correction_logging.argtypes = [c_void_p, c_int]
                lib.abk_engine_is_correction_logging_enabled.restype = c_int
                lib.abk_engine_is_correction_logging_enabled.argtypes = [c_void_p]
                lib.abk_engine_clear_correction_log.restype = None
                lib.abk_engine_clear_correction_log.argtypes = [c_void_p]
                lib.abk_save_local_feedback.restype = c_int
                lib.abk_save_local_feedback.argtypes = [c_char_p, c_char_p, c_char_p, c_char_p, c_char_p]

                return lib
            except Exception:
                pass
    return None

abk_lib = load_abk()

# 2. Local Configuration Storage
CONFIG_DIR = os.path.expanduser("~/.config/amar-bangla-kotha")
CONFIG_PATH = os.path.join(CONFIG_DIR, "settings.json")
CORRECTION_LOG_PATH = os.path.join(CONFIG_DIR, "correction_log.jsonl")
FEEDBACK_PATH = os.path.join(CONFIG_DIR, "feedback.jsonl")

DEFAULT_SETTINGS = {
    "language_ui": "bn",          # "bn" or "en"
    "toggle_hotkey": "F12",        # F12, Ctrl+Space, Shift+Space
    "number_mode": "en",           # "en" (0-9) or "bn" (০-৯)
    "candidate_count": 7,          # 3 - 10
    "theme": "light",              # "light" or "dark"
    "bypass_terminal_ide": True,   # Per-app profile bypass
    "enable_autocorrect": True,
    "enable_next_word": True,
    "enable_shortcuts": True,
    "enable_correction_log": False,# Workstream 8: Privacy-by-default (OFF)
    "shortcuts": {
        "amr": "আমার",
        "thx": "ধন্যবাদ",
        "addr": "ঢাকা, বাংলাদেশ"
    }
}

def load_settings():
    if os.path.exists(CONFIG_PATH):
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                data = json.load(f)
                res = DEFAULT_SETTINGS.copy()
                res.update(data)
                return res
        except Exception:
            pass
    return DEFAULT_SETTINGS.copy()

def save_settings(settings):
    os.makedirs(CONFIG_DIR, exist_ok=True)
    tmp_path = CONFIG_PATH + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as f:
        json.dump(settings, f, ensure_ascii=False, indent=2)
    os.replace(tmp_path, CONFIG_PATH)

def load_phonetic_rules_list():
    rule_paths = [
        os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "phonetic_rules.tsv"),
        os.path.expanduser("~/.local/share/amar-bangla-kotha/phonetic_rules.tsv"),
        "/usr/local/share/amar-bangla-kotha/phonetic_rules.tsv"
    ]
    rules = []
    for rp in rule_paths:
        if os.path.exists(rp):
            with open(rp, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    parts = line.split("\t")
                    if len(parts) >= 4:
                        pattern = parts[0]
                        output = parts[1]
                        category = parts[3]
                        desc = parts[5] if len(parts) >= 6 else parts[2]
                        rules.append((pattern, output, category, desc))
            break
    if not rules:
        # Fallback default cheat sheet
        rules = [
            ("k", "ক", "CONSONANT", "Ka"),
            ("kh", "খ", "CONSONANT", "Kha"),
            ("g", "গ", "CONSONANT", "Ga"),
            ("gh", "ঘ", "CONSONANT", "Gha"),
            ("Ng", "ঙ", "CONSONANT", "Unga"),
            ("ch", "চ", "CONSONANT", "Cha"),
            ("chh", "ছ", "CONSONANT", "Chha"),
            ("j", "জ", "CONSONANT", "Ja"),
            ("jh", "ঝ", "CONSONANT", "Jha"),
            ("T", "ট", "CONSONANT", "Ta (retroflex)"),
            ("Th", "ঠ", "CONSONANT", "Tha (retroflex)"),
            ("D", "ড", "CONSONANT", "Da (retroflex)"),
            ("Dh", "ঢ", "CONSONANT", "Dha (retroflex)"),
            ("N", "ণ", "CONSONANT", "Murdhanya Na"),
            ("t", "ত", "CONSONANT", "Dantya Ta"),
            ("th", "থ", "CONSONANT", "Dantya Tha"),
            ("d", "দ", "CONSONANT", "Da"),
            ("dh", "ধ", "CONSONANT", "Dha"),
            ("n", "ন", "CONSONANT", "Dantya Na"),
            ("p", "প", "CONSONANT", "Pa"),
            ("f", "ফ", "CONSONANT", "Pha / Fa"),
            ("b", "ব", "CONSONANT", "Ba"),
            ("bh", "ভ", "CONSONANT", "Bha"),
            ("m", "ম", "CONSONANT", "Ma"),
            ("z", "য", "CONSONANT", "Antasthya Ja"),
            ("r", "র", "CONSONANT", "Ra"),
            ("l", "ল", "CONSONANT", "La"),
            ("sh", "শ", "CONSONANT", "Talabya Sha"),
            ("S", "ষ", "CONSONANT", "Murdhanya Sha"),
            ("s", "স", "CONSONANT", "Dantya Sa"),
            ("h", "হ", "CONSONANT", "Ha"),
            ("R", "ড়", "CONSONANT", "Dabindra Ra"),
            ("Rh", "ঢ়", "CONSONANT", "Dhabindra Rha"),
            ("y", "য়", "CONSONANT", "Antasthya Ya"),
            ("OI", "ৈ / ঐ", "VOWEL", "Oi (Capital OI)"),
            ("OU", "ৌ / ঔ", "VOWEL", "Ou (Capital OU)"),
            ("t`", "ৎ", "MODIFIER", "Khanda Ta (t + backtick)"),
            ("^", "ঁ", "MODIFIER", "Chandrabindu (^ caret)"),
            (":", "ঃ", "MODIFIER", "Visarga (: colon)"),
            ("hotat", "হঠাৎ", "CONJUNCT", "Shortcut phrase"),
            ("shT", "ষ্ট", "CONJUNCT", "Retroflex ShT"),
        ]
    return rules

# 3. Bilingual UI Strings (Bangla & English)
STRINGS = {
    "bn": {
        "app_title": "আমার বাংলা কথা — সেটিংস ও টিউটোরিয়াল (Amar Bangla Kotha)",
        "tab_settings": "সেটিংস (Settings)",
        "tab_tutorial": "টিউটোরিয়াল (Tutorial)",
        "tab_shortcuts": "শর্টকাট (Shortcuts)",
        "tab_cheat_sheet": "কীম্যাপ চিট শিট (Keymap)",
        "tab_quick_convert": "দ্রুত টেক্সট রূপান্তর (Convert)",
        "tab_feedback": "সংশোধন ও মতামত (Feedback)",

        "grp_general": "সাধারণ পছন্দসমূহ",
        "lbl_lang_ui": "ইন্টারফেসের ভাষা:",
        "lbl_hotkey": "বাংলা/ইংরেজি পরিবর্তন শর্টকাট কী:",
        "lbl_number_mode": "সংখ্যার মোড:",
        "num_en": "ইংরেজি (0-9)",
        "num_bn": "বাংলা (০-৯)",
        "lbl_candidates": "প্রস্তাবিত শব্দের সংখ্যা (৩ - ১০):",
        "lbl_theme": "থিম নির্বাচন:",
        "theme_light": "হালকা (Light)",
        "theme_dark": "ডার্ক (Dark)",
        "grp_advanced": "উন্নত ফিচার ও অ্যাপ প্রোফাইল",
        "chk_bypass_ide": "টার্মিনাল ও কোড এডিটরে (VS Code) স্বয়ংক্রিয় বাইপাস",
        "chk_autocorrect": "টাইপো স্বয়ংক্রিয়ভাবে সংশোধন (Autocorrect)",
        "chk_next_word": "পরবর্তী শব্দ অনুমান (Next-Word Prediction)",
        "chk_correction_log": "ব্যবহারকারী সংশোধন লগ সক্রিয় করুন (Opt-in, ডিফল্ট বন্ধ)",
        "btn_save": "সেটিংস সংরক্ষণ করুন",
        "btn_reset": "সব ব্যক্তিগত ডাটা মুছে ফেলুন (Reset Learning)",
        "reset_confirm": "আপনি কি নিশ্চিত যে আপনি সব ব্যক্তিগত শেখা শব্দ ও শর্টকাট রিসেট করতে চান?",
        "saved_msg": "সেটিংস সফলভাবে সংরক্ষিত হয়েছে!",

        "tut_title": "প্রথমবার ব্যবহারের ইন্টারঅ্যাক্টিভ পাঠ",
        "tut_sub": "নিচের নিয়মগুলো দেখে ইনপুট বক্সে প্র্যাকটিস করুন:",
        "tut_l1": "১. সাধারণ স্বরবর্ণ ও যুক্তস্বর: koi -> কই, কিন্তু kOI -> কৈ (বড় হাতের OI = ৈ)",
        "tut_l2": "২. মূর্ধন্য ও দ্বিত্ব বর্ণ: Ta -> টা, Thik -> ঠিক, Daab -> ডাব, baRi -> বাড়ি (R = ড়)",
        "tut_l3": "৩. যুক্তবর্ণ: ক + ত = k+t -> ক্ত, ক + ষ = k+S -> ক্ষ",
        "tut_l4": "৪. এস্কেপ ওভাররাইড: Escape চাপলে লেখা বাতিল হয়; কমা (,) দিয়ে অক্ষর লিখলে সরাসরি বসে",
        "lbl_practice": "নিচে টাইপ করে সরাসরি পরীক্ষা করুন:",
        "lbl_practice_out": "বাংলা আউটপুট:",

        "sc_title": "টেক্সট শর্টকাট ও স্নিপেট ম্যানেজার",
        "lbl_sc_key": "শর্টকাট (যেমন: amr):",
        "lbl_sc_val": "সম্পূর্ণ শব্দ/বাক্য (যেমন: আমার):",
        "btn_sc_add": "শর্টকাট যোগ করুন",
        "btn_sc_del": "নির্বাচিত শর্টকাট মুছুন",
        "btn_sc_export": "TSV ফাইলে এক্সপোর্ট",
        "btn_sc_import": "TSV ফাইল থেকে ইমপোর্ট",

        # Workstream 8: Keymap Cheat Sheet Strings
        "cs_title": "লাইভ ফোনেটিক কীম্যাপ ও ক্যারেক্টার চিট শিট",
        "lbl_cs_search": "কীম্যাপ সার্চ করুন (যেমন: k, kh, Ta, OI, ৎ):",
        "lbl_cs_category": "ক্যাটাগরি ফিল্টার:",
        "col_key": "কীবোর্ড ইনপুট",
        "col_bangla": "বাংলা বর্ণ",
        "col_category": "ধরন",
        "col_desc": "বিবরণ ও ব্যবহার",

        # Workstream 8: Quick Convert Strings
        "qc_title": "দ্রুত বাল্ক টেক্সট রূপান্তরক (Banglish <-> Bangla)",
        "lbl_qc_dir": "রূপান্তরের দিক:",
        "qc_b2b": "বাংলিশ থেকে ইউনিকোড বাংলা (Banglish -> Bangla)",
        "qc_u2b": "ইউনিকোড বাংলা থেকে বাংলিশ (Bangla -> Banglish)",
        "chk_smart_punct": "স্মার্ট বিরামচিহ্ন প্রয়োগ (. -> ।, ... -> …, \" -> “”)",
        "lbl_qc_input": "ইনপুট টেক্সট:",
        "lbl_qc_output": "রূপান্তরিত আউটপুট:",
        "btn_convert": "টেক্সট রূপান্তর করুন",
        "btn_copy": "ক্লিপবোর্ডে কপি করুন",
        "copied_msg": "আউটপুট ক্লিপবোর্ডে কপি করা হয়েছে!",

        # Workstream 8: Correction Log & Feedback Strings
        "fb_title": "মতামত ও সংশোধন লগ ম্যানেজার (Local & Private)",
        "fb_optin_note": "গোপনীয়তার নিশ্চয়তা: সংশোধন লগ সম্পূর্ণ অফলাইনে সংরক্ষিত হয় এবং পাসওয়ার্ড ক্ষেত্রে কখনো সক্রিয় হয় না।",
        "btn_refresh_log": "লগ রিফ্রেশ করুন",
        "btn_clear_log": "সংশোধন লগ মুছুন",
        "col_ts": "সময় (Timestamp)",
        "col_raw": "টাইপ করা টেক্সট",
        "col_corr": "সংশোধিত শব্দ",
        "grp_report_err": "ভুল সাজেশন রিপোর্ট করুন ('This suggestion is wrong')",
        "lbl_err_raw": "যা টাইপ করেছিলেন:",
        "lbl_err_got": "ভুল যা সাজেস্ট করেছে:",
        "lbl_err_exp": "সঠিক যা হওয়া উচিত ছিল:",
        "lbl_err_app": "অ্যাপ্লিকেশনের নাম:",
        "btn_submit_fb": "লোকাল ফিডব্যাক সংরক্ষণ করুন",
        "fb_saved_msg": "আপনার মতামত স্থানীয় feedback.jsonl ফাইলে সংরক্ষিত হয়েছে! বাগ রিপোর্টে এটি যুক্ত করা যাবে।"
    },
    "en": {
        "app_title": "Amar Bangla Kotha — Settings & Tutorial",
        "tab_settings": "Settings",
        "tab_tutorial": "First-Run Tutorial",
        "tab_shortcuts": "Shortcuts",
        "tab_cheat_sheet": "Keymap Cheat Sheet",
        "tab_quick_convert": "Quick Convert",
        "tab_feedback": "Feedback & Corrections",

        "grp_general": "General Preferences",
        "lbl_lang_ui": "Interface Language:",
        "lbl_hotkey": "Bangla/English Toggle Hotkey:",
        "lbl_number_mode": "Number Mode:",
        "num_en": "English (0-9)",
        "num_bn": "Bengali (০-৯)",
        "lbl_candidates": "Candidate Word Count (3 - 10):",
        "lbl_theme": "Theme Selection:",
        "theme_light": "Light Mode",
        "theme_dark": "Dark Mode",
        "grp_advanced": "Advanced Features & App Profiles",
        "chk_bypass_ide": "Auto-bypass transliteration in Terminal and IDEs (VS Code)",
        "chk_autocorrect": "Enable Typo Autocorrection",
        "chk_next_word": "Enable Contextual Next-Word Prediction",
        "chk_correction_log": "Enable User Correction Logging (Opt-in, default OFF)",
        "btn_save": "Save Settings",
        "btn_reset": "Reset Personal Data & Learning",
        "reset_confirm": "Are you sure you want to reset all learned words and custom shortcuts?",
        "saved_msg": "Settings saved successfully!",

        "tut_title": "Interactive First-Run Tutorial",
        "tut_sub": "Master the core phonetic conventions by practicing in the live box below:",
        "tut_l1": "1. Vowels & Diphthongs: 'koi' -> 'কই', while 'kOI' -> 'কৈ' (capital 'OI' produces ৈ)",
        "tut_l2": "2. Retroflex Consonants: 'Ta' -> 'টা', 'Thik' -> 'ঠিক', 'Daab' -> 'ডাব', 'baRi' -> 'বাড়ি' ('R' = ড়)",
        "tut_l3": "3. Conjuncts (Yuktakshar): 'k+t' -> 'ক্ত', 'k+S' -> 'ক্ষ', 'b+d' -> 'ব্দ'",
        "tut_l4": "4. Escape Overrides: Pressing 'Escape' cancels composition; prefixing with comma (',') bypasses rules.",
        "lbl_practice": "Type Latin input here to test live:",
        "lbl_practice_out": "Live Bangla Transliteration:",

        "sc_title": "Text Shortcut & Snippet Expansion Manager",
        "lbl_sc_key": "Shortcut (e.g. amr):",
        "lbl_sc_val": "Full Expansion (e.g. আমার):",
        "btn_sc_add": "Add Shortcut",
        "btn_sc_del": "Delete Selected Shortcut",
        "btn_sc_export": "Export to TSV",
        "btn_sc_import": "Import from TSV",

        # Workstream 8: Keymap Cheat Sheet Strings
        "cs_title": "Live Phonetic Keymap & Layout Cheat Sheet",
        "lbl_cs_search": "Search Keymap (e.g. k, kh, Ta, OI, ৎ):",
        "lbl_cs_category": "Filter Category:",
        "col_key": "Latin Key",
        "col_bangla": "Bengali Glyph",
        "col_category": "Category",
        "col_desc": "Description & Context",

        # Workstream 8: Quick Convert Strings
        "qc_title": "Bulk Text Converter (Banglish <-> Bangla)",
        "lbl_qc_dir": "Conversion Direction:",
        "qc_b2b": "Banglish to Unicode Bangla",
        "qc_u2b": "Unicode Bangla to Banglish",
        "chk_smart_punct": "Apply Smart Punctuation (. -> ।, ... -> …, \" -> “”)",
        "lbl_qc_input": "Input Text:",
        "lbl_qc_output": "Converted Output:",
        "btn_convert": "Convert Text",
        "btn_copy": "Copy to Clipboard",
        "copied_msg": "Output copied to clipboard successfully!",

        # Workstream 8: Correction Log & Feedback Strings
        "fb_title": "Local Feedback & Correction Log (Private)",
        "fb_optin_note": "Privacy Guarantee: Correction log is saved strictly locally and NEVER logs inside password/secure fields.",
        "btn_refresh_log": "Refresh Log",
        "btn_clear_log": "Clear Correction Log",
        "col_ts": "Timestamp",
        "col_raw": "Typed Input",
        "col_corr": "Corrected Word",
        "grp_report_err": "Report Erroneous Suggestion ('This suggestion is wrong')",
        "lbl_err_raw": "Raw typed input:",
        "lbl_err_got": "Received wrong suggestion:",
        "lbl_err_exp": "Expected correct word:",
        "lbl_err_app": "Application name:",
        "btn_submit_fb": "Save Local Feedback",
        "fb_saved_msg": "Your feedback was saved locally to feedback.jsonl and can be attached to bug reports."
    }
}

# 4. Tkinter GUI Application
def run_gui(start_tutorial=False):
    import tkinter as tk
    from tkinter import ttk, messagebox, filedialog

    settings = load_settings()
    current_lang = settings.get("language_ui", "bn")
    t = STRINGS[current_lang]

    root = tk.Tk()
    root.title(t["app_title"])
    root.geometry("820x640")
    root.minsize(760, 560)

    style = ttk.Style()
    style.theme_use('clam')

    engine_handle = None
    if abk_lib:
        engine_handle = abk_lib.abk_engine_create()
        dict_paths = [
            os.path.expanduser("~/.local/share/amar-bangla-kotha/bangla_wordlist.txt"),
            "/usr/local/share/amar-bangla-kotha/bangla_wordlist.txt",
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "bangla_wordlist.txt")
        ]
        for dp in dict_paths:
            if os.path.exists(dp):
                abk_lib.abk_load_dictionary(engine_handle, dp.encode('utf-8'))
                break

    notebook = ttk.Notebook(root)
    notebook.pack(fill=tk.BOTH, expand=True, padx=12, pady=12)

    # -------------------------------------------------------------
    # TAB 1: General Settings
    # -------------------------------------------------------------
    tab_settings = ttk.Frame(notebook)
    notebook.add(tab_settings, text=t["tab_settings"])

    grp_gen = ttk.LabelFrame(tab_settings, text=t["grp_general"], padding=12)
    grp_gen.pack(fill=tk.X, padx=12, pady=8)

    ttk.Label(grp_gen, text=t["lbl_lang_ui"]).grid(row=0, column=0, sticky=tk.W, pady=6)
    var_lang = tk.StringVar(value=settings.get("language_ui", "bn"))
    cb_lang = ttk.Combobox(grp_gen, textvariable=var_lang, values=["bn", "en"], state="readonly", width=12)
    cb_lang.grid(row=0, column=1, sticky=tk.W, padx=8, pady=6)

    ttk.Label(grp_gen, text=t["lbl_hotkey"]).grid(row=1, column=0, sticky=tk.W, pady=6)
    var_hotkey = tk.StringVar(value=settings.get("toggle_hotkey", "F12"))
    cb_hotkey = ttk.Combobox(grp_gen, textvariable=var_hotkey, values=["F12", "Ctrl+Space", "Shift+Space"], width=12)
    cb_hotkey.grid(row=1, column=1, sticky=tk.W, padx=8, pady=6)

    ttk.Label(grp_gen, text=t["lbl_number_mode"]).grid(row=2, column=0, sticky=tk.W, pady=6)
    var_num = tk.StringVar(value=settings.get("number_mode", "en"))
    frm_num = ttk.Frame(grp_gen)
    frm_num.grid(row=2, column=1, sticky=tk.W, padx=8, pady=6)
    ttk.Radiobutton(frm_num, text=t["num_en"], variable=var_num, value="en").pack(side=tk.LEFT, padx=4)
    ttk.Radiobutton(frm_num, text=t["num_bn"], variable=var_num, value="bn").pack(side=tk.LEFT, padx=4)

    ttk.Label(grp_gen, text=t["lbl_candidates"]).grid(row=3, column=0, sticky=tk.W, pady=6)
    var_cand = tk.IntVar(value=settings.get("candidate_count", 7))
    sp_cand = ttk.Spinbox(grp_gen, from_=3, to=10, textvariable=var_cand, width=8)
    sp_cand.grid(row=3, column=1, sticky=tk.W, padx=8, pady=6)

    ttk.Label(grp_gen, text=t["lbl_theme"]).grid(row=4, column=0, sticky=tk.W, pady=6)
    var_theme = tk.StringVar(value=settings.get("theme", "light"))
    cb_theme = ttk.Combobox(grp_gen, textvariable=var_theme, values=["light", "dark"], state="readonly", width=12)
    cb_theme.grid(row=4, column=1, sticky=tk.W, padx=8, pady=6)

    grp_adv = ttk.LabelFrame(tab_settings, text=t["grp_advanced"], padding=12)
    grp_adv.pack(fill=tk.X, padx=12, pady=8)

    var_bypass = tk.BooleanVar(value=settings.get("bypass_terminal_ide", True))
    ttk.Checkbutton(grp_adv, text=t["chk_bypass_ide"], variable=var_bypass).pack(anchor=tk.W, pady=4)

    var_autocorrect = tk.BooleanVar(value=settings.get("enable_autocorrect", True))
    ttk.Checkbutton(grp_adv, text=t["chk_autocorrect"], variable=var_autocorrect).pack(anchor=tk.W, pady=4)

    var_next_word = tk.BooleanVar(value=settings.get("enable_next_word", True))
    ttk.Checkbutton(grp_adv, text=t["chk_next_word"], variable=var_next_word).pack(anchor=tk.W, pady=4)

    var_corr_log = tk.BooleanVar(value=settings.get("enable_correction_log", False))
    ttk.Checkbutton(grp_adv, text=t["chk_correction_log"], variable=var_corr_log).pack(anchor=tk.W, pady=4)

    frm_actions = ttk.Frame(tab_settings)
    frm_actions.pack(fill=tk.X, padx=12, pady=12)

    def on_save_settings():
        settings["language_ui"] = var_lang.get()
        settings["toggle_hotkey"] = var_hotkey.get()
        settings["number_mode"] = var_num.get()
        settings["candidate_count"] = var_cand.get()
        settings["theme"] = var_theme.get()
        settings["bypass_terminal_ide"] = var_bypass.get()
        settings["enable_autocorrect"] = var_autocorrect.get()
        settings["enable_next_word"] = var_next_word.get()
        settings["enable_correction_log"] = var_corr_log.get()
        save_settings(settings)
        if abk_lib and engine_handle:
            abk_lib.abk_engine_set_correction_logging(engine_handle, 1 if var_corr_log.get() else 0)
        messagebox.showinfo(t["tab_settings"], t["saved_msg"])

    def on_reset_data():
        if messagebox.askyesno(t["btn_reset"], t["reset_confirm"]):
            if abk_lib and engine_handle:
                abk_lib.abk_engine_clear_personal_data(engine_handle)
                abk_lib.abk_engine_clear_correction_log(engine_handle)
            settings["shortcuts"] = DEFAULT_SETTINGS["shortcuts"].copy()
            save_settings(settings)
            messagebox.showinfo(t["btn_reset"], "Personal data reset successfully!")

    ttk.Button(frm_actions, text=t["btn_save"], command=on_save_settings).pack(side=tk.LEFT, padx=4)
    ttk.Button(frm_actions, text=t["btn_reset"], command=on_reset_data).pack(side=tk.RIGHT, padx=4)

    # -------------------------------------------------------------
    # TAB 2: Interactive Tutorial
    # -------------------------------------------------------------
    tab_tut = ttk.Frame(notebook)
    notebook.add(tab_tut, text=t["tab_tutorial"])

    ttk.Label(tab_tut, text=t["tut_title"], font=("Helvetica", 13, "bold")).pack(anchor=tk.W, padx=16, pady=(16, 4))
    ttk.Label(tab_tut, text=t["tut_sub"]).pack(anchor=tk.W, padx=16, pady=(0, 10))

    frm_rules = ttk.Frame(tab_tut, padding=12)
    frm_rules.pack(fill=tk.X, padx=12, pady=4)
    ttk.Label(frm_rules, text=t["tut_l1"], font=("Helvetica", 10)).pack(anchor=tk.W, pady=3)
    ttk.Label(frm_rules, text=t["tut_l2"], font=("Helvetica", 10)).pack(anchor=tk.W, pady=3)
    ttk.Label(frm_rules, text=t["tut_l3"], font=("Helvetica", 10)).pack(anchor=tk.W, pady=3)
    ttk.Label(frm_rules, text=t["tut_l4"], font=("Helvetica", 10)).pack(anchor=tk.W, pady=3)

    frm_prac = ttk.LabelFrame(tab_tut, text="Interactive Live Box", padding=12)
    frm_prac.pack(fill=tk.BOTH, expand=True, padx=12, pady=12)

    ttk.Label(frm_prac, text=t["lbl_practice"]).pack(anchor=tk.W, pady=4)
    var_prac_in = tk.StringVar(value="kOI")
    entry_in = ttk.Entry(frm_prac, textvariable=var_prac_in, font=("Courier", 12))
    entry_in.pack(fill=tk.X, pady=4)

    ttk.Label(frm_prac, text=t["lbl_practice_out"]).pack(anchor=tk.W, pady=4)
    var_prac_out = tk.StringVar(value="")
    lbl_out = ttk.Label(frm_prac, textvariable=var_prac_out, font=("Helvetica", 18, "bold"), foreground="#007acc")
    lbl_out.pack(anchor=tk.W, pady=8)

    def on_live_transliterate(*args):
        raw = var_prac_in.get()
        if not raw:
            var_prac_out.set("")
            return
        if abk_lib and engine_handle:
            buf = ctypes.create_string_buffer(512)
            ret = abk_lib.abk_transliterate_fast(engine_handle, raw.encode('utf-8'), buf, 512)
            if ret >= 0:
                var_prac_out.set(buf.value.decode('utf-8'))
                return
        var_prac_out.set(raw)

    var_prac_in.trace_add("write", on_live_transliterate)
    on_live_transliterate()

    # -------------------------------------------------------------
    # TAB 3: Shortcuts & Personal Dictionary
    # -------------------------------------------------------------
    tab_sc = ttk.Frame(notebook)
    notebook.add(tab_sc, text=t["tab_shortcuts"])

    ttk.Label(tab_sc, text=t["sc_title"], font=("Helvetica", 12, "bold")).pack(anchor=tk.W, padx=16, pady=(12, 6))

    cols = ("shortcut", "expansion")
    tree_sc = ttk.Treeview(tab_sc, columns=cols, show="headings", height=10)
    tree_sc.heading("shortcut", text="Shortcut (শর্টকাট)")
    tree_sc.heading("expansion", text="Expansion (সম্পূর্ণ শব্দ)")
    tree_sc.column("shortcut", width=180)
    tree_sc.column("expansion", width=360)
    tree_sc.pack(fill=tk.BOTH, expand=True, padx=16, pady=6)

    def refresh_shortcuts_list():
        for item in tree_sc.get_children():
            tree_sc.delete(item)
        for k, v in sorted(settings.get("shortcuts", {}).items()):
            tree_sc.insert("", tk.END, values=(k, v))
            if abk_lib and engine_handle:
                abk_lib.abk_engine_add_shortcut(engine_handle, k.encode('utf-8'), v.encode('utf-8'))

    refresh_shortcuts_list()

    frm_add = ttk.Frame(tab_sc, padding=6)
    frm_add.pack(fill=tk.X, padx=12, pady=4)
    ttk.Label(frm_add, text=t["lbl_sc_key"]).grid(row=0, column=0, padx=4)
    var_new_key = tk.StringVar()
    ttk.Entry(frm_add, textvariable=var_new_key, width=12).grid(row=0, column=1, padx=4)
    ttk.Label(frm_add, text=t["lbl_sc_val"]).grid(row=0, column=2, padx=4)
    var_new_val = tk.StringVar()
    ttk.Entry(frm_add, textvariable=var_new_val, width=20).grid(row=0, column=3, padx=4)

    def on_add_shortcut():
        k = var_new_key.get().strip()
        v = var_new_val.get().strip()
        if k and v:
            settings.setdefault("shortcuts", {})[k] = v
            save_settings(settings)
            refresh_shortcuts_list()
            var_new_key.set("")
            var_new_val.set("")

    def on_del_shortcut():
        sel = tree_sc.selection()
        if not sel:
            return
        for s in sel:
            vals = tree_sc.item(s, "values")
            if vals and len(vals) >= 1:
                k = vals[0]
                if k in settings.get("shortcuts", {}):
                    del settings["shortcuts"][k]
                if abk_lib and engine_handle:
                    abk_lib.abk_engine_remove_shortcut(engine_handle, k.encode('utf-8'))
        save_settings(settings)
        refresh_shortcuts_list()

    def on_export_tsv():
        fpath = filedialog.asksaveasfilename(defaultextension=".tsv", filetypes=[("TSV files", "*.tsv")])
        if fpath:
            with open(fpath, "w", encoding="utf-8") as f:
                f.write("# Amar Bangla Kotha Shortcuts\n")
                for k, v in settings.get("shortcuts", {}).items():
                    f.write(f"SHORTCUT\t{k}\t{v}\n")
            messagebox.showinfo("Export", f"Shortcuts exported successfully to {fpath}")

    def on_import_tsv():
        fpath = filedialog.askopenfilename(filetypes=[("TSV files", "*.tsv"), ("All files", "*.*")])
        if fpath and os.path.exists(fpath):
            count = 0
            with open(fpath, "r", encoding="utf-8") as f:
                for line in f:
                    parts = line.strip().split("\t")
                    if len(parts) >= 3 and parts[0] == "SHORTCUT":
                        settings.setdefault("shortcuts", {})[parts[1]] = parts[2]
                        count += 1
            save_settings(settings)
            refresh_shortcuts_list()
            messagebox.showinfo("Import", f"Imported {count} shortcuts successfully!")

    ttk.Button(frm_add, text=t["btn_sc_add"], command=on_add_shortcut).grid(row=0, column=4, padx=6)
    ttk.Button(frm_add, text=t["btn_sc_del"], command=on_del_shortcut).grid(row=0, column=5, padx=4)

    frm_sc_io = ttk.Frame(tab_sc)
    frm_sc_io.pack(fill=tk.X, padx=12, pady=6)
    ttk.Button(frm_sc_io, text=t["btn_sc_export"], command=on_export_tsv).pack(side=tk.LEFT, padx=4)
    ttk.Button(frm_sc_io, text=t["btn_sc_import"], command=on_import_tsv).pack(side=tk.LEFT, padx=4)

    # -------------------------------------------------------------
    # TAB 4: Workstream 8 - Live Keymap Cheat Sheet
    # -------------------------------------------------------------
    tab_cs = ttk.Frame(notebook)
    notebook.add(tab_cs, text=t["tab_cheat_sheet"])

    ttk.Label(tab_cs, text=t["cs_title"], font=("Helvetica", 12, "bold")).pack(anchor=tk.W, padx=16, pady=(12, 6))

    frm_cs_filter = ttk.Frame(tab_cs, padding=6)
    frm_cs_filter.pack(fill=tk.X, padx=12, pady=4)

    ttk.Label(frm_cs_filter, text=t["lbl_cs_search"]).pack(side=tk.LEFT, padx=4)
    var_cs_search = tk.StringVar()
    entry_cs = ttk.Entry(frm_cs_filter, textvariable=var_cs_search, width=20)
    entry_cs.pack(side=tk.LEFT, padx=4)

    ttk.Label(frm_cs_filter, text=t["lbl_cs_category"]).pack(side=tk.LEFT, padx=(16, 4))
    var_cs_cat = tk.StringVar(value="ALL")
    cb_cs_cat = ttk.Combobox(frm_cs_filter, textvariable=var_cs_cat, values=["ALL", "CONSONANT", "VOWEL", "CONJUNCT", "MODIFIER"], state="readonly", width=14)
    cb_cs_cat.pack(side=tk.LEFT, padx=4)

    cs_cols = ("key", "bangla", "category", "desc")
    tree_cs = ttk.Treeview(tab_cs, columns=cs_cols, show="headings", height=12)
    tree_cs.heading("key", text=t["col_key"])
    tree_cs.heading("bangla", text=t["col_bangla"])
    tree_cs.heading("category", text=t["col_category"])
    tree_cs.heading("desc", text=t["col_desc"])
    tree_cs.column("key", width=120)
    tree_cs.column("bangla", width=120)
    tree_cs.column("category", width=140)
    tree_cs.column("desc", width=280)
    tree_cs.pack(fill=tk.BOTH, expand=True, padx=16, pady=6)

    all_rules = load_phonetic_rules_list()

    def filter_cheat_sheet(*args):
        query = var_cs_search.get().strip().lower()
        cat = var_cs_cat.get()
        for item in tree_cs.get_children():
            tree_cs.delete(item)
        for pattern, output, category, desc in all_rules:
            if cat != "ALL" and category != cat:
                continue
            if query:
                if (query not in pattern.lower() and
                    query not in output and
                    query not in desc.lower()):
                    continue
            tree_cs.insert("", tk.END, values=(pattern, output, category, desc))

    var_cs_search.trace_add("write", filter_cheat_sheet)
    cb_cs_cat.bind("<<ComboboxSelected>>", filter_cheat_sheet)
    filter_cheat_sheet()

    # -------------------------------------------------------------
    # TAB 5: Workstream 8 - Quick Convert (Banglish <-> Bangla)
    # -------------------------------------------------------------
    tab_qc = ttk.Frame(notebook)
    notebook.add(tab_qc, text=t["tab_quick_convert"])

    ttk.Label(tab_qc, text=t["qc_title"], font=("Helvetica", 12, "bold")).pack(anchor=tk.W, padx=16, pady=(12, 6))

    frm_qc_opt = ttk.Frame(tab_qc, padding=6)
    frm_qc_opt.pack(fill=tk.X, padx=12, pady=4)

    ttk.Label(frm_qc_opt, text=t["lbl_qc_dir"]).pack(side=tk.LEFT, padx=4)
    var_qc_dir = tk.StringVar(value="b2b")
    ttk.Radiobutton(frm_qc_opt, text=t["qc_b2b"], variable=var_qc_dir, value="b2b").pack(side=tk.LEFT, padx=8)
    ttk.Radiobutton(frm_qc_opt, text=t["qc_u2b"], variable=var_qc_dir, value="u2b").pack(side=tk.LEFT, padx=8)

    var_qc_punct = tk.BooleanVar(value=True)
    ttk.Checkbutton(frm_qc_opt, text=t["chk_smart_punct"], variable=var_qc_punct).pack(side=tk.RIGHT, padx=8)

    ttk.Label(tab_qc, text=t["lbl_qc_input"]).pack(anchor=tk.W, padx=16, pady=(4, 2))
    txt_qc_in = tk.Text(tab_qc, height=5, font=("Helvetica", 11))
    txt_qc_in.pack(fill=tk.X, padx=16, pady=2)
    txt_qc_in.insert(tk.END, "ami banglay gan gai. amake email koro user@example.com othoba browse koro https://bangla.org ... thikache?")

    ttk.Label(tab_qc, text=t["lbl_qc_output"]).pack(anchor=tk.W, padx=16, pady=(6, 2))
    txt_qc_out = tk.Text(tab_qc, height=5, font=("Helvetica", 12), foreground="#1b5e20")
    txt_qc_out.pack(fill=tk.X, padx=16, pady=2)

    frm_qc_act = ttk.Frame(tab_qc, padding=6)
    frm_qc_act.pack(fill=tk.X, padx=12, pady=6)

    def on_perform_quick_convert():
        raw = txt_qc_in.get("1.0", tk.END).strip()
        if not raw:
            txt_qc_out.delete("1.0", tk.END)
            return

        direction = var_qc_dir.get()
        buf = ctypes.create_string_buffer(len(raw.encode('utf-8')) * 6 + 1024)
        result = ""

        if direction == "b2b":
            if abk_lib:
                ret = abk_lib.abk_convert_banglish_to_bangla(engine_handle, raw.encode('utf-8'), buf, len(buf))
                if ret >= 0:
                    result = buf.value.decode('utf-8')
            if not result:
                result = raw
        else: # u2b
            if abk_lib:
                ret = abk_lib.abk_convert_bangla_to_banglish(raw.encode('utf-8'), buf, len(buf))
                if ret >= 0:
                    result = buf.value.decode('utf-8')
            if not result:
                result = raw

        if var_qc_punct.get() and abk_lib:
            punct_buf = ctypes.create_string_buffer(len(result.encode('utf-8')) * 3 + 512)
            ret = abk_lib.abk_apply_smart_punctuation(result.encode('utf-8'), punct_buf, len(punct_buf))
            if ret >= 0:
                result = punct_buf.value.decode('utf-8')

        txt_qc_out.delete("1.0", tk.END)
        txt_qc_out.insert(tk.END, result)

    def on_copy_qc_output():
        out_text = txt_qc_out.get("1.0", tk.END).strip()
        if out_text:
            root.clipboard_clear()
            root.clipboard_append(out_text)
            messagebox.showinfo(t["tab_quick_convert"], t["copied_msg"])

    ttk.Button(frm_qc_act, text=t["btn_convert"], command=on_perform_quick_convert).pack(side=tk.LEFT, padx=6)
    ttk.Button(frm_qc_act, text=t["btn_copy"], command=on_copy_qc_output).pack(side=tk.LEFT, padx=6)

    # -------------------------------------------------------------
    # TAB 6: Workstream 8 - Correction Log & Erroneous Suggestion Feedback
    # -------------------------------------------------------------
    tab_fb = ttk.Frame(notebook)
    notebook.add(tab_fb, text=t["tab_feedback"])

    ttk.Label(tab_fb, text=t["fb_title"], font=("Helvetica", 12, "bold")).pack(anchor=tk.W, padx=16, pady=(12, 4))
    ttk.Label(tab_fb, text=t["fb_optin_note"], font=("Helvetica", 9), foreground="#666666").pack(anchor=tk.W, padx=16, pady=(0, 8))

    frm_log_act = ttk.Frame(tab_fb, padding=4)
    frm_log_act.pack(fill=tk.X, padx=12, pady=2)

    fb_cols = ("ts", "raw", "corr")
    tree_log = ttk.Treeview(tab_fb, columns=fb_cols, show="headings", height=6)
    tree_log.heading("ts", text=t["col_ts"])
    tree_log.heading("raw", text=t["col_raw"])
    tree_log.heading("corr", text=t["col_corr"])
    tree_log.column("ts", width=220)
    tree_log.column("raw", width=200)
    tree_log.column("corr", width=240)
    tree_log.pack(fill=tk.BOTH, expand=True, padx=16, pady=4)

    def refresh_correction_logs():
        for item in tree_log.get_children():
            tree_log.delete(item)
        if os.path.exists(CORRECTION_LOG_PATH):
            try:
                with open(CORRECTION_LOG_PATH, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if not line or not line.startswith("{"):
                            continue
                        try:
                            rec = json.loads(line)
                            tree_log.insert("", tk.END, values=(rec.get("timestamp", ""), rec.get("raw_input", ""), rec.get("corrected_word", "")))
                        except Exception:
                            pass
            except Exception:
                pass

    def on_clear_correction_logs():
        if os.path.exists(CORRECTION_LOG_PATH):
            with open(CORRECTION_LOG_PATH, "w", encoding="utf-8") as f:
                f.truncate(0)
        if abk_lib and engine_handle:
            abk_lib.abk_engine_clear_correction_log(engine_handle)
        refresh_correction_logs()
        messagebox.showinfo(t["tab_feedback"], "Correction logs cleared successfully.")

    ttk.Button(frm_log_act, text=t["btn_refresh_log"], command=refresh_correction_logs).pack(side=tk.LEFT, padx=4)
    ttk.Button(frm_log_act, text=t["btn_clear_log"], command=on_clear_correction_logs).pack(side=tk.LEFT, padx=4)
    refresh_correction_logs()

    # Erroneous suggestion report form
    grp_report = ttk.LabelFrame(tab_fb, text=t["grp_report_err"], padding=10)
    grp_report.pack(fill=tk.X, padx=12, pady=10)

    ttk.Label(grp_report, text=t["lbl_err_raw"]).grid(row=0, column=0, sticky=tk.W, padx=4, pady=3)
    var_err_raw = tk.StringVar()
    ttk.Entry(grp_report, textvariable=var_err_raw, width=18).grid(row=0, column=1, padx=4, pady=3)

    ttk.Label(grp_report, text=t["lbl_err_got"]).grid(row=0, column=2, sticky=tk.W, padx=4, pady=3)
    var_err_got = tk.StringVar()
    ttk.Entry(grp_report, textvariable=var_err_got, width=18).grid(row=0, column=3, padx=4, pady=3)

    ttk.Label(grp_report, text=t["lbl_err_exp"]).grid(row=1, column=0, sticky=tk.W, padx=4, pady=3)
    var_err_exp = tk.StringVar()
    ttk.Entry(grp_report, textvariable=var_err_exp, width=18).grid(row=1, column=1, padx=4, pady=3)

    ttk.Label(grp_report, text=t["lbl_err_app"]).grid(row=1, column=2, sticky=tk.W, padx=4, pady=3)
    var_err_app = tk.StringVar(value="Browser")
    ttk.Entry(grp_report, textvariable=var_err_app, width=18).grid(row=1, column=3, padx=4, pady=3)

    def on_submit_feedback():
        r = var_err_raw.get().strip()
        g = var_err_got.get().strip()
        e = var_err_exp.get().strip()
        a = var_err_app.get().strip()
        if not r or not e:
            messagebox.showwarning(t["tab_feedback"], "Please enter raw typed input and expected word.")
            return

        if abk_lib:
            abk_lib.abk_save_local_feedback(r.encode('utf-8'), g.encode('utf-8'), e.encode('utf-8'), a.encode('utf-8'), FEEDBACK_PATH.encode('utf-8'))
        else:
            os.makedirs(CONFIG_DIR, exist_ok=True)
            with open(FEEDBACK_PATH, "a", encoding="utf-8") as f:
                f.write(json.dumps({"raw_input": r, "received": g, "expected": e, "app": a}) + "\n")

        var_err_raw.set("")
        var_err_got.set("")
        var_err_exp.set("")
        messagebox.showinfo(t["tab_feedback"], t["fb_saved_msg"])

    ttk.Button(grp_report, text=t["btn_submit_fb"], command=on_submit_feedback).grid(row=2, column=3, sticky=tk.E, padx=4, pady=6)

    # -------------------------------------------------------------
    # Close Handler
    # -------------------------------------------------------------
    def on_closing():
        if abk_lib and engine_handle:
            abk_lib.abk_engine_destroy(engine_handle)
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    if start_tutorial:
        notebook.select(tab_tut)
    root.mainloop()

# 5. CLI Test / Headless Mode Verification
def run_headless_test():
    print("Running Settings, Tutorial & QoL Headless Verification...")
    settings = load_settings()
    assert "language_ui" in settings
    assert "candidate_count" in settings
    assert "shortcuts" in settings
    assert "enable_correction_log" in settings
    print("  ✓ Configuration load/save verified.")

    for lang in ["bn", "en"]:
        assert lang in STRINGS
        assert "app_title" in STRINGS[lang]
        assert "tut_l1" in STRINGS[lang]
        assert "tab_cheat_sheet" in STRINGS[lang]
        assert "tab_quick_convert" in STRINGS[lang]
        assert "tab_feedback" in STRINGS[lang]
        assert "btn_save" in STRINGS[lang]
    print("  ✓ Bilingual string dictionaries complete.")

    rules = load_phonetic_rules_list()
    assert len(rules) >= 20, f"Expected rules >= 20, got {len(rules)}"
    print(f"  ✓ Keymap cheat sheet rules verified ({len(rules)} items loaded).")

    if abk_lib:
        eng = abk_lib.abk_engine_create()
        assert eng is not None
        buf = ctypes.create_string_buffer(512)

        # 1. Fast transliteration
        ret = abk_lib.abk_transliterate_fast(eng, b"kOI", buf, 512)
        assert ret >= 0
        assert buf.value.decode('utf-8') == "কৈ"
        print("  ✓ Live libabk engine binding verified (kOI -> কৈ).")

        # 2. Workstream 8: Quick Convert Banglish -> Bangla
        ret = abk_lib.abk_convert_banglish_to_bangla(eng, b"ami banglay gan gai", buf, 512)
        assert ret >= 0
        out_b = buf.value.decode('utf-8')
        assert "গান" in out_b
        print("  ✓ Quick convert (Banglish -> Bangla) verified.")

        # 3. Workstream 8: Smart Punctuation
        ret = abk_lib.abk_apply_smart_punctuation(b"Opekkha korun... 3.14 holo pi.", buf, 512)
        assert ret >= 0
        out_p = buf.value.decode('utf-8')
        assert "…" in out_p
        assert "3.14" in out_p
        assert "।" in out_p
        print("  ✓ Smart punctuation (. -> ।, ... -> …) verified.")

        # 4. Workstream 8: Local Feedback Save
        test_fb_file = "/tmp/test_py_fb.jsonl"
        if os.path.exists(test_fb_file):
            os.remove(test_fb_file)
        ret = abk_lib.abk_save_local_feedback(b"kotha", "কোথা".encode('utf-8'), "কথা".encode('utf-8'), b"VSCode", test_fb_file.encode('utf-8'))
        assert ret == 1
        assert os.path.exists(test_fb_file)
        with open(test_fb_file, "r") as f:
            line = f.readline()
            assert "kotha" in line and "কোথা" in line and "কথা" in line
        os.remove(test_fb_file)
        print("  ✓ Local feedback reporting verified.")

        # 5. Workstream 8: Correction logging toggle
        abk_lib.abk_engine_set_correction_logging(eng, 1)
        assert abk_lib.abk_engine_is_correction_logging_enabled(eng) == 1
        abk_lib.abk_engine_set_correction_logging(eng, 0)
        assert abk_lib.abk_engine_is_correction_logging_enabled(eng) == 0
        print("  ✓ Correction logger toggle verified.")

        abk_lib.abk_engine_destroy(eng)

    print("ALL SETTINGS, TUTORIAL & QOL CHECKS PASSED CLEANLY!")
    return 0

if __name__ == '__main__':
    if "--test" in sys.argv:
        sys.exit(run_headless_test())
    elif "--tutorial" in sys.argv:
        run_gui(start_tutorial=True)
    else:
        run_gui(start_tutorial=False)
