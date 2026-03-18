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

#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/mock_localization_manager.h"
#include "../helpers/test_utils.h"
#include "core/board_utils.h"

#include <algorithm>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

// Test fixture for auto-notes tests
struct AutoNotesFixture {
    std::shared_ptr<GameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::unique_ptr<GameViewModel> view_model;
    sudoku::test::TempTestDir temp_dir;

    AutoNotesFixture() {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        solver = std::make_shared<SudokuSolver>(validator);
        save_manager = std::make_shared<SaveManager>(temp_dir.path());
        stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
        view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                     std::make_shared<MockLocalizationManager>());
    }

    /// Helper: start a game and return the game state
    void startGame(Difficulty difficulty = Difficulty::Easy) {
        view_model->startNewGame(difficulty);
    }

    /// Helper: count total notes across all cells
    [[nodiscard]] size_t countTotalNotes() const {
        size_t count = 0;
        const auto& state = view_model->gameState.get();
        core::forEachCell(
            [&](size_t row, size_t col) { count += static_cast<size_t>(state.getCell(row, col).notes.count()); });
        return count;
    }

    /// Helper: check if all empty cells have auto-computed notes matching getPossibleValues
    [[nodiscard]] bool allNotesMatchPossibleValues() const {
        const auto& state = view_model->gameState.get();
        auto board = state.extractNumbers();
        bool all_match = true;
        core::forEachCell([&](size_t row, size_t col) {
            const auto& cell = state.getCell(row, col);
            auto expected = validator->getPossibleValues(board, {.row = row, .col = col});
            // Convert expected vector to CellNotes for comparison
            core::CellNotes expected_notes;
            for (int v : expected) {
                expected_notes.add(v);
            }
            if (cell.notes != expected_notes) {
                all_match = false;
            }
        });
        return all_match;
    }
};

TEST_CASE("GameViewModel - Auto Notes Toggle", "[game_view_model][auto_notes]") {
    AutoNotesFixture f;

    SECTION("Auto notes is OFF by default") {
        REQUIRE_FALSE(f.view_model->isAutoNotesEnabled());
        REQUIRE_FALSE(f.view_model->uiState.get().auto_notes_enabled);
    }

    SECTION("Toggle auto notes ON enables computed notes") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        REQUIRE(f.view_model->isAutoNotesEnabled());
        REQUIRE(f.view_model->uiState.get().auto_notes_enabled);
        REQUIRE(f.countTotalNotes() > 0);
    }

    SECTION("Toggle auto notes OFF clears all notes") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);
        REQUIRE(f.countTotalNotes() > 0);

        f.view_model->setAutoNotesEnabled(false);

        REQUIRE_FALSE(f.view_model->isAutoNotesEnabled());
        REQUIRE(f.countTotalNotes() == 0);
    }

    SECTION("toggleAutoNotes() round-trip ON->OFF->ON") {
        f.startGame();

        f.view_model->toggleAutoNotes();
        REQUIRE(f.view_model->isAutoNotesEnabled());
        size_t notes_on = f.countTotalNotes();
        REQUIRE(notes_on > 0);

        f.view_model->toggleAutoNotes();
        REQUIRE_FALSE(f.view_model->isAutoNotesEnabled());
        REQUIRE(f.countTotalNotes() == 0);

        f.view_model->toggleAutoNotes();
        REQUIRE(f.view_model->isAutoNotesEnabled());
        REQUIRE(f.countTotalNotes() == notes_on);
    }

    SECTION("setAutoNotesEnabled with same value is no-op") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(false);  // Already false
        REQUIRE(f.countTotalNotes() == 0);
    }
}

TEST_CASE("GameViewModel - Auto Notes Computation", "[game_view_model][auto_notes]") {
    AutoNotesFixture f;

    SECTION("Auto notes computes valid candidates for empty cells") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        REQUIRE(f.allNotesMatchPossibleValues());
    }

    SECTION("Auto notes does not set notes on given cells") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        const auto& state = f.view_model->gameState.get();
        bool any_given_has_notes = false;
        core::forEachCell([&](size_t row, size_t col) {
            const auto& cell = state.getCell(row, col);
            if (cell.is_given && !cell.notes.empty()) {
                any_given_has_notes = true;
            }
        });
        REQUIRE_FALSE(any_given_has_notes);
    }

    SECTION("Auto notes does not set notes on filled cells") {
        f.startGame();

        // Place a number first
        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        // Get solution value to avoid conflicts
        auto solution = state.getSolutionBoard();
        int correct_value = solution[pos.row][pos.col];

        f.view_model->selectCell(pos);
        f.view_model->enterNumber(correct_value);

        f.view_model->setAutoNotesEnabled(true);

        // The filled cell should have no notes
        const auto& filled_cell = f.view_model->gameState.get().getCell(pos);
        REQUIRE(filled_cell.notes.empty());
    }

    SECTION("Auto notes update after placing a number") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        size_t notes_before = f.countTotalNotes();

        // Place a correct number
        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        auto solution = state.getSolutionBoard();
        int correct_value = solution[pos.row][pos.col];

        f.view_model->selectCell(pos);
        f.view_model->enterNumber(correct_value);

        // Notes should have changed (fewer empty cells, candidates updated)
        REQUIRE(f.allNotesMatchPossibleValues());
        // Placing a number should reduce total notes (fewer empty cells + fewer candidates)
        REQUIRE(f.countTotalNotes() < notes_before);
    }
}

