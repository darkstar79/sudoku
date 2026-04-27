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

#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "test_utils.h"

#include <memory>

namespace sudoku::test {

/// Shared fixture that creates all DI dependencies for GameViewModel tests.
/// Provides RAII temp directory cleanup and a fully-wired GameViewModel.
struct GameViewModelFixture {
    TempTestDir temp_dir;
    std::shared_ptr<core::IGameValidator> validator;
    std::shared_ptr<core::IPuzzleGenerator> generator;
    std::shared_ptr<core::ISudokuSolver> solver;
    std::shared_ptr<core::IStatisticsManager> stats_manager;
    std::shared_ptr<core::ISaveManager> save_manager;
    std::unique_ptr<viewmodel::GameViewModel> view_model;

    GameViewModelFixture() {
        validator = std::make_shared<core::GameValidator>();
        generator = std::make_shared<core::PuzzleGenerator>();
        solver = std::make_shared<core::SudokuSolver>(validator);
        stats_manager = std::make_shared<core::StatisticsManager>(temp_dir.path());
        save_manager = std::make_shared<core::SaveManager>(temp_dir.path());
        view_model =
            std::make_unique<viewmodel::GameViewModel>(validator, generator, solver, stats_manager, save_manager);
    }
};

}  // namespace sudoku::test
