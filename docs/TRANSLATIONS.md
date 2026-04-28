# Translations

This project uses Qt's `.ts` translation files at runtime (loaded via
`QTranslator`) and Qt's standard `lupdate` tool to extract translatable
strings from C++ source.

## Source-code conventions

Two free helper functions in `src/core/i18n_helpers.h` wrap Qt's translation
API so non-`QObject` code (Model, ViewModel, free functions) can stay
non-`QObject`:

```cpp
#include "core/i18n_helpers.h"

// Simple lookup — use anywhere:
std::string s = sudoku::core::loc("Sudoku", "Open");

// Format with positional args (fmt-style {0}, {1}):
std::string fmt = sudoku::core::locFormat(
    sudoku::core::loc("Sudoku", "Score: {0}"),
    score
);
```

`core::loc(context, source)` has the same signature as
`QCoreApplication::translate(context, source)` — Qt's `lupdate` is told
about this equivalence via `-tr-function-alias translate+=loc` in
[CMakeLists.txt](../CMakeLists.txt), so it extracts every `core::loc("Sudoku", "...")`
call site as a translatable string with context `Sudoku`.

`core::locFormat` takes an already-translated string (typically the result
of `core::loc(...)`) plus format arguments. The split exists so the
literal source goes through `core::loc` where `lupdate` can see it. **The
first argument to `core::loc` must be the literal `"Sudoku"`** — every
call site uses the same context.

## Files

- **Source-of-truth `.ts` files**: `resources/translations/sudoku_<locale>.ts`
  - One `<context>Sudoku</context>` for the whole app.
  - English text is the message ID (no synthetic-key indirection).
  - Currently shipping: `en`, `de`.
- **Compiled `.qm` files**: produced at build time by `qt_add_lrelease`.
  - Linux install path: `${CMAKE_INSTALL_DATADIR}/sudoku/translations/`.
  - Flatpak: `<exe_dir>/translations/`.
  - Windows: `translations/` subfolder next to the executable.

## Translator workflow

Open the `.ts` file in **Qt Linguist** (the GUI editor that ships with
Qt 6), translate any entry with empty `<translation>` or
`type="unfinished"`, save. No `lupdate` needed for translation work —
that runs on the developer side when source code adds new strings.

```sh
# Linux: install Qt Linguist (GUI editor)
sudo dnf install qt6-linguist     # Fedora
sudo apt install linguist-qt6     # Debian/Ubuntu

# Open the German translation
linguist-qt6 resources/translations/sudoku_de.ts
```

After saving, rebuild — `qt_add_lrelease` compiles `.ts` → `.qm`
automatically and the next launch picks up the new translations.

To submit a translation: open a PR with the `.ts` diff. The CI
`i18n-check` job verifies completeness (no `type="unfinished"` entries,
no drift between source and `.ts`).

## Adding a new locale

1. Copy `sudoku_de.ts` to `sudoku_<lang>.ts` (e.g. `sudoku_ru.ts`).
2. Update the `<TS language="...">` attribute to the new language code.
3. Translate every `<translation>` element.
4. Add the language to `qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES ...)`
   in [CMakeLists.txt](../CMakeLists.txt).
5. Add the language code to the `kLocales` map in
   [src/view/main_window.cpp](../src/view/main_window.cpp) (Settings → Language dropdown).

If you have an existing translation in YAML format (e.g. an older
`sudoku_<lang>.yaml` maintained downstream), convert it with the helper
script:

```sh
python3 scripts/yaml_to_ts.py \
  --input sudoku_ru.yaml \
  --language ru \
  --output resources/translations/sudoku_ru.ts
```

## Packager workflow

Distribution packagers building from a release tarball don't need any Qt
translation tooling beyond what's already pulled in by `find_package(Qt6 ... LinguistTools)`
— `cmake --build` runs `lrelease` automatically and produces the `.qm`
files in the build directory.

If a packager wants to ship an additional locale not in the upstream
release, `scripts/yaml_to_ts.py` (kept in-tree for that purpose) converts
a YAML translation file to a `.ts` file ready for Qt Linguist.

## Contributor workflow

When you add a new user-visible string in C++ source via
`core::loc("Sudoku", "...")` or
`core::locFormat(core::loc("Sudoku", "..."), args...)`:

1. Run `cmake --build build/Release --target sudoku_lupdate`.
   This invokes `lupdate` and updates `sudoku_en.ts` / `sudoku_de.ts`
   with the new entry, marked `type="unfinished"`.
2. Open `sudoku_de.ts` in Qt Linguist and translate the new entry (or
   coordinate with a translator).
3. Commit both the source change and the `.ts` files in the same PR.

CI fails if:

- Running `lupdate` would change the `.ts` files (drift between source and
  translations), **or**
- `sudoku_de.ts` contains any `<translation type="unfinished">` entries.

The `i18n-check` GitHub Actions job at
[.github/workflows/ci.yml](../.github/workflows/ci.yml) enforces both.

### A note on the source language

`sudoku_en.ts` intentionally has every entry as
`<translation type="unfinished"></translation>` — English is the source
language and Qt's `QTranslator` falls back to the `<source>` text when
the matching translation is empty. The CI's `unfinished` check runs only
against non-English locales (`sudoku_de.ts` and any future
`sudoku_<lang>.ts` files).

### A note on removed call sites (`-no-obsolete`)

`qt_add_lupdate` is configured with `-no-obsolete` in
[CMakeLists.txt](../CMakeLists.txt). When a `core::loc(...)` call site is
deleted from the source, the matching `<message>` block is **stripped
from every `.ts` file** the next time `lupdate` runs — including
translations a contributor has invested time in. The CI drift check
catches the removal (the `.ts` diff fails `git diff --exit-code`), so
the removal lands deliberately rather than silently, but the
translation text is gone from the working tree once the change is
committed. If you decide later that the string should come back, recover
the translation via `git log -p resources/translations/sudoku_<lang>.ts`
and paste it manually — there is no `<translation type="obsolete">`
fallback the way default `lupdate` provides.

The trade-off is intentional: keeping obsolete markers around indefinitely
clutters the file as the codebase evolves, and Qt Linguist surfaces them
under a separate "Obsolete" section that translators tend to ignore. If
you find yourself recovering translations from git history regularly,
consider dropping `-no-obsolete` from the `qt_add_lupdate` `OPTIONS`
block.

## Local verification

```sh
# Rebuild + run lupdate + diff
cmake --build build/Release --target sudoku_lupdate
git diff resources/translations/

# Check for unfinished German translations
grep 'type="unfinished"' resources/translations/sudoku_de.ts
```

Empty diff and zero unfinished entries means the source and translations
are in sync.
