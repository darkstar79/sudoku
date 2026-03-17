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
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
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
    setStyleSheet("QMainWindow { background-color: #FAF8F0; }");

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
    button_panel->setStyleSheet("QWidget { background-color: #f5f5f5; border-top: 1px solid #ddd; }");
    auto* button_layout = new QHBoxLayout(button_panel);
    button_layout->setContentsMargins(20, 8, 20, 8);

    static constexpr auto* BTN_STYLE =
        "QPushButton { background-color: #e5e7eb; color: #374151; padding: 6px 14px; "
        "border-radius: 4px; border: 1px solid #d1d5db; }"
        "QPushButton:hover { background-color: #d1d5db; }"
        "QPushButton:disabled { color: #9ca3af; background-color: #f3f4f6; border-color: #e5e7eb; }"
        "QPushButton:checked { background-color: #2563eb; color: white; border-color: #1d4ed8; }";

    undo_btn_ = new QPushButton("Undo");
    redo_btn_ = new QPushButton("Redo");
    undo_valid_btn_ = new QPushButton("Undo Until Valid");
    auto_notes_btn_ = new QPushButton("Auto-Notes");
    auto_notes_btn_->setCheckable(true);

    undo_btn_->setStyleSheet(BTN_STYLE);
    redo_btn_->setStyleSheet(BTN_STYLE);
    undo_valid_btn_->setStyleSheet(BTN_STYLE);
    auto_notes_btn_->setStyleSheet(BTN_STYLE);

    button_layout->addStretch();
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
    auto* game_menu = menuBar()->addMenu("&Game");

    game_menu->addAction("&New Game", QKeySequence("Ctrl+N"), this, &MainWindow::showNewGameDialog);

    auto* reset_action =
        game_menu->addAction("&Reset Puzzle", QKeySequence("Ctrl+R"), this, &MainWindow::showResetDialog);
    reset_action->setEnabled(false);

    game_menu->addSeparator();

    game_menu->addAction("&Save", QKeySequence("Ctrl+S"), this, &MainWindow::showSaveDialog);
    game_menu->addAction("&Open/Load", QKeySequence("Ctrl+O"), this, &MainWindow::showLoadDialog);

    game_menu->addSeparator();

    game_menu->addAction("&Training Mode", this, [this]() { central_stack_->setCurrentIndex(1); });

    game_menu->addAction("Resume &Game", this, [this]() { central_stack_->setCurrentIndex(0); });

    game_menu->addSeparator();

    game_menu->addAction("S&tatistics", this, &MainWindow::showStatisticsDialog);
    game_menu->addAction("Export &Aggregate Stats to CSV", this, &MainWindow::exportAggregateStatsCsv);
    game_menu->addAction("Export Game &Sessions to CSV", this, &MainWindow::exportGameSessionsCsv);

    game_menu->addSeparator();
    game_menu->addAction("Se&ttings...", QKeySequence("Ctrl+,"), this, &MainWindow::showSettingsDialog);
    game_menu->addSeparator();
    game_menu->addAction("E&xit", QKeySequence("Alt+F4"), this, &QWidget::close);

    auto* edit_menu = menuBar()->addMenu("&Edit");
    edit_menu->addAction("&Undo", QKeySequence("Ctrl+Z"), this, [this]() {
        if (view_model_) {
            view_model_->undo();
            board_widget_->update();
        }
    });

    edit_menu->addAction("&Redo", QKeySequence("Ctrl+Y"), this, [this]() {
        if (view_model_) {
            view_model_->redo();
            board_widget_->update();
        }
    });

    edit_menu->addSeparator();
    edit_menu->addAction("&Clear Cell", QKeySequence("Delete"), this, [this]() {
        if (view_model_) {
            view_model_->clearSelectedCell();
            board_widget_->update();
        }
    });

    auto* help_menu = menuBar()->addMenu("&Help");
    help_menu->addAction("Get &Hint", QKeySequence("H"), this, [this]() {
        if (view_model_ && view_model_->getHintCount() > 0) {
            view_model_->getHint();
            board_widget_->update();
        }
    });
    help_menu->addSeparator();
    help_menu->addAction("&About", this, &MainWindow::showAboutDialog);
    help_menu->addAction("&Third-Party Licenses", this, &MainWindow::showThirdPartyLicensesDialog);
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setStyleSheet(
        "QToolBar { background-color: #f5f5f5; border-bottom: 1px solid #ddd; padding: 4px; spacing: 8px; }");

    auto* new_game_btn = new QPushButton("New Game");
    new_game_btn->setStyleSheet(
        "QPushButton { background-color: #2563eb; color: white; padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1d4ed8; }");
    connect(new_game_btn, &QPushButton::clicked, this, &MainWindow::showNewGameDialog);
    toolbar->addWidget(new_game_btn);

    toolbar->addSeparator();

    toolbar->addWidget(new QLabel(" Difficulty: "));
    difficulty_combo_ = new QComboBox;
    difficulty_combo_->addItems({"Easy", "Medium", "Hard", "Expert", "Master"});
    difficulty_combo_->setCurrentIndex(1);  // Medium
    toolbar->addWidget(difficulty_combo_);

    connect(difficulty_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!view_model_) {
            return;
        }
        auto result = QMessageBox::question(
            this, QString("New Game"),
            QString("Start a new %1 game?\nCurrent progress will be lost.").arg(difficulty_combo_->currentText()));
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

    toolbar->addWidget(new QLabel(" Hints: "));
    hints_label_ = new QLabel("10");
    hints_label_->setStyleSheet("background-color: #2563eb; color: white; padding: 2px 12px; border-radius: 12px;");
    toolbar->addWidget(hints_label_);

    toolbar->addSeparator();

    rating_btn_ = new QPushButton;
    rating_btn_->setFlat(true);
    rating_btn_->setCursor(Qt::PointingHandCursor);
    rating_btn_->setStyleSheet("QPushButton { border: none; padding: 2px 8px; text-decoration: underline; }"
                               "QPushButton:hover { color: #2563eb; }");
    connect(rating_btn_, &QPushButton::clicked, this, &MainWindow::showTechniquesDialog);
    rating_action_ = toolbar->addWidget(rating_btn_);
    rating_action_->setVisible(false);
}

