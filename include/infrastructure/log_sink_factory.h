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
#include <memory>

#include <spdlog/sinks/basic_file_sink.h>

namespace sudoku::infrastructure {

/**
 * @brief Create a truncating file sink, or return nullptr if the file cannot be opened.
 *
 * Logging is a diagnostic, never a precondition for running. The application configures its logger
 * before the main window exists, so an exception escaping here reaches `main`'s handler, which can
 * only report through spdlog — with no logger installed and no console attached to a GUI binary,
 * that is a silent exit with no window at all. Returning an empty pointer lets the caller fall back
 * to console-only logging instead.
 *
 * The path can be unopenable for ordinary reasons: a read-only install directory, a sandbox, a
 * missing parent, or (on Windows) an account name the active code page cannot represent, which
 * makes the narrow conversion of the path lossy.
 */
[[nodiscard]] std::shared_ptr<spdlog::sinks::basic_file_sink_mt>
makeTruncatingFileSink(const std::filesystem::path& log_path);

}  // namespace sudoku::infrastructure
