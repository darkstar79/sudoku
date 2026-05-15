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

#include "puzzle_import_dialog.h"

#include "../core/i18n_helpers.h"
#include "style_colors.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace sudoku::view {

namespace {

QString qstr(std::string_view sv) {
    return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
}

}  // namespace

PuzzleImportDialog::PuzzleImportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(qstr(core::loc("Sudoku", "Import Custom Puzzle")));
    setModal(true);
    resize(420, 320);

    auto* layout = new QVBoxLayout(this);

    auto* prompt = new QLabel(qstr(
        core::loc("Sudoku", "Paste an 81-character puzzle. Use digits 1-9 and '.', '0', or '_' for empty cells.")));
    prompt->setWordWrap(true);
    layout->addWidget(prompt);

    text_edit_ = new QTextEdit;
    text_edit_->setAcceptRichText(false);
    text_edit_->setPlaceholderText(qstr(core::loc("Sudoku", "Paste puzzle here…")));
    layout->addWidget(text_edit_, 1);

    error_label_ = new QLabel;
    error_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(StyleColors::ERROR_COLOR));
    error_label_->setWordWrap(true);
    error_label_->setVisible(false);
    layout->addWidget(error_label_);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    cancel_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Cancel")));
    import_btn_ = new QPushButton(qstr(core::loc("Sudoku", "Import")));
    import_btn_->setDefault(true);
    buttons->addWidget(cancel_btn_);
    buttons->addWidget(import_btn_);
    layout->addLayout(buttons);

    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(import_btn_, &QPushButton::clicked, this, [this]() {
        error_label_->clear();
        error_label_->setVisible(false);
        emit importRequested(text_edit_->toPlainText());
    });
}

void PuzzleImportDialog::setErrorMessage(const QString& message) {
    error_label_->setText(message);
    error_label_->setVisible(!message.isEmpty());
}

}  // namespace sudoku::view
