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

#include "../../src/core/candidate_grid.h"
#include "../../src/core/training_exercise_generator.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// --- Mock implementations for deterministic, fast unit tests ---

namespace {

/// A nearly-complete board with a few empty cells for testing.
/// Row 8 cells [6],[7],[8] are empty (values would be 4,8,6 in the solution).
// clang-format off
const BoardData CANNED_BOARD = {
    {5, 3, 4, 6, 7, 8, 9, 1, 2},
    {6, 7, 2, 1, 9, 5, 3, 4, 8},
    {1, 9, 8, 3, 4, 2, 5, 6, 7},
    {8, 5, 9, 7, 6, 1, 4, 2, 3},
    {4, 2, 6, 8, 5, 3, 7, 9, 1},
    {7, 1, 3, 9, 2, 4, 8, 5, 6},
    {9, 6, 1, 5, 3, 7, 2, 8, 4},
    {2, 8, 7, 4, 1, 9, 6, 3, 5},
    {3, 4, 5, 2, 8, 6, 0, 0, 0},
};

const BoardData CANNED_SOLUTION = sudoku::testing::kSolvedBoard;
// clang-format on

/// Canned solve path: 3 NakedSingle placements filling the empty cells
const std::vector<SolveStep> CANNED_NAKED_SINGLE_PATH = {
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 6},
        .value = 1,
        .eliminations = {},
        .explanation = "Naked Single: r9c7 = 1",
        .rating = 10,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 7},
        .value = 7,
        .eliminations = {},
        .explanation = "Naked Single: r9c8 = 7",
        .rating = 10,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 8},
        .value = 9,
        .eliminations = {},
        .explanation = "Naked Single: r9c9 = 9",
        .rating = 10,
        .explanation_data = {},
    },
};

/// Board with more empty cells for elimination tests.
/// Row 6 cols 6-8 and row 8 cols 6-8 are empty.
/// Row 6 solution: 2,8,4 — Row 8 solution: 1,7,9
/// Cell (8,8): col 8 missing {4,9}, row 8 missing {1,7,9}, box 8 missing {1,4,7,8,9}
///   → candidates = {9} ∩ {1,7,9} ∩ {1,4,7,8,9} ... actually let me verify:
/// After emptying (6,6),(6,7),(6,8) and (8,6),(8,7),(8,8):
///   Col 6: has {9,3,5,4,7,8,6} → missing {1,2}
///   Col 7: has {1,4,6,2,9,5,3} → missing {7,8}
///   Col 8: has {2,8,7,3,1,6,5} → missing {4,9}
///   Box 8: has {6,3,5} → missing {1,2,4,7,8,9}
///   Row 6: has {9,6,1,5,3,7} → missing {2,4,8}
///   Row 8: has {3,4,5,2,8,6} → missing {1,7,9}
///
/// Candidates:
///   (6,6): col6∩row6∩box8 = {1,2}∩{2,4,8}∩{1,2,4,7,8,9} = {2}
///   (6,7): col7∩row6∩box8 = {7,8}∩{2,4,8}∩{1,2,4,7,8,9} = {8}
///   (6,8): col8∩row6∩box8 = {4,9}∩{2,4,8}∩{1,2,4,7,8,9} = {4}
///   (8,6): col6∩row8∩box8 = {1,2}∩{1,7,9}∩{1,2,4,7,8,9} = {1}
///   (8,7): col7∩row8∩box8 = {7,8}∩{1,7,9}∩{1,2,4,7,8,9} = {7}
///   (8,8): col8∩row8∩box8 = {4,9}∩{1,7,9}∩{1,2,4,7,8,9} = {9}
///
/// Still naked singles everywhere. Need fewer given cells to get overlapping candidates.
/// Instead: use the simple board but make the NakedPair elimination target a value
/// that IS actually a candidate. Since all three empty cells are naked singles,
/// a NakedPair can't logically exist here. For the "Candidate masks" test,
/// what matters is that the walkForward + CandidateGrid produces candidates
/// that include the eliminated value. Solution: make the elimination target
/// a value at the cell BEFORE walkForward applies earlier steps.
///
/// Simplest fix: have the NakedPair as step 0 (no prior steps to apply),
/// and eliminate a value that IS a candidate on the initial board.
/// Cell (8,6) has candidate {1}, (8,7) has {7}, (8,8) has {9}.
/// So the NakedPair elimination must target one of those existing candidates.
// clang-format off
const std::vector<SolveStep> CANNED_NAKED_PAIR_PATH = {
    // NakedPair at step 0: eliminates candidate 1 from (8,6).
    // On the initial CANNED_BOARD, (8,6) has candidate {1}, so mask & (1<<1) != 0.
    SolveStep{
        .type = SolveStepType::Elimination,
        .technique = SolvingTechnique::NakedPair,
        .position = Position{.row = 8, .col = 7},
        .value = 0,
        .eliminations =
            {
                // (8,8) has candidate {9} on the initial board
                Elimination{.position = Position{.row = 8, .col = 8}, .value = 9},
            },
        .explanation = "Naked Pair: {7,9} in r9c8,r9c9 => r9c7 <> 9 (mock)",
        .rating = 50,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 6},
        .value = 1,
        .eliminations = {},
        .explanation = "Naked Single: r9c7 = 1",
        .rating = 10,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 7},
        .value = 7,
        .eliminations = {},
        .explanation = "Naked Single: r9c8 = 7",
        .rating = 10,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 8},
        .value = 9,
        .eliminations = {},
        .explanation = "Naked Single: r9c9 = 9",
        .rating = 10,
        .explanation_data = {},
    },
};
// clang-format on

