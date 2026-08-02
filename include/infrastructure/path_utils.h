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

#include <QString>

namespace sudoku::infrastructure {

/**
 * @brief Convert a Qt string to a filesystem path in the platform's native encoding.
 *
 * Use this instead of `std::filesystem::path(qstring.toStdString())`: `toStdString()` returns
 * UTF-8, but on Windows `std::filesystem::path` interprets a narrow string in the process's
 * active code page, so every non-ASCII character is mangled. That matters for any path derived
 * from the user profile — the per-user install tree lives under `%LOCALAPPDATA%`, so a profile
 * name like "Müller" reaches this code on an ordinary German-language machine.
 *
 * On Windows the conversion goes through `std::wstring` (the native wide encoding); elsewhere
 * paths are byte strings and the UTF-8 form is already native.
 */
[[nodiscard]] std::filesystem::path toFilesystemPath(const QString& value);

}  // namespace sudoku::infrastructure
