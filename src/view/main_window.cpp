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

#include "main_window.h"

#include "core/constants.h"
#include "core/i18n_helpers.h"
#include "core/i_game_validator.h"
#include "core/locale_utils.h"
#include "core/observable.h"
#include "infrastructure/app_directory_manager.h"
#include "key_utils.h"
#include "locale_utils.h"
#include "model/game_state.h"
#include "puzzle_import_dialog.h"
#include "puzzle_technique_dialog.h"
#include "style_colors.h"
#include "sudoku_board_widget.h"
#include "toast_widget.h"
#include "training_widget.h"
#include "view_model/game_view_model.h"
#include "view_model/training_view_model.h"

#include <array>
#include <compare>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QtCore/qobjectdefs.h>
#include <qflags.h>
#include <qkeysequence.h>
#include <qlist.h>
#include <qmenu.h>
#include <qnamespace.h>
#include <qstring.h>
#include <spdlog/spdlog.h>

namespace sudoku::view {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(qstr(core::loc("Sudoku", "Sudoku")));
    resize(800, 900);

    // Newspaper-like background
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(StyleColors::BACKGROUND_WARM));

    setupCentralWidget();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupAutoSaveTimer();
}

MainWindow::~MainWindow() {
    observer_.unsubscribeAll();
}

void MainWindow::setupCoachingPanel() {
    coaching_panel_ = new QWidget;
    coaching_panel_->setStyleSheet(
        QString("QWidget { background-color: %1; border: 1px solid %2; border-radius: 6px; }")
            .arg(StyleColors::COACHING_BG, StyleColors::COACHING_BORDER));
    auto* coaching_layout = new QHBoxLayout(coaching_panel_);
    coaching_layout->setContentsMargins(12, 8, 12, 8);

    coaching_prev_btn_ = new QPushButton(QStringLiteral("\u25C0"));  // ◀
    coaching_next_btn_ = new QPushButton(QStringLiteral("\u25B6"));  // ▶
    coaching_check_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Check")));
    coaching_apply_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Apply")));
    coaching_dismiss_btn_ = new QPushButton(QStringLiteral("\u2715"));  // ✕

    static const auto COACHING_BTN_STYLE =
        QString("QPushButton { background: transparent; border: none; font-size: 14px; "
                "padding: 2px 6px; color: %1; }"
                "QPushButton:hover { background-color: %2; border-radius: 3px; }")
            .arg(StyleColors::COACHING_TEXT, StyleColors::COACHING_BORDER);
    coaching_prev_btn_->setStyleSheet(COACHING_BTN_STYLE);
    coaching_next_btn_->setStyleSheet(COACHING_BTN_STYLE);
    coaching_check_btn_->setStyleSheet(COACHING_BTN_STYLE);
    coaching_apply_btn_->setStyleSheet(COACHING_BTN_STYLE);
    coaching_dismiss_btn_->setStyleSheet(COACHING_BTN_STYLE);

    coaching_level_label_ = new QLabel;
    coaching_level_label_->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; font-weight: bold; "
                                                 "background: transparent; border: none; }")
                                             .arg(StyleColors::COACHING_TEXT));

    coaching_label_ = new QLabel;
    coaching_label_->setWordWrap(true);
    coaching_label_->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13px; background: transparent; border: none; }")
            .arg(StyleColors::COACHING_TEXT));

    coaching_layout->addWidget(coaching_prev_btn_);
    coaching_layout->addWidget(coaching_level_label_);
    coaching_layout->addWidget(coaching_label_, 1);
    coaching_layout->addWidget(coaching_next_btn_);
    coaching_layout->addWidget(coaching_check_btn_);
    coaching_layout->addWidget(coaching_apply_btn_);
    coaching_layout->addWidget(coaching_dismiss_btn_);
    coaching_panel_->hide();

    connect(coaching_prev_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->navigateCoachingLevel(-1);
        }
    });
    connect(coaching_next_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->navigateCoachingLevel(1);
        }
    });
    connect(coaching_check_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->checkCoachingAnswer();
        }
    });
    connect(coaching_apply_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->applyCoachingStep();
        }
    });
    connect(coaching_dismiss_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->dismissCoaching();
        }
    });
}

void MainWindow::setupButtonPanel(QVBoxLayout* game_layout) {
    auto* button_panel = new QWidget;
    button_panel->setStyleSheet(QString("QWidget { background-color: %1; border-top: 1px solid %2; }")
                                    .arg(StyleColors::SURFACE, StyleColors::DIVIDER));
    auto* button_layout = new QHBoxLayout(button_panel);
    button_layout->setContentsMargins(20, 8, 20, 8);

    static const auto BTN_STYLE =
        QString("QPushButton { background-color: %1; color: %2; padding: 6px 14px; "
                "border-radius: 4px; border: 1px solid %3; }"
                "QPushButton:hover { background-color: %3; }"
                "QPushButton:disabled { color: %4; background-color: %5; border-color: %1; }"
                "QPushButton:checked { background-color: %6; color: white; border-color: %7; }")
            .arg(StyleColors::BTN_BG, StyleColors::BTN_TEXT, StyleColors::BTN_BORDER, StyleColors::BTN_DISABLED_TEXT,
                 StyleColors::BTN_DISABLED_BG, StyleColors::PRIMARY, StyleColors::PRIMARY_DARK);

    undo_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Undo")));
    redo_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Redo")));
    undo_valid_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Undo Until Valid")));
    auto_notes_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Fill Notes")));
    auto_notes_btn_->setCheckable(true);
    mode_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Normal")));
    mode_btn_->setToolTip(qstr(core::loc("Sudoku", "Input mode (Space to cycle, N for Notes)")));

    undo_btn_->setStyleSheet(BTN_STYLE);
    redo_btn_->setStyleSheet(BTN_STYLE);
    undo_valid_btn_->setStyleSheet(BTN_STYLE);
    auto_notes_btn_->setStyleSheet(BTN_STYLE);
    mode_btn_->setStyleSheet(BTN_STYLE);

    button_layout->addStretch();
    button_layout->addWidget(mode_btn_);
    button_layout->addWidget(undo_btn_);
    button_layout->addWidget(redo_btn_);
    button_layout->addWidget(undo_valid_btn_);
    button_layout->addWidget(auto_notes_btn_);
    button_layout->addStretch();

    game_layout->addWidget(button_panel);

    connect(undo_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->executeCommand(viewmodel::GameCommand::Undo);
        }
    });
    connect(redo_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->executeCommand(viewmodel::GameCommand::Redo);
        }
    });
    // undoToLastValid and fillNotes are not modeled as GameCommand verbs (they take no
    // parameters but have no canExecuteCommand precondition story yet); call them directly.
    connect(undo_valid_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->undoToLastValid();
        }
    });
    connect(auto_notes_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->fillNotes();
        }
    });
    connect(mode_btn_, &QPushButton::clicked, this, [this]() {
        if (view_model_) {
            view_model_->executeCommand(viewmodel::GameCommand::ToggleInputMode);
            updateButtonPanel();
        }
    });
}

void MainWindow::setupCentralWidget() {
    central_stack_ = new QStackedWidget;

    board_widget_ = new SudokuBoardWidget({.max_size = 720.0F, .min_size = 450.0F, .padding = 40.0F});
    training_widget_ = new TrainingWidget;
    toast_widget_ = new ToastWidget(this);

    setupCoachingPanel();

    auto* game_page = new QWidget;
    auto* game_layout = new QVBoxLayout(game_page);
    game_layout->setContentsMargins(0, 0, 0, 0);
    game_layout->addWidget(coaching_panel_);
    game_layout->addWidget(board_widget_, 1);

    setupButtonPanel(game_layout);

    central_stack_->addWidget(game_page);
    central_stack_->addWidget(training_widget_);
    central_stack_->setCurrentIndex(0);

    setCentralWidget(central_stack_);

    connect(training_widget_, &TrainingWidget::backToGame, this, [this]() { central_stack_->setCurrentIndex(0); });

    connect(board_widget_, &SudokuBoardWidget::digitKeyPressed, this, [this](int digit, Qt::KeyboardModifiers mods) {
        auto pos_opt = board_widget_->selectedCell();
        if (view_model_ && pos_opt.has_value()) {
            // overrideLayerFor maps Ctrl/Shift/Alt → the forced layer; std::nullopt keeps the
            // active mode (AC-1). The ViewModel owns the actual routing decision (AC-8).
            view_model_->handleNumberInput(pos_opt.value(), digit, overrideLayerFor(mods));
        }
    });
}