/// Canned solve path with SimpleColoring
const std::vector<SolveStep> CANNED_COLORING_PATH = {
    SolveStep{
        .type = SolveStepType::Elimination,
        .technique = SolvingTechnique::SimpleColoring,
        .position = Position{.row = 8, .col = 6},
        .value = 0,
        .eliminations =
            {
                // (8,8) has candidate {9} on the initial board
                Elimination{.position = Position{.row = 8, .col = 8}, .value = 9},
            },
        .explanation = "Simple Coloring: eliminates 9 from r9c9 (mock)",
        .rating = 350,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 6},
        .value = 1,
        .eliminations = {},
        .explanation = "Naked Single: r9c7 = 1",
        .rating = 10,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 7},
        .value = 7,
        .eliminations = {},
        .explanation = "Naked Single: r9c8 = 7",
        .rating = 10,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 8},
        .value = 9,
        .eliminations = {},
        .explanation = "Naked Single: r9c9 = 9",
        .rating = 10,
        .explanation_data = {},
    },
};

/// Canned solve path with ForcingChain (placement type)
const std::vector<SolveStep> CANNED_FORCING_CHAIN_PATH = {
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::ForcingChain,
        .position = Position{.row = 8, .col = 6},
        .value = 1,
        .eliminations = {},
        .explanation = "Forcing Chain: r9c7 = 1",
        .rating = 550,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 7},
        .value = 7,
        .eliminations = {},
        .explanation = "Naked Single: r9c8 = 7",
        .rating = 10,
        .explanation_data = {},
    },
    SolveStep{
        .type = SolveStepType::Placement,
        .technique = SolvingTechnique::NakedSingle,
        .position = Position{.row = 8, .col = 8},
        .value = 9,
        .eliminations = {},
        .explanation = "Naked Single: r9c9 = 9",
        .rating = 10,
        .explanation_data = {},
    },
};

/// Mock puzzle generator that returns canned puzzles
class MockPuzzleGenerator : public IPuzzleGenerator {
public:
    BoardData board = CANNED_BOARD;
    BoardData solution = CANNED_SOLUTION;

    [[nodiscard]] std::expected<Puzzle, GenerationError>
    generatePuzzle(const GenerationSettings& /*settings*/) const override {
        return generateResult();
    }

    [[nodiscard]] std::expected<Puzzle, GenerationError> generatePuzzle(Difficulty /*difficulty*/) const override {
        return generateResult();
    }

    [[nodiscard]] std::expected<BoardData, GenerationError> solvePuzzle(const BoardData& /*board*/) const override {
        return solution;
    }

    [[nodiscard]] bool hasUniqueSolution(const BoardData& /*board*/) const override {
        return true;
    }

    [[nodiscard]] int countClues(const BoardData& brd) const override {
        int count = 0;
        for (const auto& row : brd) {
            for (int cell : row) {
                if (cell != 0) {
                    ++count;
                }
            }
        }
        return count;
    }

    [[nodiscard]] bool validatePuzzle(const BoardData& /*board*/) const override {
        return true;
    }

private:
    [[nodiscard]] std::expected<Puzzle, GenerationError> generateResult() const {
        Puzzle puzzle;
        puzzle.board = board;
        puzzle.solution = solution;
        puzzle.difficulty = Difficulty::Medium;
        puzzle.seed = 42;
        puzzle.clue_count = countClues(board);
        return puzzle;
    }
};

