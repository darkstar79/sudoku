// sudoku - Offline Sudoku Game
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

#include <QMouseEvent>
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

std::optional<core::Position> BoardPainter::hitTestCell(const QMouseEvent* event, int widget_width,
                                                        int widget_height) const {
    QPointF origin = boardOrigin(widget_width, widget_height);
    float cs = cellSize(widget_width, widget_height);

    float mx = static_cast<float>(event->position().x()) - static_cast<float>(origin.x());
    float my = static_cast<float>(event->position().y()) - static_cast<float>(origin.y());

    if (mx < 0 || my < 0) {
        return std::nullopt;
    }

    auto col = static_cast<size_t>(mx / cs);
    auto row = static_cast<size_t>(my / cs);

    if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
        return core::Position{.row = row, .col = col};
    }
    return std::nullopt;
}

std::optional<core::Position> BoardPainter::arrowNavigate(const core::Position& current, int qt_key) {
    switch (qt_key) {
        case Qt::Key_Up:
            return core::Position{.row = current.row > 0 ? current.row - 1 : core::BOARD_SIZE - 1, .col = current.col};
        case Qt::Key_Down:
            return core::Position{.row = current.row < core::BOARD_SIZE - 1 ? current.row + 1 : 0, .col = current.col};
        case Qt::Key_Left:
            return core::Position{.row = current.row, .col = current.col > 0 ? current.col - 1 : core::BOARD_SIZE - 1};
        case Qt::Key_Right:
            return core::Position{.row = current.row, .col = current.col < core::BOARD_SIZE - 1 ? current.col + 1 : 0};
        default:
            return std::nullopt;
    }
}

int BoardPainter::valueFontSize(float cell_height) {
    return std::max(18, static_cast<int>(cell_height * 0.45F));
}

QRectF BoardPainter::CandidateLayout::noteRect(const QRectF& cell_rect, int value) const {
    int note_row = (value - 1) / static_cast<int>(core::BOX_SIZE);
    int note_col = (value - 1) % static_cast<int>(core::BOX_SIZE);
    return {cell_rect.x() + (static_cast<float>(note_col) * note_w),
            cell_rect.y() + (static_cast<float>(note_row) * note_h), note_w, note_h};
}

BoardPainter::CandidateLayout BoardPainter::candidateLayout(float cell_height) {
    float note_size = cell_height / static_cast<float>(core::BOX_SIZE);
    int font_size = std::max(8, static_cast<int>(cell_height / 3.0 * 0.55));
    return {.note_w = note_size, .note_h = note_size, .font_size = font_size};
}

void BoardPainter::paintHoverTint(QPainter& painter, const QRectF& cell_rect) const {
    painter.setBrush(BoardColors::HOVER_TINT);
    painter.drawRect(cell_rect);
}

QColor BoardPainter::cellRoleColor(core::CellRole role) {
    switch (role) {
        case core::CellRole::Pivot:
        case core::CellRole::LinkEndpoint:
            return AnnotationColors::HIGHLIGHT_PIVOT;
        case core::CellRole::Wing:
            return AnnotationColors::HIGHLIGHT_WING;
        case core::CellRole::Fin:
            return AnnotationColors::HIGHLIGHT_FIN;
        case core::CellRole::Roof:
        case core::CellRole::Floor:
            return AnnotationColors::HIGHLIGHT_PATTERN;
        case core::CellRole::ChainA:
            return AnnotationColors::HIGHLIGHT_CHAIN_A;
        case core::CellRole::ChainB:
            return AnnotationColors::HIGHLIGHT_CHAIN_B;
        case core::CellRole::SetA:
            return AnnotationColors::HIGHLIGHT_SET_A;
        case core::CellRole::SetB:
            return AnnotationColors::HIGHLIGHT_SET_B;
        case core::CellRole::SetC:
            return AnnotationColors::HIGHLIGHT_SET_C;
        case core::CellRole::CorrectAnswer:
            return AnnotationColors::HIGHLIGHT_CORRECT;
        case core::CellRole::WrongAnswer:
            return AnnotationColors::HIGHLIGHT_WRONG;
        case core::CellRole::MissedAnswer:
            return AnnotationColors::HIGHLIGHT_MISSED;
        case core::CellRole::Pattern:
        default:
            return AnnotationColors::HIGHLIGHT_PATTERN;
    }
}

QColor BoardPainter::playerColorBackground(int player_color) {
    if (player_color == 1) {
        return AnnotationColors::COLOR_A.lighter(170);
    }
    if (player_color == 2) {
        return AnnotationColors::COLOR_B.lighter(170);
    }
    return BoardColors::CELL_BACKGROUND;
}

}  // namespace sudoku::view
