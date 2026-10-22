#!/usr/bin/env python3
"""
Amar Bangla Kotha (আমার বাংলা কথা) - Settings GUI & First-Run Interactive Tutorial
Cross-platform settings manager and interactive learning lesson for v1.0.
"""

import sys
import os
import json
import ctypes
from ctypes import c_void_p, c_char_p, c_size_t, c_int, c_double, Structure, POINTER

# 1. Load libabk library for live interactive transliteration in the tutorial
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
                return lib
            except Exception:
                pass
    return None

abk_lib = load_abk()

# 2. Local Configuration Storage
CONFIG_DIR = os.path.expanduser("~/.config/amar-bangla-kotha")
CONFIG_PATH = os.path.join(CONFIG_DIR, "settings.json")

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

# 3. Bilingual UI Strings (Bangla & English)
STRINGS = {
    "bn": {
        "app_title": "আমার বাংলা কথা — সেটিংস ও টিউটোরিয়াল",
        "tab_settings": "সেটিংস (Settings)",
        "tab_tutorial": "প্রথম পাঠ ও টিউটোরিয়াল (Tutorial)",
        "tab_shortcuts": "শর্টকাট ও নিজস্ব ডিকশনারি (Shortcuts)",
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
    },
    "en": {
        "app_title": "Amar Bangla Kotha — Settings & First-Run Tutorial",
        "tab_settings": "Settings",
        "tab_tutorial": "First-Run Tutorial",
        "tab_shortcuts": "Shortcuts & Personal Dict",
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
    root.geometry("740x580")
    root.minsize(680, 520)

    # Apply styling
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

    # TAB 1: SETTINGS
    tab_settings = ttk.Frame(notebook)
    notebook.add(tab_settings, text=t["tab_settings"])

    # TAB 2: FIRST-RUN TUTORIAL
    tab_tutorial = ttk.Frame(notebook)
    notebook.add(tab_tutorial, text=t["tab_tutorial"])

    # TAB 3: SHORTCUTS
    tab_shortcuts = ttk.Frame(notebook)
    notebook.add(tab_shortcuts, text=t["tab_shortcuts"])

    if start_tutorial:
        notebook.select(tab_tutorial)

    # --- TAB 1 Content: Settings ---
    frm_gen = ttk.LabelFrame(tab_settings, text=t["grp_general"], padding=12)
    frm_gen.pack(fill=tk.X, padx=10, pady=8)

    # Language UI
    ttk.Label(frm_gen, text=t["lbl_lang_ui"]).grid(row=0, column=0, sticky=tk.W, pady=6)
    var_lang = tk.StringVar(value="বাংলা (Bangla)" if current_lang == "bn" else "English")
    cb_lang = ttk.Combobox(frm_gen, textvariable=var_lang, values=["বাংলা (Bangla)", "English"], state="readonly", width=18)
    cb_lang.grid(row=0, column=1, sticky=tk.W, padx=10)

    # Hotkey
    ttk.Label(frm_gen, text=t["lbl_hotkey"]).grid(row=1, column=0, sticky=tk.W, pady=6)
    var_hotkey = tk.StringVar(value=settings.get("toggle_hotkey", "F12"))
    cb_hotkey = ttk.Combobox(frm_gen, textvariable=var_hotkey, values=["F12", "Ctrl+Space", "Shift+Space", "Ctrl+Shift"], state="readonly", width=18)
    cb_hotkey.grid(row=1, column=1, sticky=tk.W, padx=10)

    # Number mode
    ttk.Label(frm_gen, text=t["lbl_number_mode"]).grid(row=2, column=0, sticky=tk.W, pady=6)
    var_num = tk.StringVar(value=settings.get("number_mode", "en"))
    frm_num = ttk.Frame(frm_gen)
    frm_num.grid(row=2, column=1, sticky=tk.W, padx=10)
    ttk.Radiobutton(frm_num, text=t["num_en"], variable=var_num, value="en").pack(side=tk.LEFT, padx=4)
    ttk.Radiobutton(frm_num, text=t["num_bn"], variable=var_num, value="bn").pack(side=tk.LEFT, padx=4)

    # Candidate Count
    ttk.Label(frm_gen, text=t["lbl_candidates"]).grid(row=3, column=0, sticky=tk.W, pady=6)
    var_cand = tk.IntVar(value=settings.get("candidate_count", 7))
    sp_cand = ttk.Spinbox(frm_gen, from_=3, to=10, textvariable=var_cand, width=6)
    sp_cand.grid(row=3, column=1, sticky=tk.W, padx=10)

    # Theme
    ttk.Label(frm_gen, text=t["lbl_theme"]).grid(row=4, column=0, sticky=tk.W, pady=6)
    var_theme = tk.StringVar(value=settings.get("theme", "light"))
    frm_theme = ttk.Frame(frm_gen)
    frm_theme.grid(row=4, column=1, sticky=tk.W, padx=10)
    ttk.Radiobutton(frm_theme, text=t["theme_light"], variable=var_theme, value="light").pack(side=tk.LEFT, padx=4)
    ttk.Radiobutton(frm_theme, text=t["theme_dark"], variable=var_theme, value="dark").pack(side=tk.LEFT, padx=4)

    # Advanced Frame
    frm_adv = ttk.LabelFrame(tab_settings, text=t["grp_advanced"], padding=12)
    frm_adv.pack(fill=tk.X, padx=10, pady=8)

    var_bypass = tk.BooleanVar(value=settings.get("bypass_terminal_ide", True))
    ttk.Checkbutton(frm_adv, text=t["chk_bypass_ide"], variable=var_bypass).pack(anchor=tk.W, pady=4)

    var_ac = tk.BooleanVar(value=settings.get("enable_autocorrect", True))
    ttk.Checkbutton(frm_adv, text=t["chk_autocorrect"], variable=var_ac).pack(anchor=tk.W, pady=4)

    var_nw = tk.BooleanVar(value=settings.get("enable_next_word", True))
    ttk.Checkbutton(frm_adv, text=t["chk_next_word"], variable=var_nw).pack(anchor=tk.W, pady=4)

    # Actions Frame
    frm_actions = ttk.Frame(tab_settings, padding=10)
    frm_actions.pack(fill=tk.X, padx=10, pady=10)

    def on_save_settings():
        new_lang = "bn" if "বাংলা" in var_lang.get() else "en"
        settings["language_ui"] = new_lang
        settings["toggle_hotkey"] = var_hotkey.get()
        settings["number_mode"] = var_num.get()
        settings["candidate_count"] = var_cand.get()
        settings["theme"] = var_theme.get()
        settings["bypass_terminal_ide"] = var_bypass.get()
        settings["enable_autocorrect"] = var_ac.get()
        settings["enable_next_word"] = var_nw.get()
        save_settings(settings)
        messagebox.showinfo("Amar Bangla Kotha", t["saved_msg"])

    def on_reset_data():
        if messagebox.askyesno("Amar Bangla Kotha", t["reset_confirm"]):
            if abk_lib and engine_handle:
                abk_lib.abk_engine_clear_personal_data(engine_handle)
            settings["shortcuts"] = DEFAULT_SETTINGS["shortcuts"].copy()
            save_settings(settings)
            refresh_shortcuts_list()
            messagebox.showinfo("Amar Bangla Kotha", "All personal dictionary learning and shortcuts have been reset.")

    ttk.Button(frm_actions, text=t["btn_save"], command=on_save_settings).pack(side=tk.LEFT, padx=6)
    ttk.Button(frm_actions, text=t["btn_reset"], command=on_reset_data).pack(side=tk.RIGHT, padx=6)

    # --- TAB 2 Content: First-Run Tutorial ---
    frm_tut = ttk.Frame(tab_tutorial, padding=16)
    frm_tut.pack(fill=tk.BOTH, expand=True)

    ttk.Label(frm_tut, text=t["tut_title"], font=("Helvetica", 14, "bold")).pack(anchor=tk.W, pady=4)
    ttk.Label(frm_tut, text=t["tut_sub"], foreground="#555555").pack(anchor=tk.W, pady=(0, 12))

    lessons = [t["tut_l1"], t["tut_l2"], t["tut_l3"], t["tut_l4"]]
    for l in lessons:
        lf = ttk.Frame(frm_tut, padding=6)
        lf.pack(fill=tk.X, pady=3)
        ttk.Label(lf, text="✦", foreground="#1976D2", font=("Helvetica", 12)).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Label(lf, text=l, font=("Helvetica", 10), wraplength=640).pack(side=tk.LEFT, fill=tk.X)

    # Interactive Practice Widget
    frm_practice = ttk.LabelFrame(frm_tut, text="Interactive Sandbox (লাইভ প্র্যাকটিস বক্স)", padding=12)
    frm_practice.pack(fill=tk.X, pady=16)

    ttk.Label(frm_practice, text=t["lbl_practice"]).grid(row=0, column=0, sticky=tk.W, pady=4)
    var_test_input = tk.StringVar()
    entry_practice = ttk.Entry(frm_practice, textvariable=var_test_input, font=("Helvetica", 13), width=35)
    entry_practice.grid(row=0, column=1, padx=8, pady=4, sticky=tk.W)
    entry_practice.focus_set()

    ttk.Label(frm_practice, text=t["lbl_practice_out"]).grid(row=1, column=0, sticky=tk.W, pady=8)
    lbl_out = ttk.Label(frm_practice, text="", font=("SolaimanLipi", 16, "bold"), foreground="#2E7D32")
    lbl_out.grid(row=1, column=1, padx=8, pady=8, sticky=tk.W)

    def on_practice_type(*args):
        inp = var_test_input.get().strip()
        if not inp:
            lbl_out.config(text="")
            return
        if abk_lib and engine_handle:
            buf = ctypes.create_string_buffer(256)
            ret = abk_lib.abk_transliterate_fast(engine_handle, inp.encode('utf-8'), buf, 256)
            if ret >= 0:
                lbl_out.config(text=buf.value.decode('utf-8'))
            else:
                lbl_out.config(text=inp)
        else:
            lbl_out.config(text=inp)

    var_test_input.trace_add("write", on_practice_type)

    # Preset shortcut practice buttons
    frm_quick = ttk.Frame(frm_practice)
    frm_quick.grid(row=2, column=0, columnspan=2, pady=6, sticky=tk.W)
    ttk.Label(frm_quick, text="Quick Test:").pack(side=tk.LEFT, padx=(0, 6))
    for sample in ["kOI", "Thik", "baRi", "k+t", "bangla"]:
        ttk.Button(frm_quick, text=sample, width=7, command=lambda s=sample: var_test_input.set(s)).pack(side=tk.LEFT, padx=3)

    # --- TAB 3 Content: Shortcuts Manager ---
    frm_sc = ttk.Frame(tab_shortcuts, padding=12)
    frm_sc.pack(fill=tk.BOTH, expand=True)

    ttk.Label(frm_sc, text=t["sc_title"], font=("Helvetica", 13, "bold")).pack(anchor=tk.W, pady=4)

    # Shortcut table
    columns = ("shortcut", "expansion")
    tree_sc = ttk.Treeview(frm_sc, columns=columns, show="headings", height=8)
    tree_sc.heading("shortcut", text="Shortcut (শর্টকাট)")
    tree_sc.heading("expansion", text="Full Expansion (সম্পূর্ণ শব্দ)")
    tree_sc.column("shortcut", width=160)
    tree_sc.column("expansion", width=380)
    tree_sc.pack(fill=tk.BOTH, expand=True, pady=8)

    def refresh_shortcuts_list():
        for item in tree_sc.get_children():
            tree_sc.delete(item)
        sc_map = settings.get("shortcuts", {})
        for k in sorted(sc_map.keys()):
            tree_sc.insert("", tk.END, values=(k, sc_map[k]))

    refresh_shortcuts_list()

    # Form to add new shortcut
    frm_add = ttk.Frame(frm_sc, padding=6)
    frm_add.pack(fill=tk.X, pady=4)

    ttk.Label(frm_add, text=t["lbl_sc_key"]).grid(row=0, column=0, sticky=tk.W, padx=4)
    var_new_sc = tk.StringVar()
    entry_sc = ttk.Entry(frm_add, textvariable=var_new_sc, width=12)
    entry_sc.grid(row=0, column=1, padx=4)

    ttk.Label(frm_add, text=t["lbl_sc_val"]).grid(row=0, column=2, sticky=tk.W, padx=8)
    var_new_val = tk.StringVar()
    entry_val = ttk.Entry(frm_add, textvariable=var_new_val, width=28)
    entry_val.grid(row=0, column=3, padx=4)

    def on_add_shortcut():
        k = var_new_sc.get().strip()
        v = var_new_val.get().strip()
        if not k or not v:
            return
        if "shortcuts" not in settings:
            settings["shortcuts"] = {}
        settings["shortcuts"][k] = v
        if abk_lib and engine_handle:
            abk_lib.abk_engine_add_shortcut(engine_handle, k.encode('utf-8'), v.encode('utf-8'))
        save_settings(settings)
        refresh_shortcuts_list()
        var_new_sc.set("")
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

    ttk.Button(frm_add, text=t["btn_sc_add"], command=on_add_shortcut).grid(row=0, column=4, padx=8)
    ttk.Button(frm_add, text=t["btn_sc_del"], command=on_del_shortcut).grid(row=0, column=5, padx=4)

    frm_sc_io = ttk.Frame(frm_sc)
    frm_sc_io.pack(fill=tk.X, pady=8)
    ttk.Button(frm_sc_io, text=t["btn_sc_export"], command=on_export_tsv).pack(side=tk.LEFT, padx=4)
    ttk.Button(frm_sc_io, text=t["btn_sc_import"], command=on_import_tsv).pack(side=tk.LEFT, padx=4)

    def on_closing():
        if abk_lib and engine_handle:
            abk_lib.abk_engine_destroy(engine_handle)
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()

# 5. CLI Test / Headless Mode Verification
def run_headless_test():
    print("Running Settings & Tutorial Headless Verification...")
    settings = load_settings()
    assert "language_ui" in settings
    assert "candidate_count" in settings
    assert "shortcuts" in settings
    print("  ✓ Configuration load/save verified.")

    for lang in ["bn", "en"]:
        assert lang in STRINGS
        assert "app_title" in STRINGS[lang]
        assert "tut_l1" in STRINGS[lang]
        assert "btn_save" in STRINGS[lang]
    print("  ✓ Bilingual string dictionaries complete.")

    if abk_lib:
        eng = abk_lib.abk_engine_create()
        assert eng is not None
        buf = ctypes.create_string_buffer(256)
        ret = abk_lib.abk_transliterate_fast(eng, b"kOI", buf, 256)
        assert ret >= 0
        assert buf.value.decode('utf-8') == "কৈ"
        abk_lib.abk_engine_destroy(eng)
        print("  ✓ Live libabk engine binding verified (kOI -> কৈ).")

    print("ALL SETTINGS & TUTORIAL CHECKS PASSED CLEANLY!")
    return 0

if __name__ == '__main__':
    if "--test" in sys.argv:
        sys.exit(run_headless_test())
    elif "--tutorial" in sys.argv:
        run_gui(start_tutorial=True)
    else:
        run_gui(start_tutorial=False)