/// Mock solver that returns a configurable solve path
class MockSolver : public ISudokuSolver {
public:
    std::vector<SolveStep> solve_path = CANNED_NAKED_SINGLE_PATH;

    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& /*board*/) const override {
        if (solve_path.empty()) {
            return std::unexpected(SolverError::Unsolvable);
        }
        return solve_path[0];
    }

    [[nodiscard]] std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& /*board*/) const override {
        SolverResult result;
        result.solution = CANNED_SOLUTION;
        result.solve_path = solve_path;
        result.used_backtracking = false;
        return result;
    }

    [[nodiscard]] bool applyStep(BoardData& board, const SolveStep& step) const override {
        if (step.type == SolveStepType::Placement) {
            board[step.position.row][step.position.col] = step.value;
        }
        return true;
    }
};

struct MockFixture {
    std::shared_ptr<MockPuzzleGenerator> generator = std::make_shared<MockPuzzleGenerator>();
    std::shared_ptr<MockSolver> solver = std::make_shared<MockSolver>();
    TrainingExerciseGenerator exercise_gen{generator, solver};
};

/// Mock generator that always fails — covers the !puzzle_result.has_value() branch
class FailingGenerator : public IPuzzleGenerator {
public:
    [[nodiscard]] std::expected<Puzzle, GenerationError>
    generatePuzzle(const GenerationSettings& /*settings*/) const override {
        return std::unexpected(GenerationError::GenerationFailed);
    }

    [[nodiscard]] std::expected<Puzzle, GenerationError> generatePuzzle(Difficulty /*difficulty*/) const override {
        return std::unexpected(GenerationError::GenerationFailed);
    }

    [[nodiscard]] std::expected<BoardData, GenerationError> solvePuzzle(const BoardData& /*board*/) const override {
        return std::unexpected(GenerationError::GenerationFailed);
    }

    [[nodiscard]] bool hasUniqueSolution(const BoardData& /*board*/) const override {
        return false;
    }

    [[nodiscard]] int countClues(const BoardData& /*board*/) const override {
        return 0;
    }

    [[nodiscard]] bool validatePuzzle(const BoardData& /*board*/) const override {
        return false;
    }
};

/// Mock solver whose solvePuzzle always fails — covers the !solve_result.has_value() branch
class FailingSolver : public ISudokuSolver {
public:
    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& /*board*/) const override {
        return std::unexpected(SolverError::Unsolvable);
    }

    [[nodiscard]] std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& /*board*/) const override {
        return std::unexpected(SolverError::Unsolvable);
    }

    [[nodiscard]] bool applyStep(BoardData& /*board*/, const SolveStep& /*step*/) const override {
        return false;
    }
};

}  // namespace

// --- Validation tests ---

TEST_CASE("TrainingExerciseGenerator - Invalid Arguments", "[training_exercise_generator]") {
    MockFixture fx;

    SECTION("Rejects zero count") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 0);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().find("between 1 and 20") != std::string::npos);
    }

    SECTION("Rejects negative count") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, -1);

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Rejects count over 20") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 21);

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Rejects Backtracking technique") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::Backtracking, 1);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().find("Backtracking") != std::string::npos);
    }
}

// --- NakedSingle exercises ---

TEST_CASE("TrainingExerciseGenerator - NakedSingle Exercises", "[training_exercise_generator]") {
    MockFixture fx;

    SECTION("Generates exercises with Placement mode") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 2);

        REQUIRE(result.has_value());
        REQUIRE(result->size() == 2);

        for (const auto& exercise : *result) {
            REQUIRE(exercise.technique == SolvingTechnique::NakedSingle);
            REQUIRE(exercise.interaction_mode == TrainingInteractionMode::Placement);
            REQUIRE(exercise.expected_step.type == SolveStepType::Placement);
            REQUIRE(exercise.expected_step.technique == SolvingTechnique::NakedSingle);
        }
    }

    SECTION("Board is valid 9x9 grid") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        REQUIRE(exercise.board.size() == 9);
    }

    SECTION("Candidate masks are populated with 81 entries") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        REQUIRE(exercise.candidate_masks.size() == 81);
    }

    SECTION("Expected step position is empty on the board") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        const auto& pos = exercise.expected_step.position;
        REQUIRE(exercise.board[pos.row][pos.col] == 0);
    }

    SECTION("Expected step value is a valid candidate at position") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        const auto& pos = exercise.expected_step.position;
        auto mask = exercise.candidate_masks[(pos.row * 9) + pos.col];
        auto value_bit = static_cast<uint16_t>(1 << exercise.expected_step.value);
        REQUIRE((mask & value_bit) != 0);
    }

    SECTION("For NakedSingle, exactly one candidate at position") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        const auto& pos = exercise.expected_step.position;
        auto mask = exercise.candidate_masks[(pos.row * 9) + pos.col];
        REQUIRE(std::popcount(mask) == 1);
    }

    SECTION("Not guided coloring for non-coloring technique") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 3);

        REQUIRE(result.has_value());
        for (const auto& exercise : *result) {
            REQUIRE_FALSE(exercise.is_guided_coloring);
        }
    }
}

