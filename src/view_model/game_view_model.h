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

#include "../core/i_game_validator.h"
#include "../core/i_localization_manager.h"
#include "../core/i_puzzle_generator.h"
#include "../core/i_save_manager.h"
#include "../core/i_settings_manager.h"
#include "../core/i_statistics_manager.h"
#include "../core/i_sudoku_solver.h"
#include "../core/observable.h"
#include "../model/game_state.h"
#include "core/solve_step.h"
#include "core/solving_technique.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>

namespace sudoku::viewmodel {

/// Input mode for number keys on the game board
enum class InputMode : std::uint8_t {
    Normal,  ///< Number keys place values
    Notes,   ///< Number keys toggle pencil marks
    Color    ///< Number keys apply analysis colors (1-6)
};

/// Commands that can be executed from the UI
enum class GameCommand : std::uint8_t {
    NewGame,
    SaveGame,
    LoadGame,
    Undo,
    Redo,
    GetHint,
    ValidateBoard,
    PauseGame,
    ResumeGame,
    ResetGame,
    ShowStatistics,
    ToggleInputMode,
    ClearNotes
};

/// UI state information
struct UIState {
    bool is_game_active{false};
    bool is_paused{false};
    bool is_complete{false};
    InputMode input_mode{InputMode::Normal};
    bool show_conflicts{true};
    bool show_hints{true};
    bool notes_filled{false};
    std::string status_message;
    std::string time_display;
    double puzzle_rating{0.0};                   // Sudoku Explainer rating (SE 1.0-12.0 scale)
    std::vector<std::string> puzzle_techniques;  // Technique names required to solve puzzle

    bool operator==(const UIState& other) const = default;
};

/// Statistics display data
struct StatsDisplay {
    int games_played{0};
    int games_completed{0};
    double completion_rate{0.0};
    std::string best_time;
    std::string average_time;
    int current_streak{0};
    int best_streak{0};

    bool operator==(const StatsDisplay& other) const = default;
};

/// Main ViewModel for the Sudoku game following MVVM pattern
class GameViewModel {
public:
    /// Constructor with dependency injection
    GameViewModel(std::shared_ptr<core::IGameValidator> validator, std::shared_ptr<core::IPuzzleGenerator> generator,
                  std::shared_ptr<core::ISudokuSolver> solver, std::shared_ptr<core::IStatisticsManager> stats_manager,
                  std::shared_ptr<core::ISaveManager> save_manager,
                  std::shared_ptr<core::ILocalizationManager> loc_manager,
                  std::shared_ptr<core::ISettingsManager> settings_manager = nullptr);

    ~GameViewModel() = default;
    GameViewModel(const GameViewModel&) = delete;
    GameViewModel& operator=(const GameViewModel&) = delete;
    GameViewModel(GameViewModel&&) = delete;
    GameViewModel& operator=(GameViewModel&&) = delete;

    // Observable properties for UI binding
    // Public per MVVM convention: View subscribes and reads, only ViewModel
    // methods mutate. Encapsulation is enforced by convention, not the type system.
    core::Observable<model::GameState> gameState;
    core::Observable<UIState> uiState;
    core::Observable<StatsDisplay> statistics;
    core::Observable<std::vector<std::string>> recentSaves;
    core::Observable<std::string> errorMessage;
    core::Observable<std::string> hintMessage;  // Educational hint explanation

    // Technique formatting (public for testability)
    [[nodiscard]] std::vector<std::string> formatTechniques(const std::set<core::SolvingTechnique>& techniques,
                                                            bool requires_backtracking) const;

    // Game operations
    void startNewGame(core::Difficulty difficulty);
    void resetGame();
    void loadGame(const std::string& save_id);
    void restoreGameState(const core::SavedGame& saved_game);
    [[nodiscard]] bool saveCurrentGame(const std::string& name = "");
    void autoSave();

    // Board interactions
    void enterNumber(const core::Position& pos, int number);
    void enterNote(const core::Position& pos, int number);
    void clearCell(const core::Position& pos);

    /// Mode-aware number input: validates and routes to enterNumber/enterNote/colorCell
    void handleNumberInput(const core::Position& pos, int number);

    // Analysis cell coloring (ephemeral, not saved/undoable)
    void colorCell(const core::Position& pos, uint8_t color_index);
    void clearAllCellColors();

    // Position analysis — finds all applicable strategies for the current board state
    struct AnalysisResult {
        core::BoardData board;
        core::BoardData given_board;
        std::vector<uint16_t> candidate_masks;
        std::vector<core::SolveStep> applicable_steps;
    };
    [[nodiscard]] std::optional<AnalysisResult> analyzePosition() const;

