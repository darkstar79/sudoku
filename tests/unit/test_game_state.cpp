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

#include "../../src/core/i_game_validator.h"
#include "../../src/core/i_time_provider.h"
#include "../../src/model/game_state.h"

#include <chrono>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::model;
using namespace sudoku::core;

TEST_CASE("GameState - Timer Operations", "[game_state][timer]") {
    // Use MockTimeProvider for deterministic timing tests
    auto mock_time = std::make_shared<MockTimeProvider>();
    GameState state(mock_time);

    SECTION("Initial timer state") {
        REQUIRE(state.getElapsedTime().count() == 0);
    }

    SECTION("Start and pause timer") {
        state.startTimer();
        mock_time->advanceSystemTime(std::chrono::milliseconds(50));
        state.pauseTimer();

        auto elapsed = state.getElapsedTime();
        REQUIRE(elapsed.count() == 50);  // Exact assertion - no tolerance needed!
    }

    SECTION("Pause without starting does nothing") {
        state.pauseTimer();
        REQUIRE(state.getElapsedTime().count() == 0);
    }

    SECTION("Resume timer") {
        state.startTimer();
        mock_time->advanceSystemTime(std::chrono::milliseconds(30));
        state.pauseTimer();

        auto first_elapsed = state.getElapsedTime();
        REQUIRE(first_elapsed.count() == 30);

        state.resumeTimer();
        mock_time->advanceSystemTime(std::chrono::milliseconds(30));
        state.pauseTimer();

        auto total_elapsed = state.getElapsedTime();
        REQUIRE(total_elapsed.count() == 60);  // Exact cumulative time
    }

    SECTION("Resume without pause does nothing") {
        state.resumeTimer();
        mock_time->advanceSystemTime(std::chrono::milliseconds(30));
        state.pauseTimer();

        auto elapsed = state.getElapsedTime();
        REQUIRE(elapsed.count() == 30);
    }

    SECTION("Reset timer") {
        state.startTimer();
        mock_time->advanceSystemTime(std::chrono::milliseconds(30));
        state.resetTimer();

        REQUIRE(state.getElapsedTime().count() == 0);
    }

    SECTION("Get elapsed time while running") {
        state.startTimer();
        mock_time->advanceSystemTime(std::chrono::milliseconds(40));

        auto elapsed1 = state.getElapsedTime();
        REQUIRE(elapsed1.count() == 40);

        mock_time->advanceSystemTime(std::chrono::milliseconds(20));
        auto elapsed2 = state.getElapsedTime();

        REQUIRE(elapsed2.count() == 60);  // Exact cumulative time
    }
}

TEST_CASE("GameState - Difficulty Management", "[game_state][difficulty]") {
    GameState state;

    SECTION("Initial difficulty") {
        REQUIRE(state.getDifficulty() == Difficulty::Medium);
    }

    SECTION("Set difficulty") {
        state.setDifficulty(Difficulty::Hard);
        REQUIRE(state.getDifficulty() == Difficulty::Hard);
    }

    SECTION("Set same difficulty (no change)") {
        state.setDifficulty(Difficulty::Medium);
        REQUIRE(state.getDifficulty() == Difficulty::Medium);
    }

    SECTION("Change difficulty multiple times") {
        state.setDifficulty(Difficulty::Easy);
        REQUIRE(state.getDifficulty() == Difficulty::Easy);

        state.setDifficulty(Difficulty::Expert);
        REQUIRE(state.getDifficulty() == Difficulty::Expert);

        state.setDifficulty(Difficulty::Medium);
        REQUIRE(state.getDifficulty() == Difficulty::Medium);
    }
}

TEST_CASE("GameState - Completion Status", "[game_state][complete]") {
    GameState state;

    SECTION("Initial completion status") {
        REQUIRE(state.isComplete() == false);
    }

    SECTION("Set completion status") {
        state.setComplete(true);
        REQUIRE(state.isComplete() == true);
    }

    SECTION("Set same completion (no change)") {
        state.setComplete(false);
        REQUIRE(state.isComplete() == false);
    }

    SECTION("Toggle completion") {
        state.setComplete(true);
        REQUIRE(state.isComplete() == true);

        state.setComplete(false);
        REQUIRE(state.isComplete() == false);
    }
}

