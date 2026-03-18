// sudoku-cpp - Offline Sudoku Game
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

#include "core/board_data.h"

namespace sudoku::core {

BoardData::BoardData(std::initializer_list<std::initializer_list<int>> init) {
    size_t row = 0;
    for (const auto& row_init : init) {
        if (row >= BOARD_SIZE) {
            break;
        }
        size_t col = 0;
        for (int value : row_init) {
            if (col >= BOARD_SIZE) {
                break;
            }
            cells_[(row * BOARD_SIZE) + col] = value;
            ++col;
        }
        ++row;
    }
}

BoardData BoardData::filled(int value) {
    BoardData board;
    board.cells_.fill(value);
    return board;
}

}  // namespace sudoku::core
