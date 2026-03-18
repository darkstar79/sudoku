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

#include "training_widget.h"

#include "../core/solving_technique.h"
#include "core/i_training_statistics_manager.h"
#include "core/technique_descriptions.h"
#include "core/training_learning_path.h"
#include "core/training_types.h"
#include "training_board_widget.h"
#include "training_number_pad.h"
#include "view_model/training_view_model.h"

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include <QGraphicsOpacityEffect>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtCore/qobjectdefs.h>
#include <fmt/format.h>
#include <qnamespace.h>
#include <qstring.h>
#include <qstringalgorithms.h>

namespace {

/// Format a technique button label with mastery badge and recommendation marker
std::string formatTechniqueLabel(sudoku::core::SolvingTechnique technique, sudoku::core::MasteryLevel mastery,
                                 bool is_recommended) {
    auto name = sudoku::core::getTechniqueName(technique);
    auto points = sudoku::core::getTechniqueRating(technique);

    std::string badge;
    switch (mastery) {
        case sudoku::core::MasteryLevel::Beginner:
            break;
        case sudoku::core::MasteryLevel::Intermediate:
            badge = " [Intermediate]";
            break;
        case sudoku::core::MasteryLevel::Proficient:
            badge = " [Proficient]";
            break;
        case sudoku::core::MasteryLevel::Mastered:
            badge = " [Mastered]";
            break;
    }

    return fmt::format("{}{} ({} pts){}", is_recommended ? ">> " : "", name, points, badge);
}

}  // namespace

namespace sudoku::view {

TrainingWidget::TrainingWidget(QWidget* parent) : QWidget(parent), pages_(new QStackedWidget) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(pages_);

    // Create 5 pages for each training phase
    buildTechniqueSelectionPage();
    buildTheoryPage();
    buildExercisePage();
    buildFeedbackPage();
    buildLessonCompletePage();
}

void TrainingWidget::setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm) {
    training_vm_ = std::move(training_vm);
    if (training_vm_) {
        observer_.observe(training_vm_->trainingState, [this](const auto&) { refreshCurrentPage(); });
        observer_.observe(training_vm_->trainingBoard, [this](const auto&) { updateExerciseBoard(); });
        observer_.observe(training_vm_->feedbackBoard, [this](const auto&) {
            if (feedback_board_ && training_vm_) {
                feedback_board_->setBoard(training_vm_->feedbackBoard.get());
            }
        });
    }
}

void TrainingWidget::refreshCurrentPage() {
    if (!training_vm_) {
        return;
    }

    const auto& state = training_vm_->trainingState.get();

    int target = 0;
    switch (state.phase) {
        case core::TrainingPhase::TechniqueSelection:
            target = 0;
            break;
        case core::TrainingPhase::TheoryReview:
            target = 1;
            break;
        case core::TrainingPhase::Exercise:
            target = 2;
            break;
        case core::TrainingPhase::Feedback:
            target = 3;
            break;
        case core::TrainingPhase::LessonComplete:
            target = 4;
            break;
    }

    animatePageTransition(target);
}

