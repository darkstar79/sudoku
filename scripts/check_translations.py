#!/usr/bin/env python3
# sudoku-cpp - Offline Sudoku Game
# Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
#
# Translation completeness check.
#
# Scans the source tree for sudoku::core::loc("...") and
# sudoku::core::locFormat("...", ...) calls, then verifies that every
# English source string referenced in code is present in
# resources/translations/sudoku_en.ts and sudoku_de.ts, that no entries
# in the .ts files are orphaned (unused), and that no German entries are
# marked unfinished.
#
# Why this exists instead of `lupdate`: our runtime translation goes
# through namespace-level helpers (`core::loc`, `core::locFormat`) that
# lupdate's built-in extractor doesn't recognize. This script does the
# equivalent extraction for those helpers.
#
# Exit codes:
#   0 - all checks pass
#   1 - drift detected (missing, orphan, or unfinished entries)

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

SOURCE_GLOBS = ("**/*.cpp", "**/*.h")
HELPER_NAMES = ("loc", "locFormat")
SCAN_ROOTS = ("src",)
TS_FILES = (
    "resources/translations/sudoku_en.ts",
    "resources/translations/sudoku_de.ts",
)


def find_matching_paren(text: str, open_idx: int) -> int:
    """Return the index of the ')' matching the '(' at open_idx, skipping
    over nested parens, char/string literals."""
    assert text[open_idx] == "("
    depth = 0
    i = open_idx
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == '"':
                    break
                i += 1
        elif c == "'":
            i += 1
            while i < n and text[i] != "'":
                if text[i] == "\\":
                    i += 2
                    continue
                i += 1
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def parse_cpp_string_literal(text: str, start: int) -> tuple[str, int] | None:
    """Parse a C++ string literal starting at text[start] (which must be
    '"'). Handles \\n, \\t, \\\", \\\\. Returns (decoded_string, end_index_after_closing_quote)
    or None if not a literal."""
    if start >= len(text) or text[start] != '"':
        return None
    out = []
    i = start + 1
    while i < len(text):
        c = text[i]
        if c == "\\" and i + 1 < len(text):
            nxt = text[i + 1]
            decoded = {"n": "\n", "t": "\t", "r": "\r",
                       '"': '"', "\\": "\\", "'": "'", "0": "\0"}.get(nxt)
            if decoded is not None:
                out.append(decoded)
                i += 2
                continue
            # Unknown escape (\x.., \NNN, \u...., \U........). Don't
            # guess — return None so the call site is reported as
            # "dynamic" and the developer sees a warning rather than a
            # silently-mismatching translation key.
            return None
        if c == '"':
            return "".join(out), i + 1
        out.append(c)
        i += 1
    return None


def extract_first_string_arg(text: str, args_start: int, args_end: int) -> str | None:
    """Given the slice text[args_start:args_end] = '(...)', extract the
    first argument if it's a (concatenated) string literal. Returns None
    if it's not a static literal (e.g., a variable)."""
    inner = text[args_start + 1:args_end]
    # Skip leading whitespace
    i = 0
    n = len(inner)
    while i < n and inner[i].isspace():
        i += 1
    if i >= n or inner[i] != '"':
        return None
    parts = []
    while i < n:
        if inner[i] == '"':
            parsed = parse_cpp_string_literal(inner, i)
            if parsed is None:
                return None
            s, end = parsed
            parts.append(s)
            i = end
            # skip whitespace; allow concatenated adjacent literals "a" "b"
            while i < n and inner[i].isspace():
                i += 1
            if i < n and inner[i] == '"':
                continue
            break
        else:
            return None
    # The arg must end here, with either ',' or end of (...).
    if i < n and inner[i] != ",":
        # something else after the literal -> not a clean literal arg
        return None
    return "".join(parts)


# Match `(sudoku::)?core::loc(` and `(sudoku::)?core::locFormat(`. We
# require the `core::` qualifier to avoid false positives on unrelated
# `loc(` symbols (member functions, lambda variables, etc.).
CALL_RE = re.compile(
    r"\b(?:sudoku::)?core::(loc|locFormat)\s*\("
)


def scan_source_file(path: Path) -> tuple[set[str], list[tuple[Path, int]]]:
    """Return (literal_sources, dynamic_sites). dynamic_sites is a list
    of (path, line_number) for calls whose first argument is not a
    static literal — useful for warning the developer."""
    literals: set[str] = set()
    dynamic: list[tuple[Path, int]] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    for m in CALL_RE.finditer(text):
        open_idx = m.end() - 1
        close_idx = find_matching_paren(text, open_idx)
        if close_idx == -1:
            continue
        first = extract_first_string_arg(text, open_idx, close_idx)
        if first is None:
            line = text.count("\n", 0, m.start()) + 1
            dynamic.append((path, line))
            continue
        literals.add(first)
    return literals, dynamic


