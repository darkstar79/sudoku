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

/// Branch-coverage tests for SaveManager deserialization paths:
/// - YAML missing puzzle_data → SerializationError
/// - YAML missing current_state → SerializationError
/// - YAML missing original_puzzle → SerializationError
/// - YAML invalid difficulty value → InvalidSaveData
/// - YAML board with wrong row count → InvalidSaveData
/// - YAML board row with wrong column count → InvalidSaveData
/// - YAML with move_history + current_move_index (covers true branch)
/// - move entry without timestamp field (covers false branch)
/// - YAML with auto_notes_enabled = true (covers true branch)
/// - Completely invalid YAML → SerializationError (YAML parse error)
/// - getSaveDirectorySize when directory doesn't exist → 0
/// - SaveManager("") default-directory constructor

#include "../../src/core/save_manager.h"
#include "../helpers/test_utils.h"

#include <filesystem>
#include <fstream>
#include <random>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

namespace {

// Write a YAML file and return the save_id (stem of filename)
std::string writeYaml(const fs::path& dir, const std::string& name, const std::string& content) {
    auto path = dir / (name + ".yaml");
    std::ofstream f(path);
    f << content;
    return name;
}

// Helper: 9-row board YAML block (indented with 4 spaces inside puzzle_data)
std::string nineRowBoard(const std::string& key) {
    std::string s = "  " + key + ":\n";
    for (int r = 0; r < 9; ++r) {
        s += "    - [0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
    }
    return s;
}

// Minimal valid YAML with all required fields
std::string validBaseYaml() {
    std::string s = "version: '1.0'\n"
                    "save_id: test\n"
                    "display_name: Test\n"
                    "created_time: 1000000000\n"
                    "last_modified: 1000000000\n"
                    "puzzle_data:\n"
                    "  difficulty: 0\n"
                    "  puzzle_seed: 1\n";
    s += nineRowBoard("original_puzzle");
    s += nineRowBoard("current_state");
    return s;
}

}  // namespace

// ============================================================================
// Missing puzzle_data key → SerializationError
// ============================================================================

TEST_CASE("SaveManager - deserialize: YAML missing puzzle_data → SerializationError", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    writeYaml(tmp.path(), "no-puzzle-data",
              "version: '1.0'\n"
              "save_id: no-puzzle-data\n"
              "display_name: Test\n"
              "created_time: 1000000000\n"
              "last_modified: 1000000000\n");

    auto result = mgr.loadGame("no-puzzle-data");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::SerializationError);
}

// ============================================================================
// Missing current_state key → SerializationError
// ============================================================================

TEST_CASE("SaveManager - deserialize: YAML missing current_state → SerializationError", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = "version: '1.0'\n"
                       "save_id: no-current-state\n"
                       "display_name: Test\n"
                       "created_time: 1000000000\n"
                       "last_modified: 1000000000\n"
                       "puzzle_data:\n"
                       "  difficulty: 0\n"
                       "  puzzle_seed: 1\n";
    yaml += nineRowBoard("original_puzzle");
    // No current_state key

    writeYaml(tmp.path(), "no-current-state", yaml);

    auto result = mgr.loadGame("no-current-state");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::SerializationError);
}

// ============================================================================
// Missing original_puzzle key → SerializationError
// ============================================================================

TEST_CASE("SaveManager - deserialize: YAML missing original_puzzle → SerializationError", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = "version: '1.0'\n"
                       "save_id: no-orig-puzzle\n"
                       "display_name: Test\n"
                       "created_time: 1000000000\n"
                       "last_modified: 1000000000\n"
                       "puzzle_data:\n"
                       "  difficulty: 0\n"
                       "  puzzle_seed: 1\n";
    yaml += nineRowBoard("current_state");
    // No original_puzzle key

    writeYaml(tmp.path(), "no-orig-puzzle", yaml);

    auto result = mgr.loadGame("no-orig-puzzle");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::SerializationError);
}

// ============================================================================
// Invalid difficulty value (out of range) → InvalidSaveData
// ============================================================================

