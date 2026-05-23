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

#include "core/constants.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <initializer_list>
#include <span>
#include <vector>

#include <bit>

namespace sudoku::core {

/// Fixed-extent row span type returned by BoardData::operator[].
using RowSpan = std::span<int, BOARD_SIZE>;
using ConstRowSpan = std::span<const int, BOARD_SIZE>;

/**
 * @brief Flat-array representation of a 9x9 Sudoku board.
 *
 * Stores 81 cells as a contiguous array of int.
 * Values: 0 = empty, 1-9 = filled digit.
 * Default-constructed boards are all zeros (empty).
 *
 * operator[] returns a std::span<int, 9> so board[row][col] works,
 * and row spans support range-for, .size(), .begin()/.end(), STL algorithms.
 *
 * Board-level iteration is supported: for (auto row : board) { ... }
 */
class BoardData {
public:
    BoardData() = default;

    /// Construct from nested initializer list: BoardData board = {{1,2,...}, {3,4,...}, ...};
    BoardData(std::initializer_list<std::initializer_list<int>> init);

    /// Test-only factory: creates a board filled with a single value (invalid for Sudoku).
    [[nodiscard]] static BoardData filled(int value);

    /// Row accessor — returns a span over 9 cells. Enables board[row][col] and range-for on rows.
    [[nodiscard]] RowSpan operator[](size_t row) {
        return RowSpan(cells_.data() + (row * BOARD_SIZE), BOARD_SIZE);
    }

    /// Row accessor (const)
    [[nodiscard]] ConstRowSpan operator[](size_t row) const {
        return ConstRowSpan(cells_.data() + (row * BOARD_SIZE), BOARD_SIZE);
    }

    /// Number of rows (always 9)
    [[nodiscard]] static constexpr size_t size() {
        return BOARD_SIZE;
    }

    /// Flat data pointer
    [[nodiscard]] int* data() {
        return cells_.data();
    }

    /// Flat data pointer (const)
    [[nodiscard]] const int* data() const {
        return cells_.data();
    }

    /// Defaulted equality comparison
    bool operator==(const BoardData&) const = default;

    // Board-level row iteration: for (auto row : board) { ... }

    /// Iterator over row spans
    class RowIterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = RowSpan;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = RowSpan;

        RowIterator(int* base, size_t row_index) : base_(base), row_index_(row_index) {
        }

        [[nodiscard]] RowSpan operator*() const {
            return RowSpan(base_ + (row_index_ * BOARD_SIZE), BOARD_SIZE);
        }

        RowIterator& operator++() {
            ++row_index_;
            return *this;
        }

        [[nodiscard]] bool operator!=(const RowIterator& other) const {
            return row_index_ != other.row_index_;
        }

        [[nodiscard]] bool operator==(const RowIterator& other) const {
            return row_index_ == other.row_index_;
        }

        [[nodiscard]] difference_type operator-(const RowIterator& other) const {
            return static_cast<difference_type>(row_index_) - static_cast<difference_type>(other.row_index_);
        }

    private:
        int* base_;
        size_t row_index_;
    };

    /// Const iterator over row spans
    class ConstRowIterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = ConstRowSpan;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = ConstRowSpan;

        ConstRowIterator(const int* base, size_t row_index) : base_(base), row_index_(row_index) {
        }

        [[nodiscard]] ConstRowSpan operator*() const {
            return ConstRowSpan(base_ + (row_index_ * BOARD_SIZE), BOARD_SIZE);
        }

        ConstRowIterator& operator++() {
            ++row_index_;
            return *this;
        }

        [[nodiscard]] bool operator!=(const ConstRowIterator& other) const {
            return row_index_ != other.row_index_;
        }

        [[nodiscard]] bool operator==(const ConstRowIterator& other) const {
            return row_index_ == other.row_index_;
        }

        [[nodiscard]] difference_type operator-(const ConstRowIterator& other) const {
            return static_cast<difference_type>(row_index_) - static_cast<difference_type>(other.row_index_);
        }

    private:
        const int* base_;
        size_t row_index_;
    };

    [[nodiscard]] RowIterator begin() {
        return {cells_.data(), 0};
    }
    [[nodiscard]] RowIterator end() {
        return {cells_.data(), BOARD_SIZE};
    }
    [[nodiscard]] ConstRowIterator begin() const {
        return {cells_.data(), 0};
    }
    [[nodiscard]] ConstRowIterator end() const {
        return {cells_.data(), BOARD_SIZE};
    }

private:
    std::array<int, TOTAL_CELLS> cells_{};
};

/**
 * @brief Bitmask-based pencil marks for a single Sudoku cell.
 *
 * Stores candidate values (1-9) as bits in a uint16_t.
 * Bit N set = value N is a pencil mark. Bit 0 is unused.
 * Same representation as CandidateGrid's elimination masks.
 */
class CellNotes {
public:
    CellNotes() = default;
    explicit CellNotes(uint16_t mask) : mask_(mask) {
    }

