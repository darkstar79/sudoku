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

#include "game_view_model.h"

#include "../core/solving_technique.h"
#include "core/board_utils.h"
#include "core/constants.h"
#include "core/i_game_validator.h"
#include "core/i_localization_manager.h"
#include "core/i_puzzle_generator.h"
#include "core/i_save_manager.h"
#include "core/i_statistics_manager.h"
#include "core/i_sudoku_solver.h"
#include "core/observable.h"
#include "core/string_keys.h"
#include "model/game_state.h"

#include <algorithm>
#include <chrono>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

GameViewModel::GameViewModel(std::shared_ptr<core::IGameValidator> validator,
                             std::shared_ptr<core::IPuzzleGenerator> generator,
                             std::shared_ptr<core::ISudokuSolver> solver,
                             std::shared_ptr<core::IStatisticsManager> stats_manager,
                             std::shared_ptr<core::ISaveManager> save_manager,
                             std::shared_ptr<core::ILocalizationManager> loc_manager,
                             std::shared_ptr<core::ISettingsManager> settings_manager)
    : gameState(model::GameState{}), uiState(UIState{}), statistics(StatsDisplay{}),
      recentSaves(std::vector<std::string>{}), errorMessage(std::string{}), hintMessage(std::string{}),
      coachingState(viewmodel::CoachingState{}), validator_(std::move(validator)), generator_(std::move(generator)),
      solver_(std::move(solver)), stats_manager_(std::move(stats_manager)), save_manager_(std::move(save_manager)),
      loc_manager_(std::move(loc_manager)), settings_manager_(std::move(settings_manager)) {
    // Apply initial settings if available
    if (settings_manager_) {
        const auto& settings = settings_manager_->getSettings();
        uiState.update([&settings](UIState& state) {
            state.show_conflicts = settings.show_conflicts;
            state.show_hints = settings.show_hints;
        });
        // Wire detailed stats settings to statistics manager
        if (stats_manager_) {
            stats_manager_->setCollectDetailedStats(settings.collect_detailed_stats);
            stats_manager_->setEncryptSessions(settings.encrypt_detailed_stats);
        }
    }
    spdlog::debug("GameViewModel initialized with dependencies");
    updateUIState();
    refreshStatistics();
    refreshRecentSaves();
}

std::string_view GameViewModel::statisticsErrorToString(core::StatisticsError error) const {
    using core::StringKeys::StatsErrFileAccess;
    using core::StringKeys::StatsErrGameAlreadyEnded;
    using core::StringKeys::StatsErrGameNotStarted;
    using core::StringKeys::StatsErrInvalidData;
    using core::StringKeys::StatsErrInvalidDifficulty;
    using core::StringKeys::StatsErrSerialization;
    using core::StringKeys::StatsErrUnknown;

    switch (error) {
        case core::StatisticsError::InvalidGameData:
            return loc(StatsErrInvalidData);
        case core::StatisticsError::FileAccessError:
            return loc(StatsErrFileAccess);
        case core::StatisticsError::SerializationError:
            return loc(StatsErrSerialization);
        case core::StatisticsError::InvalidDifficulty:
            return loc(StatsErrInvalidDifficulty);
        case core::StatisticsError::GameNotStarted:
            return loc(StatsErrGameNotStarted);
        case core::StatisticsError::GameAlreadyEnded:
            return loc(StatsErrGameAlreadyEnded);
        default:
            return loc(StatsErrUnknown);
    }
}

void GameViewModel::startNewGame(core::Difficulty difficulty) {
    spdlog::info("Starting new game with difficulty: {}", static_cast<int>(difficulty));

    resetCoachingState();

    // Configure puzzle generation settings
    core::GenerationSettings settings;
    settings.difficulty = difficulty;
    settings.seed = 0;  // Random seed
    // MCV heuristic optimization enables production use of unique solution validation
    // Performance: Easy ~200ms, Medium ~800ms, Hard ~3s, Minimal ~10s
    settings.ensure_unique = true;
    settings.max_attempts = 1000;

    auto puzzle_result = generator_->generatePuzzle(settings);
    if (!puzzle_result) {
        handleError(loc(core::StringKeys::ErrorGeneratePuzzle));
        return;
    }

    // Store puzzle rating and techniques for statistics tracking and display
    current_puzzle_rating_ = puzzle_result->rating;
    current_puzzle_techniques_ = puzzle_result->required_techniques;
    current_puzzle_requires_backtracking_ = puzzle_result->requires_backtracking;

    // Create new game state
    model::GameState new_state;
    new_state.loadPuzzle(puzzle_result->board);
    new_state.setDifficulty(difficulty);
    new_state.setSolutionBoard(puzzle_result->solution);  // Store solution for hints

    gameState.set(new_state);

    // Start timer via update() to ensure Observable internal state has timer_running_ = true
    gameState.update([](model::GameState& state) { state.startTimer(); });

    // Clear move history
    move_history_.clear();
    move_history_index_ = -1;
    last_valid_state_index_ = -1;  // Reset last valid state tracker

    // Start statistics session
    startGameSession();

    updateUIState();
    spdlog::info("New game started successfully");
}

