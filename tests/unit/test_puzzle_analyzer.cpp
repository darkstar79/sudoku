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

#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_analyzer.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/solution_counter.h"
#include "../../src/core/sudoku_solver.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace sudoku::core;

namespace {

// 81-char canonical "easy" puzzle (the same naked-single fixture from solver tests).
constexpr std::string_view kEasyDigits = "034678912"
                                         "672195348"
                                         "198342567"
                                         "859761423"
                                         "426853791"
                                         "713924856"
                                         "961537284"
                                         "287419635"
                                         "345286179";

BoardData makeEasyBoard() {
    return {{0, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
}

}  // namespace

TEST_CASE("PuzzleAnalyzer::parseString — happy paths", "[puzzle_analyzer][parser]") {
    PuzzleAnalyzer analyzer;

    SECTION("81 chars, digits with 0 for empty") {
        auto result = analyzer.parseString(kEasyDigits);
        REQUIRE(result.has_value());
        REQUIRE(*result == makeEasyBoard());
    }

    SECTION("81 chars, '.' for empty") {
        std::string input{kEasyDigits};
        // Replace the leading '0' (R1C1) with '.'.
        input[0] = '.';
        auto result = analyzer.parseString(input);
        REQUIRE(result.has_value());
        REQUIRE(*result == makeEasyBoard());
    }

    SECTION("81 chars, '_' for empty") {
        std::string input{kEasyDigits};
        input[0] = '_';
        auto result = analyzer.parseString(input);
        REQUIRE(result.has_value());
        REQUIRE(*result == makeEasyBoard());
    }

    SECTION("9 lines of 9 digits separated by newlines") {
        std::string input;
        for (size_t r = 0; r < 9; ++r) {
            input.append(kEasyDigits.substr(r * 9, 9));
            input.push_back('\n');
        }
        auto result = analyzer.parseString(input);
        REQUIRE(result.has_value());
        REQUIRE(*result == makeEasyBoard());
    }

    SECTION("Mixed empty-cell markers (0, ., _) all map to empty") {
        // Manually craft a board with all three empty markers mixed in.
        std::string input =
            "_34678912"  // _ at R1C1 (was 0)
            "672195348"
            "198342567"
            "859761423"
            "42.853791"  // . at R5C3 — but this cell is filled (6) in the easy board; swap meaning instead.
            "713924856"
            "961537284"
            "287419635"
            "345286179";
        // R5C3 is '6' in the easy board; replacing with '.' makes that cell empty in the parsed result.
        // Build the expected board accordingly.
        BoardData expected = makeEasyBoard();
        expected[4][2] = 0;  // mirror the '.' at byte 38 (row 4, col 2)
        auto result = analyzer.parseString(input);
        REQUIRE(result.has_value());
        REQUIRE(*result == expected);
    }

    SECTION("Whitespace (spaces, tabs, newlines) interspersed is stripped") {
        std::string input;
        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                input.push_back(kEasyDigits[(r * 9) + c]);
                input.push_back(' ');  // space after every digit
            }
            input.push_back('\t');
            input.push_back('\n');
        }
        auto result = analyzer.parseString(input);
        REQUIRE(result.has_value());
        REQUIRE(*result == makeEasyBoard());
    }
}

