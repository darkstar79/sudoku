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
#include "core/i18n_helpers.h"

#include <array>
#include <optional>
#include <utility>

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

SudokuBoardWidget::SudokuBoardWidget(BoardPainter::Config config, QWidget* parent) : QWidget(parent), painter_(config) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void SudokuBoardWidget::setBoard(const BoardRenderData& data) {
    board_ = data;
    has_board_ = true;
    hovered_candidate_ = 0;
    update();
}

void SudokuBoardWidget::clearBoard() {
    board_ = {};
    has_board_ = false;
    update();
}

void SudokuBoardWidget::setReadOnly(bool read_only) {
    read_only_ = read_only;
    if (read_only) {
        hovered_candidate_ = 0;
    }
    update();
}

float SudokuBoardWidget::cellSize() const {
    return painter_.cellSize(width(), height());
}

QPointF SudokuBoardWidget::boardOrigin() const {
    return painter_.boardOrigin(width(), height());
}

QSize SudokuBoardWidget::minimumSizeHint() const {
    return painter_.minimumSizeHint();
}

QSize SudokuBoardWidget::sizeHint() const {
    return painter_.sizeHint();
}

void SudokuBoardWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!has_board_) {
        auto msg = core::loc("Sudoku", "No game loaded. Start a new game!");
        painter.drawText(rect(), Qt::AlignCenter, QString::fromUtf8(msg.data(), static_cast<qsizetype>(msg.size())));
        return;
    }

    float cs = painter_.cellSize(width(), height());
    float board_size = cs * static_cast<float>(core::BOARD_SIZE);
    QPointF origin = painter_.boardOrigin(width(), height());

    painter_.paintBackground(painter, origin, board_size);

    // Highlight source: hover takes priority, falls back to selection
    auto highlight_source = hovered_cell_.has_value() ? hovered_cell_ : selected_cell_;

    int focus_value = 0;
    size_t focus_box_row = 0;
    size_t focus_box_col = 0;
    if (highlight_source.has_value()) {
        focus_value = board_[highlight_source->row][highlight_source->col].value;
        focus_box_row = (highlight_source->row / core::BOX_SIZE) * core::BOX_SIZE;
        focus_box_col = (highlight_source->col / core::BOX_SIZE) * core::BOX_SIZE;
    }

    // Unified highlight: cell value takes priority, fallback to hovered candidate
    int highlight_value = focus_value > 0 ? focus_value : hovered_candidate_;

    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            const auto& cell = board_[row][col];
            bool is_selected = selected_cell_.has_value() && selected_cell_->row == row && selected_cell_->col == col;

            bool is_region_highlight = false;
            bool is_same_value_highlight = false;
            if (highlight_source.has_value()) {
                size_t cell_box_row = (row / core::BOX_SIZE) * core::BOX_SIZE;
                size_t cell_box_col = (col / core::BOX_SIZE) * core::BOX_SIZE;
                is_region_highlight = (row == highlight_source->row) || (col == highlight_source->col) ||
                                      (cell_box_row == focus_box_row && cell_box_col == focus_box_col);
                if (focus_value > 0 && cell.value == focus_value) {
                    is_same_value_highlight = true;
                }
            }

            paintCell(painter, cell, row, col, origin, cs, is_selected, is_region_highlight, is_same_value_highlight,
                      highlight_value);
        }
    }

    painter_.paintGridLines(painter, origin, board_size, cs);
}

void SudokuBoardWidget::mousePressEvent(QMouseEvent* event) {
    if (read_only_ || !has_board_ || event->button() != Qt::LeftButton) {
        return;
    }

    auto pos = painter_.hitTestCell(event, width(), height());
    if (pos.has_value()) {
        selected_cell_ = pos.value();
        update();
        emit cellClicked(pos->row, pos->col);
    }
}

