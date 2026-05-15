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

#include "../../src/core/save_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
namespace fs = std::filesystem;

namespace {

class OriginTestFixture {
public:
    OriginTestFixture()
        : test_dir_("./test_origin_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())),
          save_manager_(test_dir_) {
        fs::create_directories(test_dir_);
    }

    ~OriginTestFixture() {
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    std::string test_dir_;
    SaveManager save_manager_;
};

// Hand-crafted pre-feature save YAML — captures the schema before the `origin` field existed.
// Stored verbatim so the deserializer's "missing key → default Generated" path is exercised.
constexpr const char* kLegacyYamlNoOrigin = R"(version: "1.0"
save_id: "legacy_test"
display_name: "Legacy save"
created_time: 1747350000
last_modified: 1747350000
puzzle_data:
  difficulty: 0
  puzzle_seed: 42
  original_puzzle:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
  current_state:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
progress:
  elapsed_time: 12345
  moves_made: 0
  hints_used: 0
  mistakes: 0
  is_complete: false
)";

}  // namespace

TEST_CASE("Loading a pre-feature save (no origin field) yields origin == Generated",
          "[integration][save_manager][origin][regression]") {
    OriginTestFixture fixture;

    // Arrange: drop a plain YAML save in the fixture's save directory.
    const std::string save_id = "legacy_test";
    auto save_path = fs::path(fixture.test_dir_) / (save_id + ".yaml");
    {
        std::ofstream out(save_path, std::ios::binary);
        REQUIRE(out.is_open());
        out << kLegacyYamlNoOrigin;
    }

    // Act
    auto loaded = fixture.save_manager_.loadGame(save_id);

    // Assert: load succeeds; missing `origin` key defaults to PuzzleOrigin::Generated.
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->origin == PuzzleOrigin::Generated);
}

TEST_CASE("Saving and loading a SavedGame round-trips the origin field", "[integration][save_manager][origin]") {
    OriginTestFixture fixture;

    // Arrange
    SavedGame game;
    game.save_id = "imported_test";
    game.display_name = "Imported test";
    game.original_puzzle = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                            {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                            {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};
    game.current_state = game.original_puzzle;
    game.difficulty = Difficulty::Easy;
    game.origin = PuzzleOrigin::ImportedString;

    // Inspectable YAML — no compression, no encryption (per R3 fixture convention).
    SaveSettings settings;
    settings.compress = false;
    settings.encrypt = false;

    // Act
    auto save_result = fixture.save_manager_.saveGame(game, settings);
    REQUIRE(save_result.has_value());
    auto loaded = fixture.save_manager_.loadGame(*save_result);

    // Assert
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->origin == PuzzleOrigin::ImportedString);
}

TEST_CASE("Default-constructed SavedGame has origin == Generated", "[save_manager][origin]") {
    SavedGame game;
    REQUIRE(game.origin == PuzzleOrigin::Generated);
}