void MainWindow::setupMenuBar() {
    setupGameMenu();
    setupEditMenu();
    setupHelpMenu();
}

void MainWindow::setupGameMenu() {
    auto* game_menu = menuBar()->addMenu(QString("&%1").arg(qstr(core::loc("Sudoku", "Game"))));

    game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "New Game"))), QKeySequence("Ctrl+N"), this,
                         &MainWindow::showNewGameDialog);

    auto* reset_action = game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Reset Puzzle"))),
                                              QKeySequence("Ctrl+R"), this, &MainWindow::showResetDialog);
    reset_action->setEnabled(false);

    game_menu->addSeparator();

    game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Save"))), QKeySequence("Ctrl+S"), this,
                         &MainWindow::showSaveDialog);
    game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Load"))), QKeySequence("Ctrl+O"), this,
                         &MainWindow::showLoadDialog);

    game_menu->addSeparator();

    game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Import Custom Puzzle…"))), QKeySequence("Ctrl+I"),
                         this, &MainWindow::showImportPuzzleDialog);

    game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Edit Custom Puzzle"))), QKeySequence("Ctrl+E"),
                         this, &MainWindow::enterEditPuzzleMode);

    game_menu->addAction(qstr(core::loc("Sudoku", "Copy Puzzle as Text")), QKeySequence("Ctrl+Shift+C"), this,
                         [this]() {
                             if (view_model_) {
                                 view_model_->exportPuzzleAsString();
                                 if (toast_widget_) {
                                     toast_widget_->show(qstr(core::loc("Sudoku", "Puzzle copied to clipboard")));
                                 }
                             }
                         });

    game_menu->addSeparator();

    training_mode_action_ = game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Training Mode"))), this,
                                                 [this]() { central_stack_->setCurrentIndex(1); });
    // Hidden by default; setSettingsManager() applies the user's preference
    // and the settingsObservable subscription keeps it in sync afterwards.
    training_mode_action_->setVisible(false);

    game_menu->addAction(
        QString("&%1").arg(qstr(core::loc("Sudoku", "Analyze Position"))), QKeySequence("F2"), this, [this]() {
            if (!view_model_ || !training_vm_) {
                return;
            }
            auto result = view_model_->analyzePosition();
            if (!result.has_value()) {
                if (toast_widget_) {
                    toast_widget_->show(qstr(core::loc("Sudoku", "No logical strategies found at this position.")));
                }
                return;
            }
            training_vm_->analyzePosition(result->board, result->given_board, result->candidate_masks,
                                          result->applicable_steps);
            central_stack_->setCurrentIndex(1);
        });

    game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Resume Game"))), this,
                         [this]() { central_stack_->setCurrentIndex(0); });

    game_menu->addSeparator();

    game_menu->addAction(qstr(core::loc("Sudoku", "Statistics")), this, &MainWindow::showStatisticsDialog);
    game_menu->addAction(qstr(core::loc("Sudoku", "Export Aggregate Stats to CSV")), this,
                         &MainWindow::exportAggregateStatsCsv);
    game_menu->addAction(qstr(core::loc("Sudoku", "Export Game Sessions to CSV")), this,
                         &MainWindow::exportGameSessionsCsv);

    game_menu->addSeparator();
    game_menu->addAction(qstr(core::loc("Sudoku", "Settings...")), QKeySequence("Ctrl+,"), this,
                         &MainWindow::showSettingsDialog);
    game_menu->addSeparator();
    game_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Exit"))), QKeySequence("Alt+F4"), this,
                         &QWidget::close);
}

void MainWindow::setupEditMenu() {
    auto* edit_menu = menuBar()->addMenu(QString("&%1").arg(qstr(core::loc("Sudoku", "Edit"))));
    edit_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Undo"))), QKeySequence("Ctrl+Z"), this, [this]() {
        if (view_model_) {
            view_model_->executeCommand(viewmodel::GameCommand::Undo);
        }
    });

    edit_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Redo"))), QKeySequence("Ctrl+Y"), this, [this]() {
        if (view_model_) {
            view_model_->executeCommand(viewmodel::GameCommand::Redo);
        }
    });

    edit_menu->addSeparator();
    edit_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "Clear Cell"))), QKeySequence("Delete"), this,
                         [this]() {
                             if (!view_model_) {
                                 return;
                             }
                             auto pos = board_widget_->selectedCell();
                             if (!pos.has_value()) {
                                 return;
                             }
                             // clearCell is blocked on given cells; edit-mode givens
                             // need their own clear path.
                             if (view_model_->getInputMode() == viewmodel::InputMode::EditGivens) {
                                 view_model_->setEditModeGiven(pos.value(), 0);
                             } else {
                                 view_model_->clearCell(pos.value());
                             }
                         });
}

void MainWindow::setupHelpMenu() {
    auto* help_menu = menuBar()->addMenu(QString("&%1").arg(qstr(core::loc("Sudoku", "Help"))));
    help_menu->addAction(qstr(core::loc("Sudoku", "Get Hint")), QKeySequence("H"), this, [this]() {
        if (view_model_ && view_model_->getHintCount() > 0) {
            view_model_->getHint(board_widget_->selectedCell());
        }
    });
    coaching_hint_action_ =
        help_menu->addAction(qstr(core::loc("Sudoku", "Get Coaching Hint")), QKeySequence("Shift+H"), this, [this]() {
            if (view_model_) {
                view_model_->requestCoachingHint();
            }
        });
    // Hidden by default; gated by settings.experimental_coaching_hints.
    coaching_hint_action_->setVisible(false);
    help_menu->addAction(qstr(core::loc("Sudoku", "Find Step by Technique…")), this,
                         &MainWindow::showFindByTechniqueDialog);
    help_menu->addSeparator();
    help_menu->addAction(QString("&%1").arg(qstr(core::loc("Sudoku", "About"))), this, &MainWindow::showAboutDialog);
    help_menu->addAction(qstr(core::loc("Sudoku", "Third-Party Licenses")), this,
                         &MainWindow::showThirdPartyLicensesDialog);
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setStyleSheet(QString("QToolBar { background-color: %1; border-bottom: 1px solid %2; "
                                   "padding: 4px; spacing: 8px; }")
                               .arg(StyleColors::SURFACE, StyleColors::DIVIDER));

    new_game_btn_ = new QPushButton(qstr(core::loc("Sudoku", "▶ New Game")));
    new_game_btn_->setStyleSheet(
        QString("QPushButton { background-color: %1; color: white; padding: 6px 16px; border-radius: 4px; }"
                "QPushButton:hover { background-color: %2; }")
            .arg(StyleColors::PRIMARY, StyleColors::PRIMARY_DARK));
    connect(new_game_btn_, &QPushButton::clicked, this, &MainWindow::showNewGameDialog);
    toolbar->addWidget(new_game_btn_);

    toolbar->addSeparator();

    difficulty_label_ = new QLabel(QString(" %1 ").arg(qstr(core::loc("Sudoku", "Difficulty:"))));
    toolbar->addWidget(difficulty_label_);
    difficulty_combo_ = new QComboBox;
    difficulty_combo_->addItems({qstr(core::loc("Sudoku", "Easy")), qstr(core::loc("Sudoku", "Medium")),
                                 qstr(core::loc("Sudoku", "Hard")), qstr(core::loc("Sudoku", "Expert")),
                                 qstr(core::loc("Sudoku", "Master"))});
    difficulty_combo_->setCurrentIndex(1);  // Medium
    toolbar->addWidget(difficulty_combo_);

    connect(difficulty_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!view_model_) {
            return;
        }
        auto result =
            QMessageBox::question(this, qstr(core::loc("Sudoku", "New Game")),
                                  QString::fromStdString(core::locFormat(
                                      core::loc("Sudoku", "Start a new {0} game?\nCurrent progress will be lost."),
                                      difficulty_combo_->currentText().toStdString())));
        if (result == QMessageBox::Yes) {
            view_model_->executeCommand(viewmodel::GameCommand::NewGame,
                                        viewmodel::GameCommandArgs::newGame(static_cast<core::Difficulty>(index)));
            board_widget_->setSelectedCell(core::Position{.row = 0, .col = 0});
        } else {
            // Revert combo without triggering signal again
            difficulty_combo_->blockSignals(true);
            difficulty_combo_->setCurrentIndex(last_difficulty_index_);
            difficulty_combo_->blockSignals(false);
        }
        last_difficulty_index_ = difficulty_combo_->currentIndex();
    });

    toolbar->addSeparator();

    hints_text_label_ = new QLabel(QString(" %1 ").arg(qstr(core::loc("Sudoku", "Hints:"))));
    toolbar->addWidget(hints_text_label_);
    hints_label_ = new QLabel("10");
    hints_label_->setStyleSheet(QString("background-color: %1; color: white; padding: 2px 12px; border-radius: 12px;")
                                    .arg(StyleColors::PRIMARY));
    toolbar->addWidget(hints_label_);

    toolbar->addSeparator();

    rating_btn_ = new QPushButton;
    rating_btn_->setFlat(true);
    rating_btn_->setCursor(Qt::PointingHandCursor);
    rating_btn_->setStyleSheet(QString("QPushButton { border: none; padding: 2px 8px; text-decoration: underline; }"
                                       "QPushButton:hover { color: %1; }")
                                   .arg(StyleColors::PRIMARY));
    connect(rating_btn_, &QPushButton::clicked, this, &MainWindow::showTechniquesDialog);
    rating_action_ = toolbar->addWidget(rating_btn_);
    rating_action_->setVisible(false);

    toolbar->addSeparator();

    done_editing_action_ =
        toolbar->addAction(qstr(core::loc("Sudoku", "Done Editing")), this, &MainWindow::commitEditedPuzzle);
    done_editing_action_->setVisible(false);
}

