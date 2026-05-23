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

#include "puzzle_analyzer.h"

#include "core/board_data.h"
#include "solution_counter.h"
#include "solving_technique.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace sudoku::core {

namespace {

constexpr std::size_t kCellsPerBoard = 81;
constexpr std::size_t kRowSize = 9;

// True for ASCII whitespace only. Non-ASCII (≥ 0x80) bytes are treated as InvalidCharacter
// even if they happen to be Unicode whitespace — the plan's parsing surface is ASCII-only.
[[nodiscard]] bool isAsciiWhitespace(unsigned char c) {
    return std::isspace(c) != 0;
}

[[nodiscard]] bool isEmptyMarker(char c) {
    return c == '0' || c == '.' || c == '_';
}

}  // namespace

std::expected<BoardData, ParseErrorDetail> PuzzleAnalyzer::parseString(std::string_view input) const {
    if (input.size() > kMaxInputBytes) {
        return std::unexpected(ParseErrorDetail{.code = ParseError::InputTooLarge});
    }

    std::array<int, kCellsPerBoard> decoded{};
    std::size_t decoded_count = 0;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto byte = static_cast<unsigned char>(input[i]);

        if (isAsciiWhitespace(byte)) {
            continue;
        }

        const auto ch = static_cast<char>(byte);
        int cell_value = -1;
        if (isEmptyMarker(ch)) {
            cell_value = 0;
        } else if (ch >= '1' && ch <= '9') {
            cell_value = ch - '0';
        } else {
            return std::unexpected(
                ParseErrorDetail{.code = ParseError::InvalidCharacter, .position = i, .bad_char = ch});
        }

        // Always count valid cells, even past 81 — gives a meaningful actual_length for WrongLength.
        if (decoded_count < kCellsPerBoard) {
            decoded[decoded_count] = cell_value;
        }
        ++decoded_count;
    }

    if (decoded_count == 0) {
        return std::unexpected(ParseErrorDetail{.code = ParseError::Empty});
    }

    if (decoded_count != kCellsPerBoard) {
        return std::unexpected(
            ParseErrorDetail{.code = ParseError::WrongLength, .actual_length = static_cast<int>(decoded_count)});
    }

    BoardData board;
    for (std::size_t r = 0; r < kRowSize; ++r) {
        for (std::size_t c = 0; c < kRowSize; ++c) {
            board[r][c] = decoded[(r * kRowSize) + c];
        }
    }
    return board;
}

std::string PuzzleAnalyzer::serializeToString(const BoardData& board) const {
    std::string out;
    out.reserve(kCellsPerBoard);
    for (std::size_t r = 0; r < kRowSize; ++r) {
        for (std::size_t c = 0; c < kRowSize; ++c) {
            const int v = board[r][c];
            out.push_back(v == 0 ? '.' : static_cast<char>('0' + v));
        }
    }
    return out;
}

std::expected<void, BoardValidationError> PuzzleAnalyzer::validate(const BoardData& board) const {
    auto conflicts = validator_->findConflicts(board);
    if (!conflicts.empty()) {
        return std::unexpected(BoardValidationError{.conflicting_cells = std::move(conflicts)});
    }
    return {};
}

UniquenessResult PuzzleAnalyzer::checkUniqueness(const BoardData& board) const {
    // Pre-mortem #9: clear cache before each call so memory does not balloon across imports.
    counter_->clearCache();

    // 1 s budget: this runs synchronously on the UI thread (import dialog / edit-mode commit).
    // Legitimate puzzles finish well under 100 ms via SolutionCounter's SIMD path; the budget
    // exists to cap adversarial inputs. Anything that needs >1 s is treated as non-unique.
    constexpr std::chrono::milliseconds kBudget{1000};
    constexpr int kMaxSolutions = 2;
    const int count = counter_->countSolutionsWithTimeout(board, kMaxSolutions, kBudget);

    // Note: countSolutionsWithTimeout returns max_solutions on timeout — at this layer we
    // cannot distinguish that from a genuine multi-solution result. UniquenessResult::Unknown
    // remains in the contract for a future timeout-aware overload.
    if (count == 1) {
        return UniquenessResult::Unique;
    }
    return UniquenessResult::MultipleSolutions;
}

std::expected<DifficultyScore, ScoringError> PuzzleAnalyzer::scoreDifficulty(const BoardData& board,
                                                                             std::chrono::milliseconds budget) const {
    if (auto valid = validate(board); !valid) {
        return std::unexpected(ScoringError::InvalidInput);
    }

    auto result = solver_->solvePuzzle(board, budget);
    if (!result) {
        switch (result.error()) {
            case SolverError::Timeout:
                return std::unexpected(ScoringError::Timeout);
            case SolverError::Unsolvable:
            case SolverError::InvalidBoard:
                return std::unexpected(ScoringError::NoSolution);
        }
        return std::unexpected(ScoringError::NoSolution);
    }

    DifficultyScore score{};
    score.requires_backtracking = result->used_backtracking;
    score.technique_ids.reserve(result->solve_path.size());

    // Track distinct techniques in order of first occurrence — small N (typically <30),
    // so a linear scan is cheaper than std::set.
    for (const auto& step : result->solve_path) {
        score.max_rating = std::max(score.max_rating, step.rating);
        const int id = static_cast<int>(step.technique);
        if (std::ranges::find(score.technique_ids, id) == score.technique_ids.end()) {
            score.technique_ids.push_back(id);
        }
    }

    if (result->used_backtracking) {
        // Pin max_rating to Backtracking's rating when the solver falls back, so the View
        // can present "very hard / required brute force" honestly.
        score.max_rating = std::max(score.max_rating, getTechniqueRating(SolvingTechnique::Backtracking));
    }

    return score;
}

}  // namespace sudoku::core
