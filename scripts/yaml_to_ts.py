#!/usr/bin/env python3
"""Convert legacy YAML translation files to Qt Linguist .ts format.

Originally written to seed sudoku_en.ts and sudoku_de.ts from the key-based
YAML system that preceded Qt Linguist. Kept in the repository so anyone
maintaining a YAML translation file (e.g. an out-of-tree ru.yaml held by a
downstream packager) can convert it once to a .ts file.

Inputs:
  --keys     src/core/string_keys.h      canonical key list (preserves order)
  --source   resources/locales/en.yaml   English source-string lookup table
  --input    <locale>.yaml               translation YAML to convert
  --language <code>                      locale code embedded in the .ts header
  --output   <locale>.ts                 destination .ts file

The .ts file uses a single <context name="Sudoku"> block. All call sites in
the project use QCoreApplication::translate("Sudoku", ...) so this single
context is what Qt Linguist sees during lupdate / lrelease.

Default behaviour with no arguments: bulk-convert resources/locales/en.yaml
and resources/locales/de.yaml into resources/translations/sudoku_{en,de}.ts.

Important: this script depends on src/core/string_keys.h and an English
en.yaml as its inputs. Both are removed in Phase 4 of the Qt Linguist
migration. After Phase 4 lands, run this script against an older commit of
this branch (or your own fork) to migrate any remaining YAML translation
files to .ts.

Example for a downstream packager with their own ru.yaml:
  python3 scripts/yaml_to_ts.py \\
      --input /path/to/ru.yaml \\
      --language ru \\
      --output resources/translations/sudoku_ru.ts
"""
import argparse
import re
import sys
from pathlib import Path
from xml.sax.saxutils import escape as xml_escape

import yaml

REPO_ROOT = Path(__file__).resolve().parent.parent

# Match: inline constexpr std::string_view <Name> = "<key>";
# Tolerant of line wrapping between = and "..." (for long key names).
KEY_RE = re.compile(
    r'inline\s+constexpr\s+std::string_view\s+(\w+)\s*=\s*"([^"]+)"\s*;',
    re.DOTALL,
)


def parse_keys(header_path: Path) -> list[tuple[str, str]]:
    """Return [(ConstantName, key), ...] in source order."""
    text = header_path.read_text()
    return [(m.group(1), m.group(2)) for m in KEY_RE.finditer(text)]


def load_strings(yaml_path: Path) -> dict[str, str]:
    data = yaml.safe_load(yaml_path.read_text())
    return data["strings"]


def emit_ts(out_path: Path, language: str, sourcelanguage: str,
            entries: list[tuple[str, str, str | None]]) -> None:
    lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<!DOCTYPE TS>',
        f'<TS version="2.1" language="{language}" sourcelanguage="{sourcelanguage}">',
        '<context>',
        '    <name>Sudoku</name>',
    ]
    for key, source, translation in entries:
        lines.append('    <message>')
        lines.append(f'        <source>{xml_escape(source)}</source>')
        lines.append(f'        <extracomment>Key: {xml_escape(key)}</extracomment>')
        if translation is None:
            lines.append('        <translation type="unfinished"></translation>')
        else:
            lines.append(f'        <translation>{xml_escape(translation)}</translation>')
        lines.append('    </message>')
    lines.append('</context>')
    lines.append('</TS>')
    lines.append('')
    out_path.write_text('\n'.join(lines))


def convert(keys_header: Path, source_yaml: Path, input_yaml: Path,
            output_ts: Path, language: str) -> None:
    keys = parse_keys(keys_header)
    source_strings = load_strings(source_yaml)
    input_strings = load_strings(input_yaml)

    print(f"Parsed {len(keys)} keys from {keys_header.name}", file=sys.stderr)
    print(f"Loaded {len(source_strings)} entries from {source_yaml.name} (source)",
          file=sys.stderr)
    print(f"Loaded {len(input_strings)} entries from {input_yaml.name} ({language})",
          file=sys.stderr)

    is_source_pass = (input_yaml.resolve() == source_yaml.resolve())
    entries: list[tuple[str, str, str | None]] = []
    missing_source: list[str] = []
    missing_translation: list[str] = []
    duplicate_sources: list[tuple[str, str, str]] = []
    seen_sources: dict[str, str] = {}

    for _const_name, key in keys:
        en_value = source_strings.get(key)
        loc_value = input_strings.get(key)
        if en_value is None:
            missing_source.append(key)
            continue
        if loc_value is None and not is_source_pass:
            missing_translation.append(key)
        if en_value in seen_sources:
            duplicate_sources.append((key, seen_sources[en_value], en_value))
            continue
        seen_sources[en_value] = key

        translation = None if is_source_pass else loc_value
        entries.append((key, en_value, translation))

    if missing_source:
        print(f"WARNING: {len(missing_source)} keys present in {keys_header.name} "
              f"but not in {source_yaml.name} (skipped):", file=sys.stderr)
        for k in missing_source:
            print(f"  - {k}", file=sys.stderr)
    if missing_translation:
        print(f"WARNING: {len(missing_translation)} keys missing translation in "
              f"{input_yaml.name} (emitted as <translation type=\"unfinished\">):",
              file=sys.stderr)
        for k in missing_translation:
            print(f"  - {k}", file=sys.stderr)
    if duplicate_sources:
        print(f"NOTE: {len(duplicate_sources)} keys deduped by English source "
              f"(single-context Qt Linguist; first occurrence wins).",
              file=sys.stderr)

    output_ts.parent.mkdir(parents=True, exist_ok=True)
    emit_ts(output_ts, language=language, sourcelanguage="en", entries=entries)
    print(f"Wrote {len(entries)} entries to {output_ts}", file=sys.stderr)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--keys", type=Path,
                   default=REPO_ROOT / "src" / "core" / "string_keys.h",
                   help="path to string_keys.h (default: in-tree)")
    p.add_argument("--source", type=Path,
                   default=REPO_ROOT / "resources" / "locales" / "en.yaml",
                   help="English source YAML (default: in-tree en.yaml)")
    p.add_argument("--input", type=Path,
                   help="translation YAML to convert (default: bulk en+de)")
    p.add_argument("--language",
                   help="locale code for the .ts header, e.g. 'ru' "
                        "(required if --input is given)")
    p.add_argument("--output", type=Path,
                   help="output .ts path (required if --input is given)")
    args = p.parse_args()

    if args.input is None:
        for lang, yaml_path in [
            ("en", REPO_ROOT / "resources" / "locales" / "en.yaml"),
            ("de", REPO_ROOT / "resources" / "locales" / "de.yaml"),
        ]:
            out = REPO_ROOT / "resources" / "translations" / f"sudoku_{lang}.ts"
            convert(args.keys, args.source, yaml_path, out, lang)
        return 0

    if not args.language or not args.output:
        p.error("--language and --output are required when --input is given")
    convert(args.keys, args.source, args.input, args.output, args.language)
    return 0


if __name__ == "__main__":
    sys.exit(main())
