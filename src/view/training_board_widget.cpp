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

#include "training_board_widget.h"

#include "core/constants.h"
#include "core/solve_step.h"
#include "core/training_types.h"

#include <algorithm>
#include <cstddef>
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
constexpr float BOARD_MAX_SIZE = 540.0F;
constexpr float BOARD_MIN_SIZE = 360.0F;
constexpr float THICK_LINE_WIDTH = 3.0F;
constexpr float THIN_LINE_WIDTH = 1.0F;
}  // namespace

TrainingBoardWidget::TrainingBoardWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void TrainingBoardWidget::setBoard(const core::TrainingBoard& board) {
    board_ = board;
    selected_cell_.reset();
    hovered_candidate_ = 0;
    update();
}

void TrainingBoardWidget::setReadOnly(bool read_only) {
    read_only_ = read_only;
    if (read_only) {
        selected_cell_.reset();
        hovered_candidate_ = 0;
    }
    update();
}

void TrainingBoardWidget::applyNumber(int value, core::TrainingInteractionMode mode) {
    if (read_only_ || !selected_cell_.has_value()) {
        return;
    }

    auto [row, col] = *selected_cell_;
    auto& cell = board_[row][col];

    if (cell.is_given || cell.is_found || cell.value != 0) {
        return;
    }

    if (mode == core::TrainingInteractionMode::Placement) {
        // Check value is a valid candidate
        if (std::ranges::find(cell.candidates, value) == cell.candidates.end()) {
            return;
        }
        cell.value = value;
        cell.player_selected = true;
    } else {
        // Elimination mode: toggle candidate removal
        auto iter = std::ranges::find(cell.candidates, value);
        if (iter != cell.candidates.end()) {
            cell.candidates.erase(iter);
            cell.player_selected = true;
        }
    }

    update();
    emit boardChanged(board_);
}

void TrainingBoardWidget::applyColor(int color) {
    if (read_only_ || !selected_cell_.has_value()) {
        return;
    }

    auto [row, col] = *selected_cell_;
    auto& cell = board_[row][col];

    if (cell.is_given) {
        return;
    }

    // Toggle: if already this color, remove it
    cell.player_color = (cell.player_color == color) ? 0 : color;
    cell.player_selected = true;

    update();
    emit boardChanged(board_);
}

QSize TrainingBoardWidget::minimumSizeHint() const {
    return {static_cast<int>(BOARD_MIN_SIZE) + 20, static_cast<int>(BOARD_MIN_SIZE) + 20};
}

QSize TrainingBoardWidget::sizeHint() const {
    return {static_cast<int>(BOARD_MAX_SIZE) + 20, static_cast<int>(BOARD_MAX_SIZE) + 20};
}

float TrainingBoardWidget::cellSize() const {
    float available = std::min(static_cast<float>(width()) - 20.0F, static_cast<float>(height()) - 20.0F);
    float board_size = std::clamp(available, BOARD_MIN_SIZE, BOARD_MAX_SIZE);
    return board_size / static_cast<float>(core::BOARD_SIZE);
}

QPointF TrainingBoardWidget::boardOrigin() const {
    float cs = cellSize();
    float board_size = cs * static_cast<float>(core::BOARD_SIZE);
    return {(static_cast<float>(width()) - board_size) / 2.0F, (static_cast<float>(height()) - board_size) / 2.0F};
}

// CPD-OFF — Qt widget boilerplate shared with sudoku_board_widget
void TrainingBoardWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    float cs = cellSize();
    float board_size = cs * static_cast<float>(core::BOARD_SIZE);
    QPointF origin = boardOrigin();

    paintBackground(painter, origin, board_size);

    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            paintCell(painter, row, col, origin, cs);
        }
    }

    paintGridLines(painter, origin, board_size, cs);
}

void TrainingBoardWidget::mousePressEvent(QMouseEvent* event) {
    if (read_only_ || event->button() != Qt::LeftButton) {
        return;
    }

    QPointF origin = boardOrigin();
    float cs = cellSize();

    float mx = static_cast<float>(event->position().x()) - static_cast<float>(origin.x());
    float my = static_cast<float>(event->position().y()) - static_cast<float>(origin.y());

    if (mx < 0 || my < 0) {
        return;
    }

    auto col = static_cast<size_t>(mx / cs);
    auto row = static_cast<size_t>(my / cs);

    if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
        selected_cell_ = {row, col};
        update();
        emit cellSelected(row, col);
    }
}

void TrainingBoardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (read_only_) {
        return;
    }

    QPointF origin = boardOrigin();
    float cs = cellSize();

    float mx = static_cast<float>(event->position().x()) - static_cast<float>(origin.x());
    float my = static_cast<float>(event->position().y()) - static_cast<float>(origin.y());

    int new_hover = 0;

    if (mx >= 0 && my >= 0) {
        auto col = static_cast<size_t>(mx / cs);
        auto row = static_cast<size_t>(my / cs);

        if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
            const auto& cell = board_[row][col];
            if (cell.value == 0 && !cell.candidates.empty()) {
                float local_x = mx - (static_cast<float>(col) * cs);
                float local_y = my - (static_cast<float>(row) * cs);
                int candidate = candidateAtPosition(local_x, local_y, cs);
                if (std::ranges::find(cell.candidates, candidate) != cell.candidates.end()) {
                    new_hover = candidate;
                }
            }
        }
    }

    if (new_hover != hovered_candidate_) {
        hovered_candidate_ = new_hover;
        update();
    }
}

int TrainingBoardWidget::candidateAtPosition(float local_x, float local_y, float cell_size) {
    float note_w = cell_size / static_cast<float>(core::BOX_SIZE);
    float note_h = cell_size / static_cast<float>(core::BOX_SIZE);
    int note_col = std::clamp(static_cast<int>(local_x / note_w), 0, 2);
    int note_row = std::clamp(static_cast<int>(local_y / note_h), 0, 2);
    return (note_row * 3) + note_col + 1;
}

void TrainingBoardWidget::paintBackground(QPainter& painter, const QPointF& origin, float board_size) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(TrainingBoardColors::BOARD_BORDER);
    painter.drawRect(QRectF(origin.x() - 2, origin.y() - 2, board_size + 4, board_size + 4));

    painter.setBrush(TrainingBoardColors::BOARD_BACKGROUND);
    painter.drawRect(QRectF(origin.x(), origin.y(), board_size, board_size));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — cell painting with multiple visual states; nesting is inherent