void MainWindow::setupStatusBar() {
    status_label_ = new QLabel("Ready");
    statusBar()->addWidget(status_label_, 1);
    statusBar()->setStyleSheet("QStatusBar { background-color: #f0f0f0; border-top: 1px solid #ddd; color: #666; }");
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
                view_model_->toggleAutoNotes();
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

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (!view_model_) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (event->isAutoRepeat()) {
        return;
    }

    int key = event->key();

    // Number keys 1-9
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        handleNumberInput(key - Qt::Key_1 + 1);
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
        view_model_->clearSelectedCell();
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

void MainWindow::handleNumberInput(int number) {
    if (!view_model_) {
        return;
    }

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

    if (view_model_->isAutoNotesEnabled()) {
        if (cell.value == 0) {
            view_model_->enterNumber(number);
        } else if (cell.value == number) {
            view_model_->clearSelectedCell();
        }
        board_widget_->update();
        return;
    }

    // Double-press detection
    auto now = std::chrono::steady_clock::now();
    auto threshold = settings_manager_
                         ? std::chrono::milliseconds(settings_manager_->getSettings().double_press_threshold_ms)
                         : DEFAULT_DOUBLE_PRESS_THRESHOLD;
    bool is_double = (number == last_number_pressed_ && now - last_press_time_ < threshold);

    if (is_double) {
        view_model_->enterNumber(number);
        last_number_pressed_ = 0;
        last_press_time_ = {};
    } else {
        if (cell.value == 0) {
            view_model_->enterNote(number);
        } else if (cell.value == number && !cell.is_given) {
            view_model_->clearSelectedCell();
        }
        last_number_pressed_ = number;
        last_press_time_ = now;
    }
    board_widget_->update();
}

void MainWindow::updateStatusBar() {
    if (!view_model_) {
        status_label_->setText("Ready");
        return;
    }

    const auto& game_state = view_model_->gameState.get();
    if (game_state.isComplete()) {
        status_label_->setText("<span style='color: green;'>Completed!</span>");
    } else if (game_state.isTimerRunning()) {
        status_label_->setText("Playing...");
    } else {
        status_label_->setText("Ready");
    }
}

void MainWindow::updateToolBar() {
    if (!view_model_) {
        return;
    }

    int hint_count = view_model_->getHintCount();
    hints_label_->setText(QString::number(hint_count));

    const auto& ui_state = view_model_->uiState.get();
    if (ui_state.puzzle_rating > 0) {
        const auto& techniques = ui_state.puzzle_techniques;
        if (!techniques.empty()) {
            rating_btn_->setText(
                QString("Rating: %1 (%2 techniques)").arg(ui_state.puzzle_rating).arg(techniques.size()));
        } else {
            rating_btn_->setText(QString("Rating: %1").arg(ui_state.puzzle_rating));
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
    auto_notes_btn_->setChecked(view_model_->isAutoNotesEnabled());
}

// Dialog methods

void MainWindow::showNewGameDialog() {
    if (!view_model_) {
        return;
    }

    int selected = difficulty_combo_ ? difficulty_combo_->currentIndex() : 1;
    QString diff_name = difficulty_combo_ ? difficulty_combo_->currentText() : "Medium";

    auto result = QMessageBox::question(this, QString("New Game"),
                                        QString("Start a new %1 game?\nCurrent progress will be lost.").arg(diff_name));

    if (result == QMessageBox::Yes) {
        view_model_->startNewGame(static_cast<core::Difficulty>(selected));
    }
}

void MainWindow::showResetDialog() {
    auto result = QMessageBox::warning(this, "Reset Puzzle",
                                       "This will reset all your progress on the current puzzle. Are you sure?",
                                       QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (result == QMessageBox::Yes && view_model_) {
        view_model_->executeCommand(viewmodel::GameCommand::ResetGame);
    }
}

void MainWindow::showSaveDialog() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Save Game", "Enter save name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty() && view_model_) {
        bool success = view_model_->saveCurrentGame(name.toStdString());
        if (success) {
            toast_widget_->show("Game saved!");
        }
    }
}

void MainWindow::showLoadDialog() {
    if (!view_model_) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Load Game");
    dialog.setMinimumSize(300, 200);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Recent Saves:"));

    auto* list = new QListWidget;
    auto saves = view_model_->recentSaves.get();
    for (const auto& save : saves) {
        list->addItem(QString::fromStdString(save));
    }
    layout->addWidget(list);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted && list->currentItem() != nullptr) {
        view_model_->loadGame(list->currentItem()->text().toStdString());
    }
}

void MainWindow::showStatisticsDialog() {
    if (!view_model_) {
        return;
    }

    auto stats = view_model_->statistics.get();

    QString text = QString("Games Played: %1\nGames Completed: %2\nCompletion Rate: %3%\n"
                           "Best Time: %4\nAverage Time: %5\nCurrent Streak: %6\nBest Streak: %7")
                       .arg(stats.games_played)
                       .arg(stats.games_completed)
                       .arg(stats.completion_rate, 0, 'f', 1)
                       .arg(QString::fromStdString(stats.best_time))
                       .arg(QString::fromStdString(stats.average_time))
                       .arg(stats.current_streak)
                       .arg(stats.best_streak);

    QMessageBox::information(this, "Statistics", text);
}

void MainWindow::showAboutDialog() {
    QMessageBox::about(this, "About Sudoku",
                       "Sudoku Game\n\nA feature-rich offline Sudoku application.\n\n"
                       "Built with:\n- Qt6\n- C++23\n\n"
                       "Copyright (C) 2025-2026 Alexander Bendlin");
}

void MainWindow::showThirdPartyLicensesDialog() {
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle("Third-Party Licenses");
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
    if (ui_state.puzzle_rating <= 0) {
        return;
    }

    QString text = QString("Puzzle Rating: %1\n\n").arg(ui_state.puzzle_rating);

    const auto& techniques = ui_state.puzzle_techniques;
    if (!techniques.empty()) {
        text += "Techniques required to solve:\n\n";
        for (const auto& tech : techniques) {
            text += QString("  %1\n").arg(QString::fromStdString(tech));
        }
    } else {
        text += "No technique details available.";
    }

    QMessageBox::information(this, "Puzzle Difficulty", text);
}

void MainWindow::exportAggregateStatsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportAggregateStatsCsv();
    if (result) {
        toast_widget_->show("Aggregate stats exported to CSV!");
    } else {
        toast_widget_->show(QString("Export failed: %1").arg(QString::fromStdString(result.error())));
    }
}

