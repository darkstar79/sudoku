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

#include "../view_model/training_view_model.h"
#include "core/i_localization_manager.h"
#include "core/observable.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <QWidget>
#include <fmt/base.h>
#include <fmt/format.h>
#include <qtmetamacros.h>

class QPushButton;
class QStackedWidget;

#ifdef SUDOKU_UI_TESTING
class TestTrainingWidget;
#endif

namespace sudoku::view {

class TrainingBoardWidget;
class TrainingNumberPad;

class TrainingWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrainingWidget(QWidget* parent = nullptr);

    void setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm);
    void setLocalizationManager(std::shared_ptr<core::ILocalizationManager> loc_manager);

signals:
    void backToGame();

private:
    std::shared_ptr<viewmodel::TrainingViewModel> training_vm_;
    std::shared_ptr<core::ILocalizationManager> loc_manager_;
    core::CompositeObserver observer_;
    QStackedWidget* pages_{nullptr};

    [[nodiscard]] const char* loc(std::string_view key) const {
        return loc_manager_ ? loc_manager_->getString(key) : key.data();
    }

    template <typename... Args>
    [[nodiscard]] std::string locFormat(std::string_view key, Args&&... args) const {
        return fmt::format(fmt::runtime(loc(key)), std::forward<Args>(args)...);
    }

    // Exercise page widgets (owned by Qt parent)
    TrainingBoardWidget* training_board_{nullptr};
    TrainingBoardWidget* feedback_board_{nullptr};
    TrainingNumberPad* number_pad_{nullptr};
    QPushButton* color_a_btn_{nullptr};
    QPushButton* color_b_btn_{nullptr};
    QPushButton* undo_btn_{nullptr};
    QPushButton* redo_btn_{nullptr};
    QWidget* color_palette_{nullptr};

    void rebuildPages();
    void buildTechniqueSelectionPage();
    void buildTheoryPage();
    void buildExercisePage();
    void buildFeedbackPage();
    void buildLessonCompletePage();

    void refreshCurrentPage();
    void updateExerciseBoard();
    void animatePageTransition(int target_index);

#ifdef SUDOKU_UI_TESTING
    friend class ::TestTrainingWidget;
#endif
};

}  // namespace sudoku::view