TEST_CASE("GameState - Move Counter", "[game_state][moves]") {
    GameState state;

    SECTION("Initial move count") {
        REQUIRE(state.getMoveCount() == 0);
    }

    SECTION("Increment moves") {
        state.incrementMoves();
        REQUIRE(state.getMoveCount() == 1);

        state.incrementMoves();
        REQUIRE(state.getMoveCount() == 2);

        state.incrementMoves();
        REQUIRE(state.getMoveCount() == 3);
    }

    SECTION("Clear board resets move count") {
        state.incrementMoves();
        state.incrementMoves();
        REQUIRE(state.getMoveCount() == 2);

        state.clearBoard();
        REQUIRE(state.getMoveCount() == 0);
    }
}

TEST_CASE("GameState - Conflict Management", "[game_state][conflicts]") {
    GameState state;

    SECTION("Update conflicts") {
        std::vector<Position> conflicts = {{.row = 0, .col = 0}, {.row = 1, .col = 1}, {.row = 2, .col = 2}};
        state.updateConflicts(conflicts);

        auto result = state.getConflictPositions();
        REQUIRE(result.size() == 3);
    }

    SECTION("Clear conflicts") {
        std::vector<Position> conflicts = {{.row = 3, .col = 3}, {.row = 4, .col = 4}};
        state.updateConflicts(conflicts);

        state.clearConflicts();
        auto result = state.getConflictPositions();
        REQUIRE(result.empty());
    }

    SECTION("Update conflicts clears previous") {
        std::vector<Position> conflicts1 = {{.row = 0, .col = 0}, {.row = 1, .col = 1}};
        state.updateConflicts(conflicts1);

        std::vector<Position> conflicts2 = {{.row = 5, .col = 5}};
        state.updateConflicts(conflicts2);

        auto result = state.getConflictPositions();
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].row == 5);
        REQUIRE(result[0].col == 5);
    }

    SECTION("Conflicts with invalid positions") {
        std::vector<Position> conflicts = {{.row = 0, .col = 0}, {.row = 100, .col = 100}, {.row = 2, .col = 2}};
        state.updateConflicts(conflicts);

        auto result = state.getConflictPositions();
        REQUIRE(result.size() == 2);  // Invalid position ignored
    }
}

TEST_CASE("GameState - Note Management", "[game_state][notes]") {
    GameState state;

    SECTION("Add note") {
        state.addNote({.row = 0, .col = 0}, 5);
        const auto& cell = state.getCell({.row = 0, .col = 0});
        REQUIRE(cell.notes.count() == 1);
        REQUIRE(cell.notes.contains(5));
    }

    SECTION("Add multiple notes") {
        state.addNote({.row = 1, .col = 1}, 3);
        state.addNote({.row = 1, .col = 1}, 7);
        state.addNote({.row = 1, .col = 1}, 1);

        const auto& cell = state.getCell({.row = 1, .col = 1});
        REQUIRE(cell.notes.count() == 3);
        REQUIRE(cell.notes.contains(1));
        REQUIRE(cell.notes.contains(3));
        REQUIRE(cell.notes.contains(7));
    }

    SECTION("Add duplicate note") {
        state.addNote({.row = 2, .col = 2}, 4);
        state.addNote({.row = 2, .col = 2}, 4);

        const auto& cell = state.getCell({.row = 2, .col = 2});
        REQUIRE(cell.notes.count() == 1);
    }

    SECTION("Remove note") {
        state.addNote({.row = 3, .col = 3}, 2);
        state.addNote({.row = 3, .col = 3}, 6);
        state.removeNote({.row = 3, .col = 3}, 2);

        const auto& cell = state.getCell({.row = 3, .col = 3});
        REQUIRE(cell.notes.count() == 1);
        REQUIRE(cell.notes.contains(6));
    }

    SECTION("Clear notes") {
        state.addNote({.row = 4, .col = 4}, 1);
        state.addNote({.row = 4, .col = 4}, 5);
        state.addNote({.row = 4, .col = 4}, 9);
        state.clearNotes({.row = 4, .col = 4});

        const auto& cell = state.getCell({.row = 4, .col = 4});
        REQUIRE(cell.notes.empty());
    }

    SECTION("Toggle note - add") {
        state.toggleNote({.row = 5, .col = 5}, 8);
        const auto& cell = state.getCell({.row = 5, .col = 5});
        REQUIRE(cell.notes.count() == 1);
        REQUIRE(cell.notes.contains(8));
    }

    SECTION("Toggle note - remove") {
        state.addNote({.row = 6, .col = 6}, 3);
        state.toggleNote({.row = 6, .col = 6}, 3);

        const auto& cell = state.getCell({.row = 6, .col = 6});
        REQUIRE(cell.notes.empty());
    }

    SECTION("Toggle note multiple times") {
        state.toggleNote({.row = 7, .col = 7}, 5);
        state.toggleNote({.row = 7, .col = 7}, 5);
        state.toggleNote({.row = 7, .col = 7}, 5);

        const auto& cell = state.getCell({.row = 7, .col = 7});
        REQUIRE(cell.notes.count() == 1);
    }

    SECTION("Add note with invalid value") {
        state.addNote({.row = 0, .col = 0}, 0);
        state.addNote({.row = 0, .col = 0}, 10);

        const auto& cell = state.getCell({.row = 0, .col = 0});
        REQUIRE(cell.notes.empty());
    }

    SECTION("Add note to invalid position") {
        state.addNote({.row = 100, .col = 100}, 5);
        // Should not crash
    }
}

