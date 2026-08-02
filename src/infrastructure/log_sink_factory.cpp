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

#include "infrastructure/log_sink_factory.h"

#include <exception>

namespace sudoku::infrastructure {

std::shared_ptr<spdlog::sinks::basic_file_sink_mt> makeTruncatingFileSink(const std::filesystem::path& log_path) {
    try {
        // path::string() is itself a narrow conversion that can fail for the very paths that make
        // this function necessary, so it stays inside the try along with the file open.
        return std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);  // truncate=true
    } catch (const std::exception&) {
        return nullptr;
    }
}

}  // namespace sudoku::infrastructure
