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

#include "core/board_serializer.h"

#include "core/board_utils.h"
#include "core/constants.h"

#include <cstddef>

namespace sudoku::core {

YAML::Node BoardSerializer::serializeIntBoard(const BoardData& board) {
    YAML::Node node;
    forEachCell([&](size_t row, size_t col) { node[row][col] = board[row][col]; });
    return node;
}

void BoardSerializer::deserializeIntBoard(const YAML::Node& node, BoardData& board) {
    board = BoardData{};
    forEachCell([&](size_t row, size_t col) {
        if (row < node.size() && col < node[row].size()) {
            board[row][col] = node[row][col].as<int>();
        }
    });
}

YAML::Node BoardSerializer::serializeBoolBoard(const HintMaskData& board) {
    YAML::Node node;
    forEachCell([&](size_t row, size_t col) { node[row][col] = board.get(row, col); });
    return node;
}

void BoardSerializer::deserializeBoolBoard(const YAML::Node& node, HintMaskData& board) {
    board = HintMaskData{};
    forEachCell([&](size_t row, size_t col) {
        if (row < node.size() && col < node[row].size()) {
            board.set(row, col, node[row][col].as<bool>());
        }
    });
}

YAML::Node BoardSerializer::serializeNotes(const NotesData& notes) {
    YAML::Node node;
    forEachCell([&](size_t row, size_t col) {
        const auto& cell_notes = notes[row][col];
        for (int note : cell_notes) {
            node[row][col].push_back(note);
        }
    });
    return node;
}

void BoardSerializer::deserializeNotes(const YAML::Node& node, NotesData& notes) {
    notes = NotesData{};
    forEachCell([&](size_t row, size_t col) {
        if (node[row] && node[row][col]) {
            const auto& cell_notes = node[row][col];
            for (const auto& note : cell_notes) {
                notes[row][col].add(note.as<int>());
            }
        }
    });
}

}  // namespace sudoku::core