TEST_CASE("PuzzleAnalyzer::parseString — rejection paths", "[puzzle_analyzer][parser]") {
    PuzzleAnalyzer analyzer;

    SECTION("80 chars → WrongLength{actual=80}") {
        std::string input{kEasyDigits.substr(0, 80)};
        auto result = analyzer.parseString(input);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ParseError::WrongLength);
        REQUIRE(result.error().actual_length == 80);
    }

    SECTION("82 chars → WrongLength{actual=82}") {
        std::string input{kEasyDigits};
        input.push_back('5');
        auto result = analyzer.parseString(input);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ParseError::WrongLength);
        REQUIRE(result.error().actual_length == 82);
    }

    SECTION("Invalid ASCII letter → InvalidCharacter with position + bad_char") {
        std::string input{kEasyDigits};
        input[40] = 'A';  // smack in the middle
        auto result = analyzer.parseString(input);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ParseError::InvalidCharacter);
        REQUIRE(result.error().position == 40);
        REQUIRE(result.error().bad_char == 'A');
    }

    SECTION("Non-ASCII byte (Unicode fullwidth digit) → InvalidCharacter") {
        // U+FF11 (FULLWIDTH DIGIT ONE) is 0xEF 0xBC 0x91 in UTF-8 — first byte 0xEF.
        std::string input{kEasyDigits.substr(0, 80)};
        input.push_back(static_cast<char>(0xEF));
        input.push_back(static_cast<char>(0xBC));
        input.push_back(static_cast<char>(0x91));
        auto result = analyzer.parseString(input);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ParseError::InvalidCharacter);
        REQUIRE(static_cast<unsigned char>(result.error().bad_char) == 0xEF);
        REQUIRE(result.error().position == 80);
    }

    SECTION("Input > 4096 bytes → InputTooLarge (and returns fast)") {
        std::string input(5000, '0');
        auto t0 = std::chrono::steady_clock::now();
        auto result = analyzer.parseString(input);
        auto elapsed = std::chrono::steady_clock::now() - t0;
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ParseError::InputTooLarge);
        REQUIRE(elapsed < std::chrono::milliseconds(10));
    }

    SECTION("Empty string → Empty") {
        auto result = analyzer.parseString("");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ParseError::Empty);
    }

    SECTION("Only whitespace → Empty") {
        auto result = analyzer.parseString("   \n\t\r   ");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == ParseError::Empty);
    }
}

TEST_CASE("PuzzleAnalyzer::serializeToString round-trip", "[puzzle_analyzer][parser][roundtrip]") {
    PuzzleAnalyzer analyzer;

    SECTION("Serialize uses '.' for empty and produces exactly 81 chars") {
        BoardData board = makeEasyBoard();
        std::string s = analyzer.serializeToString(board);
        REQUIRE(s.size() == 81);
        REQUIRE(s[0] == '.');  // R1C1 is empty
        REQUIRE(s[1] == '3');
    }

    SECTION("Property: parseString(serializeToString(b)) == b across many generated puzzles") {
        // GENERATE drives multiple seeds; PuzzleGenerator outputs are guaranteed-valid boards.
        auto seed = GENERATE(1u, 2u, 5u, 42u, 100u, 314u, 2718u);
        PuzzleGenerator generator;
        GenerationSettings settings{.difficulty = Difficulty::Medium, .seed = seed};
        auto puzzle = generator.generatePuzzle(settings);
        REQUIRE(puzzle.has_value());

        std::string serialized = analyzer.serializeToString(puzzle->board);
        auto parsed = analyzer.parseString(serialized);

        REQUIRE(parsed.has_value());
        REQUIRE(*parsed == puzzle->board);
    }
}

namespace {

// Helper: does the returned conflict set contain (row, col)?
[[nodiscard]] bool containsConflict(const std::vector<Position>& conflicts, std::size_t row, std::size_t col) {
    return std::ranges::any_of(conflicts, [&](const Position& p) { return p.row == row && p.col == col; });
}

}  // namespace

TEST_CASE("PuzzleAnalyzer::validate — happy paths", "[puzzle_analyzer][validate]") {
    auto validator = std::make_shared<GameValidator>();
    PuzzleAnalyzer analyzer(validator);

    SECTION("Empty board is trivially valid") {
        BoardData empty;
        auto result = analyzer.validate(empty);
        REQUIRE(result.has_value());
    }

    SECTION("Valid partial puzzle is valid") {
        BoardData board = makeEasyBoard();  // near-complete legitimate Sudoku
        auto result = analyzer.validate(board);
        REQUIRE(result.has_value());
    }
}

