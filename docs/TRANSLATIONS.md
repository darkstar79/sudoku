# Translations

This project uses Qt's `.ts` translation files at runtime (loaded via
`QTranslator`) but uses two free helper functions instead of Qt's class-bound
`tr()`/`translate()` for the call sites in C++ code:

```cpp
#include "core/i18n_helpers.h"

std::string s    = sudoku::core::loc("Open");                       // simple lookup
std::string fmt  = sudoku::core::locFormat("Score: {0}", score);    // fmt-style format
```

The helpers internally call `QCoreApplication::translate("Sudoku", source)`,
so the runtime side is fully Qt-native. The reason for the wrappers is that
most of our translatable strings live in non-`QObject` code (free functions,
plain structs, view-model helpers) where `tr()` would not compile.

## Files

- **Source-of-truth `.ts` files**: `resources/translations/sudoku_<locale>.ts`
  - One `<context>Sudoku</context>` for the whole app.
  - English text is the message ID (we don't use synthetic keys).
  - Currently shipping: `en`, `de`.
- **Compiled `.qm` files**: produced at build time by `qt_add_lrelease`.
  - Linux install path: `${CMAKE_INSTALL_DATADIR}/sudoku/translations/`.
  - Flatpak: `<exe_dir>/translations/`.
  - Windows: `translations/` subfolder next to the executable.

## Translator workflow

Open the `.ts` file directly in **Qt Linguist** (the GUI editor that ships
with Qt 6), translate any entry that is empty or marked `unfinished`, and
save. No `lupdate` invocation is needed for translation work — you only
need it when the source code adds new strings (see "Contributor workflow"
below).

```sh
# Linux: install Qt Linguist (GUI editor)
sudo dnf install qt6-linguist     # Fedora
sudo apt install linguist-qt6     # Debian/Ubuntu

# Open the German translation
linguist-qt6 resources/translations/sudoku_de.ts
```

After saving, rebuild the project — `qt_add_lrelease` compiles your `.ts`
edits into `.qm` files automatically and the next launch picks up the new
translations.

To submit a translation: open a PR with the `.ts` diff. CI's
`i18n-check` job will verify completeness (no `unfinished` entries, no
orphan strings).

## Adding a new locale

1. Copy `sudoku_de.ts` to `sudoku_<lang>.ts` (e.g. `sudoku_ru.ts`).
2. Update the `<TS language="...">` attribute to the new language code.
3. Translate every `<translation>` element.
4. Add the language to `qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES ...)`
   in `CMakeLists.txt`.
5. Add the language code to the `kLocales` map in
   `src/view/main_window.cpp` (Settings → Language dropdown).

If you have an external translation in YAML format (e.g. an older
`sudoku_<lang>.yaml`), convert it with the helper script:

```sh
python3 scripts/yaml_to_ts.py \
  --input sudoku_ru.yaml \
  --language ru \
  --output resources/translations/sudoku_ru.ts
```

## Packager workflow

Distribution packagers building from a release tarball don't need any Qt
translation tooling — `cmake --build` runs `lrelease` automatically and
produces the `.qm` files in the build directory. Just install them as
normal.

If a packager wants to ship an additional locale not in the release
(e.g. a downstream `sudoku_<lang>.yaml` they maintain), they can use
`scripts/yaml_to_ts.py` (kept in-tree for that purpose) to convert it.

## Contributor workflow

When you add a new user-visible string to the source code via
`core::loc("...")` or `core::locFormat("...", ...)`, you must also add
the corresponding entry to **both** `.ts` files. The CI `i18n-check`
job will fail your PR otherwise.

```xml
<message>
    <source>Your new string here</source>
    <translation>Ihre neue Zeichenfolge hier</translation>
</message>
```

The first argument to `core::loc`/`core::locFormat` **must be a string
literal** — not a variable, not a `const char*` parameter, not an
expression. The CI scanner walks the source tree and extracts the literal
arguments to verify the `.ts` files cover them; non-literal arguments are
flagged as warnings.

If you must select between strings at runtime, ternary the *call*, not
the literal:

```cpp
// Bad — scanner can't see the strings
const char* s = condition ? "Open" : "Close";
core::loc(s);

// Good — both literals visible at the call site
core::loc(condition ? "Open" : "Close");

// Also good
condition ? core::loc("Open") : core::loc("Close");
```

## Local verification

```sh
python3 scripts/check_translations.py
```

This runs the same check the CI job runs. It reports:

- **MISSING** — string used in code but absent from a `.ts` file.
- **ORPHAN** — entry in a `.ts` file with no matching call site.
- **UNFINISHED** — `<translation type="unfinished">` in `sudoku_de.ts`.

The `unfinished` check intentionally runs only against the non-English locales:
in `sudoku_en.ts` every entry is `<translation type="unfinished"></translation>`
because English is the source language and Qt's `QTranslator` falls back to the
`<source>` text when the matching translation is empty. Translators only need
to fill in the non-English files.

## Why a custom check instead of `lupdate`?

Qt's standard `lupdate` extractor only recognizes built-in patterns
(`tr(...)`, `QObject::tr(...)`, `translate(ctx, src)`,
`QT_TRANSLATE_NOOP(ctx, src)`, ...). Our `core::loc(source)` helper takes
just the source string — the `"Sudoku"` context is supplied inside the
helper body, where `lupdate` does not look. Teaching `lupdate` to recognize
our helpers is non-trivial in Qt 6, so `scripts/check_translations.py`
implements an extractor that knows about exactly our two helpers.

Future work (low priority): switch the helpers to a macro that expands
to `QT_TRANSLATE_NOOP("Sudoku", source)` so `lupdate` can see the strings,
then drop the custom script. Not blocking for shipping.
