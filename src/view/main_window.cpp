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

#include "main_window.h"

#include "../core/string_keys.h"
#include "core/constants.h"
#include "core/i_game_validator.h"
#include "core/i_localization_manager.h"
#include "core/observable.h"
#include "infrastructure/app_directory_manager.h"
#include "model/game_state.h"
#include "style_colors.h"
#include "sudoku_board_widget.h"
#include "toast_widget.h"
#include "training_widget.h"
#include "view_model/game_view_model.h"
#include "view_model/training_view_model.h"

#include <compare>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
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

using namespace core::StringKeys;

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Sudoku");
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

void MainWindow::setupCentralWidget() {
    central_stack_ = new QStackedWidget;

    board_widget_ = new SudokuBoardWidget;
    training_widget_ = new TrainingWidget;
    toast_widget_ = new ToastWidget(this);

    // Wrap board + button panel in a container
    auto* game_page = new QWidget;
    auto* game_layout = new QVBoxLayout(game_page);
    game_layout->setContentsMargins(0, 0, 0, 0);
    game_layout->addWidget(board_widget_, 1);

    // Button panel
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

    undo_btn_ = new QPushButton(qstr(loc(ButtonUndo)));
    redo_btn_ = new QPushButton(qstr(loc(ButtonRedo)));
    undo_valid_btn_ = new QPushButton(qstr(loc(ButtonUndoUntilValid)));
    auto_notes_btn_ = new QPushButton(qstr(loc(ButtonFillNotes)));
    mode_btn_ = new QPushButton(qstr(loc(ModeNormal)));
    mode_btn_->setToolTip(qstr(loc(TooltipInputMode)));

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

    central_stack_->addWidget(game_page);
    central_stack_->addWidget(training_widget_);
    central_stack_->setCurrentIndex(0);

    setCentralWidget(central_stack_);

    connect(training_widget_, &TrainingWidget::backToGame, this, [this]() { central_stack_->setCurrentIndex(0); });
}

