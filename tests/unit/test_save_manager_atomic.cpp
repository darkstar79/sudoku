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

/// Atomicity tests for SaveManager (issue #25). A crash or power loss mid-write
/// must never truncate a save: writes go to <file>.tmp, fsync, then rename. We
/// can't crash a unit test, but we can assert the observable guarantees:
///   - a successful save leaves no .tmp scratch file behind;
///   - a stray truncated .tmp (simulating a crashed prior save) does not
///     corrupt or shadow the real save, which still loads.

#include "../../src/core/save_manager.h"
#include "../helpers/test_utils.h"

#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

namespace {

SavedGame makeMinimalGame() {
    SavedGame game;
    game.current_state = BoardData{};
    game.original_puzzle = BoardData{};
    game.notes = NotesData{};
    game.hint_revealed_cells = HintMaskData{};
    game.difficulty = Difficulty::Easy;
    game.puzzle_seed = 1;
    game.display_name = "Atomic Test";
    return game;
}

}  // namespace

TEST_CASE("SaveManager - successful save leaves no .tmp scratch file", "[save_manager_atomic]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    auto save_result = mgr.saveGame(makeMinimalGame(), SaveSettings{});
    REQUIRE(save_result.has_value());

    const auto save_path = tmp.path() / (*save_result + ".yaml");
    REQUIRE(fs::exists(save_path));
    REQUIRE_FALSE(fs::exists(fs::path(save_path) += ".tmp"));
}

TEST_CASE("SaveManager - stray truncated .tmp does not shadow a valid save", "[save_manager_atomic]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    auto save_result = mgr.saveGame(makeMinimalGame(), SaveSettings{});
    REQUIRE(save_result.has_value());
    const auto save_id = *save_result;
    const auto save_path = tmp.path() / (save_id + ".yaml");

    // Simulate a crash during the *next* save: a partially written .tmp is left
    // behind. With atomic writes the real file is the rename target and is never
    // truncated, so the original save must still load.
    {
        std::ofstream partial(fs::path(save_path) += ".tmp", std::ios::binary | std::ios::trunc);
        partial << "corrupt half-written";  // never renamed into place
    }

    auto load_result = mgr.loadGame(save_id);
    REQUIRE(load_result.has_value());
    REQUIRE(load_result->difficulty == Difficulty::Easy);
}

TEST_CASE("SaveManager - autoSave leaves no .tmp scratch file", "[save_manager_atomic]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    auto result = mgr.autoSave(makeMinimalGame());
    REQUIRE(result.has_value());

    const auto auto_save_path = tmp.path() / "autosave.yaml";
    REQUIRE(fs::exists(auto_save_path));
    REQUIRE_FALSE(fs::exists(fs::path(auto_save_path) += ".tmp"));

    // The auto-save round-trips.
    auto loaded = mgr.loadAutoSave();
    REQUIRE(loaded.has_value());
}