void GameViewModel::resetGame() {
    if (!isGameActive()) {
        return;  // No active game to reset
    }

    spdlog::info("Resetting current game to original puzzle state");

    // Capture current puzzle data before resetting
    const auto& current_state = gameState.get();
    auto original_puzzle = current_state.extractGivenNumbers();
    auto solution = current_state.getSolutionBoard();
    auto difficulty = current_state.getDifficulty();

    // End current stats session as abandoned (not completed)
    endGameSession(false);

    // Create fresh game state with original puzzle
    model::GameState new_state;
    new_state.loadPuzzle(original_puzzle);
    new_state.setDifficulty(difficulty);
    new_state.setSolutionBoard(solution);

    gameState.set(new_state);

    // Start timer via update() to ensure Observable internal state has timer_running_ = true
    gameState.update([](model::GameState& state) { state.startTimer(); });

    // Clear move history
    move_history_.clear();
    move_history_index_ = -1;
    last_valid_state_index_ = -1;

    // Start fresh statistics session (same puzzle rating)
    startGameSession();

    // Clear messages and coaching state
    hintMessage.set("");
    errorMessage.set("");
    resetCoachingState();

    updateUIState();
    spdlog::info("Game reset successfully");
}

void GameViewModel::loadGame(const std::string& save_id) {
    spdlog::info("Loading game: {}", save_id);

    resetCoachingState();

    auto load_result = save_manager_->loadGame(save_id);
    if (!load_result) {
        handleError(loc(core::StringKeys::ErrorLoadGame));
        return;
    }

    restoreGameState(*load_result);
    spdlog::info("Game loaded successfully");
}

void GameViewModel::restoreGameState(const core::SavedGame& saved_game) {
    // Detect corrupted saves: if original_puzzle == current_state but the game had any
    // progress indicators, the save was created by a bug that marked all cells as given.
    // Note: the old bug also prevented number entry and timer start, so move_history and
    // elapsed_time may both be empty/zero. Check notes as an additional progress indicator.
    bool has_any_notes = !saved_game.notes.empty();
    if (saved_game.original_puzzle == saved_game.current_state && (!saved_game.move_history.empty() || has_any_notes)) {
        spdlog::warn("Corrupted auto-save detected (original_puzzle == current_state with game progress), "
                     "starting new game instead");
        startNewGame(saved_game.difficulty);
        return;
    }

    // Detect phantom-value corruption: user values exist in current_state but move_history is empty.
    // This happens when a bug placed values during startup without recording them as moves.
    bool has_user_values = saved_game.original_puzzle != saved_game.current_state;
    if (has_user_values && saved_game.move_history.empty()) {
        spdlog::warn("Corrupted auto-save detected (user values present but empty move history), "
                     "starting new game instead");
        startNewGame(saved_game.difficulty);
        return;
    }

    // Create a GameState from the saved game
    model::GameState loaded_state;

    // Load original puzzle (only clues get is_given = true)
    loaded_state.loadPuzzle(saved_game.original_puzzle);
    loaded_state.setDifficulty(saved_game.difficulty);

    // Overlay user-entered values from current_state onto non-given cells
    core::forEachCell([&](size_t row, size_t col) {
        int saved_value = saved_game.current_state[row][col];
        if (!loaded_state.isGiven(row, col) && saved_value != 0) {
            loaded_state.setValue(row, col, saved_value);
        }
    });

    // Restore notes
    if (!saved_game.notes.empty()) {
        core::forEachCell([&](size_t row, size_t col) {
            loaded_state.setNotes({.row = row, .col = col}, saved_game.notes[row][col]);
        });
    }

    // Restore hint-revealed cells
    if (!saved_game.hint_revealed_cells.empty()) {
        core::forEachCell([&](size_t row, size_t col) {
            loaded_state.setHintRevealed({.row = row, .col = col}, saved_game.hint_revealed_cells.get(row, col));
        });
    }

    gameState.set(loaded_state);

    // Start timer via update() to ensure Observable internal state has timer_running_ = true
    gameState.update([](model::GameState& state) { state.startTimer(); });

    // Restore move history if available
    move_history_ = saved_game.move_history;
    move_history_index_ = static_cast<int>(move_history_.size()) - 1;

    // Recalculate last valid state and update conflict highlighting
    auto board = gameState.get().extractNumbers();
    auto conflicts = validator_->findConflicts(board);
    if (conflicts.empty()) {
        // Current state is valid
        last_valid_state_index_ = move_history_index_;
    } else {
        // Current state has conflicts, mark as no valid state known
        last_valid_state_index_ = -1;
    }
    gameState.update([&conflicts](model::GameState& state) { state.updateConflicts(conflicts); });

    // Restore puzzle rating and techniques
    current_puzzle_rating_ = saved_game.puzzle_rating;
    current_puzzle_requires_backtracking_ = saved_game.puzzle_requires_backtracking;
    current_puzzle_techniques_.clear();
    for (int id : saved_game.puzzle_technique_ids) {
        current_puzzle_techniques_.insert(static_cast<core::SolvingTechnique>(id));
    }

    updateUIState();
    spdlog::debug("Game state restored: {} moves in history", move_history_.size());
}