void MainWindow::setupStatusBar() {
    timer_label_ = new QLabel();
    statusBar()->addWidget(timer_label_);
    status_label_ = new QLabel(qstr(core::loc("Sudoku", "Ready")));
    statusBar()->addWidget(status_label_, 1);

    // Right-side session timer (wall-clock since app launch). Toggled by
    // Settings -> Display -> "Show session timer". addPermanentWidget()
    // anchors the label to the right of the status bar. Hidden until
    // setSettingsManager() wires the observer and re-applies the persisted
    // flag; objectName lets UI tests find it without layout heuristics.
    session_time_label_ = new QLabel();
    session_time_label_->setObjectName("sessionTimerLabel");
    statusBar()->addPermanentWidget(session_time_label_);
    session_time_label_->setVisible(false);

    statusBar()->setStyleSheet(QString("QStatusBar { background-color: %1; border-top: 1px solid %2; color: %3; }")
                                   .arg(StyleColors::SURFACE_STATUS, StyleColors::DIVIDER, StyleColors::TEXT_MUTED));

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    clock_timer_->start();
}

void MainWindow::setupAutoSaveTimer() {
    auto_save_timer_ = new QTimer(this);
    auto interval = settings_manager_ ? settings_manager_->getSettings().auto_save_interval_ms : 30000;
    auto_save_timer_->setInterval(interval);
    connect(auto_save_timer_, &QTimer::timeout, this, &MainWindow::onAutoSave);
    auto_save_timer_->start();
}

void MainWindow::onAutoSave() {
    if (view_model_ && view_model_->isGameStateDirty()) {
        view_model_->autoSave();
        spdlog::debug("Periodic auto-save triggered (30s interval)");
    }
}

void MainWindow::setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model) {
    observer_.unsubscribeAll();
    view_model_ = std::move(view_model);

    if (view_model_) {
        board_widget_->setSelectedCell(core::Position{.row = 0, .col = 0});

        // Push render data on every game state change
        observer_.observe(view_model_->gameState, [this](const auto& state) {
            board_widget_->setBoard(toBoardRenderData(state));
            updateStatusBar();
            updateToolBar();
            updateButtonPanel();
        });
        observer_.observe(view_model_->uiState, [this](const auto&) {
            updateToolBar();
            updateStatusBar();
            updateButtonPanel();
        });
        observer_.observe(view_model_->errorMessage, [](const std::string& error) {
            if (!error.empty()) {
                spdlog::error("UI Error: {}", error);
            }
        });
        observer_.observe(view_model_->coachingState,
                          [this](const viewmodel::CoachingState& coaching) { onCoachingStateChanged(coaching); });

        spdlog::debug("ViewModel bound to MainWindow");
    }
}

void MainWindow::onCoachingStateChanged(const viewmodel::CoachingState& coaching) {
    if (!coaching.active) {
        if (coaching_panel_) {
            coaching_panel_->hide();
        }
        board_widget_->setBoard(toBoardRenderData(view_model_->gameState.get()));
        return;
    }

    if (coaching_panel_) {
        coaching_label_->setText(QString::fromStdString(coaching.message));

        const bool is_tryit = (coaching.phase == viewmodel::CoachingPhase::TryIt);

        if (is_tryit) {
            coaching_level_label_->setText(qstr(core::loc("Sudoku", "Try it!")));
        } else {
            coaching_level_label_->setText(
                QString::fromStdString(core::locFormat(core::loc("Sudoku", "Level {0}/3"), coaching.level)));
        }
        coaching_level_label_->setVisible(true);

        // Hinting phase: show prev/next, hide check/apply
        coaching_prev_btn_->setVisible(!is_tryit && coaching.max_level > 1);
        coaching_next_btn_->setVisible(!is_tryit && coaching.max_level > 1);
        coaching_prev_btn_->setEnabled(coaching.level > 1);
        coaching_next_btn_->setEnabled(coaching.level < coaching.max_level);

        // TryIt phase: show check/apply, hide prev/next
        coaching_check_btn_->setVisible(is_tryit);
        coaching_apply_btn_->setVisible(is_tryit);

        coaching_panel_->show();
    }

    // Overlay coaching highlights onto the current board render data
    auto render = toBoardRenderData(view_model_->gameState.get());
    for (const auto& [pos, role] : coaching.highlights) {
        render[pos.row][pos.col].highlight_role = role;
    }
    board_widget_->setBoard(render);
}

