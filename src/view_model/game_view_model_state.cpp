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

#include "../core/solving_technique.h"
#include "../core/string_keys.h"
#include "core/board_utils.h"
#include "core/i_game_validator.h"
#include "core/i_save_manager.h"
#include "core/i_statistics_manager.h"
#include "core/observable.h"
#include "game_view_model.h"
#include "infrastructure/app_directory_manager.h"
#include "model/game_state.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

void GameViewModel::executeCommand(GameCommand command) {
    switch (command) {
        case GameCommand::Undo:
            undo();
            break;
        case GameCommand::Redo:
            redo();
            break;
        case GameCommand::GetHint:
            getHint();
            break;
        case GameCommand::ResetGame:
            resetGame();
            break;
        case GameCommand::ToggleNotes:
            toggleNotesMode();
            break;
        case GameCommand::ShowStatistics:
            refreshStatistics();
            break;
        case GameCommand::ToggleAutoNotes:
            toggleAutoNotes();
            break;
        default:
            spdlog::warn("Unhandled command: {}", static_cast<int>(command));
            break;
    }
}

bool GameViewModel::canExecuteCommand(GameCommand command) const {
    switch (command) {
        case GameCommand::Undo:
            return canUndo();
        case GameCommand::Redo:
            return canRedo();
        case GameCommand::GetHint:
            return isGameActive() && getHintCount() > 0;
        case GameCommand::ResetGame:
            return isGameActive();
        default:
            return true;
    }
}

bool GameViewModel::isGameActive() const {
    return gameState.get().isTimerRunning();
}

bool GameViewModel::isGameComplete() const {
    return gameState.get().isComplete();
}

bool GameViewModel::isGamePaused() const {
    return !gameState.get().isTimerRunning();
}

bool GameViewModel::isGameStateDirty() const {
    const auto& state = gameState.get();
    return state.isDirty();
}

void GameViewModel::setNotesMode(bool enabled) {
    auto current_ui = uiState.get();
    current_ui.notes_mode = enabled;
    uiState.set(current_ui);
}

void GameViewModel::toggleNotesMode() {
    auto current_ui = uiState.get();
    current_ui.notes_mode = !current_ui.notes_mode;
    uiState.set(current_ui);
}

void GameViewModel::setAutoNotesEnabled(bool enabled) {
    if (uiState.get().auto_notes_enabled == enabled) {
        return;
    }

    uiState.update([enabled](UIState& state) { state.auto_notes_enabled = enabled; });

    if (enabled) {
        recomputeAutoNotes();
    } else {
        // Clear all notes for a fresh manual start
        gameState.update([](model::GameState& state) {
            core::forEachCell([&](size_t row, size_t col) { state.clearNotes({.row = row, .col = col}); });
        });
    }

    spdlog::info("Auto-notes {}", enabled ? "enabled" : "disabled");
}

void GameViewModel::toggleAutoNotes() {
    setAutoNotesEnabled(!isAutoNotesEnabled());
}

bool GameViewModel::isAutoNotesEnabled() const {
    return uiState.get().auto_notes_enabled;
}

void GameViewModel::refreshStatistics() {
    auto stats_result = stats_manager_->getAggregateStats();
    if (stats_result) {
        statistics.set(createStatsDisplay(*stats_result));
    }
}

void GameViewModel::refreshRecentSaves() {
    auto saves_result = save_manager_->listSaves();
    if (saves_result) {
        std::vector<std::string> save_names;
        for (const auto& save : *saves_result) {
            save_names.push_back(save.display_name);
        }
        recentSaves.set(save_names);
    }
}

void GameViewModel::clearErrorMessage() {
    errorMessage.set("");
}

bool GameViewModel::hasError() const {
    return !errorMessage.get().empty();
}

void GameViewModel::updateUIState() {
    UIState ui;
    ui.is_game_active = isGameActive();
    ui.is_complete = isGameComplete();
    ui.is_paused = isGamePaused();
    ui.time_display = getFormattedTime();
    ui.puzzle_rating = current_puzzle_rating_;
    ui.puzzle_techniques = formatTechniques(current_puzzle_techniques_, current_puzzle_requires_backtracking_);
    // Preserve user preferences across UI state updates
    ui.auto_notes_enabled = uiState.get().auto_notes_enabled;

    uiState.set(ui);
}

std::vector<std::string> GameViewModel::formatTechniques(const std::set<core::SolvingTechnique>& techniques,
                                                         bool requires_backtracking) const {
    // Sort by difficulty points (ascending)
    std::vector<core::SolvingTechnique> sorted(techniques.begin(), techniques.end());
    std::ranges::sort(sorted, [](core::SolvingTechnique lhs, core::SolvingTechnique rhs) {
        return core::getTechniqueRating(lhs) < core::getTechniqueRating(rhs);
    });

    std::vector<std::string> result;
    result.reserve(sorted.size() + (requires_backtracking ? 1 : 0));
    for (const auto& tech : sorted) {
        result.push_back(locFormat(core::StringKeys::TechniquePointsFmt,
                                   std::string(core::getLocalizedTechniqueName(*loc_manager_, tech)),
                                   fmt::format("{:.1f}", core::getTechniqueRating(tech))));
    }

    if (requires_backtracking) {
        result.emplace_back(loc(core::StringKeys::TechniqueBacktracking));
    }

    return result;
}