def scan_sources(repo_root: Path) -> tuple[set[str], list[tuple[Path, int]]]:
    all_literals: set[str] = set()
    all_dynamic: list[tuple[Path, int]] = []
    for root in SCAN_ROOTS:
        root_path = repo_root / root
        if not root_path.exists():
            continue
        for pattern in SOURCE_GLOBS:
            for path in root_path.glob(pattern):
                if not path.is_file():
                    continue
                # Skip the helper file itself; the `loc` definition
                # there isn't a translation call.
                if path.name == "i18n_helpers.h":
                    continue
                lits, dyn = scan_source_file(path)
                all_literals.update(lits)
                all_dynamic.extend(dyn)
    return all_literals, all_dynamic


def parse_ts_file(path: Path) -> tuple[set[str], list[str]]:
    """Return (set_of_source_strings, list_of_unfinished_sources)."""
    sources: set[str] = set()
    unfinished: list[str] = []
    tree = ET.parse(path)
    root = tree.getroot()
    for ctx in root.findall("context"):
        for msg in ctx.findall("message"):
            src_el = msg.find("source")
            if src_el is None or src_el.text is None:
                continue
            src = src_el.text
            sources.add(src)
            tr_el = msg.find("translation")
            if tr_el is not None and tr_el.attrib.get("type") == "unfinished":
                unfinished.append(src)
    return sources, unfinished


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo-root", default=".",
                    help="Path to repo root (default: cwd)")
    ap.add_argument("--allow-orphans", action="store_true",
                    help="Don't fail on orphan .ts entries (entries not "
                         "referenced from source). Useful during migrations.")
    args = ap.parse_args()

    repo_root = Path(args.repo_root).resolve()
    print(f"Repo root: {repo_root}")

    print("Scanning source tree for core::loc / core::locFormat calls...")
    code_sources, dynamic_calls = scan_sources(repo_root)
    print(f"  Found {len(code_sources)} unique literal source strings")
    if dynamic_calls:
        print(f"  WARNING: {len(dynamic_calls)} calls with non-literal first "
              "argument (translation must be statically extractable):")
        for p, ln in dynamic_calls[:10]:
            rel = p.relative_to(repo_root)
            print(f"    {rel}:{ln}")
        if len(dynamic_calls) > 10:
            print(f"    ... and {len(dynamic_calls) - 10} more")

    failures: list[str] = []

    for ts_rel in TS_FILES:
        ts_path = repo_root / ts_rel
        if not ts_path.exists():
            failures.append(f"Missing translation file: {ts_rel}")
            continue
        ts_sources, unfinished = parse_ts_file(ts_path)
        print(f"\n{ts_rel}: {len(ts_sources)} entries")

        missing = code_sources - ts_sources
        orphan = ts_sources - code_sources

        if missing:
            failures.append(
                f"{ts_rel}: {len(missing)} source string(s) used in code "
                f"but missing from .ts file"
            )
            for s in sorted(missing)[:20]:
                print(f"  MISSING: {s!r}")
            if len(missing) > 20:
                print(f"  ... and {len(missing) - 20} more")

        if orphan and not args.allow_orphans:
            failures.append(
                f"{ts_rel}: {len(orphan)} orphan entry/entries "
                f"(in .ts but not used in code)"
            )
            for s in sorted(orphan):
                print(f"  ORPHAN:  {s!r}")

        if ts_path.name.startswith("sudoku_de") and unfinished:
            failures.append(
                f"{ts_rel}: {len(unfinished)} unfinished translation(s)"
            )
            for s in unfinished[:10]:
                print(f"  UNFINISHED: {s!r}")
            if len(unfinished) > 10:
                print(f"  ... and {len(unfinished) - 10} more")

    print()
    if failures:
        print("Translation check FAILED:")
        for f in failures:
            print(f"  - {f}")
        print()
        print("Fix steps:")
        print("  - For missing entries: add <message><source>...</source>"
              "<translation>...</translation></message> to both .ts files.")
        print("  - For orphan entries: remove the unused entry, OR re-add"
              " the call site if the removal was unintentional.")
        print("  - For unfinished entries: provide a translation and"
              " remove type=\"unfinished\" attribute.")
        return 1

    print("Translation check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
