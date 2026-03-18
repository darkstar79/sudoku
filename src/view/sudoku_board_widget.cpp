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

#include "sudoku_board_widget.h"

#include "core/constants.h"
#include "core/i_game_validator.h"
#include "core/observable.h"
#include "model/game_state.h"
#include "view_model/game_view_model.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <qbrush.h>
#include <qnamespace.h>
#include <qpen.h>
#include <qsize.h>
#include <qsizepolicy.h>
#include <qstring.h>

namespace sudoku::view {

namespace {
constexpr float BOARD_MAX_SIZE = 720.0F;
constexpr float BOARD_MIN_SIZE = 450.0F;
constexpr float THICK_LINE_WIDTH = 3.0F;
constexpr float THIN_LINE_WIDTH = 1.0F;
}  // namespace

SudokuBoardWidget::SudokuBoardWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SudokuBoardWidget::setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model) {
    view_model_ = std::move(view_model);
    update();
}

QSize SudokuBoardWidget::minimumSizeHint() const {
    return {static_cast<int>(BOARD_MIN_SIZE) + 40, static_cast<int>(BOARD_MIN_SIZE) + 40};
}

QSize SudokuBoardWidget::sizeHint() const {
    return {static_cast<int>(BOARD_MAX_SIZE) + 40, static_cast<int>(BOARD_MAX_SIZE) + 40};
}

float SudokuBoardWidget::cellSize() const {
    float available = std::min(static_cast<float>(width()) - 40.0F, static_cast<float>(height()) - 40.0F);
    float board_size = std::clamp(available, BOARD_MIN_SIZE, BOARD_MAX_SIZE);
    return board_size / static_cast<float>(core::BOARD_SIZE);
}

QPointF SudokuBoardWidget::boardOrigin() const {
    float cs = cellSize();
    float board_size = cs * static_cast<float>(core::BOARD_SIZE);
    return {(static_cast<float>(width()) - board_size) / 2.0F, (static_cast<float>(height()) - board_size) / 2.0F};
}

// CPD-OFF — Qt widget boilerplate shared with training_board_widget
void SudokuBoardWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!view_model_) {
        painter.drawText(rect(), Qt::AlignCenter, "No game loaded");
        return;
    }

    const auto& game_state = view_model_->gameState.get();
    float cs = cellSize();
    float board_size = cs * static_cast<float>(core::BOARD_SIZE);
    QPointF origin = boardOrigin();

    paintBackground(painter, origin, board_size);

    auto selected_pos_opt = game_state.getSelectedPosition();

    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            const auto cell = game_state.getCell(row, col);
            bool is_selected =
                selected_pos_opt.has_value() && selected_pos_opt->row == row && selected_pos_opt->col == col;
            paintCell(painter, cell, row, col, origin, cs, is_selected);
        }
    }

    paintGridLines(painter, origin, board_size, cs);
}

void SudokuBoardWidget::mousePressEvent(QMouseEvent* event) {
    if (!view_model_ || event->button() != Qt::LeftButton) {
        return;
    }

    QPointF origin = boardOrigin();
    float cs = cellSize();

    float mx = static_cast<float>(event->position().x()) - static_cast<float>(origin.x());
    float my = static_cast<float>(event->position().y()) - static_cast<float>(origin.y());

    if (mx < 0 || my < 0) {
        return;
    }

    auto col = static_cast<int>(mx / cs);
    auto row = static_cast<int>(my / cs);

    if (row >= 0 && col >= 0 && std::cmp_less(row, core::BOARD_SIZE) && std::cmp_less(col, core::BOARD_SIZE)) {
        view_model_->selectCell(row, col);
        update();
    }
}

void SudokuBoardWidget::paintBackground(QPainter& painter, const QPointF& origin, float board_size) {
    // Border
    painter.setPen(Qt::NoPen);
    painter.setBrush(BoardColors::BOARD_BORDER);
    painter.drawRect(QRectF(origin.x() - 2, origin.y() - 2, board_size + 4, board_size + 4));

    // White background
    painter.setBrush(BoardColors::BOARD_BACKGROUND);
    painter.drawRect(QRectF(origin.x(), origin.y(), board_size, board_size));
}