void MainWindow::setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm) {
    training_vm_ = std::move(training_vm);
    training_widget_->setTrainingViewModel(training_vm_);

    if (training_vm_) {
        observer_.observe(training_vm_->errorMessage, [](const std::string& error) {
            if (!error.empty()) {
                spdlog::error("Training Error: {}", error);
            }
        });
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void MainWindow::setSettingsManager(std::shared_ptr<core::ISettingsManager> settings_manager) {
    settings_manager_ = std::move(settings_manager);

    if (settings_manager_) {
        // Update auto-save interval from settings
        if (auto_save_timer_) {
            auto_save_timer_->setInterval(settings_manager_->getSettings().auto_save_interval_ms);
        }

        // Load the configured locale; Qt posts QEvent::LanguageChange to all
        // top-level widgets, which retranslates the UI via changeEvent.
        applyLocale(settings_manager_->getSettings().language);

        // Apply initial visibility for experimental menu entries before the
        // observer takes over (the observer only fires on subsequent changes).
        if (training_mode_action_) {
            training_mode_action_->setVisible(settings_manager_->getSettings().experimental_training_mode);
        }
        if (coaching_hint_action_) {
            coaching_hint_action_->setVisible(settings_manager_->getSettings().experimental_coaching_hints);
        }

        // React to language changes from the Settings dialog. Compare against
        // the previous locale so we only reload the translator when it
        // actually changes (other settings — auto-save interval, hint
        // visibility, experimental flags, etc. — also fire this observer).
        //
        // Route through `observer_` (CompositeObserver) so the subscription is
        // torn down in ~MainWindow. settings_manager_ is a shared_ptr owned by
        // DIContainer and outlives MainWindow, so a direct subscribe() would
        // leave a callback dereferencing a destroyed `this` on the next
        // setSettings() (use-after-free).
        observer_.observe(settings_manager_->settingsObservable(), [this](const core::Settings& s) {
            if (auto_save_timer_) {
                auto_save_timer_->setInterval(s.auto_save_interval_ms);
            }
            if (s.language != current_locale_) {
                applyLocale(s.language);
            }
            if (training_mode_action_) {
                training_mode_action_->setVisible(s.experimental_training_mode);
            }
            if (coaching_hint_action_) {
                coaching_hint_action_->setVisible(s.experimental_coaching_hints);
            }
            if (session_time_label_) {
                session_time_label_->setVisible(s.show_session_timer);
                if (s.show_session_timer) {
                    updateStatusBar();
                }
            }
        });
    }

    spdlog::debug("SettingsManager bound to MainWindow");
}

void MainWindow::applyLocale(const std::string& locale_code) {
    // Defense in depth: by the time we reach here the code has already been
    // validated by SettingsManager, but applyLocale interpolates `locale_code`
    // into a path passed to QTranslator::load(). Rejecting bad codes here
    // closes the loop if a future caller skips the SettingsManager gate.
    if (!core::isValidLocaleCode(locale_code)) {
        spdlog::warn("Rejected invalid locale code '{}' in applyLocale", locale_code);
        return;
    }

    auto translations_dir = findTranslationsDir();

    QCoreApplication::removeTranslator(&translator_);

    if (translations_dir.empty()) {
        spdlog::warn("Qt translations directory not found; UI will use source-language strings");
        current_locale_ = locale_code;
        return;
    }

    auto qm_name = QString::fromStdString(fmt::format("sudoku_{}", locale_code));
    if (!translator_.load(qm_name, QString::fromStdString(translations_dir.string()))) {
        spdlog::warn("Failed to load Qt translation '{}.qm' from {}", qm_name.toStdString(), translations_dir.string());
        current_locale_ = locale_code;
        return;
    }
    if (!QCoreApplication::installTranslator(&translator_)) {
        spdlog::warn("QCoreApplication::installTranslator returned false for locale '{}'", locale_code);
        current_locale_ = locale_code;
        return;
    }
    current_locale_ = locale_code;
    spdlog::info("Qt translator installed: {} from {}", qm_name.toStdString(), translations_dir.string());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (view_model_) {
        spdlog::info("Window close requested, saving game state...");
        view_model_->autoSave();
        spdlog::info("Game state saved, closing window");
    }
    event->accept();
}

bool MainWindow::event(QEvent* event) {
    return QMainWindow::event(event);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (!view_model_) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (event->isAutoRepeat()) {
        return;
    }

    int key = event->key();

    // Escape dismisses coaching hints
    if (key == Qt::Key_Escape && view_model_->isCoachingActive()) {
        view_model_->dismissCoaching();
        return;
    }

    // Space cycles input mode (Normal → Notes → Color → Normal)
    if (key == Qt::Key_Space) {
        view_model_->cycleInputMode();
        updateButtonPanel();
        return;
    }

    // Number keys 1-9 (with Ctrl/Shift/Alt overrides) are handled by the board widget signal
    // (digitKeyPressed). The hardcoded 'N' jump-to-Notes key was retired in 0a.2 — Space-cycle
    // reaches any mode and the gameplay layer stays language-neutral.

    // Editing keys. Plain Delete/Backspace/0 clears the active layer; modifier combos clear a
    // specific layer in any mode: Ctrl+0/Delete → value, Alt+0/Delete → color, Shift+Delete →
    // all pencil marks. 0 is the erase digit in this domain, so Ctrl+0/Alt+0 arrive as Key_0.
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace || key == Qt::Key_0) {
        auto edit_pos = board_widget_->selectedCell();
        if (!edit_pos.has_value()) {
            return;
        }
        const auto pos = edit_pos.value();
        const auto mode = view_model_->getInputMode();
        const auto mods = event->modifiers();

        // Modifier erase overrides are meaningless while building givens (AC-9).
        const bool has_erase_modifier =
            mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::AltModifier) || mods.testFlag(Qt::ShiftModifier);
        if (mode != viewmodel::InputMode::EditGivens && has_erase_modifier) {
            if (mods.testFlag(Qt::ControlModifier)) {
                view_model_->clearCell(pos);  // Ctrl+0 / Ctrl+Delete → clear the value
            } else if (mods.testFlag(Qt::AltModifier)) {
                view_model_->colorCell(pos, 0);  // Alt+0 / Alt+Delete → clear the color
            } else if (key != Qt::Key_0) {
                view_model_->clearCellNotes(pos);  // Shift+Delete → clear all pencil marks
            }
            return;
        }

        switch (mode) {
            case viewmodel::InputMode::Color:
                view_model_->colorCell(pos, 0);  // Clear color
                break;
            case viewmodel::InputMode::EditGivens:
                // clearCell is blocked on given cells; edit-mode givens need their own clear path.
                view_model_->setEditModeGiven(pos, 0);
                break;
            case viewmodel::InputMode::Normal:
            case viewmodel::InputMode::Notes:
                view_model_->clearCell(pos);
                break;
        }
        return;
    }

    // Ctrl+Shift+Z = undo to last valid
    if (key == Qt::Key_Z && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        view_model_->undoToLastValid();
        return;
    }

    // F1 toggle menu
    if (key == Qt::Key_F1) {
        menuBar()->setVisible(!menuBar()->isVisible());
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::updateStatusBar() {
    // Right-side session timer: ticks unconditionally from app launch,
    // independent of game state — kept above the !view_model_ early return
    // below so it stays accurate even before a puzzle is loaded. Hidden
    // unless the user enabled it in Settings -> Display.
    if (session_time_label_ && session_time_label_->isVisible()) {
        const auto session_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - session_start_time_);
        session_time_label_->setText(QString::fromStdString(viewmodel::GameViewModel::formatDuration(session_elapsed)));
    }

    if (!view_model_) {
        timer_label_->clear();
        status_label_->setText(qstr(core::loc("Sudoku", "Ready")));
        return;
    }

    const auto& game_state = view_model_->gameState.get();
    if (game_state.isComplete()) {
        timer_label_->setText(QString::fromStdString(view_model_->getFormattedTime()));
        status_label_->setText(
            QString("<span style='color: green;'>%1</span>").arg(qstr(core::loc("Sudoku", "Completed!"))));
    } else if (game_state.isTimerRunning()) {
        timer_label_->setText(QString::fromStdString(view_model_->getFormattedTime()));
        status_label_->setText(qstr(core::loc("Sudoku", "Playing")));
    } else {
        timer_label_->clear();
        status_label_->setText(qstr(core::loc("Sudoku", "Ready")));
    }
}

void MainWindow::updateToolBar() {
    if (!view_model_) {
        return;
    }

    int hint_count = view_model_->getHintCount();
    hints_label_->setText(QString::number(hint_count));

    const auto& ui_state = view_model_->uiState.get();
    if (ui_state.puzzle_rating > 0.0) {
        const auto& techniques = ui_state.puzzle_techniques;
        auto rating_str = QString::number(ui_state.puzzle_rating, 'f', 1).toStdString();
        if (!techniques.empty()) {
            rating_btn_->setText(QString::fromStdString(
                core::locFormat(core::loc("Sudoku", "SE {0} ({1} techniques)"), rating_str, techniques.size())));
        } else {
            rating_btn_->setText(QString::fromStdString(core::locFormat(core::loc("Sudoku", "SE {0}"), rating_str)));
        }
        rating_action_->setVisible(true);
    } else {
        rating_action_->setVisible(false);
    }

    done_editing_action_->setVisible(ui_state.input_mode == viewmodel::InputMode::EditGivens);
}

void MainWindow::updateButtonPanel() {
    if (!view_model_) {
        return;
    }

    // Gate through the command pipeline so the enabled state matches what executeCommand
    // will actually honor.
    undo_btn_->setEnabled(view_model_->canExecuteCommand(viewmodel::GameCommand::Undo));
    redo_btn_->setEnabled(view_model_->canExecuteCommand(viewmodel::GameCommand::Redo));

    // Update input mode indicator
    switch (view_model_->getInputMode()) {
        case viewmodel::InputMode::Normal:
            mode_btn_->setText(qstr(core::loc("Sudoku", "Normal")));
            break;
        case viewmodel::InputMode::Notes:
            mode_btn_->setText(qstr(core::loc("Sudoku", "Notes")));
            break;
        case viewmodel::InputMode::Color:
            mode_btn_->setText(qstr(core::loc("Sudoku", "Color")));
            break;
        case viewmodel::InputMode::EditGivens:
            mode_btn_->setText(qstr(core::loc("Sudoku", "Edit")));
            break;
    }

    // Update fill notes toggle state
    const auto& ui = view_model_->uiState.get();
    auto_notes_btn_->setChecked(ui.notes_filled);
    auto_notes_btn_->setText(ui.notes_filled ? qstr(core::loc("Sudoku", "Clear Notes"))
                                             : qstr(core::loc("Sudoku", "Fill Notes")));
}

// Dialog methods

void MainWindow::showImportPuzzleDialog() {
    if (!view_model_) {
        return;
    }

    if (view_model_->isGameActive() && view_model_->isGameStateDirty()) {
        auto answer =
            QMessageBox::question(this, qstr(core::loc("Sudoku", "Import Custom Puzzle")),
                                  qstr(core::loc("Sudoku", "Importing replaces your current puzzle. Continue?")));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    auto* dialog = new PuzzleImportDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &PuzzleImportDialog::importRequested, this, [this, dialog](const QString& text) {
        // importPuzzleFromString runs the analyzer's uniqueness check synchronously, which can
        // burn up to its 1 s budget on adversarial input. Signal the freeze with a wait cursor.
        QApplication::setOverrideCursor(Qt::WaitCursor);
        view_model_->importPuzzleFromString(text.toStdString());
        QApplication::restoreOverrideCursor();
        const auto& err = view_model_->errorMessage.get();
        if (err.empty()) {
            board_widget_->setSelectedCell(core::Position{.row = 0, .col = 0});
            dialog->accept();
        } else {
            dialog->setErrorMessage(qstr(err));
        }
    });
    dialog->show();
}

void MainWindow::enterEditPuzzleMode() {
    if (!view_model_) {
        return;
    }

    if (view_model_->isGameActive() && view_model_->isGameStateDirty()) {
        auto answer =
            QMessageBox::question(this, qstr(core::loc("Sudoku", "Edit Custom Puzzle")),
                                  qstr(core::loc("Sudoku", "Editing replaces your current puzzle. Continue?")));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    view_model_->enterEditMode();
    board_widget_->setSelectedCell(core::Position{.row = 0, .col = 0});
}

void MainWindow::commitEditedPuzzle() {
    if (!view_model_) {
        return;
    }
    // commitEditedPuzzle runs validate + uniqueness synchronously (up to 1 s budget). Signal
    // the freeze with a wait cursor — same convention as showImportPuzzleDialog / analyzeDifficulty.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    view_model_->commitEditedPuzzle();
    QApplication::restoreOverrideCursor();
    const auto& err = view_model_->errorMessage.get();
    if (!err.empty() && toast_widget_) {
        toast_widget_->show(qstr(err));
    }
}

void MainWindow::showFindByTechniqueDialog() {
    if (!view_model_) {
        return;
    }

    auto* dialog = new PuzzleTechniqueDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &PuzzleTechniqueDialog::findRequested, this, [this, dialog](int technique_id) {
        view_model_->findStepByTechnique(static_cast<core::SolvingTechnique>(technique_id));
        const auto& err = view_model_->errorMessage.get();
        if (!err.empty()) {
            if (toast_widget_) {
                toast_widget_->show(qstr(err));
            }
        }
        dialog->accept();
    });
    dialog->show();
}

void MainWindow::showNewGameDialog() {
    if (!view_model_) {
        return;
    }

    int selected = difficulty_combo_ ? difficulty_combo_->currentIndex() : 1;
    QString diff_name = difficulty_combo_ ? difficulty_combo_->currentText() : qstr(core::loc("Sudoku", "Medium"));

    auto result = QMessageBox::question(
        this, qstr(core::loc("Sudoku", "New Game")),
        QString::fromStdString(core::locFormat(
            core::loc("Sudoku", "Start a new {0} game?\nCurrent progress will be lost."), diff_name.toStdString())));

    if (result == QMessageBox::Yes) {
        view_model_->executeCommand(viewmodel::GameCommand::NewGame,
                                    viewmodel::GameCommandArgs::newGame(static_cast<core::Difficulty>(selected)));
        board_widget_->setSelectedCell(core::Position{.row = 0, .col = 0});
    }
}

void MainWindow::showResetDialog() {
    auto result =
        QMessageBox::warning(this, qstr(core::loc("Sudoku", "Reset Puzzle")),
                             qstr(core::loc("Sudoku", "All progress on this puzzle will be lost, including placed "
                                                      "numbers, notes, and hints. The timer will restart.")),
                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (result == QMessageBox::Yes && view_model_) {
        view_model_->executeCommand(viewmodel::GameCommand::ResetGame);
    }
}

void MainWindow::showSaveDialog() {
    if (!view_model_ || !view_model_->isGameActive()) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(qstr(core::loc("Sudoku", "Save Game")));
    dialog.setMinimumWidth(380);

    auto* layout = new QVBoxLayout(&dialog);

    // Current game info preview
    auto* info_group = new QGroupBox(qstr(core::loc("Sudoku", "Current Game")), &dialog);
    auto* info_layout = new QFormLayout(info_group);
    info_layout->addRow(qstr(core::loc("Sudoku", "Difficulty")),
                        new QLabel(qstr(difficultyString(view_model_->gameState.get().getDifficulty()))));
    info_layout->addRow(qstr(core::loc("Sudoku", "Time")),
                        new QLabel(QString::fromStdString(view_model_->getFormattedTime())));
    info_layout->addRow(qstr(core::loc("Sudoku", "Moves")), new QLabel(QString::number(view_model_->getMoveCount())));
    info_layout->addRow(qstr(core::loc("Sudoku", "Mistakes")),
                        new QLabel(QString::number(view_model_->getMistakeCount())));
    layout->addWidget(info_group);

    // Name input
    layout->addWidget(new QLabel(qstr(core::loc("Sudoku", "Enter save name:"))));
    auto* name_edit = new QLineEdit(&dialog);
    name_edit->setPlaceholderText(qstr(core::loc("Sudoku", "Enter save name...")));
    layout->addWidget(name_edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QString name = name_edit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(&dialog, qstr(core::loc("Sudoku", "Save Game")),
                                 qstr(core::loc("Sudoku", "Please enter a save name.")));
            return;
        }

        // Check for existing save with same display_name
        auto existing = view_model_->getSaveList();
        bool name_exists =
            std::ranges::any_of(existing, [&](const auto& s) { return s.display_name == name.toStdString(); });
        if (name_exists) {
            auto confirm = QMessageBox::question(
                &dialog, qstr(core::loc("Sudoku", "Save Game")),
                QString::fromStdString(core::locFormat(
                    core::loc("Sudoku", "A save named \"{0}\" already exists. Overwrite it?"), name.toStdString())));
            if (confirm != QMessageBox::Yes) {
                return;
            }
        }

        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = name_edit->text().trimmed();
        // Dispatch through the command pipeline; saveCurrentGame publishes errorMessage on
        // failure, so a clean error channel after the call signals success for the toast.
        view_model_->clearErrorMessage();
        view_model_->executeCommand(viewmodel::GameCommand::SaveGame,
                                    viewmodel::GameCommandArgs::save(name.toStdString()));
        if (!view_model_->hasError() && toast_widget_) {
            toast_widget_->show(qstr(core::loc("Sudoku", "Game saved successfully")));
        }
    }
}

void MainWindow::showLoadDialog() {
    if (!view_model_) {
        return;
    }

    auto saves = view_model_->getSaveList();

    QDialog dialog(this);
    dialog.setWindowTitle(qstr(core::loc("Sudoku", "Load Game")));
    dialog.setMinimumSize(600, 350);

    auto* layout = new QVBoxLayout(&dialog);

    static constexpr int LOAD_COLS = 5;
    auto* table = new QTableWidget(static_cast<int>(saves.size()), LOAD_COLS, &dialog);
    table->setHorizontalHeaderLabels({qstr(core::loc("Sudoku", "Name")), qstr(core::loc("Sudoku", "Difficulty")),
                                      qstr(core::loc("Sudoku", "Last Modified")), qstr(core::loc("Sudoku", "Elapsed")),
                                      qstr(core::loc("Sudoku", "Rating"))});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);

    for (int row = 0; std::cmp_less(row, saves.size()); ++row) {
        const auto& s = saves[row];
        auto* name_item = new QTableWidgetItem(QString::fromStdString(s.display_name));
        name_item->setData(Qt::UserRole, QString::fromStdString(s.save_id));
        table->setItem(row, 0, name_item);
        table->setItem(row, 1, new QTableWidgetItem(qstr(difficultyString(s.difficulty))));

        auto tt = std::chrono::system_clock::to_time_t(s.last_modified);
        table->setItem(
            row, 2, new QTableWidgetItem(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(tt)).toString(Qt::ISODate)));
        table->setItem(
            row, 3,
            new QTableWidgetItem(QString::fromStdString(viewmodel::GameViewModel::formatDuration(s.elapsed_time))));
        table->setItem(row, 4,
                       new QTableWidgetItem(s.puzzle_rating > 0.0 ? QString::fromStdString(core::locFormat(
                                                                        core::loc("Sudoku", "SE {0}"),
                                                                        fmt::format("{:.1f}", s.puzzle_rating)))
                                                                  : qstr(core::loc("Sudoku", "N/A"))));
    }
    table->resizeColumnsToContents();
    layout->addWidget(table);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(table, &QTableWidget::itemDoubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted) {
        auto selected = table->selectedItems();
        if (!selected.isEmpty()) {
            auto save_id = table->item(selected.first()->row(), 0)->data(Qt::UserRole).toString().toStdString();
            view_model_->executeCommand(viewmodel::GameCommand::LoadGame, viewmodel::GameCommandArgs::load(save_id));
        }
    }
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity) — UI builder: statistics dialog with tabs and tables
void MainWindow::showStatisticsDialog() {
    if (!view_model_) {
        return;
    }

    // Probe the session history (a read latches an "unreadable" flag if the file
    // can't be decrypted/parsed). Only relevant when detailed stats are collected;
    // otherwise the file is neither read for display nor overwritten on exit.
    const bool show_detailed = settings_manager_ && settings_manager_->getSettings().collect_detailed_stats;
    std::vector<core::GameStats> recent;
    if (show_detailed) {
        recent = view_model_->getRecentGames(20);
        if (view_model_->hasUnreadableSessionHistory()) {
            auto answer = QMessageBox::question(
                this, qstr(core::loc("Sudoku", "Statistics")),
                qstr(core::loc("Sudoku",
                               "Your statistics history could not be read — it may have been encrypted on a "
                               "different system or written incompletely. Archive the unreadable file and start "
                               "a fresh history? The original file is kept and never deleted.")));
            if (answer == QMessageBox::Yes) {
                if (auto archived = view_model_->archiveUnreadableSessionHistory()) {
                    QMessageBox::information(
                        this, qstr(core::loc("Sudoku", "Statistics")),
                        qstr(core::locFormat(core::loc("Sudoku", "Old history archived to:\n{0}"), *archived)));
                }
                recent = view_model_->getRecentGames(20);  // refresh after archiving
            }
        }
    }

    auto maybe_stats = view_model_->getAggregateStats();
    const auto& display = view_model_->statistics.get();

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(qstr(core::loc("Sudoku", "Statistics")));
    dialog->setMinimumSize(520, 400);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto* tabs = new QTabWidget(dialog);

    // === Overview tab ===
    auto* overview_page = new QWidget();
    auto* overview_layout = new QFormLayout(overview_page);
    auto addStatRow = [&](const std::string& text) {
        overview_layout->addRow(new QLabel(QString::fromStdString(text)));
    };
    addStatRow(core::locFormat(core::loc("Sudoku", "Games Played: {0}"), display.games_played));
    addStatRow(core::locFormat(core::loc("Sudoku", "Games Completed: {0}"), display.games_completed));
    addStatRow(core::locFormat(core::loc("Sudoku", "Completion Rate: {0:.1f}%"), display.completion_rate));
    addStatRow(core::locFormat(core::loc("Sudoku", "Best Time: {0}"), display.best_time));
    addStatRow(core::locFormat(core::loc("Sudoku", "Average Time: {0}"), display.average_time));
    addStatRow(core::locFormat(core::loc("Sudoku", "Current Streak: {0}"), display.current_streak));
    addStatRow(core::locFormat(core::loc("Sudoku", "Best Streak: {0}"), display.best_streak));

    if (maybe_stats) {
        const auto& agg = *maybe_stats;
        overview_layout->addRow(new QLabel(""));  // spacer
        overview_layout->addRow(qstr(core::loc("Sudoku", "Total Moves")), new QLabel(QString::number(agg.total_moves)));
        overview_layout->addRow(qstr(core::loc("Sudoku", "Total Hints Used")),
                                new QLabel(QString::number(agg.total_hints)));
        overview_layout->addRow(qstr(core::loc("Sudoku", "Total Mistakes")),
                                new QLabel(QString::number(agg.total_mistakes)));
        overview_layout->addRow(
            qstr(core::loc("Sudoku", "Total Time Played")),
            new QLabel(QString::fromStdString(viewmodel::GameViewModel::formatDuration(agg.total_time_played))));
    }
    tabs->addTab(overview_page, qstr(core::loc("Sudoku", "Overview")));

    // === Per-difficulty tab ===
    if (maybe_stats) {
        const auto& agg = *maybe_stats;
        static constexpr auto NUM_DIFFICULTIES = static_cast<int>(core::DIFFICULTY_COUNT);
        static constexpr int NUM_METRICS = 5;

        auto* diff_page = new QWidget();
        auto* diff_layout = new QVBoxLayout(diff_page);
        auto* table = new QTableWidget(NUM_METRICS, NUM_DIFFICULTIES, diff_page);

        // Column headers: difficulty names
        QStringList col_headers;
        for (int d = 0; d < NUM_DIFFICULTIES; ++d) {
            col_headers << qstr(difficultyString(static_cast<core::Difficulty>(d)));
        }
        table->setHorizontalHeaderLabels(col_headers);

        // Row headers: metric names
        table->setVerticalHeaderLabels({qstr(core::loc("Sudoku", "Played")), qstr(core::loc("Sudoku", "Completed")),
                                        qstr(core::loc("Sudoku", "Best Time")), qstr(core::loc("Sudoku", "Avg Time")),
                                        qstr(core::loc("Sudoku", "Avg SE Rating"))});

        for (int d = 0; d < NUM_DIFFICULTIES; ++d) {
            table->setItem(0, d, new QTableWidgetItem(QString::number(agg.games_played[d])));
            table->setItem(1, d, new QTableWidgetItem(QString::number(agg.games_completed[d])));

            auto bt = agg.best_times[d];
            table->setItem(
                2, d,
                new QTableWidgetItem((bt != std::chrono::milliseconds::max() && agg.games_completed[d] > 0)
                                         ? QString::fromStdString(viewmodel::GameViewModel::formatDuration(bt))
                                         : qstr(core::loc("Sudoku", "N/A"))));

            auto at = agg.average_times[d];
            table->setItem(3, d,
                           new QTableWidgetItem(
                               at.count() > 0 ? QString::fromStdString(viewmodel::GameViewModel::formatDuration(at))
                                              : qstr(core::loc("Sudoku", "N/A"))));

            table->setItem(4, d,
                           new QTableWidgetItem(
                               agg.average_ratings[d] > 0.0
                                   ? QString::fromStdString(core::locFormat(
                                         core::loc("Sudoku", "SE {0}"), fmt::format("{:.1f}", agg.average_ratings[d])))
                                   : qstr(core::loc("Sudoku", "N/A"))));
        }

        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(true);
        table->resizeColumnsToContents();
        diff_layout->addWidget(table);
        tabs->addTab(diff_page, qstr(core::loc("Sudoku", "Per Difficulty")));
    }

    // === Recent Games tab (conditional on collect_detailed_stats) ===
    // show_detailed and recent were resolved above (with corrupt-history recovery).
    if (show_detailed) {
        if (!recent.empty()) {
            static constexpr int NUM_COLS = 6;
            auto* recent_page = new QWidget();
            auto* recent_layout = new QVBoxLayout(recent_page);
            auto* recent_table = new QTableWidget(static_cast<int>(recent.size()), NUM_COLS, recent_page);

            recent_table->setHorizontalHeaderLabels(
                {qstr(core::loc("Sudoku", "Date")), qstr(core::loc("Sudoku", "Difficulty")),
                 qstr(core::loc("Sudoku", "Time")), qstr(core::loc("Sudoku", "Rating")),
                 qstr(core::loc("Sudoku", "Moves")), qstr(core::loc("Sudoku", "Mistakes"))});

            for (int row = 0; std::cmp_less(row, recent.size()); ++row) {
                const auto& g = recent[row];
                auto tt = std::chrono::system_clock::to_time_t(g.end_time);
                recent_table->setItem(
                    row, 0,
                    new QTableWidgetItem(
                        QDateTime::fromSecsSinceEpoch(static_cast<qint64>(tt)).toString("yyyy-MM-dd")));
                recent_table->setItem(row, 1, new QTableWidgetItem(qstr(difficultyString(g.difficulty))));
                recent_table->setItem(row, 2,
                                      new QTableWidgetItem(QString::fromStdString(
                                          viewmodel::GameViewModel::formatDuration(g.time_played))));
                recent_table->setItem(row, 3,
                                      new QTableWidgetItem(g.puzzle_rating > 0.0
                                                               ? QString::fromStdString(core::locFormat(
                                                                     core::loc("Sudoku", "SE {0}"),
                                                                     fmt::format("{:.1f}", g.puzzle_rating)))
                                                               : qstr(core::loc("Sudoku", "N/A"))));
                recent_table->setItem(row, 4, new QTableWidgetItem(QString::number(g.moves_made)));
                recent_table->setItem(row, 5, new QTableWidgetItem(QString::number(g.mistakes)));
            }

            recent_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            recent_table->setSelectionBehavior(QAbstractItemView::SelectRows);
            recent_table->horizontalHeader()->setStretchLastSection(true);
            recent_table->resizeColumnsToContents();
            recent_layout->addWidget(recent_table);
            tabs->addTab(recent_page, qstr(core::loc("Sudoku", "Recent Games")));
        }
    }

    auto* main_layout = new QVBoxLayout(dialog);
    main_layout->addWidget(tabs);
    auto* btn_box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(btn_box, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    main_layout->addWidget(btn_box);

    dialog->exec();
}

void MainWindow::showAboutDialog() {
    QMessageBox::about(this, qstr(core::loc("Sudoku", "About")),
                       QString("%1\n\n%2\n\n"
                               "%3\n- Qt6\n- C++23\n\n"
                               "Copyright (C) 2025-2026 Alexander Bendlin")
                           .arg(qstr(core::loc("Sudoku", "Sudoku Game")),
                                qstr(core::loc("Sudoku", "A feature-rich offline Sudoku application.")),
                                qstr(core::loc("Sudoku", "Built with:"))));
}

void MainWindow::showThirdPartyLicensesDialog() {
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(qstr(core::loc("Sudoku", "Third-Party Licenses")));
    dialog->resize(600, 480);

    auto* text = new QTextEdit(dialog);
    text->setReadOnly(true);
    text->setLineWrapMode(QTextEdit::WidgetWidth);
    text->setHtml("<h3>Third-Party Libraries</h3>"
                  "<p>This application is built with the following open-source libraries:</p>"

                  "<h4>Qt6</h4>"
                  "<p>Version: 6.x &nbsp;|&nbsp; License: LGPL 3.0<br>"
                  "Copyright &copy; 2024 The Qt Company Ltd<br>"
                  "<a href=\"https://www.qt.io/\">https://www.qt.io/</a></p>"

                  "<h4>spdlog</h4>"
                  "<p>Version: 1.15.3 &nbsp;|&nbsp; License: MIT<br>"
                  "Copyright &copy; 2016 Gabi Melman<br>"
                  "Fast C++ logging library. Also bundles <b>fmt</b> (MIT) for string formatting.<br>"
                  "<a href=\"https://github.com/gabime/spdlog\">https://github.com/gabime/spdlog</a></p>"

                  "<h4>yaml-cpp</h4>"
                  "<p>Version: 0.8.0 &nbsp;|&nbsp; License: MIT<br>"
                  "Copyright &copy; 2008-2015 Jesse Beder<br>"
                  "YAML parser and emitter for C++ (used for save-game serialization).<br>"
                  "<a href=\"https://github.com/jbeder/yaml-cpp\">https://github.com/jbeder/yaml-cpp</a></p>"

                  "<h4>libsodium</h4>"
                  "<p>Version: 1.0.18 &nbsp;|&nbsp; License: ISC<br>"
                  "Copyright &copy; 2013-2024 Frank Denis<br>"
                  "Modern cryptographic library (used for save-game encryption).<br>"
                  "<a href=\"https://libsodium.gitbook.io/\">https://libsodium.gitbook.io/</a></p>"

                  "<h4>zlib</h4>"
                  "<p>Version: 1.3.1 &nbsp;|&nbsp; License: zlib License<br>"
                  "Compression library (used for save-game compression).<br>"
                  "<a href=\"https://zlib.net/\">https://zlib.net/</a></p>"

                  "<p><i>Full license texts and compatibility notes are in THIRD_PARTY_LICENSES.md "
                  "distributed with this software.</i></p>");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::accept);

    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(text);
    layout->addWidget(buttons);

    dialog->exec();
}

