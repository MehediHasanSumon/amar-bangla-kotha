#!/usr/bin/env python3
"""
Amar Bangla Kotha - Legacy Font Clipboard & Paste Converter Tool
Converts Bijoy / SutonnyMJ ANSI encoded text from clipboard to standard Unicode UTF-8.

Usage:
    python3 tools/abk_legacy_clipboard.py --once    # Convert current clipboard once
    python3 tools/abk_legacy_clipboard.py --watch   # Watch clipboard in background and auto-convert
    python3 tools/abk_legacy_clipboard.py --gui     # Launch interactive desktop converter window
"""

import sys
import os
import ctypes
import time
import argparse
import tkinter as tk
from tkinter import ttk, messagebox

# Locate shared library
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
LIB_PATHS = [
    os.path.join(ROOT_DIR, "lib", "libabk.so"),
    os.path.join(ROOT_DIR, "build", "libabk.so"),
    "/usr/lib/libabk.so",
    "/usr/local/lib/libabk.so"
]

libabk = None
for p in LIB_PATHS:
    if os.path.exists(p):
        try:
            libabk = ctypes.CDLL(p)
            break
        except Exception:
            pass

if libabk:
    # abk_ansi_to_unicode(const char* ansi, char* out, size_t cap)
    libabk.abk_ansi_to_unicode.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
    libabk.abk_ansi_to_unicode.restype = ctypes.c_int

    # abk_unicode_to_ansi(const char* u, char* out, size_t cap)
    libabk.abk_unicode_to_ansi.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
    libabk.abk_unicode_to_ansi.restype = ctypes.c_int

    # abk_is_likely_legacy_ansi(const char* text)
    libabk.abk_is_likely_legacy_ansi.argtypes = [ctypes.c_char_p]
    libabk.abk_is_likely_legacy_ansi.restype = ctypes.c_bool


def convert_ansi_to_unicode(text: str) -> str:
    if not text or not libabk:
        return text
    raw_bytes = text.encode("utf-8", errors="replace")
    buf_size = len(raw_bytes) * 4 + 256
    out_buf = ctypes.create_string_buffer(buf_size)
    ret = libabk.abk_ansi_to_unicode(raw_bytes, out_buf, buf_size)
    if ret > 0:
        return out_buf.value.decode("utf-8", errors="replace")
    return text


def convert_unicode_to_ansi(text: str) -> str:
    if not text or not libabk:
        return text
    raw_bytes = text.encode("utf-8", errors="replace")
    buf_size = len(raw_bytes) * 4 + 256
    out_buf = ctypes.create_string_buffer(buf_size)
    ret = libabk.abk_unicode_to_ansi(raw_bytes, out_buf, buf_size)
    if ret > 0:
        return out_buf.value.decode("utf-8", errors="replace")
    return text


def is_likely_legacy_ansi(text: str) -> bool:
    if not text or not libabk:
        return False
    raw_bytes = text.encode("utf-8", errors="replace")
    return bool(libabk.abk_is_likely_legacy_ansi(raw_bytes))


def get_clipboard() -> str:
    root = tk.Tk()
    root.withdraw()
    try:
        content = root.clipboard_get()
    except tk.TclError:
        content = ""
    root.destroy()
    return content


def set_clipboard(text: str):
    root = tk.Tk()
    root.withdraw()
    root.clipboard_clear()
    root.clipboard_append(text)
    root.update()
    root.destroy()


def run_once():
    content = get_clipboard()
    if not content:
        print("[ABK] Clipboard is empty.")
        return
    if is_likely_legacy_ansi(content):
        converted = convert_ansi_to_unicode(content)
        set_clipboard(converted)
        print("[ABK] Detected Bijoy ANSI! Converted to Unicode and copied to clipboard:")
        print("----------------------------------------------------------------------")
        print(converted)
        print("----------------------------------------------------------------------")
    else:
        print("[ABK] Clipboard content does not appear to be Bijoy ANSI:")
        print(content[:100])


def run_watch(interval_sec=0.5):
    print("======================================================================")
    print(" Amar Bangla Kotha - Background Clipboard Watcher Active              ")
    print(" Copy Bijoy / SutonnyMJ text anywhere; it will auto-convert to Unicode!")
    print(" Press Ctrl+C to stop.                                                ")
    print("======================================================================")
    last_text = ""
    try:
        while True:
            content = get_clipboard()
            if content and content != last_text:
                last_text = content
                if is_likely_legacy_ansi(content):
                    converted = convert_ansi_to_unicode(content)
                    if converted != content:
                        set_clipboard(converted)
                        last_text = converted
                        print(f"[{time.strftime('%H:%M:%S')}] Auto-converted Bijoy -> Unicode ({len(converted)} chars)")
            time.sleep(interval_sec)
    except KeyboardInterrupt:
        print("\nClipboard watcher stopped.")