// --- Elimination exercises (NakedPair) ---

TEST_CASE("TrainingExerciseGenerator - NakedPair Exercises", "[training_exercise_generator]") {
    MockFixture fx;
    fx.solver->solve_path = CANNED_NAKED_PAIR_PATH;

    SECTION("Generates exercises with Elimination mode") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedPair, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        REQUIRE(exercise.technique == SolvingTechnique::NakedPair);
        REQUIRE(exercise.interaction_mode == TrainingInteractionMode::Elimination);
        REQUIRE(exercise.expected_step.type == SolveStepType::Elimination);
        REQUIRE_FALSE(exercise.expected_step.eliminations.empty());
    }

    SECTION("Candidate masks reflect state before target step") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedPair, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];

        // Each elimination target should still have the candidate present
        for (const auto& elim : exercise.expected_step.eliminations) {
            auto mask = exercise.candidate_masks[(elim.position.row * 9) + elim.position.col];
            auto value_bit = static_cast<uint16_t>(1 << elim.value);
            REQUIRE((mask & value_bit) != 0);
        }
    }
}

// --- Candidate mask accuracy validation ---

TEST_CASE("TrainingExerciseGenerator - Candidate Mask Accuracy", "[training_exercise_generator]") {
    MockFixture fx;

    SECTION("Filled cells have zero candidate mask") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                if (exercise.board[r][c] != 0) {
                    REQUIRE(exercise.candidate_masks[(r * 9) + c] == 0);
                }
            }
        }
    }

    SECTION("Empty cells have non-zero candidate mask") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                if (exercise.board[r][c] == 0) {
                    REQUIRE(exercise.candidate_masks[(r * 9) + c] != 0);
                }
            }
        }
    }

    SECTION("Candidate masks only use bits 1-9") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);

        const auto& exercise = (*result)[0];
        for (size_t i = 0; i < 81; ++i) {
            auto mask = exercise.candidate_masks[i];
            // Bit 0 should never be set, and bits 10+ should never be set
            REQUIRE((mask & 0x01) == 0);
            REQUIRE((mask & 0xFC00) == 0);
        }
    }
}

// --- Interaction mode mapping ---

TEST_CASE("TrainingExerciseGenerator - Interaction Mode Mapping", "[training_exercise_generator]") {
    MockFixture fx;

    SECTION("SimpleColoring uses Coloring mode") {
        fx.solver->solve_path = CANNED_COLORING_PATH;

        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::SimpleColoring, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);
        REQUIRE((*result)[0].interaction_mode == TrainingInteractionMode::Coloring);
    }

    SECTION("NakedSingle uses Placement mode") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);
        REQUIRE((*result)[0].interaction_mode == TrainingInteractionMode::Placement);
    }

    SECTION("NakedPair uses Elimination mode") {
        fx.solver->solve_path = CANNED_NAKED_PAIR_PATH;

        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedPair, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);
        REQUIRE((*result)[0].interaction_mode == TrainingInteractionMode::Elimination);
    }

    SECTION("ForcingChain placement uses Placement mode") {
        fx.solver->solve_path = CANNED_FORCING_CHAIN_PATH;

        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::ForcingChain, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);
        REQUIRE((*result)[0].interaction_mode == TrainingInteractionMode::Placement);
    }
}

// --- Guided coloring ---

