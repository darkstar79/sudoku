// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <QString>

namespace sudoku::view {

struct LocaleEntry {
    std::string code;
    QString native_name;
};

/// Resolve the directory containing `sudoku_<code>.qm` files. Checks the
/// build-tree layout (next to the executable) first, then the FHS install
/// layout (`../share/sudoku/translations`). Returns an empty path if neither
/// exists.
[[nodiscard]] std::filesystem::path findTranslationsDir();

/// Build the list of locales offered in the Settings combo. Combines a
/// filesystem scan of `findTranslationsDir()` with `QLocale::nativeLanguageName()`
/// for display strings, and always includes `"en"` (the source language)
/// even if no `.qm` files were found. Entries are sorted by code.
[[nodiscard]] std::vector<LocaleEntry> discoverInstalledLocales();

}  // namespace sudoku::view
