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

#include "board_painter.h"

#include "core/constants.h"

#include <algorithm>
#include <cstddef>

#include <QPainter>
#include <qbrush.h>
#include <qnamespace.h>
#include <qpen.h>
#include <qsize.h>

namespace sudoku::view {

namespace {
constexpr float THICK_LINE_WIDTH = 3.0F;
constexpr float THIN_LINE_WIDTH = 1.0F;
}  // namespace

BoardPainter::BoardPainter(Config config) : config_(config) {
}

float BoardPainter::cellSize(int widget_width, int widget_height) const {
    float available = std::min(static_cast<float>(widget_width) - config_.padding,
                               static_cast<float>(widget_height) - config_.padding);
    float board_size = std::clamp(available, config_.min_size, config_.max_size);
    return board_size / static_cast<float>(core::BOARD_SIZE);
}

QPointF BoardPainter::boardOrigin(int widget_width, int widget_height) const {
    float cs = cellSize(widget_width, widget_height);
    float board_size = cs * static_cast<float>(core::BOARD_SIZE);
    return {(static_cast<float>(widget_width) - board_size) / 2.0F,
            (static_cast<float>(widget_height) - board_size) / 2.0F};
}

QSize BoardPainter::sizeHint() const {
    auto size = static_cast<int>(config_.max_size + config_.padding);
    return {size, size};
}

QSize BoardPainter::minimumSizeHint() const {
    auto size = static_cast<int>(config_.min_size + config_.padding);
    return {size, size};
}

void BoardPainter::paintBackground(QPainter& painter, const QPointF& origin, float board_size) const {
    painter.setPen(Qt::NoPen);
    painter.setBrush(BoardColors::BOARD_BORDER);
    painter.drawRect(QRectF(origin.x() - 2, origin.y() - 2, board_size + 4, board_size + 4));

    painter.setBrush(BoardColors::BOARD_BACKGROUND);
    painter.drawRect(QRectF(origin.x(), origin.y(), board_size, board_size));
}

void BoardPainter::paintGridLines(QPainter& painter, const QPointF& origin, float board_size, float cell_size) const {
    for (size_t i = 1; i < core::BOARD_SIZE; ++i) {
        bool is_box = (i % core::BOX_SIZE == 0);
        float thickness = is_box ? THICK_LINE_WIDTH : THIN_LINE_WIDTH;
        QColor color = is_box ? BoardColors::GRID_THICK_LINE : BoardColors::GRID_THIN_LINE;

        QPen pen(color, thickness);
        painter.setPen(pen);

        float pos = static_cast<float>(i) * cell_size;

        painter.drawLine(QPointF(origin.x() + pos, origin.y()), QPointF(origin.x() + pos, origin.y() + board_size));
        painter.drawLine(QPointF(origin.x(), origin.y() + pos), QPointF(origin.x() + board_size, origin.y() + pos));
    }
}

void BoardPainter::paintSelectionOutline(QPainter& painter, const QRectF& cell_rect) const {
    QPen outline_pen(BoardColors::CELL_SELECTION_OUTLINE, 3.0);
    painter.setPen(outline_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(cell_rect.adjusted(2, 2, -2, -2));
}

int BoardPainter::candidateAtPosition(float local_x, float local_y, float cell_size) {
    float note_w = cell_size / static_cast<float>(core::BOX_SIZE);
    float note_h = cell_size / static_cast<float>(core::BOX_SIZE);
    int note_col = std::clamp(static_cast<int>(local_x / note_w), 0, 2);
    int note_row = std::clamp(static_cast<int>(local_y / note_h), 0, 2);
    return (note_row * 3) + note_col + 1;
}

}  // namespace sudoku::view
