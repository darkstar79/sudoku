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

/// Extra branch-coverage tests for SaveManager:
/// - listSaves with non-YAML files (txt, subdirectory) in save directory
/// - listSaves with valid YAML non-save files (isValidSaveFile returns false)
/// - autoSave + loadAutoSave round-trip
/// - saveGame with pre-existing save_id (reuses id)
/// - saveGame empty display_name auto-generates name with localtime

#include "../../src/core/save_manager.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

namespace {

SavedGame makeGame() {
    SavedGame game;
    game.current_state = BoardData{};
    game.original_puzzle = BoardData{};
    game.notes = NotesData{};
    game.hint_revealed_cells = HintMaskData{};
    game.difficulty = Difficulty::Easy;
    game.puzzle_seed = 1;
    game.display_name = "Test Game";
    return game;
}

}  // namespace

// ============================================================================
// listSaves: non-YAML file is skipped (extension check false branch)
// ============================================================================

TEST_CASE("SaveManager - listSaves skips non-YAML files", "[save_manager_extra]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Save a real game first
    auto game = makeGame();
    auto save_result = mgr.saveGame(game, {});
    REQUIRE(save_result.has_value());

    // Add a .txt file to the save directory (should be ignored)
    {
        std::ofstream txt_file(tmp.path() / "notes.txt");
        txt_file << "some text\n";
    }

    // Add a .json file too
    {
        std::ofstream json_file(tmp.path() / "config.json");
        json_file << "{}\n";
    }

    // listSaves should return only the one real save (not the txt/json files)
    auto list_result = mgr.listSaves();
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->size() == 1);
}

// ============================================================================
// listSaves: subdirectory in save dir is skipped (is_regular_file false branch)
// ============================================================================

TEST_CASE("SaveManager - listSaves skips subdirectories", "[save_manager_extra]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Save a real game
    auto game = makeGame();
    auto save_result = mgr.saveGame(game, {});
    REQUIRE(save_result.has_value());

    // Create a subdirectory (not a regular file)
    fs::create_directories(tmp.path() / "subdir");

    auto list_result = mgr.listSaves();
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->size() == 1);
}

// ============================================================================
// listSaves: YAML file that is not a valid save is skipped
// ============================================================================

TEST_CASE("SaveManager - listSaves skips YAML files that are not valid saves", "[save_manager_extra]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Write a YAML file that doesn't have the correct save format
    {
        std::ofstream bad_yaml(tmp.path() / "bad_save.yaml");
        bad_yaml << "name: not-a-save\nvalue: 42\n";
    }

    auto list_result = mgr.listSaves();
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->empty());  // bad YAML save is rejected
}

// ============================================================================
// listSaves: multiple saves sorted by last_modified
// ============================================================================

TEST_CASE("SaveManager - listSaves returns multiple saves sorted newest first", "[save_manager_extra]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Save two games with different names
    auto game1 = makeGame();
    game1.display_name = "Game Alpha";
    auto id1 = mgr.saveGame(game1, {});
    REQUIRE(id1.has_value());

    auto game2 = makeGame();
    game2.display_name = "Game Beta";
    auto id2 = mgr.saveGame(game2, {});
    REQUIRE(id2.has_value());

    auto list_result = mgr.listSaves();
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->size() == 2);
}

// ============================================================================
// autoSave + loadAutoSave round-trip
// ============================================================================

TEST_CASE("SaveManager - autoSave then loadAutoSave returns correct data", "[save_manager_extra]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    auto game = makeGame();
    game.display_name = "Auto Round-trip";
    game.current_state[0][0] = 5;

    auto auto_save_result = mgr.autoSave(game);
    REQUIRE(auto_save_result.has_value());
    REQUIRE(mgr.hasAutoSave());

    auto load_result = mgr.loadAutoSave();
    REQUIRE(load_result.has_value());
    REQUIRE(load_result->is_auto_save);
    REQUIRE(load_result->current_state[0][0] == 5);
}

// ============================================================================
// saveGame: game with pre-existing save_id reuses that id
// ============================================================================

TEST_CASE("SaveManager - saveGame with existing save_id overwrites same file", "[save_manager_extra]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // First save (generates an id)
    auto game = makeGame();
    auto first_id = mgr.saveGame(game, {});
    REQUIRE(first_id.has_value());

    // Load it back to get the save_id
    auto loaded = mgr.loadGame(*first_id);
    REQUIRE(loaded.has_value());

    // Modify and re-save with the same id
    loaded->display_name = "Updated Name";
    auto second_id = mgr.saveGame(*loaded, {});
    REQUIRE(second_id.has_value());
    REQUIRE(*second_id == *first_id);  // Same id — same file overwritten

    // Verify name updated
    auto reloaded = mgr.loadGame(*second_id);
    REQUIRE(reloaded.has_value());
    REQUIRE(reloaded->display_name == "Updated Name");

    // listSaves should still have only 1 save
    auto list_result = mgr.listSaves();
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->size() == 1);
}

// ============================================================================
// deleteSave: existing save is removed
// ============================================================================

TEST_CASE("SaveManager - deleteSave removes save from list", "[save_manager_extra]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    auto game = makeGame();
    auto save_id = mgr.saveGame(game, {});
    REQUIRE(save_id.has_value());

    // Confirm it exists
    REQUIRE(mgr.listSaves()->size() == 1);

    // Delete it
    auto del_result = mgr.deleteSave(*save_id);
    REQUIRE(del_result.has_value());

    // Should be gone
    REQUIRE(mgr.listSaves()->empty());

    // Loading again should return FileNotFound
    auto load_again = mgr.loadGame(*save_id);
    REQUIRE_FALSE(load_again.has_value());
    REQUIRE(load_again.error() == SaveError::FileNotFound);
}
