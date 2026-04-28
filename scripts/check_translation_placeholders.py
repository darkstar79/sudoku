#!/usr/bin/env python3
"""Validate fmt-style placeholders in Qt Linguist .ts translations.

The runtime feeds translated strings to ``fmt::format(fmt::runtime(...), args)``,
so a translator who drops, renumbers, or mangles a ``{0}``-style placeholder
introduces a ``fmt::format_error`` that fires only on the affected locale.
``lupdate`` does not validate fmt syntax (it knows about Qt's ``%1`` instead).

This script parses every <message> in each non-source-language .ts file and
fails if the multiset of placeholder field names in <translation> differs
from <source>, or if either string is malformed.

Exit codes:
  0  every translation has matching placeholders.
  1  at least one mismatch or malformed string.
  2  argument error.
"""
from __future__ import annotations

import argparse
import string
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path


def extract_placeholders(s: str) -> tuple[Counter[str], str | None]:
    """Return (multiset of field names, error message or None).

    Uses Python's ``string.Formatter`` to parse the string the same way fmt does
    for positional/named substitutions. Field names like ``0`` or ``1:.1f`` are
    captured as ``"0"`` / ``"1"``; literal ``{{`` / ``}}`` are skipped.
    """
    counts: Counter[str] = Counter()
    try:
        for _literal, field_name, _spec, _conv in string.Formatter().parse(s):
            if field_name is not None:
                counts[field_name] += 1
    except ValueError as e:
        return counts, f"malformed format string: {e}"
    return counts, None


def has_positional_placeholder(counts: Counter[str]) -> bool:
    """True if any field name is digit-only (positional, e.g. {0})."""
    return any(name.isdigit() for name in counts)


def check_ts_file(path: Path) -> list[str]:
    """Return a list of human-readable error messages for the given .ts file."""
    errors: list[str] = []
    try:
        tree = ET.parse(path)
    except ET.ParseError as e:
        return [f"{path}: XML parse error: {e}"]

    for msg in tree.getroot().iter("message"):
        source_el = msg.find("source")
        translation_el = msg.find("translation")
        if source_el is None or source_el.text is None:
            continue
        if translation_el is None:
            continue
        # Skip unfinished entries — Qt falls back to <source> at runtime so the
        # placeholder set is whatever <source> has, by definition consistent.
        if translation_el.get("type") == "unfinished":
            continue
        translated = translation_el.text or ""
        if not translated:
            continue

        src_counts, src_err = extract_placeholders(source_el.text)

        # Only validate strings the developer intended to fmt::format. The
        # signal is a positional placeholder ({0}, {1}, …) in the source,
        # since that's what every locFormat call site in the codebase uses.
        # Strings without positional placeholders are rendered via plain
        # core::loc, so literal braces (e.g. Sudoku notation like "{A,B}")
        # in translations are harmless and intentional.
        if src_err:
            errors.append(f"{path}: <source> {src_err} for: {source_el.text!r}")
            continue
        if not has_positional_placeholder(src_counts):
            continue

        tr_counts, tr_err = extract_placeholders(translated)
        if tr_err:
            errors.append(f"{path}: <translation> {tr_err} for source {source_el.text!r}")
            continue

        if src_counts != tr_counts:
            errors.append(
                f"{path}: placeholder mismatch for source {source_el.text!r}\n"
                f"    source placeholders:      {dict(src_counts)}\n"
                f"    translation placeholders: {dict(tr_counts)}\n"
                f"    translation: {translated!r}"
            )

    return errors


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("paths", nargs="*", type=Path,
                   help=".ts files to check (default: resources/translations/sudoku_*.ts "
                        "excluding the source-language sudoku_en.ts)")
    p.add_argument("--source-locale", default="en",
                   help="locale code to skip as the source language (default: en)")
    args = p.parse_args(argv)

    if args.paths:
        targets = list(args.paths)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        targets = sorted((repo_root / "resources" / "translations").glob("sudoku_*.ts"))
        skip = f"sudoku_{args.source_locale}.ts"
        targets = [t for t in targets if t.name != skip]

    if not targets:
        print("No .ts files to check.", file=sys.stderr)
        return 0

    all_errors: list[str] = []
    for ts in targets:
        all_errors.extend(check_ts_file(ts))

    if all_errors:
        print("Translation placeholder validation FAILED:\n", file=sys.stderr)
        for err in all_errors:
            print(f"  {err}", file=sys.stderr)
        print(f"\n{len(all_errors)} issue(s) across {len(targets)} file(s).",
              file=sys.stderr)
        return 1

    print(f"OK — placeholders match in {len(targets)} file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
