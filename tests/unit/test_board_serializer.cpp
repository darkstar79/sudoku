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

#include "core/board_serializer.h"
#include "core/constants.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

using namespace sudoku::core;

// Helper: build a full 9x9 int board with ascending values
static BoardData makeIntBoard() {
    BoardData board;
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            board[r][c] = static_cast<int>((r * BOARD_SIZE + c) % 9 + 1);
        }
    }
    return board;
}

// Helper: build a full 9x9 bool board (alternating true/false)
static HintMaskData makeBoolBoard() {
    HintMaskData board;
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            board.set(r, c, ((r + c) % 2 == 0));
        }
    }
    return board;
}

// Helper: build a notes grid — only cell (0,0) and (4,4) have notes
static NotesData makeNotes() {
    NotesData notes;
    notes[0][0] = CellNotes{1, 2, 3};
    notes[4][4] = CellNotes{5, 9};
    return notes;
}

TEST_CASE("BoardSerializer - serializeIntBoard / deserializeIntBoard", "[board_serializer]") {
    SECTION("Full 9x9 roundtrip preserves all values") {
        auto original = makeIntBoard();
        YAML::Node node = BoardSerializer::serializeIntBoard(original);

        BoardData result;
        BoardSerializer::deserializeIntBoard(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                REQUIRE(result[r][c] == original[r][c]);
            }
        }
    }

    SECTION("Incomplete YAML (fewer rows) — missing rows default to 0") {
        // Build a YAML node with only 3 rows
        YAML::Node node;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 9; ++c) {
                node[r][c] = r * 9 + c + 1;
            }
        }

        BoardData result;
        BoardSerializer::deserializeIntBoard(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        // First 3 rows: values present
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 9; ++c) {
                REQUIRE(result[r][c] == r * 9 + c + 1);
            }
        }
        // Rows 3-8: should be 0 (default)
        for (size_t r = 3; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                REQUIRE(result[r][c] == 0);
            }
        }
    }

    SECTION("Incomplete YAML (partial row columns) — missing cols default to 0") {
        // Build a YAML node with full 9 rows but only 3 columns in each row
        YAML::Node node;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 3; ++c) {
                node[r][c] = c + 1;
            }
        }

        BoardData result;
        BoardSerializer::deserializeIntBoard(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            // First 3 columns: values present
            for (int c = 0; c < 3; ++c) {
                REQUIRE(result[r][c] == c + 1);
            }
            // Columns 3-8: should be 0 (default)
            for (size_t c = 3; c < BOARD_SIZE; ++c) {
                REQUIRE(result[r][c] == 0);
            }
        }
    }
}

TEST_CASE("BoardSerializer - serializeBoolBoard / deserializeBoolBoard", "[board_serializer]") {
    SECTION("Full 9x9 roundtrip preserves all values") {
        auto original = makeBoolBoard();
        YAML::Node node = BoardSerializer::serializeBoolBoard(original);

        HintMaskData result;
        BoardSerializer::deserializeBoolBoard(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                REQUIRE(result.get(r, c) == original.get(r, c));
            }
        }
    }

    SECTION("Incomplete YAML (fewer rows) — missing rows default to false") {
        YAML::Node node;
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 9; ++c) {
                node[r][c] = true;
            }
        }

        HintMaskData result;
        BoardSerializer::deserializeBoolBoard(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 9; ++c) {
                REQUIRE(result.get(r, c) == true);
            }
        }
        for (size_t r = 2; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                REQUIRE(result.get(r, c) == false);
            }
        }
    }

    SECTION("Incomplete YAML (partial row columns) — missing cols default to false") {
        YAML::Node node;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 4; ++c) {
                node[r][c] = true;
            }
        }

        HintMaskData result;
        BoardSerializer::deserializeBoolBoard(node, result);

        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (int c = 0; c < 4; ++c) {
                REQUIRE(result.get(r, c) == true);
            }
            for (size_t c = 4; c < BOARD_SIZE; ++c) {
                REQUIRE(result.get(r, c) == false);
            }
        }
    }
}

TEST_CASE("BoardSerializer - serializeNotes / deserializeNotes", "[board_serializer]") {
    SECTION("Full roundtrip preserves notes (sparse grid)") {
        auto original = makeNotes();
        YAML::Node node = BoardSerializer::serializeNotes(original);

        NotesData result;
        BoardSerializer::deserializeNotes(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        REQUIRE(result[0][0] == CellNotes{1, 2, 3});
        REQUIRE(result[4][4] == CellNotes{5, 9});
        // Empty cells
        REQUIRE(result[1][1].empty());
        REQUIRE(result[8][8].empty());
    }

    SECTION("YAML node missing entire row — cells default to empty") {
        // Only provide row 0
        YAML::Node node;
        node[0][0].push_back(7);

        NotesData result;
        BoardSerializer::deserializeNotes(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        REQUIRE(result[0][0] == CellNotes{7});
        // Rows 1-8 are missing entirely from YAML — should be empty
        for (size_t r = 1; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                REQUIRE(result[r][c].empty());
            }
        }
    }

    SECTION("YAML node missing specific cell — that cell defaults to empty") {
        // Provide row 0 with col 0 only (col 1 missing)
        YAML::Node node;
        node[0][0].push_back(3);
        // node[0][1] not set

        NotesData result;
        BoardSerializer::deserializeNotes(node, result);

        REQUIRE(result[0][0] == CellNotes{3});
        REQUIRE(result[0][1].empty());
    }

    SECTION("All-empty notes — all cells have empty vectors") {
        NotesData empty_notes;
        YAML::Node node = BoardSerializer::serializeNotes(empty_notes);

        NotesData result;
        BoardSerializer::deserializeNotes(node, result);

        REQUIRE(result.size() == BOARD_SIZE);
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                REQUIRE(result[r][c].empty());
            }
        }
    }
}