    // Game commands
    void executeCommand(GameCommand command);
    [[nodiscard]] bool canExecuteCommand(GameCommand command) const;

    // Undo/Redo operations
    void undo();
    void redo();
    void undoToLastValid();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    // Hint system
    void getHint(std::optional<core::Position> pos);
    [[nodiscard]] int getHintCount() const;

    // Game state queries
    [[nodiscard]] bool isGameActive() const;
    [[nodiscard]] bool isGameComplete() const;
    [[nodiscard]] bool isGamePaused() const;
    [[nodiscard]] bool isGameStateDirty() const;
    [[nodiscard]] std::string getFormattedTime() const;
    [[nodiscard]] int getMoveCount() const;
    [[nodiscard]] int getMistakeCount() const;
    void checkSolution();

    // Settings and preferences
    void setInputMode(InputMode mode);
    void cycleInputMode();
    [[nodiscard]] InputMode getInputMode() const;
    void setShowConflicts(bool show);
    void setShowHints(bool show);
    void fillNotes();

    // Statistics
    void refreshStatistics();
    void refreshRecentSaves();
    void exportStatistics(const std::string& file_path);

    // Data accessors for dialogs (non-observable snapshots)
    [[nodiscard]] std::vector<core::SavedGame> getSaveList() const;
    [[nodiscard]] std::optional<core::AggregateStats> getAggregateStats() const;
    [[nodiscard]] std::vector<core::GameStats> getRecentGames(int count = 20) const;

    // Time formatting utility
    [[nodiscard]] static std::string formatDuration(std::chrono::milliseconds time);

    // Session history management
    void deleteSessionHistory();
    void flushStatsSessions();

    // CSV Export
    [[nodiscard]] std::expected<void, std::string> exportAggregateStatsCsv() const;
    [[nodiscard]] std::expected<void, std::string> exportGameSessionsCsv() const;

    // Error handling
    void clearErrorMessage();
    [[nodiscard]] bool hasError() const;

private:
    // Dependencies
    std::shared_ptr<core::IGameValidator> validator_;
    std::shared_ptr<core::IPuzzleGenerator> generator_;
    std::shared_ptr<core::ISudokuSolver> solver_;
    std::shared_ptr<core::IStatisticsManager> stats_manager_;
    std::shared_ptr<core::ISaveManager> save_manager_;
    std::shared_ptr<core::ILocalizationManager> loc_manager_;
    std::shared_ptr<core::ISettingsManager> settings_manager_;

    // Localization helpers
    [[nodiscard]] std::string_view loc(std::string_view key) const {
        return loc_manager_->getString(key);
    }

    template <typename... Args>
    [[nodiscard]] std::string locFormat(std::string_view key, Args&&... args) const {
        return fmt::format(fmt::runtime(loc_manager_->getString(key)), std::forward<Args>(args)...);
    }

    // Internal state
    uint64_t current_game_session_{0};
    std::vector<core::Move> move_history_;
    int move_history_index_{-1};
    int last_valid_state_index_{-1};  // Index of last conflict-free board state
    bool auto_save_enabled_{true};
    double current_puzzle_rating_{0.0};  // Rating of current puzzle (SE scale)
    std::set<core::SolvingTechnique> current_puzzle_techniques_;
    bool current_puzzle_requires_backtracking_{false};

    // Private methods
    void validateCurrentBoard();
    void updateUIState();
    void updateTimeDisplay();
    void startGameSession();
    void endGameSession(bool completed);
    void recordMove(const core::Move& move, bool is_mistake = false);
    void applyMove(const core::Move& move);
    void revertMove(const core::Move& move);
    static void cleanupConflictingNotes(model::GameState& state, const core::Position& pos, int number);
    static void restoreConflictingNotes(model::GameState& state, const core::Position& pos, int number);
    void checkGameCompletion();
    void handleError(std::string_view message);
    [[nodiscard]] static std::string formatTime(std::chrono::milliseconds time);
    void autoSaveIfNeeded();
    void recomputeAutoNotes();
    void clearAllNotes();
    void updateConflictHighlighting();
    [[nodiscard]] bool hasBoardErrors() const;

    // Hint system helpers
    [[nodiscard]] std::string formatHintExplanation(const core::SolveStep& step) const;
    [[nodiscard]] std::string_view statisticsErrorToString(core::StatisticsError error) const;

    // Statistics helpers
    [[nodiscard]] StatsDisplay createStatsDisplay(const core::AggregateStats& stats) const;
    void updateStatisticsDisplay();
};

}  // namespace sudoku::viewmodel