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

#pragma once

#include "../core/i_settings_manager.h"
#include "../core/observable.h"
#include "../core/play_limit_policy.h"
#include "../view_model/game_view_model.h"
#include "../view_model/play_limit_controller.h"
#include "../view_model/training_view_model.h"
#include "core/i_puzzle_generator.h"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <QMainWindow>
#include <QTranslator>
#include <fmt/base.h>
#include <fmt/format.h>
#include <qaction.h>
#include <qtmetamacros.h>
#include <qwidget.h>
#include <qwindowdefs.h>

class QComboBox;
class QDialog;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

#ifdef SUDOKU_UI_TESTING
class TestMainWindowConstruction;
class TestBoardInteraction;
class TestKeyboardHandling;
class TestMenuToolbarActions;
class TestViewModelBinding;
class TestTrainingWidget;
class TestEditMode;
class TestKeyboardShortcuts;
#endif

namespace sudoku::view {

class SudokuBoardWidget;
class TrainingWidget;
class ToastWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;

    void setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model);
    void setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm);
    void setSettingsManager(std::shared_ptr<core::ISettingsManager> settings_manager);

    /// Wire the play-time limit coordinator (Story 6.7). Drives warn/close off the 1 s clock tick;
    /// when null (no ledger available) the feature is simply inert.
    void setPlayLimitController(std::shared_ptr<viewmodel::PlayLimitController> controller);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // ViewModel binding
    std::shared_ptr<viewmodel::GameViewModel> view_model_;
    std::shared_ptr<viewmodel::TrainingViewModel> training_vm_;
    core::CompositeObserver observer_;

    // Settings
    std::shared_ptr<core::ISettingsManager> settings_manager_;

    // Play-time limits (Story 6.7). Qt-free coordinator; the View only drives it on the tick and
    // reacts to its warn/close events.
    std::shared_ptr<viewmodel::PlayLimitController> play_limit_controller_;
    bool limit_close_in_progress_{false};  // set once a limit-triggered close starts (single save)
    bool in_play_limit_tick_{false};       // re-entrancy guard: the warn modal pumps the clock timer

    // Owns the active QTranslator so language can be swapped at runtime.
    // Initially empty — installed on the first setSettingsManager() call,
    // reloaded via applyLocale() when the user changes the language setting.
    QTranslator translator_;
    std::string current_locale_;

    [[nodiscard]] static QString qstr(std::string_view sv) {
        return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
    }

    [[nodiscard]] std::string difficultyString(core::Difficulty difficulty) const;

    // UI components
    SudokuBoardWidget* board_widget_{nullptr};
    TrainingWidget* training_widget_{nullptr};
    ToastWidget* toast_widget_{nullptr};

    // Coaching panel (persistent, above board)
    QWidget* coaching_panel_{nullptr};
    QLabel* coaching_level_label_{nullptr};
    QLabel* coaching_label_{nullptr};
    QPushButton* coaching_prev_btn_{nullptr};
    QPushButton* coaching_next_btn_{nullptr};
    QPushButton* coaching_check_btn_{nullptr};
    QPushButton* coaching_apply_btn_{nullptr};
    QPushButton* coaching_dismiss_btn_{nullptr};
    QStackedWidget* central_stack_{nullptr};
    QPushButton* new_game_btn_{nullptr};
    QLabel* difficulty_label_{nullptr};
    QLabel* hints_text_label_{nullptr};
    QComboBox* difficulty_combo_{nullptr};
    QLabel* hints_label_{nullptr};
    QPushButton* rating_btn_{nullptr};
    QAction* rating_action_{nullptr};
    QLabel* status_label_{nullptr};
    QLabel* timer_label_{nullptr};
    QLabel* session_time_label_{nullptr};
    QLabel* modifier_hint_label_{nullptr};

    // Wall-clock when MainWindow was constructed. Drives the right-side
    // session-time indicator (Settings -> Display -> "Show session timer").
    std::chrono::steady_clock::time_point session_start_time_{
        std::chrono::steady_clock::now()};  // determinism-ok: UI session-timer origin

    // Button panel below board
    QPushButton* undo_btn_{nullptr};
    QPushButton* redo_btn_{nullptr};
    QPushButton* undo_valid_btn_{nullptr};
    QPushButton* auto_notes_btn_{nullptr};
    QPushButton* mode_btn_{nullptr};
    QAction* done_editing_action_{nullptr};

    // Experimental menu entries — visibility driven by settings.experimental_*.
    // Hidden by default; revealed when the user opts in via Settings -> Experimental.
    QAction* training_mode_action_{nullptr};
    QAction* coaching_hint_action_{nullptr};

    // Timers
    QTimer* auto_save_timer_{nullptr};
    QTimer* clock_timer_{nullptr};

    // Difficulty combo tracking
    int last_difficulty_index_{1};  // Medium default

    // Setup methods
    void setupMenuBar();
    void setupGameMenu();
    void setupEditMenu();
    void setupHelpMenu();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupCoachingPanel();
    void setupButtonPanel(QVBoxLayout* game_layout);
    void setupAutoSaveTimer();

    // Dialog handlers
    void showImportPuzzleDialog();
    void enterEditPuzzleMode();
    void commitEditedPuzzle();
    void showFindByTechniqueDialog();
    void showNewGameDialog();
    void showResetDialog();
    void showSaveDialog();
    void showLoadDialog();
    void showStatisticsDialog();
    void showAboutDialog();
    void showKeyboardShortcutsDialog();
    [[nodiscard]] QDialog* buildKeyboardShortcutsDialog();
    void showThirdPartyLicensesDialog();
    void showTechniquesDialog();
    void showSettingsDialog();
    void retranslateUi();
    void applyLocale(const std::string& locale_code);

    // CSV export
    void exportAggregateStatsCsv();
    void exportGameSessionsCsv();

    // UI updates from ViewModel
    void updateStatusBar();
    void updateToolBar();
    void updateButtonPanel();
    void onCoachingStateChanged(const viewmodel::CoachingState& coaching);

    void onAutoSave();

    // Play-time limit enforcement (Story 6.7), driven off the 1 s clock tick.
    void onPlayLimitTick();
    void showTimeLimitWarning(core::LimitKind limit, int minutes_left);
    void forceCloseForLimit();

#ifdef SUDOKU_UI_TESTING
    friend class ::TestMainWindowConstruction;
    friend class ::TestBoardInteraction;
    friend class ::TestKeyboardHandling;
    friend class ::TestMenuToolbarActions;
    friend class ::TestViewModelBinding;
    friend class ::TestTrainingWidget;
    friend class ::TestEditMode;
    friend class ::TestKeyboardShortcuts;
#endif
};

}  // namespace sudoku::view