void MainWindow::showTechniquesDialog() {
    if (!view_model_) {
        return;
    }

    const auto& ui_state = view_model_->uiState.get();
    if (ui_state.puzzle_rating <= 0.0) {
        return;
    }

    QString text =
        QString::fromStdString(core::locFormat(core::loc("Sudoku", "Puzzle Rating: SE {0}"),
                                               QString::number(ui_state.puzzle_rating, 'f', 1).toStdString())) +
        "\n\n";

    const auto& techniques = ui_state.puzzle_techniques;
    if (!techniques.empty()) {
        text += QString("%1\n\n").arg(qstr(core::loc("Sudoku", "Techniques required to solve:")));
        for (const auto& tech : techniques) {
            text += QString("  %1\n").arg(QString::fromStdString(tech));
        }
    } else {
        text += qstr(core::loc("Sudoku", "No technique details available."));
    }

    QMessageBox::information(this, qstr(core::loc("Sudoku", "Puzzle Difficulty")), text);
}

void MainWindow::exportAggregateStatsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportAggregateStatsCsv();
    if (result) {
        toast_widget_->show(qstr(core::loc("Sudoku", "Aggregate stats exported to CSV")));
    } else {
        toast_widget_->show(
            QString::fromStdString(core::locFormat(core::loc("Sudoku", "Export failed: {0}"), result.error())));
    }
}

