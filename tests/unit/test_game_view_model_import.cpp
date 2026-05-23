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

#include "../../src/core/game_validator.h"
#include "../../src/core/i_save_manager.h"
#include "../../src/core/puzzle_analyzer.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/solution_counter.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/test_utils.h"
#include "../helpers/timeout_score_analyzer.h"

#include <memory>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

// Known-easy 81-char puzzle — one naked single (R1C1 = 5).
constexpr std::string_view kEasyImport = "034678912"
                                         "672195348"
                                         "198342567"
                                         "859761423"
                                         "426853791"
                                         "713924856"
                                         "961537284"
                                         "287419635"
                                         "345286179";

struct ImportFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<SolutionCounter> counter;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IPuzzleAnalyzer> analyzer;
    std::unique_ptr<GameViewModel> view_model;

    explicit ImportFixture(std::shared_ptr<IPuzzleAnalyzer> custom_analyzer = nullptr) {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        solver = std::make_shared<SudokuSolver>(validator);
        counter = std::make_shared<SolutionCounter>();
        stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
        save_manager = std::make_shared<SaveManager>(temp_dir.path());
        if (custom_analyzer) {
            analyzer = std::move(custom_analyzer);
        } else {
            analyzer = std::make_shared<PuzzleAnalyzer>(validator, solver, counter);
        }
        view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                     /*settings_manager*/ nullptr, analyzer);
    }
};

}  // namespace

TEST_CASE("importPuzzleFromString loads a valid 81-char puzzle as the new game state", "[game_view_model][import]") {
    ImportFixture fixture;
    fixture.view_model->importPuzzleFromString(kEasyImport);

    REQUIRE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->isGameActive());

    // The board's givens match the import string (R1C1 empty, R1C2=3, etc.)
    const auto& state = fixture.view_model->gameState.get();
    REQUIRE(state.getValue({.row = 0, .col = 0}) == 0);
    REQUIRE(state.getValue({.row = 0, .col = 1}) == 3);
    REQUIRE(state.getValue({.row = 8, .col = 8}) == 9);

    // The empty cell (0,0) is NOT a given (it's a solvable cell).
    REQUIRE_FALSE(state.isGiven({.row = 0, .col = 0}));
    // The filled cells from the import ARE givens.
    REQUIRE(state.isGiven({.row = 0, .col = 1}));

    // Origin is tracked as ImportedString (verified indirectly via save round-trip below).
    REQUIRE(fixture.view_model->getCurrentPuzzleOrigin() == PuzzleOrigin::ImportedString);
}

TEST_CASE("importPuzzleFromString rejects malformed input via errorMessage", "[game_view_model][import]") {
    ImportFixture fixture;
    const bool was_active_before = fixture.view_model->isGameActive();

    fixture.view_model->importPuzzleFromString("only 5 chars");

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->isGameActive() == was_active_before);
}

// Parse-error fan-out: each ParseError variant has a distinct localized message branch in
// importPuzzleFromString. Cover all four to lock in user-facing copy and prevent future regressions
// where a refactor collapses two arms into the same string (or breaks one of them).
TEST_CASE("importPuzzleFromString surfaces parse-error variants distinctly", "[game_view_model][import]") {
    ImportFixture fixture;

    SECTION("Empty input → 'no Sudoku cells' message") {
        fixture.view_model->errorMessage.set("");
        fixture.view_model->importPuzzleFromString("");
        REQUIRE(fixture.view_model->errorMessage.get().find("no Sudoku cells") != std::string::npos);
    }

    SECTION("Wrong-length numeric input → 'expected 81' message") {
        fixture.view_model->errorMessage.set("");
        // 80 digits — passes character filter, fails length check.
        std::string short_digits(80, '0');
        fixture.view_model->importPuzzleFromString(short_digits);
        REQUIRE(fixture.view_model->errorMessage.get().find("expected 81") != std::string::npos);
    }

    SECTION("Input over 4 KiB → 'too large' message") {
        fixture.view_model->errorMessage.set("");
        // PuzzleAnalyzer::kMaxInputBytes is 4096. Anything larger trips InputTooLarge before parsing.
        std::string huge(5000, '.');
        fixture.view_model->importPuzzleFromString(huge);
        REQUIRE(fixture.view_model->errorMessage.get().find("too large") != std::string::npos);
    }

    SECTION("Non-digit/dot character → 'Invalid character' message") {
        fixture.view_model->errorMessage.set("");
        std::string with_letter(81, '0');
        with_letter[40] = 'X';
        fixture.view_model->importPuzzleFromString(with_letter);
        REQUIRE(fixture.view_model->errorMessage.get().find("Invalid character") != std::string::npos);
    }
}