bool GameViewModel::saveCurrentGame(const std::string& name) {
    spdlog::info("Saving current game: {}", name.empty() ? "auto-save" : name);

    if (!isGameActive()) {
        handleError(loc(core::StringKeys::ErrorNoActiveGame));
        return false;
    }

    core::SavedGame saved_game;
    const auto& current_state = gameState.get();
    saved_game.original_puzzle = current_state.extractGivenNumbers();
    saved_game.current_state = current_state.extractNumbers();
    saved_game.difficulty = current_state.getDifficulty();
    saved_game.elapsed_time = current_state.getElapsedTime();
    saved_game.move_history = move_history_;
    saved_game.created_time = std::chrono::system_clock::now();

    // Extract notes
    core::forEachCell([&](size_t row, size_t col) { saved_game.notes[row][col] = current_state.getNotes(row, col); });

    // Extract hint-revealed cells
    core::forEachCell([&](size_t row, size_t col) {
        saved_game.hint_revealed_cells.set(row, col, current_state.isCellHintRevealed({.row = row, .col = col}));
    });

    // Persist puzzle rating
    saved_game.puzzle_rating = current_puzzle_rating_;
    saved_game.puzzle_requires_backtracking = current_puzzle_requires_backtracking_;
    for (const auto& tech : current_puzzle_techniques_) {
        saved_game.puzzle_technique_ids.push_back(static_cast<int>(tech));
    }

    saved_game.display_name = name;

    core::SaveSettings settings;
    settings.encrypt = true;
    settings.compress = true;
    settings.custom_name = name;

    auto save_result = save_manager_->saveGame(saved_game, settings);
    if (!save_result) {
        handleError(loc(core::StringKeys::ErrorSaveGame));
        return false;
    }

    refreshRecentSaves();
    spdlog::info("Game saved successfully with ID: {}", *save_result);
    return true;
}

void GameViewModel::autoSave() {
    if (auto_save_enabled_ && isGameActive()) {
        core::SavedGame auto_save_game;
        const auto& current_state = gameState.get();
        auto_save_game.original_puzzle = current_state.extractGivenNumbers();
        auto_save_game.current_state = current_state.extractNumbers();
        auto_save_game.difficulty = current_state.getDifficulty();
        auto_save_game.elapsed_time = current_state.getElapsedTime();
        auto_save_game.is_auto_save = true;
        auto_save_game.puzzle_rating = current_puzzle_rating_;
        auto_save_game.puzzle_requires_backtracking = current_puzzle_requires_backtracking_;
        for (const auto& tech : current_puzzle_techniques_) {
            auto_save_game.puzzle_technique_ids.push_back(static_cast<int>(tech));
        }

        // Extract notes
        core::forEachCell(
            [&](size_t row, size_t col) { auto_save_game.notes[row][col] = current_state.getNotes(row, col); });

        // Extract hint-revealed cells
        core::forEachCell([&](size_t row, size_t col) {
            auto_save_game.hint_revealed_cells.set(row, col,
                                                   current_state.isCellHintRevealed({.row = row, .col = col}));
        });

        auto auto_save_result = save_manager_->autoSave(auto_save_game);
        if (auto_save_result.has_value()) {
            // SUCCESS: Clear dirty flag after successful save
            gameState.update([](model::GameState& state) { state.clearDirty(); });
            spdlog::debug("Auto-save completed, dirty flag cleared");
        } else {
            spdlog::warn("Auto-save failed");
        }
    }
}

}  // namespace sudoku::viewmodel