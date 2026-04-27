// sudoku-cpp - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "core/game_validator.h"
#include "core/puzzle_generator.h"
#include "core/puzzle_rater.h"
#include "core/save_manager.h"
#include "core/statistics_manager.h"
#include "core/sudoku_solver.h"
#include "core/training_exercise_generator.h"
#include "view/main_window.h"
#include "view_model/game_view_model.h"
#include "view_model/training_view_model.h"

#include <chrono>
#include <filesystem>
#include <memory>

namespace sudoku::test {

/// Shared fixture for UI tests — creates real ViewModels with DI and manages temp directories
struct UITestContext {
    std::shared_ptr<core::IGameValidator> validator;
    std::shared_ptr<core::IPuzzleGenerator> generator;
    std::shared_ptr<core::ISudokuSolver> solver;
    std::shared_ptr<core::ISaveManager> save_manager;
    std::shared_ptr<core::IStatisticsManager> stats_manager;
    std::shared_ptr<viewmodel::GameViewModel> game_vm;
    std::shared_ptr<viewmodel::TrainingViewModel> training_vm;
    std::filesystem::path test_dir;

    UITestContext() {
        test_dir = std::filesystem::temp_directory_path() /
                   ("ui_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(test_dir);

        validator = std::make_shared<core::GameValidator>();
        solver = std::make_shared<core::SudokuSolver>(validator);
        auto rater = std::make_shared<core::PuzzleRater>(solver);
        generator = std::make_shared<core::PuzzleGenerator>(rater);
        save_manager = std::make_shared<core::SaveManager>(test_dir.string());
        stats_manager = std::make_shared<core::StatisticsManager>(test_dir.string());

        game_vm = std::make_shared<viewmodel::GameViewModel>(validator, generator, solver, stats_manager, save_manager);

        auto exercise_gen = std::make_shared<core::TrainingExerciseGenerator>(generator, solver);
        training_vm = std::make_shared<viewmodel::TrainingViewModel>(exercise_gen);
    }

    ~UITestContext() {
        std::filesystem::remove_all(test_dir);
    }

    UITestContext(const UITestContext&) = delete;
    UITestContext& operator=(const UITestContext&) = delete;

    void setupMainWindow(view::MainWindow& window) {
        window.setViewModel(game_vm);
        window.setTrainingViewModel(training_vm);
    }
};

}  // namespace sudoku::test
