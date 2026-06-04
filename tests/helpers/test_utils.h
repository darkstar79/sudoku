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

#include "core/i_game_validator.h"
#include "core/i_save_manager.h"
#include "core/i_statistics_manager.h"
#include "model/game_state.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sudoku::test {

// ============================================================================
// RAII Cleanup Utilities
// ============================================================================

/// RAII helper for temporary test directories with automatic cleanup
class TempTestDir {
public:
    TempTestDir();
    ~TempTestDir();

    // Delete copy operations (RAII resource)
    TempTestDir(const TempTestDir&) = delete;
    TempTestDir& operator=(const TempTestDir&) = delete;

    // Allow move operations
    TempTestDir(TempTestDir&&) noexcept = default;
    TempTestDir& operator=(TempTestDir&&) noexcept = default;

    /// Get the path to the temporary directory
    [[nodiscard]] const std::filesystem::path& path() const;

private:
    std::filesystem::path path_;
};

// ============================================================================
// Portable Environment Variable Helpers
// ============================================================================

/// Set an environment variable for the current process.
/// POSIX `setenv`/`unsetenv` are unavailable in the MSVC CRT, so this wraps
/// `_putenv_s` on Windows and `setenv` elsewhere. Overwrites any existing value.
void setEnvVar(const char* name, const std::string& value);

/// Remove an environment variable from the current process.
/// Wraps `_putenv_s(name, "")` on Windows (an empty value deletes the entry)
/// and `unsetenv` elsewhere.
void unsetEnvVar(const char* name);

// ============================================================================
// Board Search Helpers (Replace goto patterns)
// ============================================================================

/// Find first empty cell (value == 0) in the board
/// @return Position of empty cell, or nullopt if board is full
[[nodiscard]] std::optional<core::Position> findEmptyCell(const model::GameState& state);

/// Find N empty cells in the board
/// @param n Number of empty cells to find
/// @return Vector of positions, or nullopt if fewer than n empty cells exist
[[nodiscard]] std::optional<std::vector<core::Position>> findEmptyCells(const model::GameState& state, size_t n);

/// Find two empty cells in the same row
/// @return Pair of positions in same row, or nullopt if not found
[[nodiscard]] std::optional<std::pair<core::Position, core::Position>>
findTwoEmptyCellsInRow(const model::GameState& state);

/// Find two empty cells in the same column
/// @return Pair of positions in same column, or nullopt if not found
[[nodiscard]] std::optional<std::pair<core::Position, core::Position>>
findTwoEmptyCellsInColumn(const model::GameState& state);

/// Find two empty cells in the same 3x3 box
/// @return Pair of positions in same box, or nullopt if not found
[[nodiscard]] std::optional<std::pair<core::Position, core::Position>>
findTwoEmptyCellsInBox(const model::GameState& state);

/// Find three empty cells in the same row
/// Guarantees all three share row/col/box relations needed for tests that
/// assert on inter-cell note propagation (which require the cells to be
/// peers, not arbitrary scan-order empties).
/// @return Vector of three positions in same row, or nullopt if not found
[[nodiscard]] std::optional<std::vector<core::Position>> findThreeEmptyCellsInRow(const model::GameState& state);

/// Find first cell matching a predicate
/// @param predicate Callable taking const model::Cell& and returning bool
/// @return Position of first matching cell, or nullopt if none found
template <typename Predicate>
[[nodiscard]] std::optional<core::Position> findCell(const model::GameState& state, Predicate predicate) {
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (predicate(state.getCell(row, col))) {
                return core::Position{.row = row, .col = col};
            }
        }
    }
    return std::nullopt;
}

/// Find all cells matching a predicate
/// @param predicate Callable taking const model::Cell& and returning bool
/// @return Vector of positions for all matching cells
template <typename Predicate>
[[nodiscard]] std::vector<core::Position> findCells(const model::GameState& state, Predicate predicate) {
    std::vector<core::Position> results;
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (predicate(state.getCell(row, col))) {
                results.emplace_back(row, col);
            }
        }
    }
    return results;
}

// ============================================================================
// Deterministic Puzzle Fixtures
// ============================================================================

/// Get a deterministic easy puzzle with known patterns
/// This puzzle is designed for testing and has predictable empty cells
/// @return 9x9 board with zeros for empty cells
[[nodiscard]] core::BoardData getEasyPuzzleWithPatterns();

