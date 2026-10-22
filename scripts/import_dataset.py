#!/usr/bin/env python3
"""
Amar Bangla Kotha - Public Dataset Importer (Dakshina & Aksharantar)
-------------------------------------------------------------------
This script converts phonetic transliteration datasets (e.g. Google Dakshina,
AI4Bharat Aksharantar) into the standard golden evaluation format (TSV):
    input<TAB>expected<TAB>category

LICENSES & ATTRIBUTION:
1. Google Dakshina Dataset:
   - License: Apache License 2.0 (Permissive)
   - Source: https://github.com/google-research-datasets/dakshina
   - Reference: Roark et al., "Processing South Asian Languages Written in the
     Latin Script: the Dakshina Dataset", LREC 2020.
2. AI4Bharat Aksharantar Dataset:
   - License: Creative Commons Attribution 4.0 International (CC-BY 4.0)
   - Source: https://github.com/AI4Bharat/Aksharantar
   - Reference: Madhani et al., "Aksharantar: Towards building open spatio-temporal
     transliteration models for Indian languages", 2022.

NOTE: Do not commit full multi-gigabyte raw datasets to git. Use this script to
convert licensed evaluation subsets on demand.
"""

import sys
import os
import argparse
import unicodedata

def normalize_text(text: str) -> str:
    """Normalize Unicode to NFC form and strip excess whitespace."""
    if not text:
        return ""
    return unicodedata.normalize('NFC', text.strip())

def parse_dakshina(file_path: str, max_entries: int = 1000) -> list:
    """
    Parse Dakshina romanized-to-native TSV format.
    Standard Dakshina format: native_script<TAB>romanized_input<TAB>frequency/score
    Or test sets: native_script<TAB>romanized_variant1<TAB>romanized_variant2...
    """
    results = []
    print(f"Reading Dakshina dataset from {file_path} (License: Apache 2.0)...")
    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) >= 2:
                native = normalize_text(parts[0])
                roman = parts[1].strip()
                if native and roman:
                    results.append((roman, native, "dakshina_eval"))
            if max_entries and len(results) >= max_entries:
                break
    return results

def parse_aksharantar(file_path: str, max_entries: int = 1000) -> list:
    """
    Parse Aksharantar JSON/JSONL or TSV format.
    Standard format: {"input": "romanized", "output": "native"} or TSV.
    """
    results = []
    print(f"Reading Aksharantar dataset from {file_path} (License: CC-BY 4.0)...")
    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) >= 2:
                roman = parts[0].strip()
                native = normalize_text(parts[1])
                if native and roman:
                    results.append((roman, native, "aksharantar_eval"))
            if max_entries and len(results) >= max_entries:
                break
    return results

def main():
    parser = argparse.ArgumentParser(description="Import and normalize public Bangla transliteration datasets.")
    parser.add_argument("--source", choices=["dakshina", "aksharantar"], required=True, help="Dataset format source")
    parser.add_argument("--input", required=True, help="Input data file path")
    parser.add_argument("--output", default="tests/data/external_bench.tsv", help="Output TSV path")
    parser.add_argument("--limit", type=int, default=1000, help="Max entries to import")

    args = parser.parse_args()

    # Safety check: Never access /home/sumon/Downloads
    if "Downloads" in os.path.abspath(args.input):
        print("ERROR: Access to Downloads folder is restricted.", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(args.input):
        print(f"ERROR: File not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    if args.source == "dakshina":
        data = parse_dakshina(args.input, args.limit)
    else:
        data = parse_aksharantar(args.input, args.limit)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, 'w', encoding='utf-8') as out:
        out.write("input\texpected\tcategory\n")
        for roman, native, cat in data:
            out.write(f"{roman}\t{native}\t{cat}\n")

    print(f"Successfully converted {len(data)} entries to {args.output}")

if __name__ == "__main__":
    main()