// NOLINTNEXTLINE(readability-function-size) — UI builder: constructs grouped technique buttons with signal wiring
void TrainingWidget::buildTechniqueSelectionPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("<h1>Training Mode</h1>");
    layout->addWidget(title);
    layout->addWidget(new QLabel("Select a technique to practice:"));

    // clang-format off
    struct TechniqueGroup {
        const char* name;
        std::vector<core::SolvingTechnique> techniques;
    };

    std::array<TechniqueGroup, 9> groups = {{
        {.name = "Foundations", .techniques = {core::SolvingTechnique::NakedSingle, core::SolvingTechnique::HiddenSingle}},
        {.name = "Subset Basics", .techniques = {core::SolvingTechnique::NakedPair, core::SolvingTechnique::NakedTriple,
                           core::SolvingTechnique::HiddenPair, core::SolvingTechnique::HiddenTriple}},
        {.name = "Intersections & Quads", .techniques = {core::SolvingTechnique::PointingPair, core::SolvingTechnique::BoxLineReduction,
                                    core::SolvingTechnique::NakedQuad, core::SolvingTechnique::HiddenQuad}},
        {.name = "Basic Fish & Wings", .techniques = {core::SolvingTechnique::XWing, core::SolvingTechnique::XYWing,
                                 core::SolvingTechnique::Swordfish, core::SolvingTechnique::Skyscraper,
                                 core::SolvingTechnique::TwoStringKite, core::SolvingTechnique::XYZWing}},
        {.name = "Links & Rectangles", .techniques = {core::SolvingTechnique::UniqueRectangle, core::SolvingTechnique::WWing,
                                 core::SolvingTechnique::SimpleColoring, core::SolvingTechnique::FinnedXWing,
                                 core::SolvingTechnique::RemotePairs, core::SolvingTechnique::BUG}},
        {.name = "Advanced Fish & Wings", .techniques = {core::SolvingTechnique::Jellyfish, core::SolvingTechnique::FinnedSwordfish,
                                    core::SolvingTechnique::EmptyRectangle, core::SolvingTechnique::WXYZWing,
                                    core::SolvingTechnique::MultiColoring}},
        {.name = "Advanced Fish (Finned)", .techniques = {core::SolvingTechnique::FinnedJellyfish}},
        {.name = "Chains & Set Logic", .techniques = {core::SolvingTechnique::XYChain, core::SolvingTechnique::ALSxZ,
                                 core::SolvingTechnique::SueDeCoq}},
        {.name = "Inference Engines", .techniques = {core::SolvingTechnique::ForcingChain, core::SolvingTechnique::NiceLoop}},
    }};
    // clang-format on

    for (const auto& group : groups) {
        auto* group_box = new QGroupBox(group.name);
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) — Qt parent (group_box) takes ownership
        auto* group_layout = new QVBoxLayout(group_box);

        for (auto technique : group.techniques) {
            auto name = core::getTechniqueName(technique);
            auto points = core::getTechniqueRating(technique);
            auto label = fmt::format("{} ({} pts)", name, points);

            auto* btn = new QPushButton(QString::fromStdString(label));
            btn->setFlat(true);
            btn->setStyleSheet("text-align: left; padding: 4px 8px;");
            btn->setProperty("technique", static_cast<int>(technique));
            connect(btn, &QPushButton::clicked, this, [this, technique]() {
                if (training_vm_) {
                    training_vm_->selectTechnique(technique);
                }
            });
            group_layout->addWidget(btn);
        }

        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) — group_layout owned by group_box via Qt parent
        layout->addWidget(group_box);
    }

    auto* back_btn = new QPushButton("Back to Game");
    connect(back_btn, &QPushButton::clicked, this, &TrainingWidget::backToGame);
    layout->addWidget(back_btn);

    layout->addStretch();

    // Update mastery badges and recommended technique when selection page is shown
    connect(pages_, &QStackedWidget::currentChanged, this, [this, page](int index) {
        if (index != 0 || !training_vm_) {
            return;
        }
        auto stats_mgr = training_vm_->statsManager();
        if (!stats_mgr) {
            return;
        }

        auto recommended = core::getRecommendedTechnique(*stats_mgr);

        // Find all technique buttons and update their labels
        auto buttons = page->findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            bool ok = false;
            int tech_int = btn->property("technique").toInt(&ok);
            if (!ok) {
                continue;
            }
            auto technique = static_cast<core::SolvingTechnique>(tech_int);
            auto mastery = stats_mgr->getMastery(technique);
            bool is_recommended = recommended.has_value() && *recommended == technique;

            btn->setText(QString::fromStdString(formatTechniqueLabel(technique, mastery, is_recommended)));

            bool prereqs_met = core::arePrerequisitesMet(technique, *stats_mgr);
            btn->setEnabled(prereqs_met);
            if (!prereqs_met) {
                btn->setToolTip("Prerequisites not met");
            } else if (is_recommended) {
                btn->setToolTip("Recommended next technique");
            } else {
                btn->setToolTip("");
            }
        }
    });

    // Wrap in scroll area
    auto* scroll = new QScrollArea;
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    pages_->addWidget(scroll);
}

