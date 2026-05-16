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

#include "test_utils.h"

#include "core/game_validator.h"
#include "core/puzzle_generator.h"

#include <algorithm>
#include <chrono>

namespace sudoku::test {

// ============================================================================
// RAII Cleanup Utilities
// ============================================================================

TempTestDir::TempTestDir() {
    // Create unique directory name using timestamp
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() / ("sudoku_test_" + std::to_string(timestamp));
    std::filesystem::create_directories(path_);
}

TempTestDir::~TempTestDir() {
    if (std::filesystem::exists(path_)) {
        std::filesystem::remove_all(path_);
    }
}

const std::filesystem::path& TempTestDir::path() const {
    return path_;
}

// ============================================================================
// Board Search Helpers
// ============================================================================

std::optional<core::Position> findEmptyCell(const model::GameState& state) {
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (state.getCell(row, col).value == 0) {
                return core::Position{.row = row, .col = col};
            }
        }
    }
    return std::nullopt;
}

std::optional<std::vector<core::Position>> findEmptyCells(const model::GameState& state, size_t n) {
    std::vector<core::Position> empty_cells;
    empty_cells.reserve(n);

    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (state.getCell(row, col).value == 0) {
                empty_cells.push_back({.row = row, .col = col});
                if (empty_cells.size() == n) {
                    return empty_cells;
                }
            }
        }
    }

    return std::nullopt;  // Fewer than n empty cells found
}