void TrainingBoardWidget::paintCell(QPainter& painter, size_t row, size_t col, const QPointF& origin, float cell_size) {
    const auto& cell = board_[row][col];
    QRectF cell_rect(origin.x() + (static_cast<float>(col) * cell_size),
                     origin.y() + (static_cast<float>(row) * cell_size), cell_size, cell_size);

    bool is_selected = selected_cell_.has_value() && selected_cell_->first == row && selected_cell_->second == col;

    // Determine background color (priority: selection > found > player color > highlight role > hover > default)
    QColor bg = TrainingBoardColors::CELL_BACKGROUND;
    if (cell.is_found) {
        bg = cellRoleColor(core::CellRole::CorrectAnswer);
    } else if (cell.player_color != 0) {
        bg = playerColorBackground(cell.player_color);
    } else if (cell.player_selected && cell.highlight_role != core::CellRole::Pattern) {
        bg = cellRoleColor(cell.highlight_role);
    }

    if (is_selected) {
        bg = TrainingBoardColors::CELL_SELECTED;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRect(cell_rect);

    // Hover highlight: subtle tint on cells sharing the hovered candidate
    if (hovered_candidate_ > 0 && !is_selected && cell.value == 0) {
        if (std::ranges::find(cell.candidates, hovered_candidate_) != cell.candidates.end()) {
            painter.setBrush(TrainingBoardColors::HOVER_TINT);
            painter.drawRect(cell_rect);
        }
    }

    if (is_selected) {
        QPen outline_pen(TrainingBoardColors::CELL_SELECTION_OUTLINE, 3.0);
        painter.setPen(outline_pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(cell_rect.adjusted(2, 2, -2, -2));
    }

    if (cell.value > 0) {
        paintCellValue(painter, cell, cell_rect);
    } else if (!cell.candidates.empty()) {
        paintCellCandidates(painter, cell, cell_rect);
    }
}

void TrainingBoardWidget::paintCellValue(QPainter& painter, const core::TrainingCellState& cell,
                                         const QRectF& cell_rect) {
    int value_font_size = std::max(18, static_cast<int>(cell_rect.height() * 0.45));
    bool is_locked = cell.is_given || cell.is_found;
    QFont font("Sans", value_font_size, is_locked ? QFont::Bold : QFont::Normal);
    painter.setFont(font);

    QColor text_color = is_locked ? TrainingBoardColors::TEXT_GIVEN : TrainingBoardColors::HIGHLIGHT_CHAIN_A;
    painter.setPen(text_color);
    painter.drawText(cell_rect, Qt::AlignCenter, QString::number(cell.value));
}

void TrainingBoardWidget::paintCellCandidates(QPainter& painter, const core::TrainingCellState& cell,
                                              const QRectF& cell_rect) {
    int note_font_size = std::max(8, static_cast<int>(cell_rect.height() / 3.0 * 0.55));
    QFont font("Sans", note_font_size);
    painter.setFont(font);

    float note_w = static_cast<float>(cell_rect.width()) / static_cast<float>(core::BOX_SIZE);
    float note_h = static_cast<float>(cell_rect.height()) / static_cast<float>(core::BOX_SIZE);

    for (int v = core::MIN_VALUE; v <= core::MAX_VALUE; ++v) {
        int note_row = (v - 1) / static_cast<int>(core::BOX_SIZE);
        int note_col = (v - 1) % static_cast<int>(core::BOX_SIZE);

        QRectF note_rect(cell_rect.x() + (static_cast<float>(note_col) * note_w),
                         cell_rect.y() + (static_cast<float>(note_row) * note_h), note_w, note_h);

        bool is_present = std::ranges::find(cell.candidates, v) != cell.candidates.end();
        if (is_present) {
            // Bold the hovered candidate
            if (v == hovered_candidate_) {
                QFont bold_font("Sans", note_font_size, QFont::Bold);
                painter.setFont(bold_font);
                painter.setPen(TrainingBoardColors::HIGHLIGHT_CHAIN_A);
                painter.drawText(note_rect, Qt::AlignCenter, QString::number(v));
                painter.setFont(font);
            } else {
                painter.setPen(TrainingBoardColors::TEXT_CANDIDATE);
                painter.drawText(note_rect, Qt::AlignCenter, QString::number(v));
            }
        }
    }
}

void TrainingBoardWidget::paintGridLines(QPainter& painter, const QPointF& origin, float board_size, float cell_size) {
    for (size_t i = 1; i < core::BOARD_SIZE; ++i) {
        bool is_box = (i % core::BOX_SIZE == 0);
        float thickness = is_box ? THICK_LINE_WIDTH : THIN_LINE_WIDTH;
        QColor color = is_box ? TrainingBoardColors::GRID_THICK_LINE : TrainingBoardColors::GRID_THIN_LINE;

        QPen pen(color, thickness);
        painter.setPen(pen);

        float pos = static_cast<float>(i) * cell_size;

        painter.drawLine(QPointF(origin.x() + pos, origin.y()), QPointF(origin.x() + pos, origin.y() + board_size));
        painter.drawLine(QPointF(origin.x(), origin.y() + pos), QPointF(origin.x() + board_size, origin.y() + pos));
    }
}
// CPD-ON

QColor TrainingBoardWidget::cellRoleColor(core::CellRole role) {
    switch (role) {
        case core::CellRole::Pivot:
            return TrainingBoardColors::HIGHLIGHT_PIVOT;
        case core::CellRole::Wing:
            return TrainingBoardColors::HIGHLIGHT_WING;
        case core::CellRole::Fin:
            return TrainingBoardColors::HIGHLIGHT_FIN;
        case core::CellRole::Roof:
        case core::CellRole::Floor:
            return TrainingBoardColors::HIGHLIGHT_PATTERN;
        case core::CellRole::ChainA:
            return TrainingBoardColors::HIGHLIGHT_CHAIN_A;
        case core::CellRole::ChainB:
            return TrainingBoardColors::HIGHLIGHT_CHAIN_B;
        case core::CellRole::LinkEndpoint:
            return TrainingBoardColors::HIGHLIGHT_PIVOT;
        case core::CellRole::SetA:
            return TrainingBoardColors::HIGHLIGHT_SET_A;
        case core::CellRole::SetB:
            return TrainingBoardColors::HIGHLIGHT_SET_B;
        case core::CellRole::SetC:
            return TrainingBoardColors::HIGHLIGHT_SET_C;
        case core::CellRole::CorrectAnswer:
            return TrainingBoardColors::HIGHLIGHT_CORRECT;
        case core::CellRole::WrongAnswer:
            return TrainingBoardColors::HIGHLIGHT_WRONG;
        case core::CellRole::MissedAnswer:
            return TrainingBoardColors::HIGHLIGHT_MISSED;
        case core::CellRole::Pattern:
        default:
            return TrainingBoardColors::HIGHLIGHT_PATTERN;
    }
}

QColor TrainingBoardWidget::playerColorBackground(int player_color) {
    if (player_color == 1) {
        return TrainingBoardColors::COLOR_A.lighter(170);
    }
    if (player_color == 2) {
        return TrainingBoardColors::COLOR_B.lighter(170);
    }
    return TrainingBoardColors::CELL_BACKGROUND;
}

}  // namespace sudoku::view
