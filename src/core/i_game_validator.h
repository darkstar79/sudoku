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

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace sudoku::core {

/// Represents a cell position on the Sudoku board
struct Position {
    size_t row{0};  // 0 to BOARD_SIZE-1
    size_t col{0};  // 0 to BOARD_SIZE-1

    bool operator==(const Position& other) const = default;
};

/// Types of moves that can be made
enum class MoveType : std::uint8_t {
    PlaceNumber,
    RemoveNumber,
    AddNote,
    RemoveNote,
    PlaceHint  // Hint-revealed numbers (cannot be undone)
};

/// Represents a move on the Sudoku board
struct Move {
    Position position;
    int value{0};  // 1-9, or 0 for clearing
    MoveType move_type{MoveType::PlaceNumber};
    bool is_note{false};  // true if this is a pencil mark

    // For undo: store previous state
    int previous_value{0};               // Previous cell value before this move
    CellNotes previous_notes;            // Previous notes before this move
    bool previous_hint_revealed{false};  // Previous hint-revealed state before this move

    // Story 6.1 (#76): peers whose pencil mark for this move's relevant digit was toggled by the
    // move's note-cleanup, captured so revert/redo replay it exactly (no live-board re-derivation).
    // PlaceNumber/PlaceHint: peers `value` was removed from (revert re-adds it). RemoveNumber/clear:
    // peers `previous_value` was re-added to (revert re-removes it). Runtime-only — NOT serialized
    // (saves are full board+notes snapshots, so the delta is reconstructed only for live undo/redo).
    // The explicit {} is required so GCC's -Wmissing-field-initializers does not fire at Move's
    // many designated-init sites that omit this trailing field; clang-tidy flags it as redundant
    // (std::vector already default-constructs empty), so the check is suppressed here.
    // NOLINTNEXTLINE(readability-redundant-member-init)
    std::vector<Position> peer_note_delta{};
};

/// Error types for validation
enum class ValidationError : std::uint8_t {
    InvalidPosition,
    InvalidValue,
    ConflictInRow,
    ConflictInColumn,
    ConflictInBox,
    CellNotEmpty,
    GameComplete
};

/// Interface for validating Sudoku game moves and states
class IGameValidator {
public:
    virtual ~IGameValidator() = default;

    /// Validates if a move is legal according to Sudoku rules
    /// @param board Current 9x9 board state (0 = empty, 1-9 = filled)
    /// @param move The move to validate
    /// @return true if valid, or ValidationError if invalid
    [[nodiscard]] virtual std::expected<bool, ValidationError> validateMove(const BoardData& board,
                                                                            const Move& move) const = 0;

    /// Checks if the current board state is complete and valid
    /// @param board Current 9x9 board state
    /// @return true if complete and valid
    [[nodiscard]] virtual bool isComplete(const BoardData& board) const = 0;

    /// Finds all conflicts on the current board
    /// @param board Current 9x9 board state
    /// @return vector of conflicting positions
    [[nodiscard]] virtual std::vector<Position> findConflicts(const BoardData& board) const = 0;

    /// Validates that the entire board state is consistent
    /// @param board Current 9x9 board state
    /// @return true if board is valid (no conflicts)
    [[nodiscard]] virtual bool validateBoard(const BoardData& board) const = 0;

    /// Gets all possible valid values for a given position
    /// @param board Current 9x9 board state
    /// @param position Position to check
    /// @return vector of valid values (1-9)
    [[nodiscard]] virtual std::vector<int> getPossibleValues(const BoardData& board,
                                                             const Position& position) const = 0;

    /// Finds a naked single (cell with only one legal value)
    /// @param board Current 9x9 board state
    /// @param position Position to check
    /// @return The single legal value if found, std::nullopt otherwise
    [[nodiscard]] virtual std::optional<int> findNakedSingle(const BoardData& board,
                                                             const Position& position) const = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    IGameValidator() = default;
    IGameValidator(const IGameValidator&) = default;
    IGameValidator& operator=(const IGameValidator&) = default;
    IGameValidator(IGameValidator&&) = default;
    IGameValidator& operator=(IGameValidator&&) = default;
};

}  // namespace sudoku::core