void SudokuBoardWidget::keyPressEvent(QKeyEvent* event) {
    if (read_only_ || event->isAutoRepeat()) {
        return;
    }

    int key = event->key();

    // Arrow key navigation
    core::Position current = selected_cell_.value_or(core::Position{.row = 0, .col = 0});
    auto nav = BoardPainter::arrowNavigate(current, key);
    if (nav.has_value()) {
        selected_cell_ = nav.value();
        clearHoverHighlight();
        update();
        emit cellClicked(nav->row, nav->col);
        return;
    }

    // Number keys 1-9
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        emit numberKeyPressed(key - Qt::Key_1 + 1);
        return;
    }

    // Color keys A/B
    if (key == Qt::Key_A) {
        emit colorKeyPressed(1);
        return;
    }
    if (key == Qt::Key_B) {
        emit colorKeyPressed(2);
        return;
    }

    QWidget::keyPressEvent(event);
}

void SudokuBoardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (read_only_ || !has_board_) {
        return;
    }

    int new_hover = 0;
    auto new_hovered_cell = painter_.hitTestCell(event, width(), height());

    if (new_hovered_cell.has_value()) {
        const auto& pos = new_hovered_cell.value();
        const auto& cell = board_[pos.row][pos.col];
        if (cell.value == 0 && !cell.candidates.empty()) {
            float cs = painter_.cellSize(width(), height());
            QPointF origin = painter_.boardOrigin(width(), height());
            float local_x = static_cast<float>(event->position().x()) - static_cast<float>(origin.x()) -
                            (static_cast<float>(pos.col) * cs);
            float local_y = static_cast<float>(event->position().y()) - static_cast<float>(origin.y()) -
                            (static_cast<float>(pos.row) * cs);
            int candidate = BoardPainter::candidateAtPosition(local_x, local_y, cs);
            if (cell.candidates.contains(candidate)) {
                new_hover = candidate;
            }
        }
    }

    bool needs_update = false;
    if (new_hover != hovered_candidate_) {
        hovered_candidate_ = new_hover;
        needs_update = true;
    }
    if (new_hovered_cell != hovered_cell_) {
        hovered_cell_ = new_hovered_cell;
        needs_update = true;
    }
    if (needs_update) {
        update();
    }
}

void SudokuBoardWidget::setSelectedCell(std::optional<core::Position> cell) {
    if (selected_cell_ != cell) {
        selected_cell_ = cell;
        update();
    }
}

std::optional<core::Position> SudokuBoardWidget::selectedCell() const {
    return selected_cell_;
}

void SudokuBoardWidget::clearSelection() {
    if (selected_cell_.has_value()) {
        selected_cell_ = std::nullopt;
        update();
    }
}

void SudokuBoardWidget::clearHoverHighlight() {
    bool needs_update = false;
    if (hovered_candidate_ != 0) {
        hovered_candidate_ = 0;
        needs_update = true;
    }
    if (hovered_cell_.has_value()) {
        hovered_cell_ = std::nullopt;
        needs_update = true;
    }
    if (needs_update) {
        update();
    }
}