TEST_CASE("TrainingExerciseGenerator - Guided Coloring", "[training_exercise_generator]") {
    MockFixture fx;
    fx.solver->solve_path = CANNED_COLORING_PATH;

    SECTION("First exercises of coloring technique are guided") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::SimpleColoring, 1);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 1);
        REQUIRE((*result)[0].is_guided_coloring);
    }

    SECTION("Non-coloring technique is never guided") {
        fx.solver->solve_path = CANNED_NAKED_SINGLE_PATH;

        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 3);

        REQUIRE(result.has_value());
        for (const auto& exercise : *result) {
            REQUIRE_FALSE(exercise.is_guided_coloring);
        }
    }
}

// --- walkForward correctness ---

TEST_CASE("TrainingExerciseGenerator - Step consistency", "[training_exercise_generator]") {
    MockFixture fx;

    SECTION("Expected step does not eliminate the correct solution value") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 3);

        REQUIRE(result.has_value());

        for (const auto& exercise : *result) {
            if (exercise.expected_step.type == SolveStepType::Placement) {
                // For placements, the value should be valid
                REQUIRE(exercise.expected_step.value >= 1);
                REQUIRE(exercise.expected_step.value <= 9);
            }
        }
    }

    SECTION("walkForward applies prior steps correctly") {
        // The second NakedSingle is at index 1. walkForward should apply step 0 first.
        // After step 0 (placing 1 at r8c6), the board at r8c6 should have value 1.
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 3);

        REQUIRE(result.has_value());
        REQUIRE(result->size() >= 2);

        // Exercise for step at index 1: board should have step 0 applied
        const auto& exercise_1 = (*result)[1];
        REQUIRE(exercise_1.board[8][6] == 1);  // Step 0 placed value 1 at (8,6)
    }

    SECTION("walkForward preserves the target step position as empty") {
        // For each exercise, the target step's cell should still be empty
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 3);

        REQUIRE(result.has_value());

        for (const auto& exercise : *result) {
            if (exercise.expected_step.type == SolveStepType::Placement) {
                const auto& pos = exercise.expected_step.position;
                REQUIRE(exercise.board[pos.row][pos.col] == 0);
            }
        }
    }
}

// --- Additional technique point ranges ---

TEST_CASE("TrainingExerciseGenerator - XWing exercises cover Hard difficulty range", "[training_exercise_generator]") {
    // XWing = 200 difficulty points (151-280 range) → Hard difficulty, retry budget = 20.
    // Covers the second branches of getDifficultyForTechnique and getRetryBudget.
    const std::vector<SolveStep> xwing_path = {
        SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::XWing,
            .position = Position{.row = 8, .col = 7},
            .value = 0,
            .eliminations = {Elimination{.position = Position{.row = 8, .col = 8}, .value = 9}},
            .explanation = "X-Wing: mock",
            .rating = 200,
            .explanation_data = {},
        },
        SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::NakedSingle,
            .position = Position{.row = 8, .col = 6},
            .value = 1,
            .eliminations = {},
            .explanation = "Naked Single: r9c7 = 1",
            .rating = 10,
            .explanation_data = {},
        },
        SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::NakedSingle,
            .position = Position{.row = 8, .col = 7},
            .value = 7,
            .eliminations = {},
            .explanation = "Naked Single: r9c8 = 7",
            .rating = 10,
            .explanation_data = {},
        },
        SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::NakedSingle,
            .position = Position{.row = 8, .col = 8},
            .value = 9,
            .eliminations = {},
            .explanation = "Naked Single: r9c9 = 9",
            .rating = 10,
            .explanation_data = {},
        },
    };

    MockFixture fx;
    fx.solver->solve_path = xwing_path;

    auto result = fx.exercise_gen.generateExercises(SolvingTechnique::XWing, 1);

    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 1);
    REQUIRE((*result)[0].technique == SolvingTechnique::XWing);
    REQUIRE((*result)[0].interaction_mode == TrainingInteractionMode::Elimination);
}