void MainWindow::setupMenuBar() {
    auto* game_menu = menuBar()->addMenu(QString("&%1").arg(qstr(loc(MenuGame))));

    game_menu->addAction(QString("&%1").arg(qstr(loc(MenuNewGame))), QKeySequence("Ctrl+N"), this,
                         &MainWindow::showNewGameDialog);

    auto* reset_action = game_menu->addAction(QString("&%1").arg(qstr(loc(MenuResetPuzzle))), QKeySequence("Ctrl+R"),
                                              this, &MainWindow::showResetDialog);
    reset_action->setEnabled(false);

    game_menu->addSeparator();

    game_menu->addAction(QString("&%1").arg(qstr(loc(MenuSave))), QKeySequence("Ctrl+S"), this,
                         &MainWindow::showSaveDialog);
    game_menu->addAction(QString("&%1").arg(qstr(loc(MenuLoad))), QKeySequence("Ctrl+O"), this,
                         &MainWindow::showLoadDialog);

    game_menu->addSeparator();

    game_menu->addAction(QString("&%1").arg(qstr(loc(MenuTrainingMode))), this,
                         [this]() { central_stack_->setCurrentIndex(1); });

    game_menu->addAction(QString("&%1").arg(qstr(loc(MenuAnalyzePosition))), QKeySequence("F2"), this, [this]() {
        if (!view_model_ || !training_vm_) {
            return;
        }
        auto result = view_model_->analyzePosition();
        if (!result.has_value()) {
            if (toast_widget_) {
                toast_widget_->show(qstr(loc(ToastNoStrategies)));
            }
            return;
        }
        training_vm_->analyzePosition(result->board, result->given_board, result->candidate_masks,
                                      result->applicable_steps);
        central_stack_->setCurrentIndex(1);
    });

    game_menu->addAction(QString("&%1").arg(qstr(loc(MenuResumeGame))), this,
                         [this]() { central_stack_->setCurrentIndex(0); });

    game_menu->addSeparator();

    game_menu->addAction(qstr(loc(MenuStatistics)), this, &MainWindow::showStatisticsDialog);
    game_menu->addAction(qstr(loc(MenuExportAggregate)), this, &MainWindow::exportAggregateStatsCsv);
    game_menu->addAction(qstr(loc(MenuExportSessions)), this, &MainWindow::exportGameSessionsCsv);

    game_menu->addSeparator();
    game_menu->addAction(qstr(loc(MenuSettings)), QKeySequence("Ctrl+,"), this, &MainWindow::showSettingsDialog);
    game_menu->addSeparator();
    game_menu->addAction(QString("&%1").arg(qstr(loc(MenuExit))), QKeySequence("Alt+F4"), this, &QWidget::close);

    auto* edit_menu = menuBar()->addMenu(QString("&%1").arg(qstr(loc(MenuEdit))));
    edit_menu->addAction(QString("&%1").arg(qstr(loc(MenuUndo))), QKeySequence("Ctrl+Z"), this, [this]() {
        if (view_model_) {
            view_model_->undo();
            board_widget_->update();
        }
    });

    edit_menu->addAction(QString("&%1").arg(qstr(loc(MenuRedo))), QKeySequence("Ctrl+Y"), this, [this]() {
        if (view_model_) {
            view_model_->redo();
            board_widget_->update();
        }
    });

    edit_menu->addSeparator();
    edit_menu->addAction(QString("&%1").arg(qstr(loc(MenuClearCell))), QKeySequence("Delete"), this, [this]() {
        if (view_model_) {
            view_model_->clearSelectedCell();
            board_widget_->update();
        }
    });

    auto* help_menu = menuBar()->addMenu(QString("&%1").arg(qstr(loc(MenuHelp))));
    help_menu->addAction(qstr(loc(MenuGetHint)), QKeySequence("H"), this, [this]() {
        if (view_model_ && view_model_->getHintCount() > 0) {
            view_model_->getHint();
            board_widget_->update();
        }
    });
    help_menu->addSeparator();
    help_menu->addAction(QString("&%1").arg(qstr(loc(MenuAbout))), this, &MainWindow::showAboutDialog);
    help_menu->addAction(qstr(loc(MenuThirdPartyLicenses)), this, &MainWindow::showThirdPartyLicensesDialog);
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setStyleSheet(QString("QToolBar { background-color: %1; border-bottom: 1px solid %2; "
                                   "padding: 4px; spacing: 8px; }")
                               .arg(StyleColors::SURFACE, StyleColors::DIVIDER));

    new_game_btn_ = new QPushButton(qstr(loc(ToolbarNewGame)));
    new_game_btn_->setStyleSheet(
        QString("QPushButton { background-color: %1; color: white; padding: 6px 16px; border-radius: 4px; }"
                "QPushButton:hover { background-color: %2; }")
            .arg(StyleColors::PRIMARY, StyleColors::PRIMARY_DARK));
    connect(new_game_btn_, &QPushButton::clicked, this, &MainWindow::showNewGameDialog);
    toolbar->addWidget(new_game_btn_);

    toolbar->addSeparator();

    difficulty_label_ = new QLabel(QString(" %1 ").arg(qstr(loc(ToolbarDifficulty))));
    toolbar->addWidget(difficulty_label_);
    difficulty_combo_ = new QComboBox;
    difficulty_combo_->addItems({qstr(loc(DifficultyEasy)), qstr(loc(DifficultyMedium)), qstr(loc(DifficultyHard)),
                                 qstr(loc(DifficultyExpert)), qstr(loc(DifficultyMaster))});
    difficulty_combo_->setCurrentIndex(1);  // Medium
    toolbar->addWidget(difficulty_combo_);

    connect(difficulty_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!view_model_) {
            return;
        }
        auto result = QMessageBox::question(
            this, qstr(loc(DialogNewGame)),
            QString::fromStdString(locFormat(DialogNewGameConfirm, difficulty_combo_->currentText().toStdString())));
        if (result == QMessageBox::Yes) {
            view_model_->startNewGame(static_cast<core::Difficulty>(index));
        } else {
            // Revert combo without triggering signal again
            difficulty_combo_->blockSignals(true);
            difficulty_combo_->setCurrentIndex(last_difficulty_index_);
            difficulty_combo_->blockSignals(false);
        }
        last_difficulty_index_ = difficulty_combo_->currentIndex();
    });

    toolbar->addSeparator();

    hints_text_label_ = new QLabel(QString(" %1 ").arg(qstr(loc(ToolbarHints))));
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
}

