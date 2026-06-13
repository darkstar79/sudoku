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

#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals; complexity is inherent to the multi-section TEST_CASE
TEST_CASE("GameViewModel - Educational Hint System", "[game_view_model][hints]") {
    sudoku::test::GameViewModelFixture fixture;

    SECTION("Hint message is empty initially") {
        REQUIRE(fixture.view_model->hintMessage.get().empty());
    }

    SECTION("Hint count is zero without active game") {
        REQUIRE(fixture.view_model->getHintCount() == 0);
    }

    SECTION("Hint does nothing without active game") {
        fixture.view_model->getHint(core::Position{.row = 0, .col = 0});
        REQUIRE(fixture.view_model->hintMessage.get().empty());
    }

    SECTION("Get hint shows technique explanation") {
        // Start a new game
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE_FALSE(state.isComplete());

        // Find and select an empty cell
        auto empty_cell_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_cell_opt.has_value());

        // Get hint
        fixture.view_model->getHint(empty_cell_opt.value());

        // Verify hint message contains technique information
        const auto& hint = fixture.view_model->hintMessage.get();
        REQUIRE_FALSE(hint.empty());

        // Should contain technique English name (no QTranslator installed -> source returned).
        // An Easy board can expose a region's last empty cell, so the next step may be a Full House
        // (SE 1.0) ahead of the singles (story 0b.4b).
        bool has_technique_name = hint.find("Naked Single") != std::string::npos ||
                                  hint.find("Hidden Single") != std::string::npos ||
                                  hint.find("Full House") != std::string::npos;
        REQUIRE(has_technique_name);
    }

    SECTION("Hint count decrements after showing hint") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        int initial_hints = fixture.view_model->getHintCount();
        REQUIRE(initial_hints == 10);  // Default max hints

        // Find and select an empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_cell_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_cell_opt.has_value());
        fixture.view_model->getHint(empty_cell_opt.value());

        int after_hints = fixture.view_model->getHintCount();
        REQUIRE(after_hints == 9);
    }

    SECTION("Hint fails if no hints remaining") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Use all hints - re-find an empty cell each time since hints now place values
        for (int i = 0; i < 10; ++i) {
            const auto& current_state = fixture.view_model->gameState.get();
            auto empty_cell_opt = sudoku::test::findEmptyCell(current_state);
            REQUIRE(empty_cell_opt.has_value());
            fixture.view_model->getHint(empty_cell_opt.value());
        }

        REQUIRE(fixture.view_model->getHintCount() == 0);

        // Try to get another hint
        fixture.view_model->hintMessage.set("");  // Clear previous message
        fixture.view_model->getHint(std::nullopt);

        // Should show error, not hint
        REQUIRE(fixture.view_model->hintMessage.get().empty());
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    }

    // NOTE: This test is disabled because error message handling interacts with
    // game state in complex ways. The core functionality (hints work correctly
    // when a cell IS selected) is tested in other sections.
    /*
    SECTION("Hint requires cell selection") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Don't select a cell
        fixture.view_model->getHint();

        // In a perfect world, should show error
        // But error message state is complex, so this test is disabled
    }
    */

    SECTION("Get hint places value and marks cell as hint-revealed") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        const auto& state = fixture.view_model->gameState.get();
        int moves_before = state.getMoveCount();

        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->getHint(empty_opt.value());

        // Hint message is still set (educational explanation)
        REQUIRE_FALSE(fixture.view_model->hintMessage.get().empty());

        // Find the hint-revealed cell
        const auto& after = fixture.view_model->gameState.get();
        bool found = false;
        for (size_t r = 0; r < 9 && !found; ++r) {
            for (size_t c = 0; c < 9 && !found; ++c) {
                const auto& cell = after.getCell({r, c});
                if (cell.is_hint_revealed) {
                    REQUIRE(cell.value > 0);
                    found = true;
                }
            }
        }
        REQUIRE(found);
        REQUIRE(after.getMoveCount() == moves_before + 1);
    }

    SECTION("Hint requires cell selection") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->getHintCount() > 0);

        int hints_before = fixture.view_model->getHintCount();
        fixture.view_model->getHint(std::nullopt);

        // Should show error and not consume hint
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
        REQUIRE(fixture.view_model->getHintCount() == hints_before);
    }

    SECTION("Hint doesn't work on user-filled cells") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Place a number in an empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);

        // Try to get hint on that cell
        int hints_before = fixture.view_model->getHintCount();
        fixture.view_model->getHint(empty_opt.value());

        // Should show error and not consume hint
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
        REQUIRE(fixture.view_model->getHintCount() == hints_before);
    }

    SECTION("Hint doesn't work on given cells") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();

        // Find a given cell
        std::optional<Position> given_pos;
        for (size_t r = 0; r < 9 && !given_pos.has_value(); ++r) {
            for (size_t c = 0; c < 9 && !given_pos.has_value(); ++c) {
                if (state.getCell({r, c}).is_given) {
                    given_pos = Position{.row = r, .col = c};
                }
            }
        }

        REQUIRE(given_pos.has_value());

        // Try to get hint
        fixture.view_model->getHint(given_pos.value());

        // Should show error
        REQUIRE(fixture.view_model->hintMessage.get().empty());
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    }
}

