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

#include "core/board_data.h"
#include "core/constants.h"
#include "cpu_features.h"
#include "solving_technique.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace sudoku::core {

/// Difficulty levels for puzzle generation
enum class Difficulty : std::uint8_t {
    Easy = 0,    // 36-40 clues
    Medium = 1,  // 28-35 clues
    Hard = 2,    // 22-27 clues
    Expert = 3,  // 17-27 clues (rating determines difficulty)
    Master = 4   // 17-27 clues (open-ended, hardest puzzles)
};

// Keep DIFFICULTY_COUNT in lockstep with the enum. If a level is added or removed,
// bump DIFFICULTY_COUNT in core/constants.h or this assertion fires at compile time.
static_assert(static_cast<std::size_t>(Difficulty::Master) + 1 == DIFFICULTY_COUNT,
              "DIFFICULTY_COUNT must match the number of Difficulty enum values");

/// Puzzle generation settings
struct GenerationSettings {
    Difficulty difficulty{Difficulty::Medium};
    uint32_t seed{0};                                     // 0 for random seed
    bool ensure_unique{true};                             // Ensure puzzle has unique solution
    int max_attempts{1000};                               // Max attempts before giving up
    std::optional<SolverPath> solver_path{std::nullopt};  // nullopt = Auto
};

/// Generated puzzle data
struct Puzzle {
    BoardData board;     // 9x9 puzzle with clues
    BoardData solution;  // 9x9 complete solution
    Difficulty difficulty{Difficulty::Easy};
    uint32_t seed{0};
    int clue_count{0};
    double rating{0.0};                              // Sudoku Explainer rating (0.0 = not rated yet)
    std::set<SolvingTechnique> required_techniques;  // Logical techniques needed to solve
    bool requires_backtracking{false};               // True if logical techniques alone insufficient
};

/// Error types for puzzle generation
enum class GenerationError : std::uint8_t {
    InvalidDifficulty,
    GenerationFailed,
    NoUniqueSolution,
    TooManyAttempts,
    InvalidSeed
};

/// Interface for generating Sudoku puzzles
class IPuzzleGenerator {
public:
    virtual ~IPuzzleGenerator() = default;

    /// Generates a new puzzle with specified settings
    /// @param settings Generation parameters
    /// @return Generated puzzle or error
    [[nodiscard]] virtual std::expected<Puzzle, GenerationError>
    generatePuzzle(const GenerationSettings& settings) const = 0;

    /// Generates a puzzle with default settings for given difficulty
    /// @param difficulty Desired difficulty level
    /// @return Generated puzzle or error
    [[nodiscard]] virtual std::expected<Puzzle, GenerationError> generatePuzzle(Difficulty difficulty) const = 0;

    /// Solves a given puzzle and returns the solution
    /// @param board Puzzle to solve (9x9 with some filled cells)
    /// @return Complete solution or error if unsolvable
    [[nodiscard]] virtual std::expected<BoardData, GenerationError> solvePuzzle(const BoardData& board) const = 0;

    /// Checks if a puzzle has a unique solution
    /// @param board Puzzle to check
    /// @return true if unique solution exists
    [[nodiscard]] virtual bool hasUniqueSolution(const BoardData& board) const = 0;

    /// Counts the number of clues (filled cells) in a puzzle
    /// @param board Puzzle board
    /// @return Number of filled cells
    [[nodiscard]] virtual int countClues(const BoardData& board) const = 0;

    /// Validates that a puzzle is properly formed
    /// @param board Puzzle to validate
    /// @return true if puzzle is valid
    [[nodiscard]] virtual bool validatePuzzle(const BoardData& board) const = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    IPuzzleGenerator() = default;
    IPuzzleGenerator(const IPuzzleGenerator&) = default;
    IPuzzleGenerator& operator=(const IPuzzleGenerator&) = default;
    IPuzzleGenerator(IPuzzleGenerator&&) = default;
    IPuzzleGenerator& operator=(IPuzzleGenerator&&) = default;
};

}  // namespace sudoku::core