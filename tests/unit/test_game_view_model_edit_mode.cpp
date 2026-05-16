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
#include "../../src/core/i_save_manager.h"
#include "../../src/core/puzzle_analyzer.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/solution_counter.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <expected>
#include <memory>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

struct EditModeFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<SolutionCounter> counter;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IPuzzleAnalyzer> analyzer;
    std::unique_ptr<GameViewModel> view_model;

    explicit EditModeFixture(std::shared_ptr<IPuzzleAnalyzer> custom_analyzer = nullptr) {
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

    // Lay down the same easy 81-cell board via the edit-mode API.
    void layDownEasyPuzzle();
};

/// Wraps a real IPuzzleAnalyzer and forces scoreDifficulty to return ScoringError::Timeout.
/// Drives the auto-analysis timeout-swallow path deterministically without depending on
/// wall-clock against an adversarial board.
class TimeoutScoreWrapper : public IPuzzleAnalyzer {
public:
    explicit TimeoutScoreWrapper(std::shared_ptr<IPuzzleAnalyzer> inner) : inner_(std::move(inner)) {
    }
    [[nodiscard]] std::expected<BoardData, ParseErrorDetail> parseString(std::string_view input) const override {
        return inner_->parseString(input);
    }
    [[nodiscard]] std::string serializeToString(const BoardData& board) const override {
        return inner_->serializeToString(board);
    }
    [[nodiscard]] std::expected<void, BoardValidationError> validate(const BoardData& board) const override {
        return inner_->validate(board);
    }
    [[nodiscard]] UniquenessResult checkUniqueness(const BoardData& board) const override {
        return inner_->checkUniqueness(board);
    }
    [[nodiscard]] std::expected<DifficultyScore, ScoringError>
    scoreDifficulty(const BoardData& /*board*/, std::chrono::milliseconds /*budget*/) const override {
        return std::unexpected(ScoringError::Timeout);
    }

private:
    std::shared_ptr<IPuzzleAnalyzer> inner_;
};

inline void EditModeFixture::layDownEasyPuzzle() {
    constexpr int easy[81] = {0, 3, 4, 6, 7, 8, 9, 1, 2, 6, 7, 2, 1, 9, 5, 3, 4, 8, 1, 9, 8, 3, 4, 2, 5, 6, 7,
                              8, 5, 9, 7, 6, 1, 4, 2, 3, 4, 2, 6, 8, 5, 3, 7, 9, 1, 7, 1, 3, 9, 2, 4, 8, 5, 6,
                              9, 6, 1, 5, 3, 7, 2, 8, 4, 2, 8, 7, 4, 1, 9, 6, 3, 5, 3, 4, 5, 2, 8, 6, 1, 7, 9};
    for (size_t i = 0; i < 81; ++i) {
        const size_t r = i / 9;
        const size_t c = i % 9;
        if (easy[i] != 0) {
            view_model->setEditModeGiven({.row = r, .col = c}, easy[i]);
        }
    }
}

}  // namespace

TEST_CASE("enterEditMode clears the board and switches InputMode to EditGivens", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();

    REQUIRE(fixture.view_model->getInputMode() == InputMode::EditGivens);
    // Board is empty.
    const auto& state = fixture.view_model->gameState.get();
    bool any_value = false;
    for (size_t r = 0; r < 9 && !any_value; ++r) {
        for (size_t c = 0; c < 9 && !any_value; ++c) {
            if (state.getValue({.row = r, .col = c}) != 0) {
                any_value = true;
            }
        }
    }
    REQUIRE_FALSE(any_value);
}

TEST_CASE("setEditModeGiven places and clears given cells while in edit mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();

    fixture.view_model->setEditModeGiven({.row = 3, .col = 5}, 7);
    REQUIRE(fixture.view_model->gameState.get().getValue({.row = 3, .col = 5}) == 7);
    REQUIRE(fixture.view_model->gameState.get().isGiven({.row = 3, .col = 5}));

    fixture.view_model->setEditModeGiven({.row = 3, .col = 5}, 0);  // clear
    REQUIRE(fixture.view_model->gameState.get().getValue({.row = 3, .col = 5}) == 0);
    REQUIRE_FALSE(fixture.view_model->gameState.get().isGiven({.row = 3, .col = 5}));
}

TEST_CASE("commitEditedPuzzle on a valid unique puzzle switches back to Normal mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();
    fixture.layDownEasyPuzzle();

    fixture.view_model->commitEditedPuzzle();

    REQUIRE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->getInputMode() == InputMode::Normal);
    REQUIRE(fixture.view_model->getCurrentPuzzleOrigin() == PuzzleOrigin::ImportedEditMode);
    REQUIRE(fixture.view_model->isGameActive());
}

TEST_CASE("commitEditedPuzzle rejects a board with conflicts and stays in edit mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();
    // Two 5s in row 0 → invalid.
    fixture.view_model->setEditModeGiven({.row = 0, .col = 0}, 5);
    fixture.view_model->setEditModeGiven({.row = 0, .col = 4}, 5);

    fixture.view_model->commitEditedPuzzle();

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->getInputMode() == InputMode::EditGivens);
}