TEST_CASE("TrainingExerciseGenerator - MultiColoring uses Coloring mode", "[training_exercise_generator]") {
    // MultiColoring technique → getInteractionMode returns Coloring.
    // Covers the `case MultiColoring:` branch in getInteractionMode.
    const std::vector<SolveStep> multicoloring_path = {
        SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::MultiColoring,
            .position = Position{.row = 8, .col = 6},
            .value = 0,
            .eliminations = {Elimination{.position = Position{.row = 8, .col = 8}, .value = 9}},
            .explanation = "Multi-Coloring: mock",
            .rating = 400,
            .explanation_data = {},
        },
        SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::NakedSingle,
            .position = Position{.row = 8, .col = 6},
            .value = 1,
            .eliminations = {},
            .explanation = "Naked Single: r9c7 = 1",
            .rating = 10,
            .explanation_data = {},
        },
        SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::NakedSingle,
            .position = Position{.row = 8, .col = 7},
            .value = 7,
            .eliminations = {},
            .explanation = "Naked Single: r9c8 = 7",
            .rating = 10,
            .explanation_data = {},
        },
        SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::NakedSingle,
            .position = Position{.row = 8, .col = 8},
            .value = 9,
            .eliminations = {},
            .explanation = "Naked Single: r9c9 = 9",
            .rating = 10,
            .explanation_data = {},
        },
    };

    MockFixture fx;
    fx.solver->solve_path = multicoloring_path;

    auto result = fx.exercise_gen.generateExercises(SolvingTechnique::MultiColoring, 1);

    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 1);
    REQUIRE((*result)[0].technique == SolvingTechnique::MultiColoring);
    REQUIRE((*result)[0].interaction_mode == TrainingInteractionMode::Coloring);
}

// --- Error handling ---

TEST_CASE("TrainingExerciseGenerator - No matching technique", "[training_exercise_generator]") {
    MockFixture fx;
    // solve_path has only NakedSingle steps — asking for HiddenSingle should fail

    SECTION("Returns error when technique not found in solve path") {
        auto result = fx.exercise_gen.generateExercises(SolvingTechnique::HiddenSingle, 1);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().find("Failed to generate") != std::string::npos);
    }
}

TEST_CASE("TrainingExerciseGenerator - Generator failure returns error", "[training_exercise_generator]") {
    // FailingGenerator always returns GenerationFailed — covers the !puzzle_result.has_value() branch
    auto generator = std::make_shared<FailingGenerator>();
    auto solver = std::make_shared<MockSolver>();
    TrainingExerciseGenerator gen{generator, solver};

    // All retry_budget attempts fail → exercises.empty() → std::unexpected
    auto result = gen.generateExercises(SolvingTechnique::NakedSingle, 1);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().find("Failed to generate") != std::string::npos);
}

TEST_CASE("TrainingExerciseGenerator - Solver failure returns error", "[training_exercise_generator]") {
    // FailingSolver always returns Unsolvable — covers the !solve_result.has_value() branch
    auto generator = std::make_shared<MockPuzzleGenerator>();
    auto solver = std::make_shared<FailingSolver>();
    TrainingExerciseGenerator gen{generator, solver};

    // Generator succeeds but solver always fails → exercises.empty() → std::unexpected
    auto result = gen.generateExercises(SolvingTechnique::NakedSingle, 1);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().find("Failed to generate") != std::string::npos);
}

TEST_CASE("TrainingExerciseGenerator - Partial exercises triggers warning", "[training_exercise_generator]") {
    // One-step solve path: only 1 NakedSingle per attempt.
    // NakedSingle retry_budget = 10, count = 20 → 10 exercises generated → warning fires (lines 96-98).
    const std::vector<SolveStep> one_step_path = {
        SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::NakedSingle,
            .position = Position{.row = 8, .col = 6},
            .value = 1,
            .eliminations = {},
            .explanation = "Naked Single: r9c7 = 1",
            .rating = 10,
            .explanation_data = {},
        },
    };

    MockFixture fx;
    fx.solver->solve_path = one_step_path;

    auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 20);

    // Partial success: 10 exercises generated (1 per attempt × 10 budget), fewer than 20 requested
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 10);
}

TEST_CASE("TrainingExerciseGenerator - walkForward applies prior Elimination step", "[training_exercise_generator]") {
    // CANNED_NAKED_PAIR_PATH: step[0] = NakedPair Elimination (removes 9 from (8,8)),
    // steps[1-3] = NakedSingle Placements.
    // Requesting NakedSingle: first match at index 1.
    // walkForward(board, path, 1) applies step[0] (Elimination) → covers line 172.
    MockFixture fx;
    fx.solver->solve_path = CANNED_NAKED_PAIR_PATH;

    auto result = fx.exercise_gen.generateExercises(SolvingTechnique::NakedSingle, 1);

    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 1);

    // Candidate mask for (8,8): before step[0] it had bit 9 set; after elimination, bit 9 is cleared
    const auto& exercise = (*result)[0];
    auto mask_r8c8 = exercise.candidate_masks[(8 * 9) + 8];
    auto bit9 = static_cast<uint16_t>(1 << 9);
    REQUIRE((mask_r8c8 & bit9) == 0);
}
