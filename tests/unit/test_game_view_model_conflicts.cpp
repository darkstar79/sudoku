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

#include "../helpers/game_view_model_fixture.h"

#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

struct ConflictTestFixture : sudoku::test::GameViewModelFixture {
    // Find two empty cells in the same row. Returns {cell_a, cell_b, value_to_place}.
    // The value chosen does not conflict with any given cell in that row.
    struct ConflictSetup {
        Position cell_a;
        Position cell_b;
        int value;
    };

    // Find two empty cells in the same row where a value is valid for cell_a
    // but placing it in cell_b creates a row conflict (the ONLY conflict).
    [[nodiscard]] std::optional<ConflictSetup> findTwoEmptyCellsSameRow() const {
        const auto& state = view_model->gameState.get();
        auto board = state.extractNumbers();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            std::vector<size_t> empty_cols;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    empty_cols.push_back(col);
                }
            }
            if (empty_cols.size() >= 2) {
                // Find a value that is valid for cell_a (no conflicts anywhere)
                Position cell_a{.row = row, .col = empty_cols[0]};
                auto possible_a = validator->getPossibleValues(board, cell_a);
                for (int val : possible_a) {
                    return ConflictSetup{.cell_a = cell_a, .cell_b = {.row = row, .col = empty_cols[1]}, .value = val};
                }
            }
        }
        return std::nullopt;
    }
};

TEST_CASE("GameViewModel - Conflict Highlighting", "[game_view_model][conflicts]") {
    ConflictTestFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    SECTION("Placing a conflicting number marks cells as conflicting") {
        auto setup = fixture.findTwoEmptyCellsSameRow();
        REQUIRE(setup.has_value());

        // Place same value in two cells of the same row
        fixture.view_model->enterNumber(setup->cell_a, setup->value);

        fixture.view_model->enterNumber(setup->cell_b, setup->value);

        // Both cells should be marked as conflicting
        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.getCell(setup->cell_a).has_conflict);
        REQUIRE(state.getCell(setup->cell_b).has_conflict);
    }

    SECTION("Placing a valid number does not create conflict highlighting") {
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    Position pos{row, col};
                    auto possible = fixture.validator->getPossibleValues(state.extractNumbers(), pos);
                    if (!possible.empty()) {
                        fixture.view_model->enterNumber(pos, possible[0]);

                        const auto& after = fixture.view_model->gameState.get();
                        REQUIRE_FALSE(after.getCell(pos).has_conflict);
                        return;
                    }
                }
            }
        }
    }

    SECTION("Undoing a conflicting move clears conflict highlighting") {
        auto setup = fixture.findTwoEmptyCellsSameRow();
        REQUIRE(setup.has_value());

        // Create a conflict
        fixture.view_model->enterNumber(setup->cell_a, setup->value);
        fixture.view_model->enterNumber(setup->cell_b, setup->value);

        // Verify conflict exists
        REQUIRE(fixture.view_model->gameState.get().getCell(setup->cell_b).has_conflict);

        // Undo the second placement
        fixture.view_model->undo();

        // Conflict should be cleared
        const auto& state = fixture.view_model->gameState.get();
        REQUIRE_FALSE(state.getCell(setup->cell_a).has_conflict);
    }

    SECTION("Redoing a conflicting move restores conflict highlighting") {
        auto setup = fixture.findTwoEmptyCellsSameRow();
        REQUIRE(setup.has_value());

        // Create a conflict
        fixture.view_model->enterNumber(setup->cell_a, setup->value);
        fixture.view_model->enterNumber(setup->cell_b, setup->value);

        // Undo then redo
        fixture.view_model->undo();
        REQUIRE_FALSE(fixture.view_model->gameState.get().getCell(setup->cell_a).has_conflict);

        fixture.view_model->redo();

        // Conflict should be restored
        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.getCell(setup->cell_a).has_conflict);
        REQUIRE(state.getCell(setup->cell_b).has_conflict);
    }

    SECTION("Clearing a conflicting cell removes conflict highlighting") {
        auto setup = fixture.findTwoEmptyCellsSameRow();
        REQUIRE(setup.has_value());

        // Create a conflict
        fixture.view_model->enterNumber(setup->cell_a, setup->value);
        fixture.view_model->enterNumber(setup->cell_b, setup->value);

        REQUIRE(fixture.view_model->gameState.get().getCell(setup->cell_a).has_conflict);

        // Clear the second cell
        fixture.view_model->clearCell(setup->cell_b);

        // Conflict should be cleared for both cells
        const auto& state = fixture.view_model->gameState.get();
        REQUIRE_FALSE(state.getCell(setup->cell_a).has_conflict);
        REQUIRE_FALSE(state.getCell(setup->cell_b).has_conflict);
    }
}

TEST_CASE("GameViewModel - Mistake Counter", "[game_view_model][mistakes]") {
    ConflictTestFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    SECTION("Mistake count increments when placing wrong number") {
        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();

        REQUIRE(fixture.view_model->getMistakeCount() == 0);

        // Find an empty cell and place the correct value — no mistake
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    Position pos1{.row = row, .col = col};
                    fixture.view_model->enterNumber(pos1, solution[row][col]);
                    REQUIRE(fixture.view_model->getMistakeCount() == 0);

                    // Now find another empty cell and place a wrong value — mistake
                    const auto& state2 = fixture.view_model->gameState.get();
                    for (size_t r2 = 0; r2 < BOARD_SIZE; ++r2) {
                        for (size_t c2 = 0; c2 < BOARD_SIZE; ++c2) {
                            if (state2.getCell(r2, c2).value == 0) {
                                int wrong = (solution[r2][c2] % 9) + 1;
                                Position pos2{.row = r2, .col = c2};
                                fixture.view_model->enterNumber(pos2, wrong);
                                REQUIRE(fixture.view_model->getMistakeCount() == 1);
                                return;
                            }
                        }
                    }
                    return;
                }
            }
        }
    }

    SECTION("Mistake count does not increment for valid placements") {
        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    Position pos{.row = row, .col = col};
                    int correct_value = solution[row][col];
                    fixture.view_model->enterNumber(pos, correct_value);

                    REQUIRE(fixture.view_model->getMistakeCount() == 0);
                    return;
                }
            }
        }
    }

    SECTION("Undoing a mistake does not decrement mistake count") {
        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();

        // Find an empty cell and place a wrong value
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    int correct = solution[row][col];
                    int wrong = (correct % 9) + 1;  // Different from correct
                    Position pos{.row = row, .col = col};
                    fixture.view_model->enterNumber(pos, wrong);
                    REQUIRE(fixture.view_model->getMistakeCount() == 1);

                    // Undo the mistake
                    fixture.view_model->undo();

                    // Mistake count should NOT decrement (total mistakes made)
                    REQUIRE(fixture.view_model->getMistakeCount() == 1);
                    return;
                }
            }
        }
    }
}
