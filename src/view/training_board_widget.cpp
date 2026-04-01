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
#include <vector>

#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <qbrush.h>
#include <qnamespace.h>
#include <qpen.h>
#include <qsize.h>
#include <qsizepolicy.h>
#include <qstring.h>

namespace sudoku::view {

TrainingBoardWidget::TrainingBoardWidget(QWidget* parent)
    : QWidget(parent), painter_({.max_size = 540.0F, .min_size = 360.0F, .padding = 20.0F}) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void TrainingBoardWidget::setBoard(const core::TrainingBoard& board) {
    board_ = board;
    hovered_candidate_ = 0;
    update();
}

void TrainingBoardWidget::setSelectedCell(std::optional<core::Position> cell) {
    if (selected_cell_ != cell) {
        selected_cell_ = cell;
        update();
    }
}

void TrainingBoardWidget::keyPressEvent(QKeyEvent* event) {
    if (read_only_ || event->isAutoRepeat()) {
        return;
    }

    int key = event->key();

    // Arrow key navigation with wrapping
    if (key >= Qt::Key_Left && key <= Qt::Key_Down) {
        size_t row = 0;
        size_t col = 0;
        if (selected_cell_.has_value()) {
            row = selected_cell_->row;
            col = selected_cell_->col;
        }

        switch (key) {
            case Qt::Key_Up:
                row = row > 0 ? row - 1 : core::BOARD_SIZE - 1;
                break;
            case Qt::Key_Down:
                row = row < core::BOARD_SIZE - 1 ? row + 1 : 0;
                break;
            case Qt::Key_Left:
                col = col > 0 ? col - 1 : core::BOARD_SIZE - 1;
                break;
            case Qt::Key_Right:
                col = col < core::BOARD_SIZE - 1 ? col + 1 : 0;
                break;
            default:
                break;
        }

        emit cellSelected(row, col);
        return;
    }

    // Number keys 1-9
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        emit numberPressed(key - Qt::Key_1 + 1);
        return;
    }

    // A/B keys for coloring
    if (key == Qt::Key_A) {
        emit colorPressed(1);
        return;
    }
    if (key == Qt::Key_B) {
        emit colorPressed(2);
        return;
    }
}

void TrainingBoardWidget::setReadOnly(bool read_only) {
    read_only_ = read_only;
    if (read_only) {
        hovered_candidate_ = 0;
    }
    update();
}

QSize TrainingBoardWidget::minimumSizeHint() const {
    return painter_.minimumSizeHint();
}

QSize TrainingBoardWidget::sizeHint() const {
    return painter_.sizeHint();
}

void TrainingBoardWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    float cs = painter_.cellSize(width(), height());
    float board_size = cs * static_cast<float>(core::BOARD_SIZE);
    QPointF origin = painter_.boardOrigin(width(), height());

    painter_.paintBackground(painter, origin, board_size);

    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            paintCell(painter, row, col, origin, cs);
        }
    }

    painter_.paintGridLines(painter, origin, board_size, cs);
}

void TrainingBoardWidget::mousePressEvent(QMouseEvent* event) {
    if (read_only_ || event->button() != Qt::LeftButton) {
        return;
    }

    QPointF origin = painter_.boardOrigin(width(), height());
    float cs = painter_.cellSize(width(), height());

    float mx = static_cast<float>(event->position().x()) - static_cast<float>(origin.x());
    float my = static_cast<float>(event->position().y()) - static_cast<float>(origin.y());

    if (mx < 0 || my < 0) {
        return;
    }

    auto col = static_cast<size_t>(mx / cs);
    auto row = static_cast<size_t>(my / cs);

    if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
        emit cellSelected(row, col);
    }
}

void TrainingBoardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (read_only_) {
        return;
    }

    QPointF origin = painter_.boardOrigin(width(), height());
    float cs = painter_.cellSize(width(), height());

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
                int candidate = BoardPainter::candidateAtPosition(local_x, local_y, cs);
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — cell painting with multiple visual states; nesting is inherent
void TrainingBoardWidget::paintCell(QPainter& painter, size_t row, size_t col, const QPointF& origin, float cell_size) {
    const auto& cell = board_[row][col];
    QRectF cell_rect(origin.x() + (static_cast<float>(col) * cell_size),
                     origin.y() + (static_cast<float>(row) * cell_size), cell_size, cell_size);

    bool is_selected = selected_cell_.has_value() && selected_cell_->row == row && selected_cell_->col == col;

    // Determine background color (priority: selection > found > player color > highlight role > hover > default)
    QColor bg = BoardColors::CELL_BACKGROUND;
    if (cell.is_found) {
        bg = cellRoleColor(core::CellRole::CorrectAnswer);
    } else if (cell.player_color != 0) {
        bg = playerColorBackground(cell.player_color);
    } else if (cell.player_selected && cell.highlight_role != core::CellRole::Pattern) {
        bg = cellRoleColor(cell.highlight_role);
    }

    if (is_selected) {
        if (bg != BoardColors::CELL_BACKGROUND) {
            bg = bg.darker(120);
        } else {
            bg = BoardColors::CELL_SELECTED;
        }
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRect(cell_rect);

    // Hover highlight: subtle tint on cells sharing the hovered candidate
    if (hovered_candidate_ > 0 && !is_selected && cell.value == 0) {
        if (std::ranges::find(cell.candidates, hovered_candidate_) != cell.candidates.end()) {
            painter.setBrush(BoardColors::HOVER_TINT);
            painter.drawRect(cell_rect);
        }
    }

    if (is_selected) {
        painter_.paintSelectionOutline(painter, cell_rect);
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

    QColor text_color = is_locked ? BoardColors::TEXT_GIVEN : TrainingBoardColors::HIGHLIGHT_CHAIN_A;
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
    return BoardColors::CELL_BACKGROUND;
}

}  // namespace sudoku::view
