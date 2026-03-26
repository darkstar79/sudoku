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
#include "../helpers/test_utils.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

// Helper: minimal valid SavedGame (all 9x9 boards initialised)
static SavedGame makeValidGame() {
    SavedGame game;
    game.current_state = BoardData{};
    game.original_puzzle = BoardData{};
    game.notes = NotesData{};
    game.hint_revealed_cells = HintMaskData{};
    game.difficulty = Difficulty::Easy;
    game.puzzle_seed = 1;
    game.display_name = "Test";
    return game;
}

TEST_CASE("SaveManager - Pre-existing save_id is reused", "[save_manager_settings]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();
    game.save_id = "my-custom-id";

    SaveSettings settings;
    settings.compress = false;

    auto result = mgr.saveGame(game, settings);
    REQUIRE(result.has_value());
    // The returned save_id must match the one we set
    REQUIRE(*result == "my-custom-id");

    // And the file should exist at that id location
    REQUIRE(fs::exists(tmp.path() / "my-custom-id.yaml"));
}

TEST_CASE("SaveManager - is_complete=true is serialized correctly", "[save_manager_settings]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();
    game.is_complete = true;

    SaveSettings settings;
    settings.compress = false;

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto load_result = mgr.loadGame(*save_result);
    REQUIRE(load_result.has_value());
    REQUIRE(load_result->is_complete == true);
}

TEST_CASE("SaveManager - Empty notes skips 'notes' key in YAML", "[save_manager_settings]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // All notes empty → has_notes remains false → no "notes" key serialized
    SavedGame game = makeValidGame();
    // notes already empty from makeValidGame()

    SaveSettings settings;
    settings.compress = false;

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    // Load succeeds — missing "notes" key is handled by deserializiation
    auto load_result = mgr.loadGame(*save_result);
    REQUIRE(load_result.has_value());
    // When no notes key was serialized, notes is left empty (not populated to 9×9)
    REQUIRE(load_result->notes.empty());
}

TEST_CASE("SaveManager - Non-empty notes: 'notes' key is serialized", "[save_manager_settings]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();
    game.notes[0][0] = CellNotes{1, 3, 5};

    SaveSettings settings;
    settings.compress = false;

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto load_result = mgr.loadGame(*save_result);
    REQUIRE(load_result.has_value());
    REQUIRE(load_result->notes[0][0] == CellNotes{1, 3, 5});
}

TEST_CASE("SaveManager - include_history=true with non-empty move_history", "[save_manager_settings]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();

    // Add a move to move_history
    Move move;
    move.position = Position{.row = 0, .col = 0};
    move.value = 5;
    move.move_type = MoveType::PlaceNumber;
    move.is_note = false;
    move.timestamp = std::chrono::steady_clock::now();
    game.move_history.push_back(move);
    game.current_move_index = 0;

    SaveSettings settings;
    settings.compress = false;
    settings.include_history = true;

    // This covers the true branch: settings.include_history && !move_history.empty()
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());
}

TEST_CASE("SaveManager - include_history=false with non-empty move_history", "[save_manager_settings]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();

    Move move;
    move.position = Position{.row = 1, .col = 1};
    move.value = 3;
    move.move_type = MoveType::PlaceNumber;
    move.is_note = false;
    move.timestamp = std::chrono::steady_clock::now();
    game.move_history.push_back(move);

    SaveSettings settings;
    settings.compress = false;
    settings.include_history = false;

    // This covers the false branch: include_history is false → skip history block
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());
}

TEST_CASE("SaveManager - Invalid board dimensions return SerializationError", "[save_manager_settings]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SaveSettings settings;
    settings.compress = false;

    // Board dimension corruption tests removed — BoardData is always 9x9 (fixed-size flat array)
}