void MainWindow::exportGameSessionsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportGameSessionsCsv();
    if (result) {
        toast_widget_->show("Game sessions exported to CSV!");
    } else {
        toast_widget_->show(QString("Export failed: %1").arg(QString::fromStdString(result.error())));
    }
}

const char* MainWindow::difficultyString(core::Difficulty difficulty) const {
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

// NOLINTNEXTLINE(readability-function-size) — UI builder: settings dialog with tabs and signal wiring
void MainWindow::showSettingsDialog() {
    if (!settings_manager_) {
        return;
    }

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle("Settings");
    dialog->setMinimumWidth(400);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto* tabs = new QTabWidget(dialog);

    // === Gameplay Tab ===
    auto* gameplay_page = new QWidget();
    auto* gameplay_layout = new QFormLayout(gameplay_page);

    auto* max_hints_spin = new QSpinBox();
    max_hints_spin->setRange(1, 50);
    max_hints_spin->setValue(settings_manager_->getSettings().max_hints);
    gameplay_layout->addRow("Maximum Hints:", max_hints_spin);

    auto* auto_save_spin = new QSpinBox();
    auto_save_spin->setRange(10, 300);
    auto_save_spin->setSuffix(" seconds");
    auto_save_spin->setValue(settings_manager_->getSettings().auto_save_interval_ms / 1000);
    gameplay_layout->addRow("Auto-save Interval:", auto_save_spin);

    auto* double_press_spin = new QSpinBox();
    double_press_spin->setRange(100, 1000);
    double_press_spin->setSuffix(" ms");
    double_press_spin->setSingleStep(50);
    double_press_spin->setValue(settings_manager_->getSettings().double_press_threshold_ms);
    gameplay_layout->addRow("Double-press Threshold:", double_press_spin);

    auto* difficulty_combo = new QComboBox();
    difficulty_combo->addItems({"Easy", "Medium", "Hard", "Expert", "Master"});
    difficulty_combo->setCurrentIndex(static_cast<int>(settings_manager_->getSettings().default_difficulty));
    gameplay_layout->addRow("Default Difficulty:", difficulty_combo);

    tabs->addTab(gameplay_page, "Gameplay");

    // === Display Tab ===
    auto* display_page = new QWidget();
    auto* display_layout = new QVBoxLayout(display_page);

    auto* show_conflicts_cb = new QCheckBox("Highlight Conflicts");
    show_conflicts_cb->setChecked(settings_manager_->getSettings().show_conflicts);
    display_layout->addWidget(show_conflicts_cb);

    auto* show_hints_cb = new QCheckBox("Show Hints");
    show_hints_cb->setChecked(settings_manager_->getSettings().show_hints);
    display_layout->addWidget(show_hints_cb);

    auto* auto_notes_cb = new QCheckBox("Auto Notes on Startup");
    auto_notes_cb->setChecked(settings_manager_->getSettings().auto_notes_on_startup);
    display_layout->addWidget(auto_notes_cb);

    // Language selection
    if (loc_manager_) {
        display_layout->addSpacing(10);
        auto* lang_layout = new QHBoxLayout();
        lang_layout->addWidget(new QLabel("Language:"));
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
            if (loc_manager_) {
                [[maybe_unused]] auto result = loc_manager_->setLocale(code);
            }
        });
    }

    display_layout->addStretch();
    tabs->addTab(display_page, "Display");

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

    connect(double_press_spin, &QSpinBox::valueChanged, this,
            [this](int v) { settings_manager_->setDoublePressThreshold(v); });

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

    connectCheckBox(auto_notes_cb, [this](bool checked) { settings_manager_->setAutoNotesOnStartup(checked); });

    dialog->exec();
}

}  // namespace sudoku::view
