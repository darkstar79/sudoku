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

/// Branch-coverage tests for AppDirectoryManager:
/// - getDefaultDirectory(DirectoryType::Logs) covers lines 40-41
/// - getDefaultDirectory(static_cast<DirectoryType>(99)) covers default (lines 42-43)

#include "infrastructure/app_directory_manager.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::infrastructure;

TEST_CASE("AppDirectoryManager - Logs directory type", "[app_directory_manager]") {
    // Covers lines 40-41: case DirectoryType::Logs → return "logs"sv
    auto path = AppDirectoryManager::getDefaultDirectory(DirectoryType::Logs);
    REQUIRE(!path.empty());
    // Path should end with "logs" subdirectory
    REQUIRE(path.filename() == "logs");
}

TEST_CASE("AppDirectoryManager - Saves directory type", "[app_directory_manager]") {
    auto path = AppDirectoryManager::getDefaultDirectory(DirectoryType::Saves);
    REQUIRE(!path.empty());
    REQUIRE(path.filename() == "saves");
}

TEST_CASE("AppDirectoryManager - Statistics directory type", "[app_directory_manager]") {
    auto path = AppDirectoryManager::getDefaultDirectory(DirectoryType::Statistics);
    REQUIRE(!path.empty());
    REQUIRE(path.filename() == "stats");
}

TEST_CASE("AppDirectoryManager - Invalid directory type uses default", "[app_directory_manager]") {
    // Covers lines 42-43: default case → return "data"sv
    auto path = AppDirectoryManager::getDefaultDirectory(static_cast<DirectoryType>(99));
    REQUIRE(!path.empty());
    REQUIRE(path.filename() == "data");
}