void TrainingWidget::buildTheoryPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title_label = new QLabel;
    title_label->setObjectName("theoryTitle");
    title_label->setStyleSheet("font-size: 24px; font-weight: bold;");
    layout->addWidget(title_label);

    auto* points_label = new QLabel;
    points_label->setObjectName("theoryPoints");
    points_label->setStyleSheet("color: #666;");
    layout->addWidget(points_label);

    auto* prereqs_label = new QLabel;
    prereqs_label->setObjectName("theoryPrereqs");
    prereqs_label->setWordWrap(true);
    prereqs_label->setStyleSheet("color: #555; font-style: italic;");
    layout->addWidget(prereqs_label);

    layout->addWidget(new QLabel("<b>What It Is:</b>"));
    auto* what_label = new QLabel;
    what_label->setObjectName("theoryWhat");
    what_label->setWordWrap(true);
    layout->addWidget(what_label);

    layout->addWidget(new QLabel("<b>What to Look For:</b>"));
    auto* look_label = new QLabel;
    look_label->setObjectName("theoryLook");
    look_label->setWordWrap(true);
    layout->addWidget(look_label);

    auto* btn_layout = new QHBoxLayout;
    auto* start_btn = new QPushButton("Start Exercises");
    auto* back_btn = new QPushButton("Back");
    btn_layout->addWidget(start_btn);
    btn_layout->addWidget(back_btn);
    btn_layout->addStretch();
    layout->addLayout(btn_layout);
    layout->addStretch();

    connect(start_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->startExercises();
        }
    });
    connect(back_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });

    // Dynamic content update via showEvent-like mechanism
    connect(pages_, &QStackedWidget::currentChanged, this,
            [this, title_label, points_label, prereqs_label, what_label, look_label](int index) {
                if (index != 1 || !training_vm_) {
                    return;
                }
                auto desc = training_vm_->currentDescription();
                const auto& state = training_vm_->trainingState.get();
                auto points = core::getTechniqueRating(state.current_technique);

                title_label->setText(QString::fromUtf8(desc.title.data(), static_cast<int>(desc.title.size())));
                points_label->setText(QString("%1 difficulty points").arg(points));

                auto prereqs = core::getPrerequisites(state.current_technique);
                if (prereqs.empty()) {
                    prereqs_label->hide();
                } else {
                    std::string prereq_text = "Prerequisites: ";
                    for (size_t i = 0; i < prereqs.size(); ++i) {
                        if (i > 0) {
                            prereq_text += ", ";
                        }
                        prereq_text += core::getTechniqueName(prereqs[i].prerequisite);
                    }
                    prereqs_label->setText(QString::fromStdString(prereq_text));
                    prereqs_label->show();
                }

                what_label->setText(
                    QString::fromUtf8(desc.what_it_is.data(), static_cast<int>(desc.what_it_is.size())));
                look_label->setText(
                    QString::fromUtf8(desc.what_to_look_for.data(), static_cast<int>(desc.what_to_look_for.size())));
            });

    pages_->addWidget(page);
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity) — UI builder: widget tree + signal wiring
void TrainingWidget::buildExercisePage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* header = new QLabel;
    header->setObjectName("exerciseHeader");
    layout->addWidget(header);

    // Interactive training board
    training_board_ = new TrainingBoardWidget;
    layout->addWidget(training_board_, 1);

    // Number pad
    number_pad_ = new TrainingNumberPad;
    layout->addWidget(number_pad_);

    // Color palette (hidden by default, shown only for Coloring exercises)
    color_palette_ = new QWidget;
    auto* color_layout = new QHBoxLayout(color_palette_);
    color_layout->setContentsMargins(0, 0, 0, 0);
    auto* color_label = new QLabel("Color:");
    color_a_btn_ = new QPushButton("A");
    color_a_btn_->setFixedSize(44, 44);
    color_a_btn_->setStyleSheet("background-color: #a0c4ff; font-weight: bold; font-size: 16px;");
    color_a_btn_->setCheckable(true);
    color_a_btn_->setChecked(true);
    color_b_btn_ = new QPushButton("B");
    color_b_btn_->setFixedSize(44, 44);
    color_b_btn_->setStyleSheet("background-color: #a0ffcc; font-weight: bold; font-size: 16px;");
    color_b_btn_->setCheckable(true);
    color_layout->addWidget(color_label);
    color_layout->addWidget(color_a_btn_);
    color_layout->addWidget(color_b_btn_);
    color_layout->addStretch();
    color_palette_->setVisible(false);
    layout->addWidget(color_palette_);

    // Hint text label (shown when player requests hints)
    auto* hint_label = new QLabel;
    hint_label->setObjectName("exerciseHint");
    hint_label->setWordWrap(true);
    hint_label->setStyleSheet("color: #0052cc; font-style: italic; padding: 4px 0;");
    hint_label->setVisible(false);
    layout->addWidget(hint_label);

    // Action buttons
    auto* btn_layout = new QHBoxLayout;
    undo_btn_ = new QPushButton("Undo");
    redo_btn_ = new QPushButton("Redo");
    undo_btn_->setEnabled(false);
    redo_btn_->setEnabled(false);
    auto* submit_btn = new QPushButton("Submit");
    auto* hint_btn = new QPushButton("Hint");
    auto* skip_btn = new QPushButton("Skip");
    auto* quit_btn = new QPushButton("Quit Lesson");
    btn_layout->addWidget(undo_btn_);
    btn_layout->addWidget(redo_btn_);
    btn_layout->addWidget(submit_btn);
    btn_layout->addWidget(hint_btn);
    btn_layout->addWidget(skip_btn);
    btn_layout->addStretch();
    btn_layout->addWidget(quit_btn);
    layout->addLayout(btn_layout);

    // Keyboard shortcuts for undo/redo
    // Keyboard shortcuts — Qt parent (this) takes ownership
    auto* undo_shortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undo_shortcut, &QShortcut::activated, this, [this]() {
        if (training_vm_) {
            training_vm_->undo();
            undo_btn_->setEnabled(training_vm_->canUndo());
            redo_btn_->setEnabled(training_vm_->canRedo());
        }
    });
    auto* redo_shortcut = new QShortcut(QKeySequence::Redo, this);
    connect(redo_shortcut, &QShortcut::activated, this, [this]() {
        if (training_vm_) {
            training_vm_->redo();
            undo_btn_->setEnabled(training_vm_->canUndo());
            redo_btn_->setEnabled(training_vm_->canRedo());
        }
    });

    // --- Connections ---

    // Number pad → board
    connect(number_pad_, &TrainingNumberPad::numberPressed, this, [this](int value) {
        if (!training_vm_ || !training_board_) {
            return;
        }
        const auto& exercises = training_vm_->exercises();
        auto idx = static_cast<size_t>(training_vm_->trainingState.get().exercise_index);
        if (idx >= exercises.size()) {
            return;
        }
        training_board_->applyNumber(value, exercises[idx].interaction_mode);
    });

    // Color palette mutual exclusivity + board coloring
    connect(color_a_btn_, &QPushButton::clicked, this, [this]() {
        color_a_btn_->setChecked(true);
        color_b_btn_->setChecked(false);
    });
    connect(color_b_btn_, &QPushButton::clicked, this, [this]() {
        color_a_btn_->setChecked(false);
        color_b_btn_->setChecked(true);
    });

    // Cell selection → apply color (in coloring mode) or update number pad
    connect(training_board_, &TrainingBoardWidget::cellSelected, this, [this](size_t row, size_t col) {
        if (!training_vm_) {
            return;
        }
        const auto& exercises = training_vm_->exercises();
        auto idx = static_cast<size_t>(training_vm_->trainingState.get().exercise_index);
        if (idx >= exercises.size()) {
            return;
        }

        if (exercises[idx].interaction_mode == core::TrainingInteractionMode::Coloring) {
            int color = color_a_btn_->isChecked() ? 1 : 2;
            training_board_->applyColor(color);
        }

        // Update number pad enabled state
        const auto& cell = training_board_->board()[row][col];
        number_pad_->updateEnabledNumbers(cell.candidates);
    });

    // Board changes → record in undo history and update ViewModel
    connect(training_board_, &TrainingBoardWidget::boardChanged, this, [this](const core::TrainingBoard& board) {
        if (training_vm_) {
            training_vm_->recordBoardChange(board);
            undo_btn_->setEnabled(training_vm_->canUndo());
            redo_btn_->setEnabled(training_vm_->canRedo());
        }
    });

    // Undo/Redo buttons
    connect(undo_btn_, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->undo();
            undo_btn_->setEnabled(training_vm_->canUndo());
            redo_btn_->setEnabled(training_vm_->canRedo());
        }
    });
    connect(redo_btn_, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->redo();
            undo_btn_->setEnabled(training_vm_->canUndo());
            redo_btn_->setEnabled(training_vm_->canRedo());
        }
    });

    // Submit/Hint/Skip/Quit
    connect(submit_btn, &QPushButton::clicked, this, [this, hint_label]() {
        if (training_vm_ && training_board_) {
            training_vm_->submitAnswer(training_board_->board());
            // If still in Exercise phase, a correct step was applied — show message
            const auto& state = training_vm_->trainingState.get();
            if (state.phase == core::TrainingPhase::Exercise && !state.found_step_message.empty()) {
                hint_label->setText(QString::fromStdString(state.found_step_message));
                hint_label->setStyleSheet("color: #2e7d32; font-weight: bold; padding: 4px 0;");
                hint_label->setVisible(true);
            }
        }
    });
    connect(hint_btn, &QPushButton::clicked, this, [this, hint_label]() {
        if (training_vm_) {
            training_vm_->requestHint();
            const auto& state = training_vm_->trainingState.get();
            if (state.current_hint_level > 0) {
                hint_label->setText(QString::fromStdString(state.feedback_message));
                hint_label->setVisible(true);
            }
        }
    });
    connect(skip_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->skipExercise();
        }
    });
    connect(quit_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });

    // Update header when exercise page is shown
    connect(pages_, &QStackedWidget::currentChanged, this, [this, header, hint_label](int index) {
        if (index != 2 || !training_vm_) {
            return;
        }
        const auto& state = training_vm_->trainingState.get();
        auto name = core::getTechniqueName(state.current_technique);
        header->setText(QString("Exercise %1 / %2  -  %3")
                            .arg(state.exercise_index + 1)
                            .arg(state.total_exercises)
                            .arg(QString::fromUtf8(name.data(), static_cast<int>(name.size()))));

        // Show/hide color palette based on interaction mode
        const auto& exercises = training_vm_->exercises();
        auto idx = static_cast<size_t>(state.exercise_index);
        if (idx < exercises.size()) {
            bool is_coloring = exercises[idx].interaction_mode == core::TrainingInteractionMode::Coloring;
            color_palette_->setVisible(is_coloring);
            number_pad_->setVisible(!is_coloring);
            number_pad_->setInteractionMode(exercises[idx].interaction_mode);
        }

        // Reset hint label and undo/redo buttons for new exercise
        hint_label->setVisible(false);
        hint_label->clear();
        undo_btn_->setEnabled(training_vm_->canUndo());
        redo_btn_->setEnabled(training_vm_->canRedo());
    });

    pages_->addWidget(page);
}

