#!/usr/bin/env python3
"""One-shot migration: rewrite loc(KEY) -> core::loc("English literal").

Substitutes:
  loc(MenuGetCoachingHint)
    -> core::loc("Get Coaching Hint")
  loc(core::StringKeys::MenuGame)
    -> core::loc("Game")
  loc(cond ? ButtonClearNotes : ButtonFillNotes)
    -> core::loc(cond ? "Clear Notes" : "Fill Notes")
  locFormat(RatingFormat, fmt::format("{:.1f}", v))
    -> core::locFormat("SE {0}", fmt::format("{:.1f}", v))

Adds `#include "core/i18n_helpers.h"` to every modified file.

Uses a balanced-paren matcher (not pure regex) for locFormat so nested calls
like locFormat(K, foo().bar()) substitute correctly.

Throwaway script — delete after Phase 5.
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
    out: dict[str, str] = {}
    for const_name, key in keys:
        en = en_strings.get(key)
        if en is not None:
            out[const_name] = en
    return out


def cpp_escape(s: str) -> str:
    return (s.replace('\\', '\\\\')
             .replace('"', '\\"')
             .replace('\n', '\\n')
             .replace('\t', '\\t')
             .replace('\r', '\\r'))


def cpp_literal(s: str) -> str:
    return f'"{cpp_escape(s)}"'


def find_matching_paren(text: str, open_pos: int) -> int:
    """Given the index of an open '(', return the index of the matching ')'.
    Handles nested parens, character literals, and string literals.
    Raises IndexError if no match.
    """
    assert text[open_pos] == '('
    depth = 0
    i = open_pos
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            # skip string literal
            i += 1
            while i < n and text[i] != '"':
                if text[i] == '\\':
                    i += 2
                    continue
                i += 1
            i += 1
            continue
        if c == "'":
            # skip char literal
            i += 1
            while i < n and text[i] != "'":
                if text[i] == '\\':
                    i += 2
                    continue
                i += 1
            i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            # skip line comment
            while i < n and text[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            # skip block comment
            i += 2
            while i + 1 < n and not (text[i] == '*' and text[i + 1] == '/'):
                i += 1
            i += 2
            continue
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise IndexError(f"unmatched '(' at {open_pos}")


# Match a call to `name(` where `name` may be plain `loc` / `locFormat` (not core::loc).
# Captures the function name and the start of the argument list.
CALL_START_RE = re.compile(r'\b(loc|locFormat)\s*\(')


def replace_loc_calls(text: str, k2en: dict[str, str], log: list[str]) -> str:
    """Walk the file and replace each loc()/locFormat() call with core:: variant."""
    out_parts: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        m = CALL_START_RE.search(text, i)
        if m is None:
            out_parts.append(text[i:])
            break
        # Skip text up to the match.
        out_parts.append(text[i:m.start()])
        # Already a core:: prefix? skip.
        if m.start() >= 6 and text[m.start() - 6:m.start()] == 'core::':
            out_parts.append(text[m.start():m.end()])
            i = m.end()
            continue
        func = m.group(1)
        open_paren = m.end() - 1
        try:
            close_paren = find_matching_paren(text, open_paren)
        except IndexError:
            log.append(f"  unmatched paren at offset {open_paren}")
            out_parts.append(text[m.start():m.end()])
            i = m.end()
            continue
        # Argument substring (without the parens).
        arg_str = text[open_paren + 1:close_paren].strip()
        replacement = transform_call(func, arg_str, k2en, log)
        if replacement is None:
            # Couldn't transform — leave intact.
            out_parts.append(text[m.start():close_paren + 1])
        else:
            out_parts.append(replacement)
        i = close_paren + 1
    return ''.join(out_parts)


# Match a constant identifier (possibly qualified).
CONST_RE = re.compile(r'^(?:core::StringKeys::|StringKeys::)?(\w+)$')

# Match a ternary  cond ? IDENT1 : IDENT2  where cond may contain parens etc.
# We split on the FIRST top-level '?' and ':'.
def split_ternary(arg: str) -> tuple[str, str, str] | None:
    """Return (cond, branch1, branch2) if arg is a top-level ternary, else None."""
    depth = 0
    q_pos = -1
    i = 0
    n = len(arg)
    while i < n:
        c = arg[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        elif c == '?' and depth == 0:
            q_pos = i
            break
        i += 1
    if q_pos < 0:
        return None
    # Find the matching ':'  (skip ?: ternaries inside; for our usage this is safe).
    depth = 0
    c_pos = -1
    j = q_pos + 1
    while j < n:
        c = arg[j]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        elif c == ':' and depth == 0:
            c_pos = j
            break
        j += 1
    if c_pos < 0:
        return None
    return arg[:q_pos].strip(), arg[q_pos + 1:c_pos].strip(), arg[c_pos + 1:].strip()


def resolve_const(arg: str, k2en: dict[str, str]) -> str | None:
    m = CONST_RE.match(arg.strip())
    if not m:
        return None
    return k2en.get(m.group(1))


def transform_call(func: str, arg_str: str, k2en: dict[str, str],
                   log: list[str]) -> str | None:
    """Transform a single loc/locFormat call. Returns None if can't transform."""
    if func == 'loc':
        # Single-constant case.
        eng = resolve_const(arg_str, k2en)
        if eng is not None:
            return f'core::loc({cpp_literal(eng)})'
        # Ternary case: cond ? KEY1 : KEY2
        ternary = split_ternary(arg_str)
        if ternary is not None:
            cond, b1, b2 = ternary
            e1 = resolve_const(b1, k2en)
            e2 = resolve_const(b2, k2en)
            if e1 is not None and e2 is not None:
                return f'core::loc({cond} ? {cpp_literal(e1)} : {cpp_literal(e2)})'
            log.append(f"  loc(): unresolved ternary branches: {b1!r} / {b2!r}")
            return None
        log.append(f"  loc(): unresolved arg: {arg_str!r}")
        return None
    if func == 'locFormat':
        # Split on first top-level comma.
        depth = 0
        i = 0
        n = len(arg_str)
        while i < n:
            c = arg_str[i]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
            elif c == ',' and depth == 0:
                break
            i += 1
        if i >= n:
            log.append(f"  locFormat(): no comma found in {arg_str!r}")
            return None
        key_part = arg_str[:i].strip()
        rest = arg_str[i:]  # includes the leading comma
        eng = resolve_const(key_part, k2en)
        if eng is None:
            log.append(f"  locFormat(): unresolved key: {key_part!r}")
            return None
        return f'core::locFormat({cpp_literal(eng)}{rest})'
    return None