// Defensive: GameViewModel constructed without an analyzer must surface a clean error rather
// than crash. Covers the analyzer-nullptr guard at the top of importPuzzleFromString.
TEST_CASE("importPuzzleFromString reports gracefully when no analyzer is wired", "[game_view_model][import]") {
    sudoku::test::TempTestDir temp_dir;
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
    auto save_manager = std::make_shared<SaveManager>(temp_dir.path());
    auto view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                      /*settings_manager*/ nullptr, /*analyzer*/ nullptr);

    view_model->importPuzzleFromString(kEasyImport);

    REQUIRE_FALSE(view_model->errorMessage.get().empty());
    REQUIRE(view_model->errorMessage.get().find("analyzer") != std::string::npos);
    REQUIRE_FALSE(view_model->isGameActive());
}

TEST_CASE("importPuzzleFromString rejects a board with rule conflicts", "[game_view_model][import]") {
    ImportFixture fixture;
    // Same easy import but with a duplicate 3 in row 0.
    std::string bad{kEasyImport};
    bad[0] = '3';  // R1C1 also = 3 → row 0 has two 3s

    fixture.view_model->importPuzzleFromString(bad);

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
}

TEST_CASE("importPuzzleFromString rejects a multi-solution puzzle", "[game_view_model][import]") {
    ImportFixture fixture;
    // Near-empty board with a couple clues — many solutions.
    std::string sparse(81, '.');
    sparse[0] = '1';
    sparse[10] = '2';

    fixture.view_model->importPuzzleFromString(sparse);

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
}

// Auto-analysis: after a successful import the analyzer's scoreDifficulty runs synchronously
// and the resulting rating + technique set are published to UIState. This makes imported
// puzzles consistent with generated puzzles (which arrive pre-rated from the generator).
TEST_CASE("importPuzzleFromString auto-populates puzzle rating + techniques", "[game_view_model][import][rating]") {
    ImportFixture fixture;
    fixture.view_model->importPuzzleFromString(kEasyImport);

    REQUIRE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->isGameActive());

    const auto& ui = fixture.view_model->uiState.get();
    REQUIRE(ui.puzzle_rating > 0.0);
    REQUIRE_FALSE(ui.puzzle_techniques.empty());
}

// Timeout swallowing: an adversarial paste-input can take longer than the 1 s scoring budget.
// The import itself must still succeed (it's valid + unique), the user must see no error
// toast, and the rating must stay at its default (0 = "unknown") rather than triggering a
// confusing "Could not analyze difficulty" message on what is otherwise a successful import.
TEST_CASE("importPuzzleFromString silently swallows scoring timeout", "[game_view_model][import][rating][timeout]") {
    auto inner = std::make_shared<PuzzleAnalyzer>(std::make_shared<GameValidator>(),
                                                  std::make_shared<SudokuSolver>(std::make_shared<GameValidator>()),
                                                  std::make_shared<SolutionCounter>());
    auto timeout_analyzer = std::make_shared<sudoku::test::TimeoutScoreAnalyzer>(inner);
    ImportFixture fixture(timeout_analyzer);

    fixture.view_model->importPuzzleFromString(kEasyImport);

    // Import itself succeeded — game is active, no error toast surfaced to the user.
    REQUIRE(fixture.view_model->isGameActive());
    REQUIRE(fixture.view_model->errorMessage.get().empty());

    // But scoring was swallowed: the rating stays at the unknown default.
    const auto& ui = fixture.view_model->uiState.get();
    REQUIRE(ui.puzzle_rating == 0.0);
}

TEST_CASE("importPuzzleFromString persists origin through save/load round-trip", "[game_view_model][import][origin]") {
    ImportFixture fixture;
    fixture.view_model->importPuzzleFromString(kEasyImport);
    REQUIRE(fixture.view_model->isGameActive());

    // Save with inspectable settings.
    const auto& current = fixture.view_model->gameState.get();
    SavedGame to_save;
    to_save.save_id = "import_origin_test";
    to_save.display_name = "Import origin test";
    to_save.original_puzzle = current.extractGivenNumbers();
    to_save.current_state = current.extractNumbers();
    to_save.origin = fixture.view_model->getCurrentPuzzleOrigin();

    SaveSettings settings;
    settings.compress = false;
    settings.encrypt = false;
    auto save_result = fixture.save_manager->saveGame(to_save, settings);
    REQUIRE(save_result.has_value());

    auto loaded = fixture.save_manager->loadGame(*save_result);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->origin == PuzzleOrigin::ImportedString);
}