TEST_CASE("commitEditedPuzzle rejects a multi-solution board and stays in edit mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();
    // Only a few clues — multiple solutions.
    fixture.view_model->setEditModeGiven({.row = 0, .col = 0}, 1);
    fixture.view_model->setEditModeGiven({.row = 1, .col = 1}, 2);

    fixture.view_model->commitEditedPuzzle();

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->getInputMode() == InputMode::EditGivens);
}

// commitEditedPuzzle without an analyzer wired must surface a clean error rather than crash.
// Covers the analyzer-nullptr guard before validation.
TEST_CASE("commitEditedPuzzle reports gracefully when no analyzer is wired", "[game_view_model][edit_mode]") {
    sudoku::test::TempTestDir temp_dir;
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
    auto save_manager = std::make_shared<SaveManager>(temp_dir.path());
    auto view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                      /*settings_manager*/ nullptr, /*analyzer*/ nullptr);

    view_model->enterEditMode();
    view_model->setEditModeGiven({.row = 0, .col = 0}, 5);
    view_model->commitEditedPuzzle();

    REQUIRE_FALSE(view_model->errorMessage.get().empty());
    REQUIRE(view_model->errorMessage.get().find("analyzer") != std::string::npos);
    // Stays in edit mode since commit cannot be validated.
    REQUIRE(view_model->getInputMode() == InputMode::EditGivens);
}

// Reviewer note (Issue #2): entering edit mode while a game is in progress used to leave the
// previous game's StatisticsManager session unfinalized — orphaned, never counted in
// aggregate stats, never recoverable if the user closed the app from edit mode. The fix is to
// have enterEditMode finalize-as-abandoned, the same way startGameSession does when a session
// is already active. Observable proof: games_played for the abandoned game's difficulty
// increments without games_completed.
TEST_CASE("enterEditMode finalizes the prior active game session as abandoned", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;

    // Lay down a game first: enter edit mode, commit a valid easy puzzle so a real stats
    // session is open. (This avoids depending on the random PuzzleGenerator.)
    fixture.view_model->enterEditMode();
    fixture.layDownEasyPuzzle();
    fixture.view_model->commitEditedPuzzle();
    REQUIRE(fixture.view_model->isGameActive());

    auto before = fixture.stats_manager->getAggregateStats();
    REQUIRE(before.has_value());
    const int played_before = before->games_played[static_cast<int>(core::Difficulty::Easy)];
    const int completed_before = before->games_completed[static_cast<int>(core::Difficulty::Easy)];

    // Now enter edit mode again — the prior session must be closed as abandoned (not completed).
    fixture.view_model->enterEditMode();

    auto after = fixture.stats_manager->getAggregateStats();
    REQUIRE(after.has_value());
    REQUIRE(after->games_played[static_cast<int>(core::Difficulty::Easy)] == played_before + 1);
    REQUIRE(after->games_completed[static_cast<int>(core::Difficulty::Easy)] == completed_before);
}

// Auto-analysis: a successful commit synchronously scores the new puzzle and publishes the
// rating + technique set to UIState. Makes edited puzzles consistent with generated puzzles
// (which arrive pre-rated from the generator).
TEST_CASE("commitEditedPuzzle auto-populates puzzle rating + techniques", "[game_view_model][edit_mode][rating]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();
    fixture.layDownEasyPuzzle();

    fixture.view_model->commitEditedPuzzle();

    REQUIRE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->isGameActive());

    const auto& ui = fixture.view_model->uiState.get();
    REQUIRE(ui.puzzle_rating > 0.0);
    REQUIRE_FALSE(ui.puzzle_techniques.empty());
}

// Timeout swallowing: a committed adversarial puzzle could exceed the 1 s scoring budget.
// The commit itself still succeeds (validated + unique), the user sees no error toast, and
// the rating stays at its default (0 = "unknown") rather than fabricating a misleading toast
// on what is otherwise a successful commit.
TEST_CASE("commitEditedPuzzle silently swallows scoring timeout", "[game_view_model][edit_mode][rating][timeout]") {
    auto inner = std::make_shared<PuzzleAnalyzer>(std::make_shared<GameValidator>(),
                                                  std::make_shared<SudokuSolver>(std::make_shared<GameValidator>()),
                                                  std::make_shared<SolutionCounter>());
    auto timeout_analyzer = std::make_shared<TimeoutScoreWrapper>(inner);
    EditModeFixture fixture(timeout_analyzer);

    fixture.view_model->enterEditMode();
    fixture.layDownEasyPuzzle();
    fixture.view_model->commitEditedPuzzle();

    // Commit itself succeeded — game is active, in Normal mode, no error toast.
    REQUIRE(fixture.view_model->isGameActive());
    REQUIRE(fixture.view_model->getInputMode() == InputMode::Normal);
    REQUIRE(fixture.view_model->errorMessage.get().empty());

    // But scoring was swallowed: rating stays at the unknown default.
    const auto& ui = fixture.view_model->uiState.get();
    REQUIRE(ui.puzzle_rating == 0.0);
}