void TrainingWidget::updateExerciseBoard() {
    if (!training_vm_ || !training_board_) {
        return;
    }
    const auto& state = training_vm_->trainingState.get();
    if (state.phase != core::TrainingPhase::Exercise) {
        return;
    }
    training_board_->setBoard(training_vm_->trainingBoard.get());
}

void TrainingWidget::buildFeedbackPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* result_label = new QLabel;
    result_label->setObjectName("feedbackResult");
    result_label->setStyleSheet("font-size: 24px; font-weight: bold;");
    layout->addWidget(result_label);

    auto* msg_label = new QLabel;
    msg_label->setObjectName("feedbackMessage");
    msg_label->setWordWrap(true);
    layout->addWidget(msg_label);

    auto* score_label = new QLabel;
    score_label->setObjectName("feedbackScore");
    layout->addWidget(score_label);

    // Read-only diff board showing correct/wrong/missed highlights
    feedback_board_ = new TrainingBoardWidget;
    feedback_board_->setReadOnly(true);
    layout->addWidget(feedback_board_, 1);

    auto* btn_layout = new QHBoxLayout;
    auto* next_btn = new QPushButton("Next Exercise");
    auto* retry_btn = new QPushButton("Retry");
    auto* solution_btn = new QPushButton("Show Solution");
    auto* quit_btn = new QPushButton("Quit Lesson");
    btn_layout->addWidget(next_btn);
    btn_layout->addWidget(retry_btn);
    btn_layout->addWidget(solution_btn);
    btn_layout->addStretch();
    btn_layout->addWidget(quit_btn);
    layout->addLayout(btn_layout);

    connect(next_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->nextExercise();
        }
    });
    connect(retry_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->retryExercise();
        }
    });
    connect(solution_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->revealSolution();
        }
    });
    connect(quit_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });

    connect(pages_, &QStackedWidget::currentChanged, this, [this, result_label, msg_label, score_label](int index) {
        if (index != 3 || !training_vm_) {
            return;
        }
        const auto& state = training_vm_->trainingState.get();

        QString result_text;
        QString color;
        switch (state.last_result) {
            case core::AnswerResult::Correct:
                result_text = "Correct!";
                color = "color: green;";
                break;
            case core::AnswerResult::PartiallyCorrect:
                result_text = "Partially Correct";
                color = "color: #cc9900;";
                break;
            case core::AnswerResult::Incorrect:
                result_text = "Incorrect";
                color = "color: red;";
                break;
        }

        result_label->setText(result_text);
        result_label->setStyleSheet(QString("font-size: 24px; font-weight: bold; %1").arg(color));
        msg_label->setText(QString::fromStdString(state.feedback_message));
        score_label->setText(QString("Score: %1 / %2").arg(state.correct_count).arg(state.exercise_index + 1));

        // Update feedback board with diff highlights
        if (feedback_board_) {
            feedback_board_->setBoard(training_vm_->feedbackBoard.get());
        }
    });

    pages_->addWidget(page);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — UI builder with mastery level display; nesting is inherent