void MainWindow::setupStatusBar() {
    status_label_ = new QLabel(qstr(loc(StatusReady)));
    statusBar()->addWidget(status_label_, 1);
    statusBar()->setStyleSheet(QString("QStatusBar { background-color: %1; border-top: 1px solid %2; color: %3; }")
                                   .arg(StyleColors::SURFACE_STATUS, StyleColors::DIVIDER, StyleColors::TEXT_MUTED));
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
    view_model_ = std::move(view_model);

    if (view_model_) {
        board_widget_->setViewModel(view_model_);

        // Connect button panel
        connect(undo_btn_, &QPushButton::clicked, this, [this]() {
            if (view_model_) {
                view_model_->undo();
                board_widget_->update();
            }
        });
        connect(redo_btn_, &QPushButton::clicked, this, [this]() {
            if (view_model_) {
                view_model_->redo();
                board_widget_->update();
            }
        });
        connect(undo_valid_btn_, &QPushButton::clicked, this, [this]() {
            if (view_model_) {
                view_model_->undoToLastValid();
                board_widget_->update();
            }
        });
        connect(auto_notes_btn_, &QPushButton::clicked, this, [this]() {
            if (view_model_) {
                view_model_->fillNotes();
                board_widget_->update();
            }
        });
        connect(mode_btn_, &QPushButton::clicked, this, [this]() {
            if (view_model_) {
                view_model_->cycleInputMode();
                updateButtonPanel();
                board_widget_->update();
            }
        });

        observer_.observe(view_model_->gameState, [this](const auto&) {
            board_widget_->update();
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

        spdlog::debug("ViewModel bound to MainWindow");
    }
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

void MainWindow::setLocalizationManager(std::shared_ptr<core::ILocalizationManager> loc_manager) {
    loc_manager_ = std::move(loc_manager);
    spdlog::debug("LocalizationManager bound to MainWindow (locale: {})", loc_manager_->getCurrentLocale());
    if (board_widget_) {
        board_widget_->setLocalizationManager(loc_manager_);
    }
    if (training_widget_) {
        training_widget_->setLocalizationManager(loc_manager_);
        // Re-bind training VM since rebuildPages() destroyed the old page widgets
        if (training_vm_) {
            training_widget_->setTrainingViewModel(training_vm_);
        }
    }
    retranslateUi();
}

void MainWindow::setSettingsManager(std::shared_ptr<core::ISettingsManager> settings_manager) {
    settings_manager_ = std::move(settings_manager);

    if (settings_manager_) {
        // Update auto-save interval from settings
        if (auto_save_timer_) {
            auto_save_timer_->setInterval(settings_manager_->getSettings().auto_save_interval_ms);
        }

        // Subscribe to settings changes
        settings_manager_->settingsObservable().subscribe([this](const core::Settings& s) {
            if (auto_save_timer_) {
                auto_save_timer_->setInterval(s.auto_save_interval_ms);
            }
            // Retranslate UI if language changed
            if (loc_manager_ && s.language != std::string(loc_manager_->getCurrentLocale())) {
                [[maybe_unused]] auto result = loc_manager_->setLocale(s.language);
                retranslateUi();
            }
        });
    }

    spdlog::debug("SettingsManager bound to MainWindow");
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

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity) — event handler with inherent branching
void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (!view_model_) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (event->isAutoRepeat()) {
        return;
    }

    int key = event->key();

    // Space cycles input mode (Normal → Notes → Color → Normal)
    if (key == Qt::Key_Space) {
        view_model_->cycleInputMode();
        updateButtonPanel();
        board_widget_->update();
        return;
    }

    // N key toggles Notes mode directly
    if (key == Qt::Key_N && event->modifiers() == Qt::NoModifier) {
        auto current = view_model_->getInputMode();
        view_model_->setInputMode(current == viewmodel::InputMode::Notes ? viewmodel::InputMode::Normal
                                                                         : viewmodel::InputMode::Notes);
        updateButtonPanel();
        board_widget_->update();
        return;
    }

    // Number keys 1-9: action depends on input mode
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        int number = key - Qt::Key_1 + 1;
        const auto& game_state = view_model_->gameState.get();
        if (!game_state.isTimerRunning()) {
            return;
        }
        auto pos_opt = game_state.getSelectedPosition();
        if (!pos_opt.has_value()) {
            return;
        }
        const auto& cell = game_state.getCell(pos_opt->row, pos_opt->col);
        if (cell.is_given) {
            return;
        }

        switch (view_model_->getInputMode()) {
            case viewmodel::InputMode::Normal:
                if (cell.value == 0) {
                    view_model_->enterNumber(number);
                } else if (cell.value == number) {
                    view_model_->clearSelectedCell();
                }
                break;
            case viewmodel::InputMode::Notes:
                if (cell.value == 0) {
                    view_model_->enterNote(number);
                }
                break;
            case viewmodel::InputMode::Color:
                if (number <= 6) {
                    view_model_->colorSelectedCell(static_cast<uint8_t>(number));
                }
                break;
        }
        board_widget_->update();
        return;
    }

    // Navigation
    const auto& game_state = view_model_->gameState.get();
    auto pos_opt = game_state.getSelectedPosition();

    if (pos_opt.has_value()) {
        const auto& pos = pos_opt.value();
        switch (key) {
            case Qt::Key_Up:
                view_model_->selectCell(pos.row > 0 ? pos.row - 1 : core::BOARD_SIZE - 1, pos.col);
                board_widget_->update();
                return;
            case Qt::Key_Down:
                view_model_->selectCell(pos.row < core::BOARD_SIZE - 1 ? pos.row + 1 : 0, pos.col);
                board_widget_->update();
                return;
            case Qt::Key_Left:
                view_model_->selectCell(pos.row, pos.col > 0 ? pos.col - 1 : core::BOARD_SIZE - 1);
                board_widget_->update();
                return;
            case Qt::Key_Right:
                view_model_->selectCell(pos.row, pos.col < core::BOARD_SIZE - 1 ? pos.col + 1 : 0);
                board_widget_->update();
                return;
            default:
                break;
        }
    }

    // Editing keys
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace || key == Qt::Key_0) {
        if (view_model_->getInputMode() == viewmodel::InputMode::Color) {
            view_model_->colorSelectedCell(0);  // Clear color
        } else {
            view_model_->clearSelectedCell();
        }
        board_widget_->update();
        return;
    }

    // Ctrl+Shift+Z = undo to last valid
    if (key == Qt::Key_Z && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        view_model_->undoToLastValid();
        board_widget_->update();
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
    if (!view_model_) {
        status_label_->setText(qstr(loc(StatusReady)));
        return;
    }

    const auto& game_state = view_model_->gameState.get();
    if (game_state.isComplete()) {
        status_label_->setText(QString("<span style='color: green;'>%1</span>").arg(qstr(loc(StatusCompleted))));
    } else if (game_state.isTimerRunning()) {
        status_label_->setText(qstr(loc(StatusPlaying)));
    } else {
        status_label_->setText(qstr(loc(StatusReady)));
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
            rating_btn_->setText(
                QString::fromStdString(locFormat(ToolbarRatingWithTechniques, rating_str, techniques.size())));
        } else {
            rating_btn_->setText(QString::fromStdString(locFormat(ToolbarRatingSimple, rating_str)));
        }
        rating_action_->setVisible(true);
    } else {
        rating_action_->setVisible(false);
    }
}