TEST_CASE("PuzzleAnalyzer::validate — conflict reporting", "[puzzle_analyzer][validate]") {
    auto validator = std::make_shared<GameValidator>();
    PuzzleAnalyzer analyzer(validator);

    SECTION("Row conflict: two 5s in row 0 → both cells flagged") {
        BoardData board;
        board[0][1] = 5;
        board[0][6] = 5;
        auto result = analyzer.validate(board);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(containsConflict(result.error().conflicting_cells, 0, 1));
        REQUIRE(containsConflict(result.error().conflicting_cells, 0, 6));
    }

    SECTION("Column conflict: two 7s in col 4 → both cells flagged") {
        BoardData board;
        board[2][4] = 7;
        board[5][4] = 7;
        auto result = analyzer.validate(board);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(containsConflict(result.error().conflicting_cells, 2, 4));
        REQUIRE(containsConflict(result.error().conflicting_cells, 5, 4));
    }

    SECTION("Box conflict: two 3s in the same 3x3 box → both cells flagged") {
        BoardData board;
        // Top-left box: (0,0) and (2,2)
        board[0][0] = 3;
        board[2][2] = 3;
        auto result = analyzer.validate(board);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(containsConflict(result.error().conflicting_cells, 0, 0));
        REQUIRE(containsConflict(result.error().conflicting_cells, 2, 2));
    }
}

TEST_CASE("PuzzleAnalyzer::checkUniqueness", "[puzzle_analyzer][uniqueness]") {
    auto validator = std::make_shared<GameValidator>();
    auto counter = std::make_shared<SolutionCounter>();
    PuzzleAnalyzer analyzer(validator, counter);

    SECTION("Generated puzzle has unique solution") {
        PuzzleGenerator generator;
        GenerationSettings settings{.difficulty = Difficulty::Easy, .seed = 42};
        auto puzzle = generator.generatePuzzle(settings);
        REQUIRE(puzzle.has_value());
        REQUIRE(analyzer.checkUniqueness(puzzle->board) == UniquenessResult::Unique);
    }

    SECTION("Empty board has multiple solutions") {
        BoardData empty;
        REQUIRE(analyzer.checkUniqueness(empty) == UniquenessResult::MultipleSolutions);
    }

    SECTION("Hand-crafted near-empty board with only 1s in row 0 has multiple solutions") {
        BoardData board;
        // Just two clues — nowhere near enough to determine a unique solution.
        board[0][0] = 1;
        board[1][3] = 2;
        REQUIRE(analyzer.checkUniqueness(board) == UniquenessResult::MultipleSolutions);
    }

    SECTION("Completely solved board has unique solution (itself)") {
        BoardData solved = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
        REQUIRE(analyzer.checkUniqueness(solved) == UniquenessResult::Unique);
    }
}

TEST_CASE("PuzzleAnalyzer::scoreDifficulty", "[puzzle_analyzer][scoring]") {
    using namespace std::chrono_literals;

    auto validator = std::make_shared<GameValidator>();
    auto counter = std::make_shared<SolutionCounter>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleAnalyzer analyzer(validator, solver, counter);

    SECTION("Easy puzzle: returns score with max_rating in [1.5, 3.0]") {
        BoardData easy = makeEasyBoard();  // single naked-single puzzle
        auto result = analyzer.scoreDifficulty(easy, 1000ms);
        REQUIRE(result.has_value());
        REQUIRE(result->max_rating >= 1.5);
        REQUIRE(result->max_rating <= 3.0);
        REQUIRE_FALSE(result->requires_backtracking);
        REQUIRE_FALSE(result->technique_ids.empty());
    }

    SECTION("Invalid board (with conflicts) returns InvalidInput") {
        BoardData bad;
        bad[0][0] = 5;
        bad[0][3] = 5;  // row conflict
        auto result = analyzer.scoreDifficulty(bad, 1000ms);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ScoringError::InvalidInput);
    }

    SECTION("Timeout (negative budget plumbing test) returns Timeout") {
        // Re-uses the deterministic R2 idiom from Step 1.1.
        BoardData easy = makeEasyBoard();
        auto result = analyzer.scoreDifficulty(easy, -1ms);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ScoringError::Timeout);
    }
}