void MainWindow::exportGameSessionsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportGameSessionsCsv();
    if (result) {
        toast_widget_->show(qstr(core::loc("Sudoku", "Game sessions exported to CSV")));
    } else {
        toast_widget_->show(
            QString::fromStdString(core::locFormat(core::loc("Sudoku", "Export failed: {0}"), result.error())));
    }
}

std::string MainWindow::difficultyString(core::Difficulty difficulty) const {
    switch (difficulty) {
        case core::Difficulty::Easy:
            return core::loc("Sudoku", "Easy");
        case core::Difficulty::Medium:
            return core::loc("Sudoku", "Medium");
        case core::Difficulty::Hard:
            return core::loc("Sudoku", "Hard");
        case core::Difficulty::Expert:
            return core::loc("Sudoku", "Expert");
        case core::Difficulty::Master:
            return core::loc("Sudoku", "Master");
        default:
            return core::loc("Sudoku", "Unknown");
    }
}

void MainWindow::retranslateUi() {
    // Window title
    setWindowTitle(qstr(core::loc("Sudoku", "Sudoku")));

    // Menu bar: rebuild entirely (avoids storing ~15 action pointers)
    menuBar()->clear();
    setupMenuBar();

    // Toolbar labels
    new_game_btn_->setText(qstr(core::loc("Sudoku", "▶ New Game")));
    difficulty_label_->setText(QString(" %1 ").arg(qstr(core::loc("Sudoku", "Difficulty:"))));
    hints_text_label_->setText(QString(" %1 ").arg(qstr(core::loc("Sudoku", "Hints:"))));

    // Difficulty combo items
    difficulty_combo_->blockSignals(true);
    for (int i = 0; i < difficulty_combo_->count(); ++i) {
        difficulty_combo_->setItemText(i, qstr(difficultyString(static_cast<core::Difficulty>(i))));
    }
    difficulty_combo_->blockSignals(false);

    // Button panel
    undo_btn_->setText(qstr(core::loc("Sudoku", "Undo")));
    redo_btn_->setText(qstr(core::loc("Sudoku", "Redo")));
    undo_valid_btn_->setText(qstr(core::loc("Sudoku", "Undo Until Valid")));
    auto_notes_btn_->setText(auto_notes_btn_->isChecked() ? qstr(core::loc("Sudoku", "Clear Notes"))
                                                          : qstr(core::loc("Sudoku", "Fill Notes")));
    mode_btn_->setToolTip(qstr(core::loc("Sudoku", "Input mode (Space to cycle, N for Notes)")));

    // Status bar and mode button
    updateStatusBar();
    updateButtonPanel();
}

