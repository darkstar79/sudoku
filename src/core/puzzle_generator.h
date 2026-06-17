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

#pragma once

#include "core/cpu_features.h"
#include "core/i_game_validator.h"
#include "game_validator.h"
#include "i_puzzle_generator.h"
#include "i_puzzle_rater.h"
#include "solution_counter.h"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <utility>
#include <vector>

namespace sudoku::core {

/// Analysis of a clue's constraint dependencies
/// Used for intelligent clue selection in Phase 2 optimization
struct ClueAnalysis {
    Position pos{};            ///< Position of the clue
    int value{0};              ///< Value at this position
    int alternative_count{0};  ///< Number of alternative values if clue removed
    int constraint_score{0};   ///< Higher score = more constrained (9 - alternative_count)
};

/// Concrete implementation of Sudoku puzzle generator
class PuzzleGenerator : public IPuzzleGenerator {
public:
    /// Constructor without rating (backward compatible)
    PuzzleGenerator();

    /// Constructor with rating validation
    /// @param rater Puzzle rater for validating difficulty
    explicit PuzzleGenerator(std::shared_ptr<IPuzzleRater> rater);

    ~PuzzleGenerator() override = default;
    PuzzleGenerator(const PuzzleGenerator&) = delete;
    PuzzleGenerator& operator=(const PuzzleGenerator&) = delete;
    PuzzleGenerator(PuzzleGenerator&&) = delete;
    PuzzleGenerator& operator=(PuzzleGenerator&&) = delete;

    /// Generates a new puzzle with specified settings
    std::expected<Puzzle, GenerationError> generatePuzzle(const GenerationSettings& settings) const override;

    /// Generates a puzzle with default settings for given difficulty
    std::expected<Puzzle, GenerationError> generatePuzzle(Difficulty difficulty) const override;

    /// Solves a given puzzle and returns the solution
    std::expected<BoardData, GenerationError> solvePuzzle(const BoardData& board) const override;

    /// Checks if a puzzle has a unique solution
    bool hasUniqueSolution(const BoardData& board) const override;

    /// Counts the number of clues (filled cells) in a puzzle
    int countClues(const BoardData& board) const override;

    /// Validates that a puzzle is properly formed
    bool validatePuzzle(const BoardData& board) const override;

    // Phase 1: Iterative Deepening Methods (for Expert puzzle generation)

    /// Attempts to remove clues from a solution to reach an exact target clue count
    [[nodiscard]] std::optional<BoardData> removeCluesToTarget(const BoardData& solution, int target_clues,
                                                               int max_attempts, std::mt19937& rng) const;

    /// Removes clues using iterative deepening strategy (Expert difficulty only)
    [[nodiscard]] std::optional<BoardData> removeCluesToCreatePuzzleIterative(const BoardData& solution,
                                                                              const GenerationSettings& settings,
                                                                              std::mt19937& rng) const;

    /// Runs one clue removal ordering on a copy of the solution.
    [[nodiscard]] int runRemovalOrdering(BoardData& puzzle, const std::vector<std::pair<size_t, size_t>>& positions,
                                         bool ensure_unique, int min_clues, int max_clues) const;

    // Phase 2: Intelligent Clue Dropping Methods (for improved re-completion)

    /// Analyzes constraint dependencies for all clues on the board
    [[nodiscard]] static std::vector<ClueAnalysis> analyzeClueConstraints(const BoardData& board);

    /// Selects clues to drop during re-completion phase
    [[nodiscard]] static std::vector<Position> selectCluesForDropping(const BoardData& board, int num_clues,
                                                                      std::mt19937& rng);

    // Phase 3: Constraint Propagation (delegated to SolutionCounter)

    /// Applies iterative constraint propagation until fixed point
    [[nodiscard]] static std::expected<BoardData, GenerationError> propagateConstraints(const BoardData& board) {
        return SolutionCounter::propagateConstraints(board);
    }

    /// Checks if board has any contradictions (empty domains)
    [[nodiscard]] static bool hasContradiction(const BoardData& board) {
        return SolutionCounter::hasContradiction(board);
    }

    /// Scalar version of constraint propagation (for testing)
    [[nodiscard]] static std::expected<BoardData, GenerationError> propagateConstraintsScalar(const BoardData& board) {
        return SolutionCounter::propagateConstraintsScalar(board);
    }

    /// SIMD version of constraint propagation (for testing)
    [[nodiscard]] static std::expected<BoardData, GenerationError> propagateConstraintsSIMD(const BoardData& board) {
        return SolutionCounter::propagateConstraintsSIMD(board);
    }

    /// Set solver path override for benchmarking (persists across calls until changed)
    void setSolverPath(SolverPath path) {
        solution_counter_.setSolverPath(path);
    }

    /// Get the currently active solver path
    [[nodiscard]] SolverPath solverPath() const {
        return solution_counter_.solverPath();
    }

private:
    GameValidator validator_;
    std::shared_ptr<IPuzzleRater> rater_;
    mutable SolutionCounter solution_counter_;

    /// Generates a complete valid Sudoku solution
    std::optional<BoardData> generateCompleteSolution(std::mt19937& rng) const;

    /// Rates a candidate puzzle (when a rater is configured) and confirms it falls within the
    /// expected SE range for its difficulty. Populates rating / required_techniques / backtracking
    /// flags on @p result. Returns false when the puzzle must be rejected so the caller retries.
    [[nodiscard]] bool rateAndValidatePuzzle(Puzzle& result, const GenerationSettings& settings) const;

    /// Removes clues from a complete solution to create a puzzle
    std::optional<BoardData> removeCluesToCreatePuzzle(const BoardData& solution, const GenerationSettings& settings,
                                                       std::mt19937& rng) const;

    /// Expert-only re-completion phase: drops clues, re-solves, retries removal orderings
    void runRecompletionPhase(BoardData& best_puzzle, int& best_clue_count,
                              std::vector<std::pair<size_t, size_t>>& positions, bool ensure_unique, int min_clues,
                              int max_clues, int num_orderings, std::mt19937& rng) const;

    /// Solves a puzzle using backtracking algorithm
    bool solvePuzzleBacktrack(BoardData& board) const;

    /// Fills the board with a valid complete solution using backtracking
    bool fillBoardRecursively(BoardData& board, std::mt19937& rng) const;

    /// Gets target clue count range for difficulty level
    static std::pair<int, int> getClueRange(Difficulty difficulty);

    /// Finds next empty position in board
    std::optional<Position> findEmptyPosition(const BoardData& board, bool use_mcv_heuristic = false) const;

    /// Board overload for findEmptyPosition
    std::optional<Position> findEmptyPosition(const Board& board, bool use_mcv_heuristic = false) const;

    /// Gets randomized list of numbers 1-9
    static std::vector<int> getShuffledNumbers(std::mt19937& rng);

    /// Creates empty 9x9 board
    static BoardData createEmptyBoard();
};

}  // namespace sudoku::core
