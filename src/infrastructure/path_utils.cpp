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

#include "infrastructure/path_utils.h"

namespace sudoku::infrastructure {

std::filesystem::path toFilesystemPath(const QString& value) {
#ifdef _WIN32
    // Wide, not narrow: MSVC decodes a narrow string using the active code page, which is not UTF-8
    // by default, so QString::toStdString() bytes would be reinterpreted and any non-ASCII path
    // component destroyed.
    return {value.toStdWString()};
#else
    // POSIX paths are byte strings and the native encoding is UTF-8 on every platform we target.
    return {value.toStdString()};
#endif
}

}  // namespace sudoku::infrastructure