void SudokuBoardWidget::paintCell(QPainter& painter, const model::Cell& cell, size_t row, size_t col,
                                  const QPointF& origin, float cell_size, bool is_selected) {
    QRectF cell_rect(origin.x() + (static_cast<float>(col) * cell_size),
                     origin.y() + (static_cast<float>(row) * cell_size), cell_size, cell_size);

    // Cell background
    painter.setPen(Qt::NoPen);
    painter.setBrush(is_selected ? BoardColors::CELL_SELECTED : BoardColors::CELL_BACKGROUND);
    painter.drawRect(cell_rect);

    // Selection outline
    if (is_selected) {
        QPen outline_pen(BoardColors::CELL_SELECTION_OUTLINE, 3.0);
        painter.setPen(outline_pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(cell_rect.adjusted(2, 2, -2, -2));
    }

    // Cell content
    if (cell.value > 0) {
        paintCellValue(painter, cell, cell_rect);
    } else if (!cell.notes.empty()) {
        paintCellNotes(painter, cell, cell_rect);
    }
}

void SudokuBoardWidget::paintCellValue(QPainter& painter, const model::Cell& cell, const QRectF& cell_rect) {
    int value_font_size = std::max(18, static_cast<int>(cell_rect.height() * 0.45));
    QFont font("Sans", value_font_size, QFont::Bold);
    painter.setFont(font);

    QColor text_color = BoardColors::TEXT_GIVEN;
    if (cell.is_hint_revealed) {
        text_color = BoardColors::TEXT_HINT;
    } else if (!cell.is_given) {
        text_color = cell.has_conflict ? BoardColors::TEXT_ERROR : BoardColors::TEXT_USER;
    }

    painter.setPen(text_color);
    painter.drawText(cell_rect, Qt::AlignCenter, QString::number(cell.value));
}

void SudokuBoardWidget::paintCellNotes(QPainter& painter, const model::Cell& cell, const QRectF& cell_rect) {
    int note_font_size = std::max(8, static_cast<int>(cell_rect.height() / 3.0 * 0.55));
    QFont font("Sans", note_font_size);
    painter.setFont(font);
    painter.setPen(BoardColors::TEXT_NOTE);

    float note_w = static_cast<float>(cell_rect.width()) / static_cast<float>(core::BOX_SIZE);
    float note_h = static_cast<float>(cell_rect.height()) / static_cast<float>(core::BOX_SIZE);

    for (int note : cell.notes) {
        if (note >= core::MIN_VALUE && note <= core::MAX_VALUE) {
            int note_row = (note - 1) / static_cast<int>(core::BOX_SIZE);
            int note_col = (note - 1) % static_cast<int>(core::BOX_SIZE);

            QRectF note_rect(cell_rect.x() + (static_cast<float>(note_col) * note_w),
                             cell_rect.y() + (static_cast<float>(note_row) * note_h), note_w, note_h);

            painter.drawText(note_rect, Qt::AlignCenter, QString::number(note));
        }
    }
}

void SudokuBoardWidget::paintGridLines(QPainter& painter, const QPointF& origin, float board_size, float cell_size) {
    for (size_t i = 1; i < core::BOARD_SIZE; ++i) {
        bool is_box = (i % core::BOX_SIZE == 0);
        float thickness = is_box ? THICK_LINE_WIDTH : THIN_LINE_WIDTH;
        QColor color = is_box ? BoardColors::GRID_THICK_LINE : BoardColors::GRID_THIN_LINE;

        QPen pen(color, thickness);
        painter.setPen(pen);

        float pos = static_cast<float>(i) * cell_size;

        // Vertical
        painter.drawLine(QPointF(origin.x() + pos, origin.y()), QPointF(origin.x() + pos, origin.y() + board_size));
        // Horizontal
        painter.drawLine(QPointF(origin.x(), origin.y() + pos), QPointF(origin.x() + board_size, origin.y() + pos));
    }
}
// CPD-ON

}  // namespace sudoku::view
