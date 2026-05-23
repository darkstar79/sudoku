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
#include <string_view>
#include <vector>

namespace sudoku::core {

/// Validate a BCP 47 locale code subset: 2-3 lowercase language letters,
/// optionally followed by `_<2 uppercase letters>` (region).
///
/// Equivalent regex: ^[a-z]{2,3}(_[A-Z]{2})?$
///
/// Script subtags (e.g. `sr_Latn`) are intentionally not supported — Qt's
/// .ts/.qm tooling produces language-or-region forms by default and no
/// installed translation relies on a script subtag. If that ever changes,
/// loosen this gate at the same time.
///
/// Reject anything else, including path separators, shell metacharacters, and
/// embedded NULs. Used as a gate before any locale code reaches the
/// filesystem via `sudoku_<code>.qm` lookups, so it must be conservative.
[[nodiscard]] bool isValidLocaleCode(std::string_view code) noexcept;

/// Scan `translations_dir` (non-recursive) for `sudoku_<code>.qm` files and
/// return the sorted, deduplicated list of valid locale codes found. Filenames
/// whose code fails `isValidLocaleCode` are silently skipped. Returns an empty
/// vector if the directory does not exist.
[[nodiscard]] std::vector<std::string> scanInstalledLocales(const std::filesystem::path& translations_dir);

}  // namespace sudoku::core
