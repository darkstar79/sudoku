// sudoku-cpp - Offline Sudoku Game
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
#include <string_view>

namespace sudoku::infrastructure {

/**
 * @brief Manages platform-specific application data directories.
 *
 * This class provides a centralized way to determine the appropriate directory
 * for storing application data (saves, statistics, logs) based on the operating
 * system and environment.
 *
 * Eliminates 100% code duplication between SaveManager and StatisticsManager.
 */
enum class DirectoryType {
    Saves,               ///< Game save files
    Statistics,          ///< Statistics and progress tracking
    TrainingStatistics,  ///< Training mode progress tracking
    Logs                 ///< Application logs (future use)
};

class AppDirectoryManager {
public:
    /**
     * @brief Get the default directory for a specific type of application data.
     *
     * On Windows: %APPDATA%/Sudoku/{subdirectory}
     * On Linux/Unix: $XDG_DATA_HOME/sudoku/{subdirectory} (or ~/.local/share/sudoku/{subdirectory})
     * Fallback: ./​{subdirectory} (current directory)
     *
     * @param type Type of directory to retrieve
     * @return Platform-appropriate filesystem path
     */
    [[nodiscard]] static std::filesystem::path getDefaultDirectory(DirectoryType type);

private:
    /**
     * @brief Get the base application data directory for the current platform.
     *
     * @return Platform-specific base directory
     */
    [[nodiscard]] static std::filesystem::path getPlatformBaseDirectory();

    /**
     * @brief Get the subdirectory name for a specific directory type.
     *
     * @param type Directory type
     * @return Subdirectory name ("saves", "stats", "logs")
     */
    [[nodiscard]] static std::string_view getSubdirectoryName(DirectoryType type);
};

}  // namespace sudoku::infrastructure