    /// Construct from initializer list: CellNotes notes = {1, 3, 7};
    CellNotes(std::initializer_list<int> values) {
        for (int v : values) {
            add(v);
        }
    }

    /// Add a pencil mark
    void add(int value) {
        mask_ |= valueToBit(value);
    }

    /// Remove a pencil mark
    void remove(int value) {
        mask_ &= ~valueToBit(value);
    }

    /// Check if a value is marked
    [[nodiscard]] bool contains(int value) const {
        return (mask_ & valueToBit(value)) != 0;
    }

    /// Remove all pencil marks
    void clear() {
        mask_ = 0;
    }

    /// Check if no pencil marks are set
    [[nodiscard]] bool empty() const {
        return mask_ == 0;
    }

    /// Number of pencil marks set
    [[nodiscard]] int count() const {
        return std::popcount(mask_);
    }

    /// Raw bitmask access
    [[nodiscard]] uint16_t mask() const {
        return mask_;
    }

    bool operator==(const CellNotes&) const = default;
    bool operator!=(const CellNotes&) const = default;

    /// Iterator over set values (1-9) in the bitmask
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = int;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = int;

        Iterator() = default;
        explicit Iterator(uint16_t remaining) : remaining_(remaining) {
            advance();
        }

        [[nodiscard]] int operator*() const {
            return current_;
        }

        Iterator& operator++() {
            remaining_ &= ~valueToBit(current_);
            advance();
            return *this;
        }

        [[nodiscard]] bool operator==(const Iterator& other) const {
            return remaining_ == other.remaining_;
        }

        [[nodiscard]] bool operator!=(const Iterator& other) const {
            return remaining_ != other.remaining_;
        }

    private:
        void advance() {
            if (remaining_ == 0) {
                current_ = 0;
                return;
            }
            current_ = std::countr_zero(remaining_);
        }

        uint16_t remaining_{0};
        int current_{0};
    };

    [[nodiscard]] Iterator begin() const {
        return Iterator(mask_);
    }
    [[nodiscard]] Iterator end() const {
        return Iterator(0);
    }

private:
    uint16_t mask_{0};
};

/**
 * @brief Flat-array representation of a 9x9 grid of pencil-mark notes.
 *
 * Stores 81 CellNotes (uint16_t bitmasks) as a contiguous array.
 * Supports notes[row][col] syntax via span-based operator[].
 */
class NotesData {
public:
    using CellSpan = std::span<CellNotes, BOARD_SIZE>;
    using ConstCellSpan = std::span<const CellNotes, BOARD_SIZE>;

    NotesData() = default;

    /// Row accessor — returns span of 9 CellNotes
    [[nodiscard]] CellSpan operator[](size_t row) {
        return CellSpan(cells_.data() + (row * BOARD_SIZE), BOARD_SIZE);
    }

    /// Row accessor (const)
    [[nodiscard]] ConstCellSpan operator[](size_t row) const {
        return ConstCellSpan(cells_.data() + (row * BOARD_SIZE), BOARD_SIZE);
    }

    [[nodiscard]] static constexpr size_t size() {
        return BOARD_SIZE;
    }

    /// Check if all cells have no notes
    [[nodiscard]] bool empty() const {
        return std::ranges::all_of(cells_, [](const CellNotes& cell) { return cell.empty(); });
    }

    bool operator==(const NotesData&) const = default;

private:
    std::array<CellNotes, TOTAL_CELLS> cells_{};
};

/**
 * @brief Flat bitset representation of a 9x9 grid of boolean flags.
 *
 * General-purpose per-cell boolean flag grid backed by std::bitset<81>.
 * Used for hint-revealed cells, given/clue flags, conflict markers, and highlights.
 * Default-constructed with all bits clear.
 */
class CellFlags {
public:
    CellFlags() = default;

    /// Set flag at (row, col)
    void set(size_t row, size_t col, bool value = true) {
        bits_.set((row * BOARD_SIZE) + col, value);
    }

    /// Get flag at (row, col)
    [[nodiscard]] bool get(size_t row, size_t col) const {
        return bits_.test((row * BOARD_SIZE) + col);
    }

    /// Shorthand: flags(row, col)
    [[nodiscard]] bool operator()(size_t row, size_t col) const {
        return get(row, col);
    }

    /// True if no flags are set
    [[nodiscard]] bool empty() const {
        return bits_.none();
    }

    /// Clear all flags
    void reset() {
        bits_.reset();
    }

    /// Number of rows (always 9)
    [[nodiscard]] static constexpr size_t size() {
        return BOARD_SIZE;
    }

    /// Number of flags set
    [[nodiscard]] size_t count() const {
        return bits_.count();
    }

    bool operator==(const CellFlags&) const = default;

private:
    std::bitset<TOTAL_CELLS> bits_;
};

/// Backward-compatible alias for hint-revealed cell flags.
using HintMaskData = CellFlags;

}  // namespace sudoku::core