void MainWindow::updateButtonPanel() {
    if (!view_model_) {
        return;
    }

    undo_btn_->setEnabled(view_model_->canUndo());
    redo_btn_->setEnabled(view_model_->canRedo());

    // Update input mode indicator
    switch (view_model_->getInputMode()) {
        case viewmodel::InputMode::Normal:
            mode_btn_->setText(qstr(loc(ModeNormal)));
            break;
        case viewmodel::InputMode::Notes:
            mode_btn_->setText(qstr(loc(ModeNotes)));
            break;
        case viewmodel::InputMode::Color:
            mode_btn_->setText(qstr(loc(ModeColor)));
            break;
    }
}

// Dialog methods

void MainWindow::showNewGameDialog() {
    if (!view_model_) {
        return;
    }

    int selected = difficulty_combo_ ? difficulty_combo_->currentIndex() : 1;
    QString diff_name = difficulty_combo_ ? difficulty_combo_->currentText() : qstr(loc(DifficultyMedium));

    auto result =
        QMessageBox::question(this, qstr(loc(DialogNewGame)),
                              QString::fromStdString(locFormat(DialogNewGameConfirm, diff_name.toStdString())));

    if (result == QMessageBox::Yes) {
        view_model_->startNewGame(static_cast<core::Difficulty>(selected));
    }
}