TEST_CASE("GameViewModel - Auto Notes and Manual Notes Interaction", "[game_view_model][auto_notes]") {
    AutoNotesFixture f;

    SECTION("enterNote is no-op when auto notes is enabled") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        // Get notes before manual attempt
        auto notes_before = f.view_model->gameState.get().getCell(pos).notes;

        f.view_model->selectCell(pos);
        f.view_model->enterNote(1);

        // Notes should be unchanged (enterNote was blocked)
        auto notes_after = f.view_model->gameState.get().getCell(pos).notes;
        REQUIRE(notes_before == notes_after);
    }

    SECTION("Manual notes work normally when auto notes is disabled") {
        f.startGame();

        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        f.view_model->selectCell(pos);
        f.view_model->enterNote(5);

        auto notes = f.view_model->gameState.get().getCell(pos).notes;
        REQUIRE(std::find(notes.begin(), notes.end(), 5) != notes.end());
    }

    SECTION("Turning auto notes ON replaces manual notes with computed") {
        f.startGame();

        // Add a manual note
        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        f.view_model->selectCell(pos);
        f.view_model->enterNote(5);

        // Enable auto-notes — should overwrite manual notes
        f.view_model->setAutoNotesEnabled(true);

        REQUIRE(f.allNotesMatchPossibleValues());
    }
}

TEST_CASE("GameViewModel - Auto Notes and Undo/Redo", "[game_view_model][auto_notes]") {
    AutoNotesFixture f;

    SECTION("Undo recomputes auto notes when enabled") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        // Place a number
        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        auto solution = state.getSolutionBoard();
        int correct_value = solution[pos.row][pos.col];

        f.view_model->selectCell(pos);
        f.view_model->enterNumber(correct_value);
        REQUIRE(f.allNotesMatchPossibleValues());

        // Undo
        f.view_model->undo();

        // After undo, notes should still match possible values for the restored board
        REQUIRE(f.allNotesMatchPossibleValues());
        // The undone cell should have notes again (it's empty again)
        REQUIRE_FALSE(f.view_model->gameState.get().getCell(pos).notes.empty());
    }

    SECTION("Redo recomputes auto notes when enabled") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        auto solution = state.getSolutionBoard();
        int correct_value = solution[pos.row][pos.col];

        f.view_model->selectCell(pos);
        f.view_model->enterNumber(correct_value);
        f.view_model->undo();
        f.view_model->redo();

        REQUIRE(f.allNotesMatchPossibleValues());
        // The cell should be filled again, no notes
        REQUIRE(f.view_model->gameState.get().getCell(pos).value == correct_value);
        REQUIRE(f.view_model->gameState.get().getCell(pos).notes.empty());
    }
}

TEST_CASE("GameViewModel - Auto Notes and New Game/Reset", "[game_view_model][auto_notes]") {
    AutoNotesFixture f;

    SECTION("Auto notes preference persists across new game") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);
        REQUIRE(f.view_model->isAutoNotesEnabled());

        f.view_model->startNewGame(Difficulty::Medium);

        REQUIRE(f.view_model->isAutoNotesEnabled());
        REQUIRE(f.countTotalNotes() > 0);
        REQUIRE(f.allNotesMatchPossibleValues());
    }

    SECTION("Auto notes preference persists across reset") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        // Place a number to make reset meaningful
        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        auto pos = empty_opt.value();

        auto solution = state.getSolutionBoard();
        f.view_model->selectCell(pos);
        f.view_model->enterNumber(solution[pos.row][pos.col]);

        f.view_model->resetGame();

        REQUIRE(f.view_model->isAutoNotesEnabled());
        REQUIRE(f.allNotesMatchPossibleValues());
    }
}

TEST_CASE("GameViewModel - Auto Notes and Save/Load", "[game_view_model][auto_notes]") {
    AutoNotesFixture f;

    SECTION("Auto notes flag is saved and restored via restoreGameState") {
        f.startGame();

        // Build a SavedGame with auto_notes_enabled = true
        const auto& current_state = f.view_model->gameState.get();
        core::SavedGame saved_game;
        saved_game.original_puzzle = current_state.extractGivenNumbers();
        saved_game.current_state = current_state.extractNumbers();
        saved_game.difficulty = current_state.getDifficulty();
        saved_game.auto_notes_enabled = true;

        // Ensure auto-notes is OFF before restore
        REQUIRE_FALSE(f.view_model->isAutoNotesEnabled());

        // Restore the game with auto_notes_enabled flag set
        f.view_model->restoreGameState(saved_game);

        REQUIRE(f.view_model->isAutoNotesEnabled());
        REQUIRE(f.allNotesMatchPossibleValues());
    }

    SECTION("Loading save without auto_notes_enabled defaults to OFF") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);
        REQUIRE(f.view_model->isAutoNotesEnabled());

        // Build a SavedGame without auto_notes_enabled (simulates old save format)
        const auto& current_state = f.view_model->gameState.get();
        core::SavedGame saved_game;
        saved_game.original_puzzle = current_state.extractGivenNumbers();
        saved_game.current_state = current_state.extractNumbers();
        saved_game.difficulty = current_state.getDifficulty();
        // auto_notes_enabled defaults to false

        f.view_model->restoreGameState(saved_game);

        REQUIRE_FALSE(f.view_model->isAutoNotesEnabled());
    }
}

TEST_CASE("GameViewModel - Auto Notes and Hints", "[game_view_model][auto_notes]") {
    AutoNotesFixture f;

    SECTION("Hint placement recomputes auto notes") {
        f.startGame();
        f.view_model->setAutoNotesEnabled(true);

        const auto& state = f.view_model->gameState.get();
        auto empty_opt = test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());

        f.view_model->selectCell(empty_opt.value());
        f.view_model->getHint();

        // After hint, all notes should still be consistent
        REQUIRE(f.allNotesMatchPossibleValues());
    }
}