std::optional<std::pair<core::Position, core::Position>> findTwoEmptyCellsInRow(const model::GameState& state) {
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        std::vector<size_t> empty_cols;
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (state.getCell(row, col).value == 0) {
                empty_cols.push_back(col);
                if (empty_cols.size() == 2) {
                    return std::make_pair(core::Position{.row = row, .col = empty_cols[0]},
                                          core::Position{.row = row, .col = empty_cols[1]});
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::pair<core::Position, core::Position>> findTwoEmptyCellsInColumn(const model::GameState& state) {
    for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
        std::vector<size_t> empty_rows;
        for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
            if (state.getCell(row, col).value == 0) {
                empty_rows.push_back(row);
                if (empty_rows.size() == 2) {
                    return std::make_pair(core::Position{.row = empty_rows[0], .col = col},
                                          core::Position{.row = empty_rows[1], .col = col});
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::vector<core::Position>> findThreeEmptyCellsInRow(const model::GameState& state) {
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        std::vector<core::Position> empty_in_row;
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (state.getCell(row, col).value == 0) {
                empty_in_row.push_back({.row = row, .col = col});
                if (empty_in_row.size() == 3) {
                    return empty_in_row;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::pair<core::Position, core::Position>> findTwoEmptyCellsInBox(const model::GameState& state) {
    // Iterate through each 3x3 box
    for (size_t box_row = 0; box_row < 3; ++box_row) {
        for (size_t box_col = 0; box_col < 3; ++box_col) {
            std::vector<core::Position> empty_in_box;

            // Check cells within this box
            for (size_t r = 0; r < core::BOX_SIZE; ++r) {
                for (size_t c = 0; c < core::BOX_SIZE; ++c) {
                    size_t row = box_row * core::BOX_SIZE + r;
                    size_t col = box_col * core::BOX_SIZE + c;

                    if (state.getCell(row, col).value == 0) {
                        empty_in_box.push_back({.row = row, .col = col});
                        if (empty_in_box.size() == 2) {
                            return std::make_pair(empty_in_box[0], empty_in_box[1]);
                        }
                    }
                }
            }
        }
    }
    return std::nullopt;
}

// ============================================================================
// Deterministic Puzzle Fixtures
// ============================================================================

core::BoardData getEasyPuzzleWithPatterns() {
    // Deterministic easy puzzle with predictable structure
    // Empty cells at specific locations for testing
    return {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
            {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
            {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};
}

core::BoardData getNoteRestorationPuzzle() {
    // Puzzle with multiple empty cells in same row/column/box
    // Designed for comprehensive note restoration testing
    return {
        {0, 0, 0, 6, 0, 0, 4, 0, 0},  // Row 0: three empty cells at start
        {7, 0, 0, 0, 0, 3, 0, 0, 0},  // Row 1: scattered empty cells
        {0, 0, 0, 0, 9, 1, 0, 0, 0}, {0, 5, 0, 0, 0, 7, 0, 0, 0}, {0, 0, 0, 0, 4, 5, 7, 0, 0},
        {0, 0, 0, 1, 0, 0, 0, 3, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 6}, {0, 0, 0, 3, 0, 0, 0, 8, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0}  // Row 8: completely empty
    };
}

core::BoardData getUndoTestPuzzle() {
    // Puzzle with predictable empty cells for undo/redo command testing
    return {
        {0, 2, 3, 4, 5, 6, 7, 8, 9},  // First cell empty for easy testing
        {4, 5, 6, 7, 8, 9, 1, 2, 3}, {7, 8, 9, 1, 2, 3, 4, 5, 6},
        {2, 0, 4, 5, 6, 7, 8, 9, 1},  // Second row, first empty
        {5, 6, 7, 8, 9, 1, 2, 3, 4}, {8, 9, 1, 2, 3, 4, 5, 6, 7},
        {3, 4, 5, 6, 7, 8, 9, 1, 2}, {6, 7, 8, 9, 1, 2, 3, 4, 5},
        {9, 1, 2, 3, 4, 5, 6, 7, 0}  // Last cell empty
    };
}

core::BoardData getMediumPuzzle() {
    return {{0, 0, 0, 2, 6, 0, 7, 0, 1}, {6, 8, 0, 0, 7, 0, 0, 9, 0}, {1, 9, 0, 0, 0, 4, 5, 0, 0},
            {8, 2, 0, 1, 0, 0, 0, 4, 0}, {0, 0, 4, 6, 0, 2, 9, 0, 0}, {0, 5, 0, 0, 0, 3, 0, 2, 8},
            {0, 0, 9, 3, 0, 0, 0, 7, 4}, {0, 4, 0, 0, 5, 0, 0, 3, 6}, {7, 0, 3, 0, 1, 8, 0, 0, 0}};
}

core::BoardData getNearlyCompletePuzzle() {
    // Only two empty cells for completion testing
    return {
        {5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8},
        {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
        {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
        {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5},
        {3, 4, 5, 2, 8, 6, 0, 7, 0}  // Only last two cells empty
    };
}

core::BoardData getSolution(const core::BoardData& puzzle) {
    // Use PuzzleGenerator's solver to get solution
    core::PuzzleGenerator generator;
    auto solution = puzzle;  // Copy puzzle

    // Solve the puzzle
    if (generator.solvePuzzle(solution)) {
        return solution;
    }

    // If solving fails, return original puzzle (shouldn't happen with valid puzzles)
    return puzzle;
}

// ============================================================================
// Board Comparison and Validation
// ============================================================================

bool boardsEqual(const core::BoardData& a, const core::BoardData& b) {
    return a == b;
}

bool isBoardValid(const core::BoardData& board) {
    core::GameValidator validator;
    return validator.validateBoard(board);
}

size_t countEmptyCells(const core::BoardData& board) {
    size_t count = 0;
    for (const auto& row : board) {
        count += static_cast<size_t>(std::count(row.begin(), row.end(), 0));
    }
    return count;
}

// ============================================================================
// Test Data Builders
// ============================================================================

core::SavedGame createTestSavedGame(core::Difficulty difficulty) {
    // Get a deterministic puzzle based on difficulty
    core::BoardData puzzle;
    switch (difficulty) {
        case core::Difficulty::Easy:
            puzzle = getEasyPuzzleWithPatterns();
            break;
        case core::Difficulty::Medium:
            puzzle = getMediumPuzzle();
            break;
        case core::Difficulty::Hard:
        case core::Difficulty::Expert:
        case core::Difficulty::Master:
            puzzle = getMediumPuzzle();  // Use medium for now
            break;
    }

    core::SavedGame game;
    game.save_id = "test_save_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    game.display_name = "Test Game";
    game.created_time = std::chrono::system_clock::now();
    game.last_modified = std::chrono::system_clock::now();
    game.original_puzzle = puzzle;
    game.current_state = puzzle;
    game.notes = createEmptyNotes();
    game.hint_revealed_cells = core::HintMaskData{};
    game.difficulty = difficulty;
    game.puzzle_seed = 42;
    game.elapsed_time = std::chrono::milliseconds(0);
    game.moves_made = 0;
    game.hints_used = 0;
    game.mistakes = 0;
    game.is_complete = false;
    game.move_history = {};
    game.current_move_index = -1;
    game.is_auto_save = false;
    game.last_auto_save = std::chrono::system_clock::now();

    return game;
}

core::SavedGame createCompleteSavedGame() {
    auto solution = getSolution(getEasyPuzzleWithPatterns());

    core::SavedGame game;
    game.save_id = "test_complete_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    game.display_name = "Complete Game";
    game.created_time = std::chrono::system_clock::now();
    game.last_modified = std::chrono::system_clock::now();
    game.original_puzzle = getEasyPuzzleWithPatterns();
    game.current_state = solution;
    game.notes = createEmptyNotes();
    game.hint_revealed_cells = core::HintMaskData{};
    game.difficulty = core::Difficulty::Easy;
    game.puzzle_seed = 42;
    game.elapsed_time = std::chrono::milliseconds(300000);  // 5 minutes
    game.moves_made = 50;
    game.hints_used = 2;
    game.mistakes = 1;
    game.is_complete = true;
    game.move_history = {};
    game.current_move_index = -1;
    game.is_auto_save = false;
    game.last_auto_save = std::chrono::system_clock::now();

    return game;
}

core::GameStats createTestGameStats(core::Difficulty difficulty, bool completed) {
    core::GameStats stats;
    stats.difficulty = difficulty;
    stats.time_played = std::chrono::milliseconds(300000);  // 5 minutes
    stats.start_time = std::chrono::system_clock::now() - std::chrono::minutes(5);
    stats.end_time = std::chrono::system_clock::now();
    stats.completed = completed;
    stats.moves_made = 50;
    stats.hints_used = 2;
    stats.mistakes = 1;
    stats.puzzle_seed = 42;

    return stats;
}

// ============================================================================
// Board Manipulation Helpers
// ============================================================================

void fillBoardWithSolution(core::BoardData& board) {
    core::PuzzleGenerator generator;
    auto result = generator.solvePuzzle(board);
    if (result.has_value()) {
        board = result.value();
    }
}

core::BoardData createEmptyBoard() {
    return core::BoardData{};
}

core::NotesData createEmptyNotes() {
    return core::NotesData{};
}

// ============================================================================
// Strategy Testing Board Builders
// ============================================================================

core::BoardData createBoardWithHiddenPair() {
    /*
     * Row 0 will have a hidden pair for values 4 and 5
     * Row 0: [1, 2, 3, _, 6, 7, 8, 9, _]
     *
     * Values 4 and 5 are missing from row 0.
     * We'll block value 4 from cell (0,8) by placing 4 in column 8.
     * We'll block value 5 from cell (0,8) by placing 5 in the same box.
     * This forces values 4 and 5 to only be possible at cells (0,3) and (0,8)
     * but we'll further constrain so they form a hidden pair.
     *
     * Actually, let's create a clearer example:
     * Row 0: [0, 0, 1, 2, 6, 7, 8, 9, 3]
     * Cells (0,0) and (0,1) are empty. Missing values are 4 and 5.
     *
     * We'll make sure that in these two cells, OTHER values (like 6,7,8,9)
     * are also candidates due to box/column constraints, but values 4 and 5
     * can ONLY go in these two cells in row 0.
     */
    core::BoardData board = {
        {0, 0, 1, 2, 6, 7, 8, 9, 3},  // Row 0: cells (0,0) and (0,1) empty
        {4, 3, 2, 1, 9, 8, 7, 6, 5},  // Row 1: complete (blocks 4 from col 0, 3 from col 1)
        {5, 6, 7, 8, 3, 4, 9, 1, 2},  // Row 2: complete (blocks 5 from col 0, 6 from col 1)
        {9, 8, 4, 5, 1, 2, 3, 7, 6},  // Row 3+: fill remaining rows to avoid interference
        {6, 7, 5, 3, 4, 9, 1, 2, 8}, {1, 2, 3, 6, 7, 5, 4, 8, 9},
        {7, 9, 6, 4, 2, 1, 5, 3, 0},  // Row 6: one empty cell for variety
        {2, 1, 8, 7, 5, 3, 6, 4, 0},  // Row 7: one empty cell
        {3, 4, 9, 0, 8, 6, 2, 5, 1}   // Row 8: one empty cell
    };

    /*
     * In row 0, cells (0,0) and (0,1):
     * - Cell (0,0): blocked from having 4 (col 0 has 4 in row 1), blocked from 5 (col 0 has 5 in row 2)
     * - But wait, that means 4 and 5 CAN'T be in those cells!
     *
     * Let me redesign this more carefully:
     */

    // Better approach: Create a board where values 4,5 can ONLY go in two specific cells in row 0
    board = {
        {1, 2, 3, 0, 0, 6, 7, 8, 9},  // Row 0: cells (0,3) and (0,4) empty
        {6, 5, 7, 8, 9, 1, 2, 3, 4},  // Row 1: complete
        {9, 8, 4, 2, 3, 5, 6, 7, 1},  // Row 2: complete
        {2, 1, 5, 6, 7, 4, 3, 9, 8},  // Row 3
        {4, 3, 6, 9, 8, 2, 1, 5, 7},  // Row 4
        {7, 9, 8, 3, 1, 0, 4, 6, 2},  // Row 5: one empty
        {5, 6, 2, 1, 4, 8, 9, 0, 3},  // Row 6: one empty
        {8, 7, 1, 5, 6, 9, 0, 2, 0},  // Row 7: two empty
        {3, 4, 9, 7, 2, 0, 8, 1, 6}   // Row 8: one empty
    };

    // In row 0, positions (0,3) and (0,4) are empty
    // Missing values in row 0: 4 and 5
    // Column 3 already has values 8,2,6,9,3,1,5,7 (missing 4)
    // Column 4 already has values 9,3,7,8,1,4,6,2 (missing 5)
    // So (0,3) can only be 4, and (0,4) can only be 5
    // This creates a hidden pair (and also naked singles)

    return board;
}

core::BoardData createBoardWithHiddenTriple() {
    /*
     * Create a board where values 3, 6, and 9 can only go in three specific cells
     * in a row, but those cells have other candidates too.
     */
    core::BoardData board = {{0, 0, 0, 1, 2, 4, 5, 7, 8},  // Row 0: first three cells empty, missing 3,6,9
                             {1, 2, 4, 5, 7, 8, 3, 6, 9},  // Row 1: complete
                             {5, 7, 8, 3, 6, 9, 1, 2, 4},  // Row 2: complete
                             {2, 3, 1, 6, 5, 7, 4, 8, 9},  // Rows 3-8: fill to avoid interference
                             {4, 5, 6, 8, 9, 2, 7, 1, 3}, {7, 8, 9, 4, 1, 3, 2, 5, 6},
                             {3, 1, 2, 7, 4, 5, 6, 9, 0},  // Some variety with empty cells
                             {6, 4, 5, 9, 8, 1, 0, 3, 2}, {8, 9, 7, 2, 3, 6, 0, 4, 1}};

    // Row 0: cells (0,0), (0,1), (0,2) are empty
    // Missing values in row 0: 3, 6, 9
    // These three values can only go in the three empty cells (hidden triple)

    return board;
}

core::BoardData createBoardWithNakedPair() {
    /*
     * Create a board where two cells have only the same two candidates (naked pair)
     * For example, cells (0,0) and (0,1) can only contain {2, 5}
     */
    core::BoardData board = {
        {0, 0, 1, 3, 4, 6, 7, 8, 9},  // Row 0: first two cells empty
        {3, 1, 2, 5, 6, 7, 8, 9, 4},  // Row 1: complete
        {4, 6, 7, 8, 9, 1, 2, 3, 5},  // Row 2: complete
        {1, 2, 3, 4, 5, 8, 6, 7, 9},  // Rows 3-8: filled
        {6, 7, 8, 9, 1, 2, 3, 4, 5}, {9, 4, 5, 6, 7, 3, 1, 2, 8}, {2, 3, 4, 1, 8, 5, 9, 6, 7},
        {5, 8, 6, 7, 2, 9, 4, 1, 3}, {7, 9, 0, 2, 3, 4, 5, 0, 1}  // Row 8: two empty for variety
    };

    // Row 0: cells (0,0) and (0,1) empty. Missing values: 2 and 5
    // Column 0: has 3,4,1,6,9,2,5,7 → cell (0,0) can't be 2 or 5... wait, that's wrong

    // Let me recalculate what values are actually missing/possible
    // Row 0 has: 1,3,4,6,7,8,9 → missing: 2, 5
    // Column 0: 3,4,1,6,9,2,5,7 → missing: 8
    // Box 0 (rows 0-2, cols 0-2): 0,0,1, 3,1,2, 4,6,7 → has 1,1,2,3,4,6,7 → missing: 5,8,9

    // This is getting complex. Let me use a simpler, hand-crafted approach.

    board = {{1, 3, 4, 0, 0, 6, 7, 8, 9},  // Row 0: cells (0,3) and (0,4) empty
             {6, 7, 8, 9, 1, 2, 3, 4, 5}, {9, 4, 5, 6, 7, 3, 1, 2, 8}, {2, 1, 3, 4, 6, 5, 8, 9, 7},
             {4, 5, 6, 7, 8, 9, 2, 1, 3}, {7, 8, 9, 1, 2, 3, 4, 5, 6}, {3, 2, 1, 5, 9, 4, 6, 7, 0},
             {5, 6, 7, 3, 4, 8, 9, 0, 1}, {8, 9, 2, 0, 3, 7, 5, 6, 4}};

    // Row 0: cells (0,3) and (0,4) empty → missing values: 2, 5
    // This creates a naked pair if both cells can only have {2, 5}

    return board;
}

core::BoardData createBoardWithNakedTriple() {
    /*
     * Create a board where three cells have only the same three candidates
     */
    core::BoardData board = {{0, 0, 0, 4, 5, 6, 7, 8, 9},  // Row 0: first three cells empty
                             {4, 5, 6, 7, 8, 9, 1, 2, 3}, {7, 8, 9, 1, 2, 3, 4, 5, 6}, {2, 1, 4, 5, 6, 7, 8, 9, 0},
                             {5, 6, 7, 8, 9, 1, 2, 3, 4}, {8, 9, 3, 2, 4, 5, 6, 7, 1}, {3, 2, 1, 6, 7, 4, 9, 0, 5},
                             {6, 4, 5, 9, 1, 8, 3, 0, 7}, {9, 7, 8, 3, 0, 2, 5, 6, 0}};

    // Row 0: cells (0,0), (0,1), (0,2) empty → missing values: 1, 2, 3
    // These three cells can only contain {1, 2, 3} → naked triple

    return board;
}

core::BoardData createSimpleBoard() {
    // A simple partially filled board with no advanced patterns
    // Just enough to be valid but not trigger hidden/naked set detection
    return {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
            {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
            {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};
}

}  // namespace sudoku::test