void MainWindow::showResetDialog() {
    auto result = QMessageBox::warning(this, qstr(loc(DialogResetPuzzle)), qstr(loc(DialogResetWarning)),
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
    dialog.setWindowTitle(qstr(loc(DialogSaveGame)));
    dialog.setMinimumWidth(380);

    auto* layout = new QVBoxLayout(&dialog);

    // Current game info preview
    auto* info_group = new QGroupBox(qstr(loc(SavePreviewTitle)), &dialog);
    auto* info_layout = new QFormLayout(info_group);
    info_layout->addRow(qstr(loc(SavePreviewDifficulty)),
                        new QLabel(qstr(difficultyString(view_model_->gameState.get().getDifficulty()))));
    info_layout->addRow(qstr(loc(SavePreviewTime)),
                        new QLabel(QString::fromStdString(view_model_->getFormattedTime())));
    info_layout->addRow(qstr(loc(SavePreviewMoves)), new QLabel(QString::number(view_model_->getMoveCount())));
    info_layout->addRow(qstr(loc(SavePreviewMistakes)), new QLabel(QString::number(view_model_->getMistakeCount())));
    layout->addWidget(info_group);

    // Name input
    layout->addWidget(new QLabel(qstr(loc(DialogEnterSaveName))));
    auto* name_edit = new QLineEdit(&dialog);
    name_edit->setPlaceholderText(qstr(loc(SaveNamePlaceholder)));
    layout->addWidget(name_edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QString name = name_edit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(&dialog, qstr(loc(DialogSaveGame)), qstr(loc(SaveNameEmpty)));
            return;
        }

        // Check for existing save with same display_name
        auto existing = view_model_->getSaveList();
        bool name_exists =
            std::ranges::any_of(existing, [&](const auto& s) { return s.display_name == name.toStdString(); });
        if (name_exists) {
            auto confirm =
                QMessageBox::question(&dialog, qstr(loc(DialogSaveGame)),
                                      QString::fromStdString(locFormat(SaveOverwriteConfirm, name.toStdString())));
            if (confirm != QMessageBox::Yes) {
                return;
            }
        }

        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = name_edit->text().trimmed();
        bool success = view_model_->saveCurrentGame(name.toStdString());
        if (success && toast_widget_) {
            toast_widget_->show(qstr(loc(ToastGameSaved)));
        }
    }
}

void MainWindow::showLoadDialog() {
    if (!view_model_) {
        return;
    }

    auto saves = view_model_->getSaveList();

    QDialog dialog(this);
    dialog.setWindowTitle(qstr(loc(DialogLoadGame)));
    dialog.setMinimumSize(600, 350);

    auto* layout = new QVBoxLayout(&dialog);

    static constexpr int LOAD_COLS = 5;
    auto* table = new QTableWidget(static_cast<int>(saves.size()), LOAD_COLS, &dialog);
    table->setHorizontalHeaderLabels({qstr(loc(LoadColName)), qstr(loc(LoadColDifficulty)), qstr(loc(LoadColDate)),
                                      qstr(loc(LoadColTime)), qstr(loc(LoadColRating))});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);

    for (int row = 0; row < static_cast<int>(saves.size()); ++row) {
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
                       new QTableWidgetItem(s.puzzle_rating > 0.0 ? QString("SE %1").arg(s.puzzle_rating, 0, 'f', 1)
                                                                  : qstr(loc(StatsTimeNa))));
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
            view_model_->loadGame(save_id);
        }
    }
}

