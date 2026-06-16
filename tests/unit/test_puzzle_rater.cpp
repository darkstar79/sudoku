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

#include "../../src/core/game_validator.h"
#include "../../src/core/i_time_provider.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/puzzle_rater.h"
#include "../../src/core/sudoku_solver.h"
#include "../helpers/candidate_test_utils.h"

#include <chrono>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// Test helper: Create an easy puzzle with only naked singles
BoardData createEasyPuzzle() {
    // Near-complete board with one naked single
    BoardData board = {{0, 3, 4, 6, 7, 8, 9, 1, 2},  // R1C1 must be 5
                       {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
                       {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6}, {9, 6, 1, 5, 3, 7, 2, 8, 4},
                       {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    return board;
}

BoardData createCompleteBoard() {
    BoardData board = sudoku::testing::kSolvedBoard;
    return board;
}

TEST_CASE("PuzzleRater - Constructor and Dependencies", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);

    SECTION("Constructs with solver dependency") {
        REQUIRE_NOTHROW(PuzzleRater(solver));
    }
}

TEST_CASE("PuzzleRater - Rate Easy Puzzle", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("Rates easy puzzle with a single Full House") {
        // Near-complete board: R1C1 is the only empty cell in its row, column, AND box — a Full House
        // (SE 1.0), not a Naked Single. Story 0b.4b models this correctly (previously over-rated 2.3).
        auto board = createEasyPuzzle();

        auto result = rater.ratePuzzle(board);

        REQUIRE(result.has_value());
        REQUIRE(result->se_rating == 1.0);  // One Full House (SE "Last value")
        REQUIRE(result->solve_path.size() == 1);
        REQUIRE(result->solve_path[0].technique == SolvingTechnique::FullHouse);
        REQUIRE_FALSE(result->requires_backtracking);
        REQUIRE(result->estimated_difficulty == Difficulty::Easy);
    }
}

TEST_CASE("PuzzleRater - Rate Complete Board", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("Complete board has rating of 0") {
        auto board = createCompleteBoard();

        auto result = rater.ratePuzzle(board);

        REQUIRE(result.has_value());
        REQUIRE(result->se_rating == 0);  // No steps needed
        REQUIRE(result->solve_path.empty());
        REQUIRE_FALSE(result->requires_backtracking);
        REQUIRE(result->estimated_difficulty == Difficulty::Easy);  // 0 < 500
    }
}

TEST_CASE("PuzzleRater - Generated Puzzle Rating", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("Rates generated Easy puzzle") {
        auto puzzle_result = generator->generatePuzzle({.difficulty = Difficulty::Easy});
        REQUIRE(puzzle_result.has_value());

        auto rating = rater.ratePuzzle(puzzle_result->board);

        REQUIRE(rating.has_value());
        REQUIRE(rating->se_rating >= 0);            // Any score is valid
        REQUIRE_FALSE(rating->solve_path.empty());  // Generated puzzle requires steps
        // Note: Rating may not match Easy range yet (Phase 6 will add validation)
    }

    SECTION("Rates generated Medium puzzle") {
        auto puzzle_result = generator->generatePuzzle({.difficulty = Difficulty::Medium});
        REQUIRE(puzzle_result.has_value());

        auto rating = rater.ratePuzzle(puzzle_result->board);

        REQUIRE(rating.has_value());
        REQUIRE(rating->se_rating >= 0);
        REQUIRE_FALSE(rating->solve_path.empty());
    }
}

TEST_CASE("PuzzleRater - Rating Calculation", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("SE rating equals max technique rating") {
        // Generate a puzzle and verify rating calculation
        auto puzzle_result = generator->generatePuzzle({.difficulty = Difficulty::Easy});
        REQUIRE(puzzle_result.has_value());

        auto rating = rater.ratePuzzle(puzzle_result->board);

        REQUIRE(rating.has_value());

        // Manually sum logical technique points (backtracking excluded from score)
        double expected_max = 0.0;
        for (const auto& step : rating->solve_path) {
            if (step.technique != SolvingTechnique::Backtracking) {
                expected_max = std::max(expected_max, step.rating);
            }
        }

        REQUIRE(rating->se_rating == expected_max);
    }
}

TEST_CASE("PuzzleRater - Backtracking Flag", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("Sets requires_backtracking flag correctly") {
        // Test with easy puzzle that doesn't need backtracking
        auto board = createEasyPuzzle();
        auto rating = rater.ratePuzzle(board);

        REQUIRE(rating.has_value());
        REQUIRE_FALSE(rating->requires_backtracking);  // Easy puzzle solved logically
    }
}

TEST_CASE("PuzzleRater - Difficulty Estimation", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("Correctly estimates difficulty from rating") {
        // Test with known board
        auto board = createEasyPuzzle();
        auto rating = rater.ratePuzzle(board);

        REQUIRE(rating.has_value());
        REQUIRE(rating->estimated_difficulty == ratingToDifficulty(rating->se_rating));
    }
}

TEST_CASE("PuzzleRater - Polymorphic Usage", "[puzzle_rater]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);

    SECTION("Can be used through IPuzzleRater interface") {
        std::unique_ptr<IPuzzleRater> rater = std::make_unique<PuzzleRater>(solver);

        auto board = createEasyPuzzle();
        auto rating = rater->ratePuzzle(board);

        REQUIRE(rating.has_value());
        REQUIRE(rating->se_rating >= 0);
    }
}

// Mock solver that always returns a specific error
class MockErrorSolver : public ISudokuSolver {
public:
    explicit MockErrorSolver(SolverError error) : error_(error) {
    }

    [[nodiscard]] std::expected<SolveStep, SolverError>
    findNextStep([[maybe_unused]] const BoardData& board) const override {
        return std::unexpected(error_);
    }

    [[nodiscard]] std::expected<SolverResult, SolverError>
    solvePuzzle([[maybe_unused]] const BoardData& board) const override {
        return std::unexpected(error_);
    }

    [[nodiscard]] bool applyStep([[maybe_unused]] BoardData& board,
                                 [[maybe_unused]] const SolveStep& step) const override {
        return false;
    }

private:
    SolverError error_;
};

TEST_CASE("PuzzleRater - Solver error paths", "[puzzle_rater]") {
    auto board = createEasyPuzzle();

    SECTION("InvalidBoard error maps to RatingError::InvalidBoard") {
        auto mock_solver = std::make_shared<MockErrorSolver>(SolverError::InvalidBoard);
        PuzzleRater rater(mock_solver);

        auto result = rater.ratePuzzle(board);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == RatingError::InvalidBoard);
    }

    SECTION("Unsolvable error maps to RatingError::Unsolvable") {
        auto mock_solver = std::make_shared<MockErrorSolver>(SolverError::Unsolvable);
        PuzzleRater rater(mock_solver);

        auto result = rater.ratePuzzle(board);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == RatingError::Unsolvable);
    }

    SECTION("Timeout error maps to RatingError::Timeout") {
        auto mock_solver = std::make_shared<MockErrorSolver>(SolverError::Timeout);
        PuzzleRater rater(mock_solver);

        auto result = rater.ratePuzzle(board);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == RatingError::Timeout);
    }
}

