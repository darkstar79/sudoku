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
#include "i_game_validator.h"
#include "i_sudoku_solver.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sudoku::core {

class SolutionCounter;  // forward — full header pulled into the .cpp

/// Errors produced by PuzzleAnalyzer::parseString for malformed input.
enum class ParseError : std::uint8_t {
    InputTooLarge,     ///< Input length exceeded the 4 KiB cap (pre-mortem #10).
    WrongLength,       ///< After stripping whitespace, decoded cell count != 81.
    InvalidCharacter,  ///< Encountered a byte that is neither digit, '.', '_', nor whitespace.
    Empty,             ///< Zero cells decoded after parsing.
};

/// Structured detail accompanying a ParseError. Empty fields are zero-initialized for
/// codes that don't populate them.
struct ParseErrorDetail {
    ParseError code{};
    std::size_t position{0};  ///< Byte offset into input — set for InvalidCharacter.
    int actual_length{0};     ///< Decoded cell count — set for WrongLength.
    char bad_char{'\0'};      ///< Offending byte — set for InvalidCharacter.

    bool operator==(const ParseErrorDetail&) const = default;
};

/// Validation error: the input board violates Sudoku rules.
///
/// Carries the offending cells so the View can highlight them (pre-mortem #5).
/// Empty `conflicting_cells` is a sentinel meaning "no specific cells" (not currently emitted).
struct BoardValidationError {
    std::vector<Position> conflicting_cells;

    bool operator==(const BoardValidationError&) const = default;
};

/// Uniqueness check outcome.
///
/// `Unknown` is reserved for the case where the solution counter exceeded its budget without
/// finalizing a count — at this layer we currently emit only Unique / MultipleSolutions because
/// SolutionCounter::countSolutionsWithTimeout collapses timeout into "looks non-unique." A
/// future API can plumb the timed_out flag through to surface Unknown cleanly.
enum class UniquenessResult : std::uint8_t {
    Unique,
    MultipleSolutions,
    Unknown,
};

/// Difficulty score produced by scoring an imported puzzle.
///
/// `max_rating` is the highest Sudoku Explainer rating across the solve path's steps.
/// `requires_backtracking` is set when the logical techniques didn't suffice and the solver
/// fell back to brute force. `technique_ids` is the set of distinct SolvingTechnique enum
/// values (cast to int) the solver actually used — useful for the View's "techniques used"
/// callout. There is no "fallback label" — that's a presentation concern.
struct DifficultyScore {
    double max_rating{0.0};
    bool requires_backtracking{false};
    std::vector<int> technique_ids;

    bool operator==(const DifficultyScore&) const = default;
};

/// Failure modes for scoreDifficulty.
enum class ScoringError : std::uint8_t {
    Timeout,       ///< Solver exceeded the supplied wall-clock budget.
    NoSolution,    ///< Solver returned Unsolvable.
    InvalidInput,  ///< Board failed validate() before scoring was attempted.
};

/// Interface for puzzle analyzer operations: parsing, validation, uniqueness, scoring.
/// Implementations are DI-resolved so the ViewModel layer can collaborate with one shared
/// abstraction regardless of which validator/solver/counter is wired up.
class IPuzzleAnalyzer {
public:
    virtual ~IPuzzleAnalyzer() = default;

    [[nodiscard]] virtual std::expected<BoardData, ParseErrorDetail> parseString(std::string_view input) const = 0;

    [[nodiscard]] virtual std::string serializeToString(const BoardData& board) const = 0;

    [[nodiscard]] virtual std::expected<void, BoardValidationError> validate(const BoardData& board) const = 0;

    [[nodiscard]] virtual UniquenessResult checkUniqueness(const BoardData& board) const = 0;

    [[nodiscard]] virtual std::expected<DifficultyScore, ScoringError>
    scoreDifficulty(const BoardData& board, std::chrono::milliseconds budget) const = 0;

protected:
    IPuzzleAnalyzer() = default;
    IPuzzleAnalyzer(const IPuzzleAnalyzer&) = default;
    IPuzzleAnalyzer& operator=(const IPuzzleAnalyzer&) = default;
    IPuzzleAnalyzer(IPuzzleAnalyzer&&) = default;
    IPuzzleAnalyzer& operator=(IPuzzleAnalyzer&&) = default;
};

/// Concrete puzzle analyzer. Wraps an IGameValidator (rule conflicts), an ISudokuSolver
/// (scoring via solve path), and a SolutionCounter (uniqueness). Each dependency is optional
/// for partial setups — parsing/serializing only require no deps.
class PuzzleAnalyzer : public IPuzzleAnalyzer {
public:
    /// Max input size accepted by parseString. Larger inputs short-circuit with InputTooLarge.
    static constexpr std::size_t kMaxInputBytes = 4096;

    /// Default constructor — supports parseString/serializeToString without any deps.
    PuzzleAnalyzer() = default;

    /// Constructor with validator for the validate() method.
    explicit PuzzleAnalyzer(std::shared_ptr<IGameValidator> validator) : validator_(std::move(validator)) {
    }

    /// Constructor with validator and solution counter for validate() + checkUniqueness().
    PuzzleAnalyzer(std::shared_ptr<IGameValidator> validator, std::shared_ptr<SolutionCounter> counter)
        : validator_(std::move(validator)), counter_(std::move(counter)) {
    }

    /// Full constructor — required for scoreDifficulty().
    PuzzleAnalyzer(std::shared_ptr<IGameValidator> validator, std::shared_ptr<ISudokuSolver> solver,
                   std::shared_ptr<SolutionCounter> counter)
        : validator_(std::move(validator)), solver_(std::move(solver)), counter_(std::move(counter)) {
    }

    /// Parses a paste-string into a BoardData. See ParseError for the full ruleset.
    [[nodiscard]] std::expected<BoardData, ParseErrorDetail> parseString(std::string_view input) const override;

    /// Serializes a BoardData as an 81-character string. Empty cells render as '.'.
    [[nodiscard]] std::string serializeToString(const BoardData& board) const override;

    /// Validates that the board has no rule conflicts (rows, columns, boxes).
    [[nodiscard]] std::expected<void, BoardValidationError> validate(const BoardData& board) const override;

    /// Determines whether the board has exactly one solution. Clears the counter's cache first
    /// (pre-mortem #9). Uses a 5-second internal budget.
    [[nodiscard]] UniquenessResult checkUniqueness(const BoardData& board) const override;

    /// Scores the difficulty of a puzzle by solving it within the supplied wall-clock budget.
    /// Returns max_rating + technique set on success, or ScoringError on failure.
    [[nodiscard]] std::expected<DifficultyScore, ScoringError>
    scoreDifficulty(const BoardData& board, std::chrono::milliseconds budget) const override;

private:
    std::shared_ptr<IGameValidator> validator_;
    std::shared_ptr<ISudokuSolver> solver_;
    std::shared_ptr<SolutionCounter> counter_;
};

}  // namespace sudoku::core