std::string GameViewModel::getFormattedTime() const {
    if (!isGameActive()) {
        return "00:00:00";
    }

    const auto& current_state = gameState.get();
    auto elapsed = current_state.getElapsedTime();
    return formatTime(elapsed);
}

void GameViewModel::startGameSession() {
    if (current_game_session_ > 0) {
        endGameSession(false);  // End previous session
    }

    auto session_result =
        stats_manager_->startGame(gameState.get().getDifficulty(),
                                  0,                      // puzzle_seed (0 for random)
                                  current_puzzle_rating_  // Pass puzzle rating for statistics tracking
        );
    if (session_result) {
        current_game_session_ = *session_result;
        spdlog::debug("Started game session: {} with rating: {}", current_game_session_, current_puzzle_rating_);
    }
}

void GameViewModel::endGameSession(bool completed) {
    if (current_game_session_ > 0) {
        auto end_result = stats_manager_->endGame(current_game_session_, completed);
        if (!end_result) {
            spdlog::warn("Failed to end game session: {}", statisticsErrorToString(end_result.error()));
        }
        current_game_session_ = 0;
    }
}

void GameViewModel::checkGameCompletion() {
    const auto& current_state = gameState.get();

    if (validator_->isComplete(current_state.extractNumbers())) {
        gameState.update([](model::GameState& state) {
            state.setComplete(true);
            state.pauseTimer();
        });

        endGameSession(true);
        spdlog::info("Game completed!");
    }
}

void GameViewModel::handleError(const std::string& message) {
    errorMessage.set(message);
    spdlog::error("GameViewModel error: {}", message);
}

std::string GameViewModel::formatTime(std::chrono::milliseconds time) {
    auto total_seconds = time.count() / 1000;
    auto hours = total_seconds / 3600;
    auto minutes = (total_seconds % 3600) / 60;
    auto seconds = total_seconds % 60;

    return fmt::format("{:02d}:{:02d}:{:02d}", hours, minutes, seconds);
}

void GameViewModel::autoSaveIfNeeded() {
    if (auto_save_enabled_) {
        autoSave();
    }
}

StatsDisplay GameViewModel::createStatsDisplay(const core::AggregateStats& stats) {
    StatsDisplay display;
    display.games_played = stats.total_games;
    display.games_completed = stats.total_completed;
    display.completion_rate =
        stats.total_games > 0 ? static_cast<double>(stats.total_completed) / stats.total_games * 100.0 : 0.0;
    display.current_streak = stats.current_win_streak;
    display.best_streak = stats.best_win_streak;

    // Calculate overall average from all difficulty levels
    std::chrono::milliseconds total_average_time{0};
    int total_completed_games = 0;
    for (int i = 0; i < 4; ++i) {
        if (stats.games_completed[i] > 0 && stats.average_times[i].count() > 0) {
            total_average_time += stats.average_times[i] * stats.games_completed[i];
            total_completed_games += stats.games_completed[i];
        }
    }

    if (total_completed_games > 0) {
        std::chrono::milliseconds weighted_average = total_average_time / total_completed_games;
        display.average_time = formatTime(weighted_average);
    } else {
        display.average_time = "N/A";
    }

    // Use the first element from best_times array for overall best time
    if (stats.best_times[0].count() > 0) {
        display.best_time = formatTime(stats.best_times[0]);
    } else {
        display.best_time = "N/A";
    }

    return display;
}

int GameViewModel::getMoveCount() const {
    return static_cast<int>(move_history_.size());
}

int GameViewModel::getMistakeCount() const {
    return gameState.get().getMistakeCount();
}

void GameViewModel::setShowConflicts(bool show) {
    auto current_ui = uiState.get();
    current_ui.show_conflicts = show;
    uiState.set(current_ui);
}

void GameViewModel::setShowHints(bool show) {
    auto current_ui = uiState.get();
    current_ui.show_hints = show;
    uiState.set(current_ui);
}

void GameViewModel::exportStatistics(const std::string& file_path) {
    auto export_result = stats_manager_->exportStats(file_path);
    if (!export_result) {
        handleError(loc(core::StringKeys::ErrorExportStats));
    }
}

std::expected<void, std::string> GameViewModel::exportAggregateStatsCsv() const {
    // Get default stats directory
    auto stats_dir =
        infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Statistics);
    auto file_path = stats_dir / "aggregate_stats.csv";

    // Export aggregate statistics
    auto result = stats_manager_->exportAggregateStatsCsv(file_path.string());
    if (!result) {
        std::string error_msg = loc(core::StringKeys::ErrorExportAggregate);
        error_msg += ": ";
        error_msg += statisticsErrorToString(result.error());
        return std::unexpected(error_msg);
    }

    spdlog::info("Aggregate stats exported to: {}", file_path.string());
    return {};
}

std::expected<void, std::string> GameViewModel::exportGameSessionsCsv() const {
    // Get default stats directory
    auto stats_dir =
        infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Statistics);
    auto file_path = stats_dir / "game_sessions.csv";

    // Export game sessions
    auto result = stats_manager_->exportGameSessionsCsv(file_path.string());
    if (!result) {
        std::string error_msg = loc(core::StringKeys::ErrorExportSessions);
        error_msg += ": ";
        error_msg += statisticsErrorToString(result.error());
        return std::unexpected(error_msg);
    }

    spdlog::info("Game sessions exported to: {}", file_path.string());
    return {};
}

}  // namespace sudoku::viewmodel
