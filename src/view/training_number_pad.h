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

#include "../core/training_types.h"

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include <QWidget>
#include <qtmetamacros.h>

class QPushButton;

namespace sudoku::view {

/// Compact 1-9 number pad for training exercises.
/// In Placement mode, clicking a number places it in the selected cell.
/// In Elimination mode, clicking a number toggles that candidate's elimination.
class TrainingNumberPad : public QWidget {
    Q_OBJECT

public:
    explicit TrainingNumberPad(QWidget* parent = nullptr);

    /// Inject localization manager for tooltip translations
    /// Set the interaction mode (changes button labels/behavior)
    void setInteractionMode(core::TrainingInteractionMode mode);

    /// Update which numbers are enabled based on the selected cell's candidates
    void updateEnabledNumbers(const std::vector<int>& candidates);

signals:
    /// Emitted when a number button is pressed
    void numberPressed(int value);

private:
    std::array<QPushButton*, 9> buttons_{};
    core::TrainingInteractionMode mode_{core::TrainingInteractionMode::Placement};
};

}  // namespace sudoku::view