// NOLINTNEXTLINE(readability-function-size) — UI builder: settings dialog with tabs and signal wiring
void MainWindow::showSettingsDialog() {
    if (!settings_manager_) {
        return;
    }

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(qstr(core::loc("Sudoku", "Settings")));
    dialog->setMinimumWidth(400);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto* tabs = new QTabWidget(dialog);

    // === Gameplay Tab ===
    auto* gameplay_page = new QWidget();
    auto* gameplay_layout = new QFormLayout(gameplay_page);

    auto* max_hints_spin = new QSpinBox();
    max_hints_spin->setRange(1, 50);
    max_hints_spin->setValue(settings_manager_->getSettings().max_hints);
    gameplay_layout->addRow(qstr(core::loc("Sudoku", "Maximum Hints:")), max_hints_spin);

    auto* auto_save_spin = new QSpinBox();
    auto_save_spin->setRange(10, 300);
    auto_save_spin->setSuffix(qstr(core::loc("Sudoku", " seconds")));
    auto_save_spin->setValue(settings_manager_->getSettings().auto_save_interval_ms / 1000);
    gameplay_layout->addRow(qstr(core::loc("Sudoku", "Auto-save Interval:")), auto_save_spin);

    auto* difficulty_combo = new QComboBox();
    difficulty_combo->addItems({qstr(core::loc("Sudoku", "Easy")), qstr(core::loc("Sudoku", "Medium")),
                                qstr(core::loc("Sudoku", "Hard")), qstr(core::loc("Sudoku", "Expert")),
                                qstr(core::loc("Sudoku", "Master"))});
    difficulty_combo->setCurrentIndex(static_cast<int>(settings_manager_->getSettings().default_difficulty));
    gameplay_layout->addRow(qstr(core::loc("Sudoku", "Default Difficulty:")), difficulty_combo);

    tabs->addTab(gameplay_page, qstr(core::loc("Sudoku", "Gameplay")));

    // === Display Tab ===
    auto* display_page = new QWidget();
    auto* display_layout = new QVBoxLayout(display_page);

    auto* show_conflicts_cb = new QCheckBox(qstr(core::loc("Sudoku", "Highlight Conflicts")));
    show_conflicts_cb->setChecked(settings_manager_->getSettings().show_conflicts);
    display_layout->addWidget(show_conflicts_cb);

    auto* show_hints_cb = new QCheckBox(qstr(core::loc("Sudoku", "Show Hints")));
    show_hints_cb->setChecked(settings_manager_->getSettings().show_hints);
    display_layout->addWidget(show_hints_cb);

    auto* show_session_timer_cb = new QCheckBox(qstr(core::loc("Sudoku", "Show session timer (right of status bar)")));
    show_session_timer_cb->setChecked(settings_manager_->getSettings().show_session_timer);
    show_session_timer_cb->setToolTip(qstr(
        core::loc("Sudoku", "Display total time since the app launched, on the right side of the status bar. "
                            "Helpful as a reminder to take breaks. Independent of the per-puzzle timer on the left.")));
    display_layout->addWidget(show_session_timer_cb);

    // Language selection. The combo applies the new locale via setLanguage(),
    // which fires the settings observer -> applyLocale() -> LanguageChange
    // events on top-level widgets. This dialog itself was built once, so its
    // labels don't update live -- close and reopen to see them retranslated.
    {
        display_layout->addSpacing(10);
        auto* lang_layout = new QHBoxLayout();
        lang_layout->addWidget(new QLabel(qstr(core::loc("Sudoku", "Language"))));
        auto* lang_combo = new QComboBox();
        const auto locales = discoverInstalledLocales();
        int current_idx = 0;
        for (size_t i = 0; i < locales.size(); ++i) {
            lang_combo->addItem(locales[i].native_name, QString::fromStdString(locales[i].code));
            if (locales[i].code == settings_manager_->getSettings().language) {
                current_idx = static_cast<int>(i);
            }
        }
        lang_combo->setCurrentIndex(current_idx);
        lang_layout->addWidget(lang_combo);
        display_layout->addLayout(lang_layout);

        connect(lang_combo, &QComboBox::currentIndexChanged, this, [this, lang_combo](int idx) {
            auto code = lang_combo->itemData(idx).toString().toStdString();
            settings_manager_->setLanguage(code);
        });
    }

    display_layout->addStretch();
    tabs->addTab(display_page, qstr(core::loc("Sudoku", "Display")));

    // === Statistics Tab ===
    auto* stats_page = new QWidget();
    auto* stats_layout = new QVBoxLayout(stats_page);

    auto* collect_stats_cb = new QCheckBox(qstr(core::loc("Sudoku", "Collect detailed match statistics")));
    collect_stats_cb->setChecked(settings_manager_->getSettings().collect_detailed_stats);
    stats_layout->addWidget(collect_stats_cb);

    auto* encrypt_stats_cb = new QCheckBox(qstr(core::loc("Sudoku", "Encrypt session data")));
    encrypt_stats_cb->setChecked(settings_manager_->getSettings().encrypt_detailed_stats);
    encrypt_stats_cb->setToolTip(qstr(core::loc(
        "Sudoku", "Session data is encrypted by default for privacy. Disable to inspect the raw data file yourself.")));
    encrypt_stats_cb->setEnabled(settings_manager_->getSettings().collect_detailed_stats);
    stats_layout->addWidget(encrypt_stats_cb);

    stats_layout->addStretch();
    tabs->addTab(stats_page, qstr(core::loc("Sudoku", "Statistics")));

    // === Experimental Tab ===
    auto* experimental_page = new QWidget();
    auto* experimental_layout = new QVBoxLayout(experimental_page);

    auto* experimental_header = new QLabel(qstr(core::loc(
        "Sudoku",
        "These features are not part of the 1.0 stability commitment. Their UI, data formats, and behavior may change "
        "or be removed in any 1.x release. Disable if you prefer to stay on the stable surface.")));
    experimental_header->setWordWrap(true);
    QFont header_font = experimental_header->font();
    header_font.setItalic(true);
    experimental_header->setFont(header_font);
    experimental_layout->addWidget(experimental_header);
    experimental_layout->addSpacing(8);

    auto* training_mode_cb = new QCheckBox(qstr(core::loc("Sudoku", "Enable Training Mode")));
    training_mode_cb->setChecked(settings_manager_->getSettings().experimental_training_mode);
    training_mode_cb->setToolTip(
        qstr(core::loc("Sudoku", "Interactive technique trainer. When enabled, appears under Game → Training Mode.")));
    experimental_layout->addWidget(training_mode_cb);

    auto* coaching_hints_cb = new QCheckBox(qstr(core::loc("Sudoku", "Enable progressive coaching hints")));
    coaching_hints_cb->setChecked(settings_manager_->getSettings().experimental_coaching_hints);
    coaching_hints_cb->setToolTip(qstr(core::loc(
        "Sudoku",
        "Three-level guided hint with check/apply workflow. When enabled, appears under Help → Get Coaching Hint "
        "(Shift+H).")));
    experimental_layout->addWidget(coaching_hints_cb);

    experimental_layout->addStretch();
    tabs->addTab(experimental_page, qstr(core::loc("Sudoku", "Experimental")));

    // === Dialog layout ===
    auto* main_layout = new QVBoxLayout(dialog);
    main_layout->addWidget(tabs);
    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(button_box, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    main_layout->addWidget(button_box);

    // === Connect signals — apply immediately ===
    connect(max_hints_spin, &QSpinBox::valueChanged, this, [this](int v) { settings_manager_->setMaxHints(v); });

    connect(auto_save_spin, &QSpinBox::valueChanged, this,
            [this](int v) { settings_manager_->setAutoSaveInterval(v * 1000); });

    connect(difficulty_combo, &QComboBox::currentIndexChanged, this,
            [this](int idx) { settings_manager_->setDefaultDifficulty(static_cast<core::Difficulty>(idx)); });

    auto connectCheckBox = [this](QCheckBox* cb, auto callback) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(cb, &QCheckBox::checkStateChanged, this, [callback](Qt::CheckState s) { callback(s == Qt::Checked); });
#else
        connect(cb, &QCheckBox::stateChanged, this, [callback](int s) { callback(s == Qt::Checked); });
#endif
    };

    connectCheckBox(show_conflicts_cb, [this](bool checked) {
        settings_manager_->setShowConflicts(checked);
        if (view_model_) {
            view_model_->setShowConflicts(checked);
        }
    });

    connectCheckBox(show_hints_cb, [this](bool checked) {
        settings_manager_->setShowHints(checked);
        if (view_model_) {
            view_model_->setShowHints(checked);
        }
    });

    connectCheckBox(show_session_timer_cb, [this](bool checked) { settings_manager_->setShowSessionTimer(checked); });

    connectCheckBox(collect_stats_cb, [this, encrypt_stats_cb](bool checked) {
        encrypt_stats_cb->setEnabled(checked);
        if (!checked) {
            auto answer = QMessageBox::question(
                this, qstr(core::loc("Sudoku", "Settings")),
                qstr(core::loc("Sudoku",
                               "Disabling session tracking will stop recording per-game statistics. Would you like to "
                               "delete existing session history?")));
            if (answer == QMessageBox::Yes && view_model_) {
                view_model_->deleteSessionHistory();
            }
        }
        settings_manager_->setCollectDetailedStats(checked);
    });

    connectCheckBox(encrypt_stats_cb, [this](bool checked) { settings_manager_->setEncryptDetailedStats(checked); });

    connectCheckBox(training_mode_cb,
                    [this](bool checked) { settings_manager_->setExperimentalTrainingMode(checked); });

    connectCheckBox(coaching_hints_cb,
                    [this](bool checked) { settings_manager_->setExperimentalCoachingHints(checked); });

    dialog->exec();
}

}  // namespace sudoku::view