def launch_gui():
    root = tk.Tk()
    root.title("Amar Bangla Kotha - Bijoy / Unicode Converter")
    root.geometry("750x550")
    root.minsize(600, 450)

    # Frame top
    header_frame = ttk.Frame(root, padding=10)
    header_frame.pack(fill=tk.X)
    title_lbl = ttk.Label(header_frame, text="Bijoy (SutonnyMJ) ⇄ Unicode Converter", font=("Arial", 14, "bold"))
    title_lbl.pack(side=tk.LEFT)

    # Text areas
    main_frame = ttk.Frame(root, padding=10)
    main_frame.pack(fill=tk.BOTH, expand=True)

    lbl1 = ttk.Label(main_frame, text="Legacy ANSI (Bijoy / SutonnyMJ):", font=("Arial", 10, "bold"))
    lbl1.pack(anchor=tk.W)
    txt_ansi = tk.Text(main_frame, height=9, font=("Courier", 11))
    txt_ansi.pack(fill=tk.BOTH, expand=True, pady=(2, 8))

    btn_frame = ttk.Frame(main_frame)
    btn_frame.pack(fill=tk.X, pady=5)

    def do_ansi_to_uni():
        src = txt_ansi.get("1.0", tk.END).rstrip("\n")
        res = convert_ansi_to_unicode(src)
        txt_uni.delete("1.0", tk.END)
        txt_uni.insert("1.0", res)

    def do_uni_to_ansi():
        src = txt_uni.get("1.0", tk.END).rstrip("\n")
        res = convert_unicode_to_ansi(src)
        txt_ansi.delete("1.0", tk.END)
        txt_ansi.insert("1.0", res)

    def do_paste_ansi():
        clip = get_clipboard()
        txt_ansi.delete("1.0", tk.END)
        txt_ansi.insert("1.0", clip)
        do_ansi_to_uni()

    def do_copy_uni():
        res = txt_uni.get("1.0", tk.END).rstrip("\n")
        set_clipboard(res)
        messagebox.showinfo("Copied", "Converted Unicode text copied to clipboard!")

    btn_down = ttk.Button(btn_frame, text="  ⬇ Convert to Unicode (ANSI ➔ Unicode)  ", command=do_ansi_to_uni)
    btn_down.pack(side=tk.LEFT, padx=5)

    btn_up = ttk.Button(btn_frame, text="  ⬆ Convert to Bijoy (Unicode ➔ ANSI)  ", command=do_uni_to_ansi)
    btn_up.pack(side=tk.LEFT, padx=5)

    btn_paste = ttk.Button(btn_frame, text="📋 Paste & Convert", command=do_paste_ansi)
    btn_paste.pack(side=tk.RIGHT, padx=5)

    lbl2 = ttk.Label(main_frame, text="Standard Unicode (UTF-8):", font=("Arial", 10, "bold"))
    lbl2.pack(anchor=tk.W, pady=(8, 2))
    txt_uni = tk.Text(main_frame, height=9, font=("Arial", 12))
    txt_uni.pack(fill=tk.BOTH, expand=True, pady=(2, 8))

    bottom_frame = ttk.Frame(root, padding=10)
    bottom_frame.pack(fill=tk.X)
    btn_copy = ttk.Button(bottom_frame, text="📋 Copy Unicode Result", command=do_copy_uni)
    btn_copy.pack(side=tk.RIGHT)

    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description="Amar Bangla Kotha - Bijoy / Unicode Converter Tool")
    parser.add_argument("--once", action="store_true", help="Convert current clipboard once")
    parser.add_argument("--watch", action="store_true", help="Watch clipboard in background and auto-convert")
    parser.add_argument("--gui", action="store_true", help="Launch GUI window")
    parser.add_argument("--convert", type=str, help="Directly convert string argument from ANSI to Unicode")
    args = parser.parse_args()

    if args.convert:
        print(convert_ansi_to_unicode(args.convert))
    elif args.once:
        run_once()
    elif args.watch:
        run_watch()
    elif args.gui or len(sys.argv) == 1:
        launch_gui()


if __name__ == "__main__":
    main()
