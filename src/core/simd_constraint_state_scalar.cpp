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

// Scalar fallback implementations of SIMDConstraintState non-inline methods.
// Compiled only on non-x86 platforms (ARM64 / Apple Silicon) where the
// AVX2/AVX-512 intrinsics in simd_constraint_state.cpp are unavailable.
// The SolverPath::AVX2 check in the callers (constraint_state.cpp, etc.) is
// gated by getCpuFeatures().has_avx2, which always returns false on ARM64,
// so these paths are never actually reached at runtime — they exist solely
// to satisfy the linker.

#include "simd_constraint_state.h"

#include "core/board.h"

#include <bit>

namespace sudoku::core {

void SIMDConstraintState::initFromBoard(const BoardData& board) {
    for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
        candidates[i] = ALL_CANDIDATES_MASK;
    }
    for (size_t i = BOARD_SIZE * BOARD_SIZE; i < SIMD_PADDED_CELLS; ++i) {
        candidates[i] = 0;
    }
    for (size_t i = 0; i < BOARD_SIZE; ++i) {
        row_used_[i] = 0;
        col_used_[i] = 0;
        box_used_[i] = 0;
    }
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            int digit = board[row][col];
            if (digit != 0) {
                place(row, col, digit);
            }
        }
    }
}

void SIMDConstraintState::initFromBoard(const Board& board) {
    for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
        candidates[i] = ALL_CANDIDATES_MASK;
    }
    for (size_t i = BOARD_SIZE * BOARD_SIZE; i < SIMD_PADDED_CELLS; ++i) {
        candidates[i] = 0;
    }
    for (size_t i = 0; i < BOARD_SIZE; ++i) {
        row_used_[i] = 0;
        col_used_[i] = 0;
        box_used_[i] = 0;
    }
    for (size_t i = 0; i < TOTAL_CELLS; ++i) {
        int digit = static_cast<unsigned char>(board.cells[i]);
        if (digit != 0) {
            place(i, digit);
        }
    }
}

int SIMDConstraintState::countCandidates(size_t cell) const {
    return std::popcount(candidates[cell]);
}

int SIMDConstraintState::findMRVCell() const {
    int best_cell = -1;
    int best_count = 10;
    for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
        int count = countCandidates(cell);
        if (count > 1 && count < best_count) {
            best_count = count;
            best_cell = static_cast<int>(cell);
            if (best_count == 2) {
                return best_cell;
            }
        }
    }
    return best_cell;
}

void SIMDConstraintState::place(size_t cell, int digit) {
    auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
    candidates[cell] = digit_mask;

    size_t row = cellRow(cell);
    size_t col = cellCol(cell);
    size_t box = getBoxIndex(row, col);
    row_used_[row] |= digit_mask;
    col_used_[col] |= digit_mask;
    box_used_[box] |= digit_mask;

    for (uint8_t peer : PEER_TABLE[cell].peers) {
        candidates[peer] &= ~digit_mask;
    }
}

int SIMDConstraintState::getSolvedDigit(size_t cell) const {
    uint16_t cands = candidates[cell];
    if (cands == 0 || (cands & (cands - 1)) != 0) {
        return 0;
    }
    return std::countr_zero(static_cast<unsigned>(cands)) + 1;
}

int SIMDConstraintState::findNakedSingle(const BoardData& board) const {
    for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
        size_t row = cellRow(cell);
        size_t col = cellCol(cell);
        if (board[row][col] == 0) {
            uint16_t cands = candidates[cell];
            if (cands != 0 && (cands & (cands - 1)) == 0) {
                return static_cast<int>(cell);
            }
        }
    }
    return -1;
}

int SIMDConstraintState::findNakedSingle(const Board& board) const {
    for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
        if (board.cells[cell] == 0) {
            uint16_t cands = candidates[cell];
            if (cands != 0 && (cands & (cands - 1)) == 0) {
                return static_cast<int>(cell);
            }
        }
    }
    return -1;
}

template <typename BoardT>
std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl(const BoardT& board) const {
    // --- Rows ---
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }
        uint16_t exactly_once = at_least_once & ~at_least_twice & ~row_used_[row];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Columns ---
    for (size_t col = 0; col < BOARD_SIZE; ++col) {
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }
        uint16_t exactly_once = at_least_once & ~at_least_twice & ~col_used_[col];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Boxes ---
    for (size_t box = 0; box < BOARD_SIZE; ++box) {
        size_t box_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_col = (box % BOX_SIZE) * BOX_SIZE;
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t row = box_row + br;
                size_t col = box_col + bc;
                size_t cell = cellIndex(row, col);
                uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
                at_least_twice |= (at_least_once & cand);
                at_least_once |= cand;
            }
        }
        uint16_t exactly_once = at_least_once & ~at_least_twice & ~box_used_[box];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t br = 0; br < BOX_SIZE; ++br) {
                for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                    size_t row = box_row + br;
                    size_t col = box_col + bc;
                    size_t cell = cellIndex(row, col);
                    if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                        return {static_cast<int>(cell), digit};
                    }
                }
            }
        }
    }

    return {-1, 0};
}

template <typename BoardT>
std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl(const BoardT& board,
                                                               uint32_t dirty_regions) const {
    // --- Rows (bits 0-8) ---
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        if (!(dirty_regions & (1U << row))) {
            continue;
        }
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }
        uint16_t exactly_once = at_least_once & ~at_least_twice & ~row_used_[row];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Columns (bits 9-17) ---
    for (size_t col = 0; col < BOARD_SIZE; ++col) {
        if (!(dirty_regions & (1U << (9 + col)))) {
            continue;
        }
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }
        uint16_t exactly_once = at_least_once & ~at_least_twice & ~col_used_[col];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Boxes (bits 18-26) ---
    for (size_t box = 0; box < BOARD_SIZE; ++box) {
        if (!(dirty_regions & (1U << (18 + box)))) {
            continue;
        }
        size_t box_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_col = (box % BOX_SIZE) * BOX_SIZE;
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t row = box_row + br;
                size_t col = box_col + bc;
                size_t cell = cellIndex(row, col);
                uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
                at_least_twice |= (at_least_once & cand);
                at_least_once |= cand;
            }
        }
        uint16_t exactly_once = at_least_once & ~at_least_twice & ~box_used_[box];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t br = 0; br < BOX_SIZE; ++br) {
                for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                    size_t row = box_row + br;
                    size_t col = box_col + bc;
                    size_t cell = cellIndex(row, col);
                    if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                        return {static_cast<int>(cell), digit};
                    }
                }
            }
        }
    }

    return {-1, 0};
}

std::pair<int, int> SIMDConstraintState::findHiddenSingle(const BoardData& board) const {
    return findHiddenSingleImpl(board);
}

std::pair<int, int> SIMDConstraintState::findHiddenSingle(const Board& board) const {
    return findHiddenSingleImpl(board);
}

std::pair<int, int> SIMDConstraintState::findHiddenSingle(const Board& board,
                                                           uint32_t dirty_regions) const {
    return findHiddenSingleImpl(board, dirty_regions);
}

template std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl<BoardData>(const BoardData&) const;
template std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl<Board>(const Board&) const;
template std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl<Board>(const Board&,
                                                                               uint32_t) const;

}  // namespace sudoku::core
