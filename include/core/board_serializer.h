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

#pragma once

#include "core/board_data.h"

#include <vector>

#include <yaml-cpp/yaml.h>

namespace sudoku::core {

/**
 * @brief YAML serialization utilities for Sudoku board data structures.
 *
 * This class provides helper methods to serialize and deserialize 2D/3D arrays
 * used in the game (boards, notes, flags) to/from YAML format.
 *
 * Eliminates 60+ lines of duplicated code in SaveManager.
 */
class BoardSerializer {
public:
    /**
     * @brief Serialize a 2D integer board to YAML.
     *
     * @param board 9x9 integer board
     * @return YAML node representing the board
     */
    [[nodiscard]] static YAML::Node serializeIntBoard(const BoardData& board);

    /**
     * @brief Deserialize a 2D integer board from YAML.
     *
     * @param node YAML node containing board data
     * @param board Output 9x9 integer board (will be resized)
     */
    static void deserializeIntBoard(const YAML::Node& node, BoardData& board);

    /**
     * @brief Serialize a 2D boolean board to YAML.
     *
     * @param board 9x9 boolean board
     * @return YAML node representing the board
     */
    [[nodiscard]] static YAML::Node serializeBoolBoard(const HintMaskData& board);

    /**
     * @brief Deserialize a 2D boolean board from YAML.
     *
     * @param node YAML node containing board data
     * @param board Output 9x9 boolean board (will be resized)
     */
    static void deserializeBoolBoard(const YAML::Node& node, HintMaskData& board);

    /**
     * @brief Serialize a 3D notes array to YAML.
     *
     * @param notes 9x9xN notes array (each cell has variable-length notes)
     * @return YAML node representing the notes
     */
    [[nodiscard]] static YAML::Node serializeNotes(const NotesData& notes);

    /**
     * @brief Deserialize a 3D notes array from YAML.
     *
     * @param node YAML node containing notes data
     * @param notes Output 9x9xN notes array (will be resized)
     */
    static void deserializeNotes(const YAML::Node& node, NotesData& notes);
};

}  // namespace sudoku::core