def add_include_if_needed(text: str) -> str:
    if 'core::loc(' not in text and 'core::locFormat(' not in text:
        return text
    if '"core/i18n_helpers.h"' in text:
        return text

    lines = text.splitlines(keepends=True)
    last_quote_include = -1
    last_angle_include = -1
    for i, line in enumerate(lines):
        if re.match(r'\s*#include\s+"', line):
            last_quote_include = i
        elif re.match(r'\s*#include\s+<', line):
            last_angle_include = i

    insert_at = last_quote_include if last_quote_include >= 0 else last_angle_include
    if insert_at < 0:
        return text

    new_line = '#include "core/i18n_helpers.h"\n'
    lines.insert(insert_at + 1, new_line)
    return ''.join(lines)


def process_file(path: Path, k2en: dict[str, str], dry_run: bool) -> tuple[int, list[str]]:
    text = path.read_text()
    if 'loc(' not in text and 'locFormat(' not in text:
        return 0, []

    log: list[str] = []
    new_text = replace_loc_calls(text, k2en, log)
    new_text = add_include_if_needed(new_text)

    if new_text == text:
        return 0, log

    n = (new_text.count('core::loc(') - text.count('core::loc(')
         + new_text.count('core::locFormat(') - text.count('core::locFormat('))

    if not dry_run:
        path.write_text(new_text)
    return n, log


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dry-run", action="store_true",
                   help="show counts but do not write")
    p.add_argument("paths", nargs='*',
                   help="files or directories to process (default: src/ tests/)")
    args = p.parse_args()

    k2en = build_constant_to_english()
    print(f"Loaded {len(k2en)} constant -> English mappings", file=sys.stderr)

    if args.paths:
        roots = [Path(p) for p in args.paths]
    else:
        roots = [REPO_ROOT / "src", REPO_ROOT / "tests"]

    files: list[Path] = []
    for root in roots:
        if root.is_file():
            files.append(root)
        else:
            files.extend(sorted(root.rglob("*.cpp")))
            files.extend(sorted(root.rglob("*.h")))

    total_subs = 0
    files_changed = 0
    issues: list[tuple[Path, list[str]]] = []
    for f in files:
        # Skip our own helper to avoid touching its parameter named `source`.
        if f.name == 'i18n_helpers.h':
            continue
        n, log = process_file(f, k2en, args.dry_run)
        if n > 0:
            files_changed += 1
            total_subs += n
            print(f"{f.relative_to(REPO_ROOT)}: {n} substitutions", file=sys.stderr)
        if log:
            issues.append((f, log))

    print(f"\nTotal: {total_subs} substitutions in {files_changed} files",
          file=sys.stderr)
    if issues:
        print(f"\nUnresolved patterns ({len(issues)} files):", file=sys.stderr)
        for f, log in issues:
            print(f"  {f.relative_to(REPO_ROOT)}:", file=sys.stderr)
            for m in log:
                print(m, file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