// NOLINTNEXTLINE(readability-function-size) — UI builder: statistics dialog with tabs and tables
void MainWindow::showStatisticsDialog() {
    if (!view_model_) {
        return;
    }

    auto maybe_stats = view_model_->getAggregateStats();
    const auto& display = view_model_->statistics.get();

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(qstr(loc(DialogStatistics)));
    dialog->setMinimumSize(520, 400);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto* tabs = new QTabWidget(dialog);

    // === Overview tab ===
    auto* overview_page = new QWidget();
    auto* overview_layout = new QFormLayout(overview_page);
    auto addStatRow = [&](std::string_view key, const auto& value) {
        overview_layout->addRow(new QLabel(QString::fromStdString(locFormat(key, value))));
    };
    addStatRow(StatsGamesPlayed, display.games_played);
    addStatRow(StatsGamesCompleted, display.games_completed);
    addStatRow(StatsCompletionRate, display.completion_rate);
    addStatRow(StatsBestTime, display.best_time);
    addStatRow(StatsAverageTime, display.average_time);
    addStatRow(StatsCurrentStreak, display.current_streak);
    addStatRow(StatsBestStreak, display.best_streak);

    if (maybe_stats) {
        const auto& agg = *maybe_stats;
        overview_layout->addRow(new QLabel(""));  // spacer
        overview_layout->addRow(qstr(loc(StatsTotalMoves)), new QLabel(QString::number(agg.total_moves)));
        overview_layout->addRow(qstr(loc(StatsTotalHints)), new QLabel(QString::number(agg.total_hints)));
        overview_layout->addRow(qstr(loc(StatsTotalMistakes)), new QLabel(QString::number(agg.total_mistakes)));
        overview_layout->addRow(
            qstr(loc(StatsTotalTime)),
            new QLabel(QString::fromStdString(viewmodel::GameViewModel::formatDuration(agg.total_time_played))));
    }
    tabs->addTab(overview_page, qstr(loc(StatsTabOverview)));

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
        table->setVerticalHeaderLabels({qstr(loc(StatsRowPlayed)), qstr(loc(StatsRowCompleted)),
                                        qstr(loc(StatsRowBestTime)), qstr(loc(StatsRowAvgTime)),
                                        qstr(loc(StatsRowAvgRating))});

        for (int d = 0; d < NUM_DIFFICULTIES; ++d) {
            table->setItem(0, d, new QTableWidgetItem(QString::number(agg.games_played[d])));
            table->setItem(1, d, new QTableWidgetItem(QString::number(agg.games_completed[d])));

            auto bt = agg.best_times[d];
            table->setItem(
                2, d,
                new QTableWidgetItem((bt != std::chrono::milliseconds::max() && agg.games_completed[d] > 0)
                                         ? QString::fromStdString(viewmodel::GameViewModel::formatDuration(bt))
                                         : qstr(loc(StatsTimeNa))));

            auto at = agg.average_times[d];
            table->setItem(3, d,
                           new QTableWidgetItem(
                               at.count() > 0 ? QString::fromStdString(viewmodel::GameViewModel::formatDuration(at))
                                              : qstr(loc(StatsTimeNa))));

            table->setItem(4, d,
                           new QTableWidgetItem(agg.average_ratings[d] > 0.0
                                                    ? QString("SE %1").arg(agg.average_ratings[d], 0, 'f', 1)
                                                    : qstr(loc(StatsTimeNa))));
        }

        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(true);
        table->resizeColumnsToContents();
        diff_layout->addWidget(table);
        tabs->addTab(diff_page, qstr(loc(StatsTabPerDifficulty)));
    }

    // === Recent Games tab (conditional on collect_detailed_stats) ===
    bool show_detailed = settings_manager_ && settings_manager_->getSettings().collect_detailed_stats;
    if (show_detailed) {
        auto recent = view_model_->getRecentGames(20);
        if (!recent.empty()) {
            static constexpr int NUM_COLS = 6;
            auto* recent_page = new QWidget();
            auto* recent_layout = new QVBoxLayout(recent_page);
            auto* recent_table = new QTableWidget(static_cast<int>(recent.size()), NUM_COLS, recent_page);

            recent_table->setHorizontalHeaderLabels({qstr(loc(StatsColDate)), qstr(loc(StatsColDifficulty)),
                                                     qstr(loc(StatsColTime)), qstr(loc(StatsColRating)),
                                                     qstr(loc(StatsColMoves)), qstr(loc(StatsColMistakes))});

            for (int row = 0; row < static_cast<int>(recent.size()); ++row) {
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
                                                               ? QString("SE %1").arg(g.puzzle_rating, 0, 'f', 1)
                                                               : "-"));
                recent_table->setItem(row, 4, new QTableWidgetItem(QString::number(g.moves_made)));
                recent_table->setItem(row, 5, new QTableWidgetItem(QString::number(g.mistakes)));
            }

            recent_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            recent_table->setSelectionBehavior(QAbstractItemView::SelectRows);
            recent_table->horizontalHeader()->setStretchLastSection(true);
            recent_table->resizeColumnsToContents();
            recent_layout->addWidget(recent_table);
            tabs->addTab(recent_page, qstr(loc(StatsTabRecentGames)));
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
    QMessageBox::about(this, qstr(loc(DialogAbout)),
                       QString("%1\n\n%2\n\n"
                               "%3\n- Qt6\n- C++23\n\n"
                               "Copyright (C) 2025-2026 Alexander Bendlin")
                           .arg(qstr(loc(AboutSudokuGame)), qstr(loc(AboutDescription)), qstr(loc(AboutBuiltWith))));
}

