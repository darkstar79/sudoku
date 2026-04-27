#!/usr/bin/env python3
"""One-shot Phase-4 migration: rewrite <ident>.getString(KEY) -> core::loc("English").

This is the companion to migrate_loc_to_tr.py for the table-lookup helper
functions in src/core/{solving_technique,technique_descriptions,localized_explanations,training_hints}*
that took an `ILocalizationManager&` argument and called `.getString(KEY)` directly.

After running this script, the function signatures still have the unused
`const ILocalizationManager& loc` parameter — they must be hand-edited to drop
the parameter, change return types where needed (string_view -> string), and
update callers. This script only does the mechanical literal substitution.

Throwaway script — delete after Phase 5 lands.
"""
import argparse
import re
import sys
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parent.parent
KEYS_HEADER = REPO_ROOT / "src" / "core" / "string_keys.h"
EN_YAML = REPO_ROOT / "resources" / "locales" / "en.yaml"

KEY_DEF_RE = re.compile(
    r'inline\s+constexpr\s+std::string_view\s+(\w+)\s*=\s*"([^"]+)"\s*;',
    re.DOTALL,
)


def build_constant_to_english() -> dict[str, str]:
    keys = [(m.group(1), m.group(2)) for m in KEY_DEF_RE.finditer(KEYS_HEADER.read_text())]
    en_strings = yaml.safe_load(EN_YAML.read_text())["strings"]
    return {n: en_strings[k] for n, k in keys if k in en_strings}


def cpp_literal(s: str) -> str:
    escaped = (s.replace('\\', '\\\\').replace('"', '\\"')
                .replace('\n', '\\n').replace('\t', '\\t'))
    return f'"{escaped}"'


# Match <ident>(.|->) getString( (qualifier)? <KEY> )
GETSTRING_RE = re.compile(
    r'\b\w+\s*(?:\.|->)\s*getString\s*\(\s*(?:core::)?(?:StringKeys::)?(\w+)\s*\)'
)


def process_file(path: Path, k2en: dict[str, str], dry_run: bool) -> tuple[int, list[str]]:
    text = path.read_text()
    if 'getString(' not in text:
        return 0, []
    log: list[str] = []
    n_subs = 0

    def repl(m: re.Match) -> str:
        nonlocal n_subs
        const_name = m.group(1)
        if const_name not in k2en:
            log.append(f"  unknown constant: {const_name}")
            return m.group(0)
        n_subs += 1
        return f'core::loc({cpp_literal(k2en[const_name])})'

    new_text = GETSTRING_RE.sub(repl, text)
    if new_text == text:
        return 0, log

    # Add include if needed.
    if 'core::loc(' in new_text and '"core/i18n_helpers.h"' not in new_text:
        lines = new_text.splitlines(keepends=True)
        # Find last #include to insert after.
        last_inc = -1
        for i, line in enumerate(lines):
            if re.match(r'\s*#include\s+["<]', line):
                last_inc = i
        if last_inc >= 0:
            lines.insert(last_inc + 1, '#include "core/i18n_helpers.h"\n')
            new_text = ''.join(lines)

    if not dry_run:
        path.write_text(new_text)
    return n_subs, log


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("paths", nargs='+', help="files to process")
    args = p.parse_args()

    k2en = build_constant_to_english()
    print(f"Loaded {len(k2en)} constant -> English mappings", file=sys.stderr)

    total = 0
    for path_str in args.paths:
        path = Path(path_str).resolve()
        n, log = process_file(path, k2en, args.dry_run)
        if n > 0:
            print(f"{path.relative_to(REPO_ROOT)}: {n} substitutions", file=sys.stderr)
            total += n
        for msg in log:
            print(f"{path.relative_to(REPO_ROOT)}:{msg}", file=sys.stderr)
    print(f"\nTotal: {total} substitutions", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
