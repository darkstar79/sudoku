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
#include "../../src/core/i_sudoku_solver.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <expected>
#include <memory>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

/// Decorator solver that delegates to an inner solver for the non-budget API but always
/// returns SolverError::Timeout for the new budgeted findNextStep overload. Lets us drive
/// the hint timeout path deterministically without depending on real wall-clock behavior.
class TimeoutForBudgetedHintSolver : public ISudokuSolver {
public:
    explicit TimeoutForBudgetedHintSolver(std::shared_ptr<ISudokuSolver> inner) : inner_(std::move(inner)) {
    }

    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& board) const override {
        return inner_->findNextStep(board);
    }

    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& board,
                                                                     const BoardData& original_puzzle) const override {
        return inner_->findNextStep(board, original_puzzle);
    }

    [[nodiscard]] std::expected<SolveStep, SolverError>
    findNextStep(const BoardData& /*board*/, const BoardData& /*original_puzzle*/,
                 std::chrono::milliseconds /*budget*/) const override {
        return std::unexpected(SolverError::Timeout);
    }

    [[nodiscard]] std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& board) const override {
        return inner_->solvePuzzle(board);
    }

    [[nodiscard]] bool applyStep(BoardData& board, const SolveStep& step) const override {
        return inner_->applyStep(board, step);
    }

private:
    std::shared_ptr<ISudokuSolver> inner_;
};

struct HintTimeoutFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> real_solver;
    std::shared_ptr<ISudokuSolver> timeout_solver;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::shared_ptr<ISaveManager> save_manager;
    std::unique_ptr<GameViewModel> view_model;

    HintTimeoutFixture() {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        real_solver = std::make_shared<SudokuSolver>(validator);
        timeout_solver = std::make_shared<TimeoutForBudgetedHintSolver>(real_solver);
        stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
        save_manager = std::make_shared<SaveManager>(temp_dir.path());
        view_model = std::make_unique<GameViewModel>(validator, generator, timeout_solver, stats_manager, save_manager);
    }
};

}  // namespace

TEST_CASE("getHint reports timeout via errorMessage when solver exceeds the budget",
          "[game_view_model][hints][timeout]") {
    HintTimeoutFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    const auto& state = fixture.view_model->gameState.get();
    auto empty_cell = sudoku::test::findEmptyCell(state);
    REQUIRE(empty_cell.has_value());

    const int initial_hints = fixture.view_model->getHintCount();
    fixture.view_model->errorMessage.set("");
    fixture.view_model->hintMessage.set("");

    fixture.view_model->getHint(*empty_cell);

    // No hint provided (no SolveStep returned).
    REQUIRE(fixture.view_model->hintMessage.get().empty());
    // errorMessage is populated with a budget-specific message — distinct from the existing
    // "No logical technique found" message so the View can render the right copy.
    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->errorMessage.get().find("budget") != std::string::npos);

    // Hint credit is NOT consumed on timeout (same convention as validation failures).
    REQUIRE(fixture.view_model->getHintCount() == initial_hints);
}

TEST_CASE("requestCoachingHint reports timeout via errorMessage when solver exceeds the budget",
          "[game_view_model][hints][timeout]") {
    HintTimeoutFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    const int initial_hints = fixture.view_model->getHintCount();
    fixture.view_model->errorMessage.set("");

    fixture.view_model->requestCoachingHint();

    // Coaching state should not become active on a timed-out lookup.
    REQUIRE_FALSE(fixture.view_model->coachingState.get().active);
    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->errorMessage.get().find("budget") != std::string::npos);

    // Hint credit is NOT consumed on timeout.
    REQUIRE(fixture.view_model->getHintCount() == initial_hints);
}

// Stale-hintMessage regression: a successful hint leaves text in hintMessage; if the user
// then requests another hint and the solver times out (or any validation fails), the previous
// hint text was left visible alongside the error message. The View has no way to know it's
// stale because hintMessage observers only fire on .set(). Fix: getHint() must clear
// hintMessage on every non-success exit so the View renders a clean slate.
TEST_CASE("getHint clears any prior hintMessage when the new lookup times out", "[game_view_model][hints][timeout]") {
    HintTimeoutFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    // Simulate the residue of a previous successful hint.
    const std::string stale = "Previous hint: NakedSingle at R3C5 = 7";
    fixture.view_model->hintMessage.set(stale);
    REQUIRE(fixture.view_model->hintMessage.get() == stale);

    const auto& state = fixture.view_model->gameState.get();
    auto empty_cell = sudoku::test::findEmptyCell(state);
    REQUIRE(empty_cell.has_value());

    fixture.view_model->getHint(*empty_cell);

    REQUIRE(fixture.view_model->hintMessage.get().empty());
    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
}
