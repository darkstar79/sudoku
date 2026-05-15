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

class QLabel;
class QPushButton;
class QTextEdit;

#ifdef SUDOKU_UI_TESTING
class TestImportDialog;
#endif

namespace sudoku::view {

/// Paste-string import dialog (I-1). Emits `importRequested(QString)` when the user clicks
/// Import; MainWindow drives the actual analyzer dispatch and reports failures back via
/// `setErrorMessage()`. The dialog deliberately does not depend on the ViewModel.
class PuzzleImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit PuzzleImportDialog(QWidget* parent = nullptr);
    ~PuzzleImportDialog() override = default;
    PuzzleImportDialog(const PuzzleImportDialog&) = delete;
    PuzzleImportDialog& operator=(const PuzzleImportDialog&) = delete;
    PuzzleImportDialog(PuzzleImportDialog&&) = delete;
    PuzzleImportDialog& operator=(PuzzleImportDialog&&) = delete;

    /// Display an error message returned by the analyzer. Caller invokes this when the
    /// dispatched import fails so the user can correct the paste without losing it.
    void setErrorMessage(const QString& message);

signals:
    void importRequested(const QString& text);

private:
    QTextEdit* text_edit_{nullptr};
    QPushButton* import_btn_{nullptr};
    QPushButton* cancel_btn_{nullptr};
    QLabel* error_label_{nullptr};

#ifdef SUDOKU_UI_TESTING
    friend class ::TestImportDialog;
#endif
};

}  // namespace sudoku::view