TEST_CASE("GameViewModel - Hint Message Format", "[game_view_model][hints]") {
    sudoku::test::GameViewModelFixture fixture;

    SECTION("Hint message contains explanation") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find and select an empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_cell_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_cell_opt.has_value());
        fixture.view_model->getHint(empty_cell_opt.value());

        const auto& hint = fixture.view_model->hintMessage.get();

        // Should have content (technique name + explanation)
        REQUIRE(hint.length() > 20);  // Reasonably detailed message
    }
}

TEST_CASE("GameViewModel - findStepByTechnique", "[game_view_model][hints][by_technique]") {
    sudoku::test::GameViewModelFixture fixture;

    // Deterministic near-complete board with exactly one naked single at R1C1.
    // Avoids relying on random-seeded generation which can yield boards where SueDeCoq
    // happens to fire — this fixture has no candidates for any advanced strategy.
    auto loadNakedSingleBoard = [&]() {
        SavedGame saved;
        saved.original_puzzle = {{0, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                                 {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                                 {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
        saved.current_state = saved.original_puzzle;
        saved.difficulty = Difficulty::Easy;
        // Empty move_history + no notes + original==current is the "fresh game" path in
        // restoreGameState — the corruption heuristic only fires when those don't all hold.
        fixture.view_model->restoreGameState(saved);
    };

    SECTION("Returns hint for an applicable technique on a deterministic board") {
        loadNakedSingleBoard();
        fixture.view_model->hintMessage.set("");
        fixture.view_model->errorMessage.set("");

        fixture.view_model->findStepByTechnique(core::SolvingTechnique::NakedSingle);

        REQUIRE_FALSE(fixture.view_model->hintMessage.get().empty());
        REQUIRE(fixture.view_model->errorMessage.get().empty());
    }

    SECTION("Reports no-such-technique via errorMessage when the requested technique does not apply") {
        loadNakedSingleBoard();
        fixture.view_model->hintMessage.set("");
        fixture.view_model->errorMessage.set("");

        // A near-complete board with one empty cell cannot surface SueDeCoq (needs ALSs
        // across line+box intersections, which require many candidates).
        fixture.view_model->findStepByTechnique(core::SolvingTechnique::SueDeCoq);

        REQUIRE(fixture.view_model->hintMessage.get().empty());
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    }

    SECTION("Reports error for Backtracking sentinel (no strategy registered)") {
        loadNakedSingleBoard();
        fixture.view_model->hintMessage.set("");
        fixture.view_model->errorMessage.set("");

        fixture.view_model->findStepByTechnique(core::SolvingTechnique::Backtracking);

        REQUIRE(fixture.view_model->hintMessage.get().empty());
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    }

    SECTION("Does not consume a hint credit") {
        loadNakedSingleBoard();
        const int initial_hints = fixture.view_model->getHintCount();

        fixture.view_model->findStepByTechnique(core::SolvingTechnique::NakedSingle);

        // Learning-mode lookup is not the standard hint flow — credits stay untouched.
        REQUIRE(fixture.view_model->getHintCount() == initial_hints);
    }
}
