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

#include "../../src/core/puzzle_generator.h"
#include "../helpers/test_utils.h"

#include <algorithm>
#include <set>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// Helper function to create a partially filled board for testing
static BoardData createPartialBoard() {
    // Create a valid partial board with varying constraint levels
    return sudoku::test::getEasyPuzzleWithPatterns();
}

// Helper function to create a highly constrained board
static BoardData createHighlyConstrainedBoard() {
    // Board with many filled cells (few alternatives for remaining cells)
    return {
        {5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8},
        {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
        {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
        {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5},
        {3, 4, 5, 2, 8, 6, 0, 0, 0}  // Last 3 cells have varying constraints
    };
}

TEST_CASE("PuzzleGenerator - Intelligent Clue Dropping - analyzeClueConstraints",
          "[puzzle_generator][intelligent_clues]") {
    PuzzleGenerator generator;

    SECTION("Analyzes partial board and computes constraint scores") {
        auto board = createPartialBoard();
        auto analysis = generator.analyzeClueConstraints(board);

        // Should analyze all non-zero clues
        int clue_count = generator.countClues(board);
        REQUIRE(analysis.size() == static_cast<size_t>(clue_count));

        // All analyzed clues should have valid positions and values
        for (const auto& clue : analysis) {
            REQUIRE(clue.pos.row < 9);
            REQUIRE(clue.pos.col < 9);
            REQUIRE(clue.value >= 1);
            REQUIRE(clue.value <= 9);
            REQUIRE(board[clue.pos.row][clue.pos.col] == clue.value);
        }
    }

    SECTION("Constraint score calculation is correct") {
        auto board = createPartialBoard();
        auto analysis = generator.analyzeClueConstraints(board);

        // Constraint score should be: 9 - alternative_count
        for (const auto& clue : analysis) {
            REQUIRE(clue.constraint_score == (9 - clue.alternative_count));

            // Alternative count should be in valid range [0, 9]
            REQUIRE(clue.alternative_count >= 0);
            REQUIRE(clue.alternative_count <= 9);

            // Constraint score should be in valid range [0, 9]
            REQUIRE(clue.constraint_score >= 0);
            REQUIRE(clue.constraint_score <= 9);
        }
    }

    SECTION("Highly constrained cells have higher scores") {
        auto board = createHighlyConstrainedBoard();
        auto analysis = generator.analyzeClueConstraints(board);

        // Find clues near the end (more constrained due to filled neighbors)
        auto high_constraint_clue = std::find_if(
            analysis.begin(), analysis.end(), [](const ClueAnalysis& c) { return c.pos.row >= 7 && c.pos.col >= 6; });

        // Find clues at the beginning (less constrained, more open)
        auto low_constraint_clue = std::find_if(analysis.begin(), analysis.end(),
                                                [](const ClueAnalysis& c) { return c.pos.row <= 2 && c.pos.col <= 2; });

        if (high_constraint_clue != analysis.end() && low_constraint_clue != analysis.end()) {
            // Cells with more filled neighbors should have fewer alternatives
            REQUIRE(high_constraint_clue->alternative_count <= low_constraint_clue->alternative_count);
        }
    }

    SECTION("Empty board returns empty analysis") {
        BoardData empty_board;
        auto analysis = generator.analyzeClueConstraints(empty_board);

        REQUIRE(analysis.empty());
    }

    SECTION("Full board analyzes all 81 clues") {
        auto board = createHighlyConstrainedBoard();
        // Fill remaining cells
        board[8][6] = 1;
        board[8][7] = 7;
        board[8][8] = 9;

        auto analysis = generator.analyzeClueConstraints(board);
        REQUIRE(analysis.size() == 81);
    }
}

TEST_CASE("PuzzleGenerator - Intelligent Clue Dropping - selectCluesForDropping",
          "[puzzle_generator][intelligent_clues]") {
    PuzzleGenerator generator;
    std::mt19937 rng(12345);  // Fixed seed for reproducibility

    SECTION("Selects requested number of clues") {
        auto board = createPartialBoard();
        int num_to_drop = 5;

        auto selected = generator.selectCluesForDropping(board, num_to_drop, rng);

        REQUIRE(selected.size() == static_cast<size_t>(num_to_drop));
    }

    SECTION("Selected clues are all valid positions with non-zero values") {
        auto board = createPartialBoard();
        auto selected = generator.selectCluesForDropping(board, 5, rng);

        for (const auto& pos : selected) {
            REQUIRE(pos.row < 9);
            REQUIRE(pos.col < 9);
            REQUIRE(board[pos.row][pos.col] != 0);  // Must be a clue
        }
    }

    SECTION("No duplicate positions selected") {
        auto board = createPartialBoard();
        auto selected = generator.selectCluesForDropping(board, 10, rng);

        // Convert to set and check size
        std::set<std::pair<size_t, size_t>> unique_positions;
        for (const auto& pos : selected) {
            unique_positions.insert({pos.row, pos.col});
        }

        REQUIRE(unique_positions.size() == selected.size());
    }

    SECTION("Prioritizes highly constrained clues (fewer alternatives)") {
        auto board = createHighlyConstrainedBoard();
        auto selected = generator.selectCluesForDropping(board, 5, rng);

        // Analyze all clues to get constraint scores
        auto analysis = generator.analyzeClueConstraints(board);

        // Compute average constraint score of selected clues
        double avg_selected_score = 0.0;
        for (const auto& pos : selected) {
            auto it = std::find_if(analysis.begin(), analysis.end(), [&pos](const ClueAnalysis& c) {
                return c.pos.row == pos.row && c.pos.col == pos.col;
            });
            if (it != analysis.end()) {
                avg_selected_score += it->constraint_score;
            }
        }
        avg_selected_score /= selected.size();

        // Compute average constraint score of all clues
        double avg_all_score = 0.0;
        for (const auto& clue : analysis) {
            avg_all_score += clue.constraint_score;
        }
        avg_all_score /= analysis.size();

        // Selected clues should have higher average constraint score
        // (prioritize dropping highly constrained clues)
        REQUIRE(avg_selected_score >= avg_all_score);
    }

    SECTION("Returns empty vector if num_clues is 0") {
        auto board = createPartialBoard();
        auto selected = generator.selectCluesForDropping(board, 0, rng);

        REQUIRE(selected.empty());
    }

    SECTION("Caps selection at total number of clues") {
        auto board = createPartialBoard();
        int total_clues = generator.countClues(board);

        // Request more clues than available
        auto selected = generator.selectCluesForDropping(board, total_clues + 10, rng);

        REQUIRE(selected.size() <= static_cast<size_t>(total_clues));
    }

    SECTION("Deterministic with same RNG seed") {
        auto board = createPartialBoard();

        std::mt19937 rng1(54321);
        auto selected1 = generator.selectCluesForDropping(board, 7, rng1);

        std::mt19937 rng2(54321);  // Same seed
        auto selected2 = generator.selectCluesForDropping(board, 7, rng2);

        // Should produce identical results
        REQUIRE(selected1.size() == selected2.size());
        for (size_t i = 0; i < selected1.size(); ++i) {
            REQUIRE(selected1[i].row == selected2[i].row);
            REQUIRE(selected1[i].col == selected2[i].col);
        }
    }

    SECTION("Different RNG seeds produce different selections") {
        auto board = createPartialBoard();

        std::mt19937 rng1(11111);
        auto selected1 = generator.selectCluesForDropping(board, 7, rng1);

        std::mt19937 rng2(99999);  // Different seed
        auto selected2 = generator.selectCluesForDropping(board, 7, rng2);

        // Should likely produce different results (not guaranteed but very likely)
        bool different = false;
        if (selected1.size() == selected2.size()) {
            for (size_t i = 0; i < selected1.size(); ++i) {
                if (selected1[i].row != selected2[i].row || selected1[i].col != selected2[i].col) {
                    different = true;
                    break;
                }
            }
        } else {
            different = true;
        }

        REQUIRE(different);
    }
}

TEST_CASE("PuzzleGenerator - Intelligent Clue Dropping - Edge Cases", "[puzzle_generator][intelligent_clues]") {
    PuzzleGenerator generator;
    std::mt19937 rng(77777);

    SECTION("analyzeClueConstraints handles single clue") {
        BoardData board;
        board[0][0] = 5;

        auto analysis = generator.analyzeClueConstraints(board);
        REQUIRE(analysis.size() == 1);
        REQUIRE(analysis[0].pos.row == 0);
        REQUIRE(analysis[0].pos.col == 0);
        REQUIRE(analysis[0].value == 5);

        // With mostly empty board, many alternatives
        REQUIRE(analysis[0].alternative_count >= 0);
        REQUIRE(analysis[0].constraint_score >= 0);
    }

    SECTION("selectCluesForDropping handles board with one clue") {
        BoardData board;
        board[4][4] = 9;

        auto selected = generator.selectCluesForDropping(board, 1, rng);
        REQUIRE(selected.size() == 1);
        REQUIRE(selected[0].row == 4);
        REQUIRE(selected[0].col == 4);
    }

    SECTION("analyzeClueConstraints handles board with conflicts") {
        // Create a board with duplicate values (invalid Sudoku)
        BoardData invalid_board;
        invalid_board[0][0] = 5;
        invalid_board[0][1] = 5;  // Conflict in same row

        // Should still analyze without crashing
        auto analysis = generator.analyzeClueConstraints(invalid_board);
        REQUIRE(analysis.size() == 2);
    }
}