// #24 H2: the rater must enforce a wall-clock budget so a no-logical-path board (AI Escargot) cannot
// livelock generation-time rating. Injecting a negative budget makes the solver's deadline already
// past, so the budget overload short-circuits to Timeout deterministically (mirrors the solver's
// negative-budget plumbing idiom in test_sudoku_solver_timeout.cpp). The pre-#24 rater called the
// no-budget overload and would instead grind into the backtracking fallback on this board.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) — Catch2 TEST_CASE macro expansion
TEST_CASE("PuzzleRater honors its solve budget (negative budget short-circuits to Timeout)",
          "[puzzle_rater][timeout]") {
    auto validator = std::make_shared<GameValidator>();
    auto mock_time = std::make_shared<MockTimeProvider>();
    auto solver = std::make_shared<SudokuSolver>(validator, mock_time);
    PuzzleRater rater(solver, std::chrono::milliseconds{-1});

    // AI Escargot — 17 clues, no logical-only path.
    BoardData board = {{1, 0, 0, 0, 0, 7, 0, 9, 0}, {0, 3, 0, 0, 2, 0, 0, 0, 8}, {0, 0, 9, 6, 0, 0, 5, 0, 0},
                       {0, 0, 5, 3, 0, 0, 9, 0, 0}, {0, 1, 0, 0, 8, 0, 0, 0, 2}, {6, 0, 0, 0, 0, 4, 0, 0, 0},
                       {3, 0, 0, 0, 0, 0, 0, 1, 0}, {0, 4, 0, 0, 0, 0, 0, 0, 7}, {0, 0, 7, 0, 0, 0, 3, 0, 0}};

    auto result = rater.ratePuzzle(board);

    REQUIRE(!result.has_value());
    REQUIRE(result.error() == RatingError::Timeout);
}

}  // anonymous namespace
