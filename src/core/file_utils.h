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

#include <cstdint>
#include <expected>
#include <filesystem>
#include <system_error>
#include <vector>

namespace sudoku::core::file_utils {

/// Writes @p data to @p target atomically, so a crash or power loss can never
/// leave a truncated or partially written file in place.
///
/// The bytes are written to a sibling `<target>.tmp`, fsync'd to durable
/// storage, then `rename`d over @p target. Because rename is atomic on POSIX
/// filesystems, a reader sees either the old file or the fully written new one
/// — never a half-written mix. On any failure the temp file is removed and the
/// existing @p target is left untouched.
///
/// @param target Final destination path.
/// @param data   Raw bytes to write (may be empty).
/// @return Nothing on success, or the failing operation's std::error_code.
///         Callers map this to their own domain error type.
[[nodiscard]] std::expected<void, std::error_code> atomicWriteFile(const std::filesystem::path& target,
                                                                   const std::vector<uint8_t>& data);

}  // namespace sudoku::core::file_utils