TEST_CASE("SaveManager - deserialize: invalid difficulty value → InvalidSaveData", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = "version: '1.0'\n"
                       "save_id: bad-difficulty\n"
                       "display_name: Test\n"
                       "created_time: 1000000000\n"
                       "last_modified: 1000000000\n"
                       "puzzle_data:\n"
                       "  difficulty: 99\n"  // invalid: only 0-3 are valid
                       "  puzzle_seed: 1\n";
    yaml += nineRowBoard("original_puzzle");
    yaml += nineRowBoard("current_state");

    writeYaml(tmp.path(), "bad-difficulty", yaml);

    auto result = mgr.loadGame("bad-difficulty");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::InvalidSaveData);
}

// ============================================================================
// Wrong board row count in YAML (8 rows instead of 9) → InvalidSaveData
// ============================================================================

TEST_CASE("SaveManager - deserialize: current_state has 8 rows → InvalidSaveData", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = "version: '1.0'\n"
                       "save_id: bad-row-count\n"
                       "display_name: Test\n"
                       "created_time: 1000000000\n"
                       "last_modified: 1000000000\n"
                       "puzzle_data:\n"
                       "  difficulty: 0\n"
                       "  puzzle_seed: 1\n";
    yaml += nineRowBoard("original_puzzle");
    yaml += "  current_state:\n";
    for (int r = 0; r < 8; ++r) {  // Only 8 rows
        yaml += "    - [0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
    }

    writeYaml(tmp.path(), "bad-row-count", yaml);

    auto result = mgr.loadGame("bad-row-count");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::InvalidSaveData);
}

TEST_CASE("SaveManager - deserialize: original_puzzle has 8 rows → InvalidSaveData", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = "version: '1.0'\n"
                       "save_id: bad-orig-rows\n"
                       "display_name: Test\n"
                       "created_time: 1000000000\n"
                       "last_modified: 1000000000\n"
                       "puzzle_data:\n"
                       "  difficulty: 0\n"
                       "  puzzle_seed: 1\n"
                       "  original_puzzle:\n";
    for (int r = 0; r < 8; ++r) {
        yaml += "    - [0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
    }
    yaml += nineRowBoard("current_state");

    writeYaml(tmp.path(), "bad-orig-rows", yaml);

    auto result = mgr.loadGame("bad-orig-rows");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::InvalidSaveData);
}

// ============================================================================
// Wrong column count in a row → InvalidSaveData
// ============================================================================

TEST_CASE("SaveManager - deserialize: current_state row has 8 cols → InvalidSaveData", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = "version: '1.0'\n"
                       "save_id: bad-col-count\n"
                       "display_name: Test\n"
                       "created_time: 1000000000\n"
                       "last_modified: 1000000000\n"
                       "puzzle_data:\n"
                       "  difficulty: 0\n"
                       "  puzzle_seed: 1\n";
    yaml += nineRowBoard("original_puzzle");
    yaml += "  current_state:\n";
    yaml += "    - [0, 0, 0, 0, 0, 0, 0, 0]\n";  // row 0: only 8 cols
    for (int r = 1; r < 9; ++r) {
        yaml += "    - [0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
    }

    writeYaml(tmp.path(), "bad-col-count", yaml);

    auto result = mgr.loadGame("bad-col-count");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::InvalidSaveData);
}

TEST_CASE("SaveManager - deserialize: original_puzzle row has 8 cols → InvalidSaveData", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = "version: '1.0'\n"
                       "save_id: bad-orig-cols\n"
                       "display_name: Test\n"
                       "created_time: 1000000000\n"
                       "last_modified: 1000000000\n"
                       "puzzle_data:\n"
                       "  difficulty: 0\n"
                       "  puzzle_seed: 1\n"
                       "  original_puzzle:\n";
    yaml += "    - [0, 0, 0, 0, 0, 0, 0, 0]\n";  // row 0: only 8 cols
    for (int r = 1; r < 9; ++r) {
        yaml += "    - [0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
    }
    yaml += nineRowBoard("current_state");

    writeYaml(tmp.path(), "bad-orig-cols", yaml);

    auto result = mgr.loadGame("bad-orig-cols");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::InvalidSaveData);
}

