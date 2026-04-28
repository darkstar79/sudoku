#!/usr/bin/env python3
"""Unit tests for scripts/check_translation_placeholders.py.

Run via `python3 -m scripts.tests.test_check_translation_placeholders` from
the project root, or as a CI step. No third-party dependencies.
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Allow `python3 scripts/tests/test_check_translation_placeholders.py` from
# the repo root by adding scripts/ to sys.path.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from check_translation_placeholders import (  # noqa: E402
    check_ts_file,
    extract_placeholders,
    main,
)


def write_ts(path: Path, body: str) -> None:
    path.write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<!DOCTYPE TS>\n'
        '<TS version="2.1" language="de" sourcelanguage="en">\n'
        '<context>\n'
        '    <name>Sudoku</name>\n'
        f'{body}\n'
        '</context>\n'
        '</TS>\n'
    )


def message(source: str, translation: str, *, unfinished: bool = False) -> str:
    attr = ' type="unfinished"' if unfinished else ""
    return (
        '    <message>\n'
        f'        <source>{source}</source>\n'
        f'        <translation{attr}>{translation}</translation>\n'
        '    </message>'
    )


class ExtractPlaceholdersTest(unittest.TestCase):
    def test_no_placeholders(self):
        counts, err = extract_placeholders("Hello world")
        self.assertEqual(counts, {})
        self.assertIsNone(err)

    def test_positional_indices(self):
        counts, err = extract_placeholders("Score: {0} of {1}")
        self.assertEqual(counts, {"0": 1, "1": 1})
        self.assertIsNone(err)

    def test_format_spec_does_not_change_field_name(self):
        counts, err = extract_placeholders("Rate: {0:.1f}%")
        self.assertEqual(counts, {"0": 1})
        self.assertIsNone(err)

    def test_repeated_index(self):
        counts, err = extract_placeholders("{0} and {0} again")
        self.assertEqual(counts, {"0": 2})
        self.assertIsNone(err)

    def test_double_brace_escaped(self):
        counts, err = extract_placeholders("Use {{0}} as a literal")
        self.assertEqual(counts, {})
        self.assertIsNone(err)

    def test_unmatched_brace_is_error(self):
        _counts, err = extract_placeholders("Score: {0")
        self.assertIsNotNone(err)


class CheckTsFileTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(self.id() + ".ts")

    def tearDown(self):
        if self.tmp.exists():
            self.tmp.unlink()

    def test_matching_placeholders_passes(self):
        write_ts(self.tmp, message("Score: {0}", "Punkte: {0}"))
        self.assertEqual(check_ts_file(self.tmp), [])

    def test_missing_placeholder_fails(self):
        write_ts(self.tmp, message("Score: {0} of {1}", "Punkte: {0}"))
        errs = check_ts_file(self.tmp)
        self.assertEqual(len(errs), 1)
        self.assertIn("placeholder mismatch", errs[0])

    def test_extra_placeholder_fails(self):
        write_ts(self.tmp, message("Score: {0}", "Punkte: {0} {1}"))
        errs = check_ts_file(self.tmp)
        self.assertEqual(len(errs), 1)
        self.assertIn("placeholder mismatch", errs[0])

    def test_renumbered_placeholder_fails(self):
        write_ts(self.tmp, message("Got {0} from {1}", "{1} hat {0}"))
        # Multisets are equal here — both have one {0} and one {1} —
        # so this *passes*. That's the intended trade-off: positional
        # reordering for grammar reasons is legitimate.
        self.assertEqual(check_ts_file(self.tmp), [])

    def test_format_spec_preserved(self):
        write_ts(self.tmp, message("Rate: {0:.1f}%", "Quote: {0:.1f}%"))
        self.assertEqual(check_ts_file(self.tmp), [])

    def test_format_spec_lost_passes(self):
        # We only check field-name multisets; losing the format spec is bad
        # but does not cause a fmt::format_error, so we don't flag it.
        write_ts(self.tmp, message("Rate: {0:.1f}%", "Quote: {0}%"))
        self.assertEqual(check_ts_file(self.tmp), [])

    def test_malformed_translation_fails(self):
        write_ts(self.tmp, message("Score: {0}", "Punkte: {0"))
        errs = check_ts_file(self.tmp)
        self.assertEqual(len(errs), 1)
        self.assertIn("malformed", errs[0])

    def test_unfinished_entries_skipped(self):
        write_ts(self.tmp, message("Score: {0}", "", unfinished=True))
        self.assertEqual(check_ts_file(self.tmp), [])

    def test_empty_translation_skipped(self):
        write_ts(self.tmp, message("Score: {0}", ""))
        self.assertEqual(check_ts_file(self.tmp), [])

    def test_named_field_mismatch_fails(self):
        # Translator changed {0} → {name}.
        write_ts(self.tmp, message("Score: {0}", "Punkte: {name}"))
        errs = check_ts_file(self.tmp)
        self.assertEqual(len(errs), 1)
        self.assertIn("placeholder mismatch", errs[0])

    def test_non_positional_source_skipped(self):
        # Source is plain prose containing literal Sudoku notation in braces.
        # The call site is core::loc (not locFormat), so the {A,B} in the
        # translation is harmless display text — must not be flagged.
        write_ts(self.tmp, message(
            "Find cells sharing a candidate pair.",
            "Suche nach Zellen mit dem Kandidatenpaar {A,B}."))
        self.assertEqual(check_ts_file(self.tmp), [])

    def test_malformed_source_reported(self):
        # If the source itself is malformed, that's a code bug — flag it
        # regardless of placeholder presence so it surfaces in CI.
        write_ts(self.tmp, message("Score: {0", "Punkte: {0"))
        errs = check_ts_file(self.tmp)
        self.assertEqual(len(errs), 1)
        self.assertIn("<source>", errs[0])


class CliTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(self.id() + ".ts")

    def tearDown(self):
        if self.tmp.exists():
            self.tmp.unlink()

    def test_passing_file_returns_zero(self):
        write_ts(self.tmp, message("Score: {0}", "Punkte: {0}"))
        self.assertEqual(main([str(self.tmp)]), 0)

    def test_failing_file_returns_one(self):
        write_ts(self.tmp, message("Score: {0}", "Punkte: {0} {1}"))
        self.assertEqual(main([str(self.tmp)]), 1)


if __name__ == "__main__":
    unittest.main()
