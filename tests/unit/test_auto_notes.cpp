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

// Test fixture for fill-notes tests
struct FillNotesFixture {
    std::shared_ptr<GameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::unique_ptr<GameViewModel> view_model;
    sudoku::test::TempTestDir temp_dir;

    FillNotesFixture() {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        solver = std::make_shared<SudokuSolver>(validator);
        save_manager = std::make_shared<SaveManager>(temp_dir.path());
        stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
        view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                     std::make_shared<MockLocalizationManager>());
    }

    void startGame(Difficulty difficulty = Difficulty::Easy) {
        view_model->startNewGame(difficulty);
    }

    [[nodiscard]] size_t countTotalNotes() const {
        size_t count = 0;
        const auto& state = view_model->gameState.get();
        core::forEachCell(
            [&](size_t row, size_t col) { count += static_cast<size_t>(state.getCell(row, col).notes.count()); });
        return count;
    }

    [[nodiscard]] bool allNotesMatchPossibleValues() const {
        const auto& state = view_model->gameState.get();
        auto board = state.extractNumbers();
        bool all_match = true;
        core::forEachCell([&](size_t row, size_t col) {
            const auto& cell = state.getCell(row, col);
            auto expected = validator->getPossibleValues(board, {.row = row, .col = col});
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

TEST_CASE("GameViewModel - fillNotes fills candidates for all empty cells", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();

    REQUIRE(f.countTotalNotes() == 0);

    f.view_model->fillNotes();

    REQUIRE(f.countTotalNotes() > 0);
    REQUIRE(f.allNotesMatchPossibleValues());
}

TEST_CASE("GameViewModel - fillNotes does not set notes on given or filled cells", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();
    f.view_model->fillNotes();

    const auto& state = f.view_model->gameState.get();
    bool any_given_has_notes = false;
    core::forEachCell([&](size_t row, size_t col) {
        const auto& cell = state.getCell(row, col);
        if ((cell.is_given || cell.value > 0) && !cell.notes.empty()) {
            any_given_has_notes = true;
        }
    });
    REQUIRE_FALSE(any_given_has_notes);
}

TEST_CASE("GameViewModel - fillNotes updates after placing a number", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();
    f.view_model->fillNotes();

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

    // Fill again — should reflect the new board state
    f.view_model->fillNotes();

    REQUIRE(f.allNotesMatchPossibleValues());
    REQUIRE(f.countTotalNotes() < notes_before);
}

TEST_CASE("GameViewModel - manual notes work independently of fillNotes", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();

    const auto& state = f.view_model->gameState.get();
    auto empty_opt = test::findEmptyCell(state);
    REQUIRE(empty_opt.has_value());
    auto pos = empty_opt.value();

    // Manual note entry works
    f.view_model->selectCell(pos);
    f.view_model->enterNote(5);

    auto notes = f.view_model->gameState.get().getCell(pos).notes;
    REQUIRE(std::find(notes.begin(), notes.end(), 5) != notes.end());

    // fillNotes overwrites manual notes with computed values
    f.view_model->fillNotes();
    REQUIRE(f.allNotesMatchPossibleValues());
}

TEST_CASE("GameViewModel - fillNotes is one-shot, not persistent", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();
    f.view_model->fillNotes();

    REQUIRE(f.countTotalNotes() > 0);

    // Place a number — notes are NOT auto-recomputed (only cleanup runs)
    const auto& state = f.view_model->gameState.get();
    auto empty_opt = test::findEmptyCell(state);
    REQUIRE(empty_opt.has_value());
    auto pos = empty_opt.value();
    auto solution = state.getSolutionBoard();

    f.view_model->selectCell(pos);
    f.view_model->enterNumber(solution[pos.row][pos.col]);

    // Notes were cleaned up (conflicting removed) but not fully recomputed
    // Some cells may have stale notes — this is expected (one-shot behavior)
    // The placed cell should have no notes
    REQUIRE(f.view_model->gameState.get().getCell(pos).notes.empty());
}
