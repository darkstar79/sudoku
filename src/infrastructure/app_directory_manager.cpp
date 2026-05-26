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

#include "infrastructure/app_directory_manager.h"

#include <cstdlib>

namespace sudoku::infrastructure {

std::filesystem::path AppDirectoryManager::getDefaultDirectory(DirectoryType type) {
    auto base = getPlatformBaseDirectory();
    auto subdirectory = getSubdirectoryName(type);
    return base / subdirectory;
}

std::filesystem::path AppDirectoryManager::getPlatformBaseDirectory() {
#ifdef _WIN32
    // Windows: %APPDATA%/Sudoku
    auto appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::filesystem::path(appdata) / "Sudoku";
    }
    return std::filesystem::current_path();
#elifdef __APPLE__
    // macOS: ~/Library/Application Support/Sudoku
    auto* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "Sudoku";
    }
    return std::filesystem::current_path();
#else
    // Linux/Unix: XDG_DATA_HOME/sudoku (Flatpak-compatible), fallback to ~/.local/share/sudoku
    auto* xdg_data_home = std::getenv("XDG_DATA_HOME");
    if (xdg_data_home && xdg_data_home[0] != '\0') {
        return std::filesystem::path(xdg_data_home) / "sudoku";
    }
    auto* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".local" / "share" / "sudoku";
    }
    return std::filesystem::current_path();
#endif
}

std::string_view AppDirectoryManager::getSubdirectoryName(DirectoryType type) {
    using namespace std::string_view_literals;
    switch (type) {
        case DirectoryType::Saves:
            return "saves"sv;
        case DirectoryType::Statistics:
            return "stats"sv;
        case DirectoryType::TrainingStatistics:
            return "training"sv;
        case DirectoryType::Logs:
            return "logs"sv;
        default:
            return "data"sv;  // Fallback
    }
}

}  // namespace sudoku::infrastructure