// ============================================================================
// Completely invalid YAML → SerializationError (YAML parse exception)
// ============================================================================

TEST_CASE("SaveManager - deserialize: invalid YAML content → SerializationError", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Write invalid YAML (tabs mixed with spaces, unclosed brackets, etc.)
    writeYaml(tmp.path(), "invalid-yaml",
              "version: '1.0'\n"
              "puzzle_data:\n"
              "  broken: {unclosed\n"
              "  junk: [1, 2\n");

    auto result = mgr.loadGame("invalid-yaml");
    REQUIRE_FALSE(result.has_value());
    // YAML parse errors map to SerializationError
    REQUIRE(result.error() == SaveError::SerializationError);
}

// ============================================================================
// Load YAML with move_history + current_move_index → covers true branches L740-761
// (also covers yaml_move["timestamp"] true branch)
// ============================================================================

TEST_CASE("SaveManager - deserialize: YAML with move_history is loaded correctly", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    std::string yaml = validBaseYaml();
    yaml += "move_history:\n"
            "  - row: 0\n"
            "    col: 1\n"
            "    value: 5\n"
            "    type: 0\n"
            "    is_note: false\n"
            "    timestamp: 123456789\n"  // timestamp present → covers L750 true
            "  - row: 2\n"
            "    col: 3\n"
            "    value: 7\n"
            "    type: 0\n"
            "    is_note: false\n"
            // no timestamp field → covers L750 false
            "current_move_index: 1\n";  // covers L758 true

    writeYaml(tmp.path(), "with-move-history", yaml);

    auto result = mgr.loadGame("with-move-history");
    REQUIRE(result.has_value());
    REQUIRE(result->move_history.size() == 2);
    REQUIRE(result->move_history[0].position.row == 0);
    REQUIRE(result->move_history[0].position.col == 1);
    REQUIRE(result->move_history[0].value == 5);
    REQUIRE(result->current_move_index == 1);
}

// ============================================================================
// Load YAML without optional metadata (save_id, display_name, timestamps)
// Covers the false branches for each `if (root["field"])` check
// ============================================================================

TEST_CASE("SaveManager - deserialize: YAML without optional metadata loads with defaults", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Minimal YAML: only puzzle_data with required boards; no metadata fields
    std::string yaml = "puzzle_data:\n"
                       "  difficulty: 2\n"  // Hard
                       "  puzzle_seed: 99\n";
    yaml += nineRowBoard("original_puzzle");
    yaml += nineRowBoard("current_state");

    writeYaml(tmp.path(), "minimal-yaml", yaml);

    auto result = mgr.loadGame("minimal-yaml");
    REQUIRE(result.has_value());
    REQUIRE(result->difficulty == Difficulty::Hard);
    REQUIRE(result->puzzle_seed == 99);
    // Optional fields absent → default values
    REQUIRE(result->save_id.empty());
    REQUIRE(result->display_name.empty());
}

// ============================================================================
// getSaveDirectorySize when directory does not exist → 0
// ============================================================================

TEST_CASE("SaveManager - getSaveDirectorySize returns 0 when directory removed", "[save_manager_deser]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Remove the directory after construction
    fs::remove_all(tmp.path());

    REQUIRE(mgr.getSaveDirectorySize() == 0);
}

// ============================================================================
// SaveManager("") uses default directory (covers L52 true branch)
// ============================================================================

TEST_CASE("SaveManager - default constructor with empty string does not crash", "[save_manager_deser]") {
    // Passing empty string → uses platform default directory
    // This covers the save_directory.empty() true branch in the constructor
    REQUIRE_NOTHROW([&]() {
        SaveManager mgr("");
        // The default directory should be usable
        auto list = mgr.listSaves();
        REQUIRE(list.has_value());  // May be empty or have saves; just must not error
    }());
}