void MainWindow::showThirdPartyLicensesDialog() {
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(qstr(loc(DialogThirdPartyLicenses)));
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

    QString text = QString::fromStdString(
                       locFormat(DialogPuzzleRating, QString::number(ui_state.puzzle_rating, 'f', 1).toStdString())) +
                   "\n\n";

    const auto& techniques = ui_state.puzzle_techniques;
    if (!techniques.empty()) {
        text += QString("%1\n\n").arg(qstr(loc(DialogTechniquesRequired)));
        for (const auto& tech : techniques) {
            text += QString("  %1\n").arg(QString::fromStdString(tech));
        }
    } else {
        text += qstr(loc(DialogNoTechniqueDetails));
    }

    QMessageBox::information(this, qstr(loc(DialogPuzzleDifficulty)), text);
}

void MainWindow::exportAggregateStatsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportAggregateStatsCsv();
    if (result) {
        toast_widget_->show(qstr(loc(ToastAggregateExported)));
    } else {
        toast_widget_->show(QString::fromStdString(locFormat(ToastExportFailed, result.error())));
    }
}

void MainWindow::exportGameSessionsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportGameSessionsCsv();
    if (result) {
        toast_widget_->show(qstr(loc(ToastSessionsExported)));
    } else {
        toast_widget_->show(QString::fromStdString(locFormat(ToastExportFailed, result.error())));
    }
}

std::string_view MainWindow::difficultyString(core::Difficulty difficulty) const {
    switch (difficulty) {
        case core::Difficulty::Easy:
            return loc(DifficultyEasy);
        case core::Difficulty::Medium:
            return loc(DifficultyMedium);
        case core::Difficulty::Hard:
            return loc(DifficultyHard);
        case core::Difficulty::Expert:
            return loc(DifficultyExpert);
        case core::Difficulty::Master:
            return loc(DifficultyMaster);
        default:
            return loc(DifficultyUnknown);
    }
}