TEST_CASE("GameState - Board Operations", "[game_state][board]") {
    GameState state;

    SECTION("Clear board") {
        state.setValue({.row = 0, .col = 0}, 5);
        state.setNotes({.row = 1, .col = 1}, {1, 2, 3});
        state.setComplete(true);
        state.incrementMoves();
        state.incrementMistakes();
        state.clearBoard();

        REQUIRE(state.getCell({.row = 0, .col = 0}).value == 0);
        REQUIRE(state.getCell({.row = 1, .col = 1}).notes.empty());
        REQUIRE(state.isComplete() == false);
        REQUIRE(state.getMoveCount() == 0);
        REQUIRE(state.getMistakeCount() == 0);
    }

    SECTION("Load puzzle") {
        BoardData puzzle;
        puzzle[0][0] = 5;
        puzzle[1][1] = 3;
        puzzle[2][2] = 7;

        state.loadPuzzle(puzzle);

        REQUIRE(state.getCell({.row = 0, .col = 0}).value == 5);
        REQUIRE(state.getCell({.row = 0, .col = 0}).is_given == true);
        REQUIRE(state.getCell({.row = 1, .col = 1}).value == 3);
        REQUIRE(state.getCell({.row = 1, .col = 1}).is_given == true);
        REQUIRE(state.getCell({.row = 2, .col = 2}).value == 7);
        REQUIRE(state.getCell({.row = 2, .col = 2}).is_given == true);
        REQUIRE(state.getCell({.row = 3, .col = 3}).value == 0);
        REQUIRE(state.getCell({.row = 3, .col = 3}).is_given == false);
    }

    SECTION("Extract numbers") {
        state.setValue({.row = 0, .col = 0}, 1);
        state.setValue({.row = 1, .col = 1}, 2);
        state.setValue({.row = 2, .col = 2}, 3);

        auto numbers = state.extractNumbers();

        REQUIRE(numbers.size() == 9);
        REQUIRE(numbers[0].size() == 9);
        REQUIRE(numbers[0][0] == 1);
        REQUIRE(numbers[1][1] == 2);
        REQUIRE(numbers[2][2] == 3);
        REQUIRE(numbers[3][3] == 0);
    }
}

TEST_CASE("GameState - Counters", "[game_state][counters]") {
    GameState state;

    SECTION("Increment mistakes") {
        REQUIRE(state.getMistakeCount() == 0);
        state.incrementMistakes();
        REQUIRE(state.getMistakeCount() == 1);
        state.incrementMistakes();
        REQUIRE(state.getMistakeCount() == 2);
    }

    SECTION("Puzzle seed") {
        state.setPuzzleSeed(12345);
        REQUIRE(state.getPuzzleSeed() == 12345);
    }
}

TEST_CASE("GameState - Equality Operator", "[game_state][equality]") {
    GameState state1;
    GameState state2;

    SECTION("Equal when default") {
        REQUIRE(state1 == state2);
    }

    SECTION("Not equal after board change") {
        state1.setValue({.row = 0, .col = 0}, 5);
        REQUIRE(!(state1 == state2));
    }

    SECTION("Not equal after difficulty change") {
        state1.setDifficulty(Difficulty::Hard);
        REQUIRE(!(state1 == state2));
    }

    SECTION("Not equal after completion change") {
        state1.setComplete(true);
        REQUIRE(!(state1 == state2));
    }

    SECTION("Not equal after move count change") {
        state1.incrementMoves();
        REQUIRE(!(state1 == state2));
    }

    SECTION("Not equal after mistake count change") {
        state1.incrementMistakes();
        REQUIRE(!(state1 == state2));
    }

    SECTION("Not equal after cell color change") {
        // Cell colors are rendered UI state, so a color-only change must be observable —
        // otherwise Observable<GameState>::update() skips notification and the board does not
        // repaint until the next unrelated change (regression: colors not immediately visible).
        state1.setCellColor(0, 0, 3);
        REQUIRE(!(state1 == state2));
    }
}
