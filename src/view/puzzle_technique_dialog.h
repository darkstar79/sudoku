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

#include <QDialog>
#include <qtmetamacros.h>

class QComboBox;
class QPushButton;

#ifdef SUDOKU_UI_TESTING
class TestFindByTechnique;
#endif

namespace sudoku::view {

/// Learning-mode L-D dialog. Lets the user pick a SolvingTechnique from the full list and
/// asks the solver "where can I apply this here?". Emits `findRequested(int technique_id)`
/// with the SolvingTechnique enum value; MainWindow drives the dispatch.
class PuzzleTechniqueDialog : public QDialog {
    Q_OBJECT

public:
    explicit PuzzleTechniqueDialog(QWidget* parent = nullptr);
    ~PuzzleTechniqueDialog() override = default;
    PuzzleTechniqueDialog(const PuzzleTechniqueDialog&) = delete;
    PuzzleTechniqueDialog& operator=(const PuzzleTechniqueDialog&) = delete;
    PuzzleTechniqueDialog(PuzzleTechniqueDialog&&) = delete;
    PuzzleTechniqueDialog& operator=(PuzzleTechniqueDialog&&) = delete;

signals:
    void findRequested(int technique_id);

private:
    QComboBox* technique_combo_{nullptr};
    QPushButton* find_btn_{nullptr};
    QPushButton* cancel_btn_{nullptr};

#ifdef SUDOKU_UI_TESTING
    friend class ::TestFindByTechnique;
#endif
};

}  // namespace sudoku::view