void TrainingWidget::buildLessonCompletePage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("<h1>Lesson Complete!</h1>");
    layout->addWidget(title);

    auto* info_label = new QLabel;
    info_label->setObjectName("completeInfo");
    layout->addWidget(info_label);

    auto* verdict_label = new QLabel;
    verdict_label->setObjectName("completeVerdict");
    layout->addWidget(verdict_label);

    auto* btn_layout = new QHBoxLayout;
    auto* again_btn = new QPushButton("Try Again");
    auto* pick_btn = new QPushButton("Pick Technique");
    auto* game_btn = new QPushButton("Return to Game");
    btn_layout->addWidget(again_btn);
    btn_layout->addWidget(pick_btn);
    btn_layout->addWidget(game_btn);
    btn_layout->addStretch();
    layout->addLayout(btn_layout);
    layout->addStretch();

    connect(again_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->selectTechnique(training_vm_->trainingState.get().current_technique);
        }
    });
    connect(pick_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });
    connect(game_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
        emit backToGame();
    });

    connect(pages_, &QStackedWidget::currentChanged, this, [this, info_label, verdict_label](int index) {
        if (index != 4 || !training_vm_) {
            return;
        }
        const auto& state = training_vm_->trainingState.get();
        auto name = core::getTechniqueName(state.current_technique);

        info_label->setText(QString("Technique: %1\nScore: %2 / %3 correct\nHints used: %4")
                                .arg(QString::fromUtf8(name.data(), static_cast<int>(name.size())))
                                .arg(state.correct_count)
                                .arg(state.total_exercises)
                                .arg(state.hints_used));

        // Show mastery level if stats manager is available
        auto stats_mgr = training_vm_->statsManager();
        if (stats_mgr) {
            auto mastery = stats_mgr->getMastery(state.current_technique);
            std::string_view mastery_text;
            switch (mastery) {
                case core::MasteryLevel::Beginner:
                    mastery_text = "Beginner";
                    break;
                case core::MasteryLevel::Intermediate:
                    mastery_text = "Intermediate";
                    break;
                case core::MasteryLevel::Proficient:
                    mastery_text = "Proficient";
                    break;
                case core::MasteryLevel::Mastered:
                    mastery_text = "Mastered";
                    break;
            }
            verdict_label->setText(
                QString("Mastery: %1")
                    .arg(QString::fromUtf8(mastery_text.data(), static_cast<int>(mastery_text.size()))));
            verdict_label->setStyleSheet(mastery == core::MasteryLevel::Mastered ? "color: green; font-weight: bold;"
                                                                                 : "color: #333; font-weight: bold;");
        } else {
            float ratio = state.total_exercises > 0
                              ? static_cast<float>(state.correct_count) / static_cast<float>(state.total_exercises)
                              : 0.0f;
            if (ratio >= 0.8f) {
                verdict_label->setText("Excellent! You've mastered this technique.");
                verdict_label->setStyleSheet("color: green; font-weight: bold;");
            } else if (ratio >= 0.5f) {
                verdict_label->setText("Good progress. Try again for a higher score.");
                verdict_label->setStyleSheet("color: #cc9900; font-weight: bold;");
            } else {
                verdict_label->setText("Keep practicing! Review the theory and try again.");
                verdict_label->setStyleSheet("color: red; font-weight: bold;");
            }
        }
    });

    pages_->addWidget(page);
}

void TrainingWidget::animatePageTransition(int target_index) {
    if (pages_->currentIndex() == target_index) {
        pages_->currentWidget()->update();
        return;
    }

    auto* target_widget = pages_->widget(target_index);
    if (target_widget == nullptr) {
        return;
    }

    // Apply fade-in effect on the target page
    auto* effect = new QGraphicsOpacityEffect(target_widget);
    target_widget->setGraphicsEffect(effect);

    pages_->setCurrentIndex(target_index);

    auto* animation = new QPropertyAnimation(effect, "opacity", this);
    animation->setDuration(200);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    connect(animation, &QPropertyAnimation::finished, this,
            [target_widget]() { target_widget->setGraphicsEffect(nullptr); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

}  // namespace sudoku::view