/// Get a puzzle specifically designed for note restoration testing
/// Has multiple empty cells in same row/column/box for comprehensive testing
/// @return 9x9 board with zeros for empty cells
[[nodiscard]] core::BoardData getNoteRestorationPuzzle();

/// Get a puzzle specifically designed for undo/redo testing
/// Has predictable empty cells for deterministic command testing
/// @return 9x9 board with zeros for empty cells
[[nodiscard]] core::BoardData getUndoTestPuzzle();

/// Get a medium difficulty puzzle for general testing
/// @return 9x9 board with zeros for empty cells
[[nodiscard]] core::BoardData getMediumPuzzle();

/// Get a nearly complete puzzle (1-2 empty cells) for completion testing
/// @return 9x9 board with zeros for empty cells
[[nodiscard]] core::BoardData getNearlyCompletePuzzle();

/// Get the complete solution for any test puzzle
/// @param puzzle The puzzle to solve
/// @return 9x9 board with complete solution
[[nodiscard]] core::BoardData getSolution(const core::BoardData& puzzle);

// ============================================================================
// Board Comparison and Validation
// ============================================================================

/// Compare two boards for equality
/// @return true if boards are identical, false otherwise
[[nodiscard]] bool boardsEqual(const core::BoardData& a, const core::BoardData& b);

/// Check if a board is valid (no conflicts)
/// @return true if board has no row/column/box conflicts
[[nodiscard]] bool isBoardValid(const core::BoardData& board);

/// Count empty cells (zeros) in a board
/// @return Number of empty cells
[[nodiscard]] size_t countEmptyCells(const core::BoardData& board);

// ============================================================================
// Test Data Builders
// ============================================================================

/// Create a SavedGame with all fields properly initialized
/// @param difficulty Difficulty level for the game
/// @return Fully initialized SavedGame ready for testing
[[nodiscard]] core::SavedGame createTestSavedGame(core::Difficulty difficulty = core::Difficulty::Medium);

/// Create a complete SavedGame (no empty cells) for completion testing
/// @return Fully initialized completed SavedGame
[[nodiscard]] core::SavedGame createCompleteSavedGame();

/// Create GameStats with realistic test data
/// @param difficulty Difficulty level for the game stats
/// @param completed Whether the game was completed
/// @return GameStats with sample statistics
[[nodiscard]] core::GameStats createTestGameStats(core::Difficulty difficulty = core::Difficulty::Medium,
                                                  bool completed = true);

// ============================================================================
// Board Manipulation Helpers
// ============================================================================

/// Fill a board with a valid complete solution
/// @param board Board to fill (modified in-place)
void fillBoardWithSolution(core::BoardData& board);

/// Create an empty 9x9 board (all zeros)
/// @return 9x9 board filled with zeros
[[nodiscard]] core::BoardData createEmptyBoard();

/// Create empty notes structure (9x9x9 all zeros)
/// @return 3D vector for notes (row, col, value)
[[nodiscard]] core::NotesData createEmptyNotes();

// ============================================================================
// Strategy Testing Board Builders
// ============================================================================

/// Create a board with a hidden pair pattern
/// Hidden pair: values 4 and 5 only possible in two cells of row 0
/// @return 9x9 board designed to test hidden pair detection
[[nodiscard]] core::BoardData createBoardWithHiddenPair();

/// Create a board with a hidden triple pattern
/// Hidden triple: values 3, 6, and 9 only possible in three cells of a region
/// @return 9x9 board designed to test hidden triple detection
[[nodiscard]] core::BoardData createBoardWithHiddenTriple();

/// Create a board with a naked pair pattern
/// Naked pair: two cells in row/column/box with only same two candidates
/// @return 9x9 board designed to test naked pair detection
[[nodiscard]] core::BoardData createBoardWithNakedPair();

/// Create a board with a naked triple pattern
/// Naked triple: three cells with only same three candidates
/// @return 9x9 board designed to test naked triple detection
[[nodiscard]] core::BoardData createBoardWithNakedTriple();

/// Create a simple board with no advanced patterns (for negative tests)
/// @return 9x9 partially filled board with no hidden/naked sets
[[nodiscard]] core::BoardData createSimpleBoard();

}  // namespace sudoku::test