void SudokuBoardWidget::leaveEvent(QEvent* /*event*/) {
    clearHoverHighlight();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — cell painting with multiple visual states
void SudokuBoardWidget::paintCell(QPainter& painter, const RenderCell& cell, size_t row, size_t col,
                                  const QPointF& origin, float cell_size, bool is_selected, bool is_region_highlight,
                                  bool is_same_value_highlight, int highlight_value) {
    QRectF cell_rect(origin.x() + (static_cast<float>(col) * cell_size),
                     origin.y() + (static_cast<float>(row) * cell_size), cell_size, cell_size);

    // Unified background priority: found > player_color > highlight_role > analysis_color > selection > highlights
    static constexpr std::array<QColor, 7> analysis_colors = {
        BoardColors::CELL_BACKGROUND,         // 0 = no color
        SudokuBoardColors::ANALYSIS_COLOR_1,  // 1 = blue
        SudokuBoardColors::ANALYSIS_COLOR_2,  // 2 = green
        SudokuBoardColors::ANALYSIS_COLOR_3,  // 3 = orange
        SudokuBoardColors::ANALYSIS_COLOR_4,  // 4 = purple
        SudokuBoardColors::ANALYSIS_COLOR_5,  // 5 = red
        SudokuBoardColors::ANALYSIS_COLOR_6,  // 6 = yellow
    };

    QColor bg = BoardColors::CELL_BACKGROUND;
    bool has_annotation = false;

    if (cell.is_found) {
        bg = BoardPainter::cellRoleColor(core::CellRole::CorrectAnswer);
        has_annotation = true;
    } else if (cell.player_color != 0) {
        bg = BoardPainter::playerColorBackground(cell.player_color);
        has_annotation = true;
    } else if (cell.highlight_role.has_value()) {
        bg = BoardPainter::cellRoleColor(cell.highlight_role.value());
        has_annotation = true;
    } else if (cell.analysis_color > 0 && cell.analysis_color < analysis_colors.size()) {
        bg = analysis_colors[cell.analysis_color];
        has_annotation = true;
    }

    if (is_selected) {
        if (has_annotation) {
            bg = bg.darker(120);
        } else {
            bg = BoardColors::CELL_SELECTED;
        }
    } else if (!has_annotation) {
        if (is_same_value_highlight) {
            bg = BoardColors::HIGHLIGHT_SAME_VALUE;
        } else if (is_region_highlight) {
            bg = BoardColors::HIGHLIGHT_REGION;
        }
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRect(cell_rect);

    // Hover highlight: subtle tint on cells sharing the hovered candidate
    if (hovered_candidate_ > 0 && !is_selected && cell.value == 0) {
        if (cell.candidates.contains(hovered_candidate_)) {
            painter_.paintHoverTint(painter, cell_rect);
        }
    }

    // Selection outline
    if (is_selected) {
        painter_.paintSelectionOutline(painter, cell_rect);
    }

    // Cell content
    if (cell.value > 0) {
        paintCellValue(painter, cell, cell_rect);
    } else if (!cell.candidates.empty()) {
        paintCellNotes(painter, cell, cell_rect, highlight_value);
    }
}

void SudokuBoardWidget::paintCellValue(QPainter& painter, const RenderCell& cell, const QRectF& cell_rect) {
    QFont font("Sans", BoardPainter::valueFontSize(static_cast<float>(cell_rect.height())), QFont::Bold);
    painter.setFont(font);

    QColor text_color = BoardColors::TEXT_GIVEN;
    if (cell.is_hint_revealed) {
        text_color = SudokuBoardColors::TEXT_HINT;
    } else if (!cell.is_given && !cell.is_found) {
        text_color = cell.has_conflict ? SudokuBoardColors::TEXT_ERROR : SudokuBoardColors::TEXT_USER;
    }

    painter.setPen(text_color);
    painter.drawText(cell_rect, Qt::AlignCenter, QString::number(cell.value));
}

void SudokuBoardWidget::paintCellNotes(QPainter& painter, const RenderCell& cell, const QRectF& cell_rect,
                                       int highlight_value) {
    auto layout = BoardPainter::candidateLayout(static_cast<float>(cell_rect.height()));
    QFont font("Sans", layout.font_size);
    painter.setFont(font);

    for (int note : cell.candidates) {
        if (note >= core::MIN_VALUE && note <= core::MAX_VALUE) {
            QRectF note_rect = layout.noteRect(cell_rect, note);

            if (note == highlight_value) {
                QFont bold_font("Sans", layout.font_size, QFont::Bold);
                painter.setFont(bold_font);
                painter.setPen(SudokuBoardColors::TEXT_USER);
                painter.drawText(note_rect, Qt::AlignCenter, QString::number(note));
                painter.setFont(font);
            } else {
                painter.setPen(SudokuBoardColors::TEXT_NOTE);
                painter.drawText(note_rect, Qt::AlignCenter, QString::number(note));
            }
        }
    }
}

}  // namespace sudoku::view