void MainWindow::retranslateUi() {
    // Window title
    setWindowTitle(qstr(loc(AppTitle)));

    // Menu bar: rebuild entirely (avoids storing ~15 action pointers)
    menuBar()->clear();
    setupMenuBar();

    // Toolbar labels
    new_game_btn_->setText(qstr(loc(ToolbarNewGame)));
    difficulty_label_->setText(QString(" %1 ").arg(qstr(loc(ToolbarDifficulty))));
    hints_text_label_->setText(QString(" %1 ").arg(qstr(loc(ToolbarHints))));

    // Difficulty combo items
    difficulty_combo_->blockSignals(true);
    for (int i = 0; i < difficulty_combo_->count(); ++i) {
        difficulty_combo_->setItemText(i, qstr(difficultyString(static_cast<core::Difficulty>(i))));
    }
    difficulty_combo_->blockSignals(false);

    // Button panel
    undo_btn_->setText(qstr(loc(ButtonUndo)));
    redo_btn_->setText(qstr(loc(ButtonRedo)));
    undo_valid_btn_->setText(qstr(loc(ButtonUndoUntilValid)));
    auto_notes_btn_->setText(qstr(loc(ButtonFillNotes)));
    mode_btn_->setToolTip(qstr(loc(TooltipInputMode)));

    // Training widget: rebuild pages with new locale, then re-bind VM
    if (training_widget_ && loc_manager_) {
        training_widget_->setLocalizationManager(loc_manager_);
        if (training_vm_) {
            training_widget_->setTrainingViewModel(training_vm_);
        }
    }

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
    dialog->setWindowTitle(qstr(loc(DialogSettings)));
    dialog->setMinimumWidth(400);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto* tabs = new QTabWidget(dialog);

    // === Gameplay Tab ===
    auto* gameplay_page = new QWidget();
    auto* gameplay_layout = new QFormLayout(gameplay_page);

    auto* max_hints_spin = new QSpinBox();
    max_hints_spin->setRange(1, 50);
    max_hints_spin->setValue(settings_manager_->getSettings().max_hints);
    gameplay_layout->addRow(qstr(loc(SettingsMaxHints)), max_hints_spin);

    auto* auto_save_spin = new QSpinBox();
    auto_save_spin->setRange(10, 300);
    auto_save_spin->setSuffix(qstr(loc(SettingsSecondsSuffix)));
    auto_save_spin->setValue(settings_manager_->getSettings().auto_save_interval_ms / 1000);
    gameplay_layout->addRow(qstr(loc(SettingsAutoSaveInterval)), auto_save_spin);

    auto* difficulty_combo = new QComboBox();
    difficulty_combo->addItems({qstr(loc(DifficultyEasy)), qstr(loc(DifficultyMedium)), qstr(loc(DifficultyHard)),
                                qstr(loc(DifficultyExpert)), qstr(loc(DifficultyMaster))});
    difficulty_combo->setCurrentIndex(static_cast<int>(settings_manager_->getSettings().default_difficulty));
    gameplay_layout->addRow(qstr(loc(SettingsDefaultDifficulty)), difficulty_combo);

    tabs->addTab(gameplay_page, qstr(loc(SettingsTabGameplay)));

    // === Display Tab ===
    auto* display_page = new QWidget();
    auto* display_layout = new QVBoxLayout(display_page);

    auto* show_conflicts_cb = new QCheckBox(qstr(loc(SettingsHighlightConflicts)));
    show_conflicts_cb->setChecked(settings_manager_->getSettings().show_conflicts);
    display_layout->addWidget(show_conflicts_cb);

    auto* show_hints_cb = new QCheckBox(qstr(loc(SettingsShowHints)));
    show_hints_cb->setChecked(settings_manager_->getSettings().show_hints);
    display_layout->addWidget(show_hints_cb);

    // Language selection
    if (loc_manager_) {
        display_layout->addSpacing(10);
        auto* lang_layout = new QHBoxLayout();
        lang_layout->addWidget(new QLabel(qstr(loc(SidebarLanguage))));
        auto* lang_combo = new QComboBox();
        auto locales = loc_manager_->getAvailableLocales();
        int current_idx = 0;
        for (size_t i = 0; i < locales.size(); ++i) {
            lang_combo->addItem(QString::fromStdString(locales[i].second), QString::fromStdString(locales[i].first));
            if (locales[i].first == settings_manager_->getSettings().language) {
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
    tabs->addTab(display_page, qstr(loc(SettingsTabDisplay)));

    // === Statistics Tab ===
    auto* stats_page = new QWidget();
    auto* stats_layout = new QVBoxLayout(stats_page);

    auto* collect_stats_cb = new QCheckBox(qstr(loc(SettingsCollectDetailedStats)));
    collect_stats_cb->setChecked(settings_manager_->getSettings().collect_detailed_stats);
    stats_layout->addWidget(collect_stats_cb);

    auto* encrypt_stats_cb = new QCheckBox(qstr(loc(SettingsEncryptDetailedStats)));
    encrypt_stats_cb->setChecked(settings_manager_->getSettings().encrypt_detailed_stats);
    encrypt_stats_cb->setToolTip(qstr(loc(SettingsEncryptDetailedStatsTooltip)));
    encrypt_stats_cb->setEnabled(settings_manager_->getSettings().collect_detailed_stats);
    stats_layout->addWidget(encrypt_stats_cb);

    stats_layout->addStretch();
    tabs->addTab(stats_page, qstr(loc(SettingsTabStatistics)));

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

    connectCheckBox(collect_stats_cb, [this, encrypt_stats_cb](bool checked) {
        encrypt_stats_cb->setEnabled(checked);
        if (!checked) {
            auto answer = QMessageBox::question(this, qstr(loc(DialogSettings)), qstr(loc(StatsDeletePrompt)));
            if (answer == QMessageBox::Yes && view_model_) {
                view_model_->deleteSessionHistory();
            }
        }
        settings_manager_->setCollectDetailedStats(checked);
    });

    connectCheckBox(encrypt_stats_cb, [this](bool checked) { settings_manager_->setEncryptDetailedStats(checked); });

    dialog->exec();
}

}  // namespace sudoku::view
