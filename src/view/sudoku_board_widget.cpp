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
#include "core/string_keys.h"
#include "model/game_state.h"
#include "view_model/game_view_model.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
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

SudokuBoardWidget::SudokuBoardWidget(QWidget* parent)
    : QWidget(parent), painter_({.max_size = 720.0F, .min_size = 450.0F, .padding = 40.0F}) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void SudokuBoardWidget::setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model) {
    view_model_ = std::move(view_model);
    selected_cell_ = core::Position{.row = 0, .col = 0};
    update();
}

void SudokuBoardWidget::setLocalizationManager(std::shared_ptr<core::ILocalizationManager> loc_manager) {
    loc_manager_ = std::move(loc_manager);
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

    if (!view_model_) {
        painter.drawText(rect(), Qt::AlignCenter,
                         QString::fromUtf8(loc(core::StringKeys::BoardNoGameLoaded).data(),
                                           static_cast<qsizetype>(loc(core::StringKeys::BoardNoGameLoaded).size())));
        return;
    }

    const auto& game_state = view_model_->gameState.get();
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
        focus_value = game_state.getValue(highlight_source->row, highlight_source->col);
        focus_box_row = (highlight_source->row / core::BOX_SIZE) * core::BOX_SIZE;
        focus_box_col = (highlight_source->col / core::BOX_SIZE) * core::BOX_SIZE;
    }

    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            const auto cell = game_state.getCell(row, col);
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

            paintCell(painter, cell, row, col, origin, cs, is_selected, is_region_highlight, is_same_value_highlight);
        }
    }

    painter_.paintGridLines(painter, origin, board_size, cs);
}

void SudokuBoardWidget::mousePressEvent(QMouseEvent* event) {
    if (!view_model_ || event->button() != Qt::LeftButton) {
        return;
    }

    QPointF origin = painter_.boardOrigin(width(), height());
    float cs = painter_.cellSize(width(), height());

    float mx = static_cast<float>(event->position().x()) - static_cast<float>(origin.x());
    float my = static_cast<float>(event->position().y()) - static_cast<float>(origin.y());

    if (mx < 0 || my < 0) {
        return;
    }

    auto col = static_cast<int>(mx / cs);
    auto row = static_cast<int>(my / cs);

    if (row >= 0 && col >= 0 && std::cmp_less(row, core::BOARD_SIZE) && std::cmp_less(col, core::BOARD_SIZE)) {
        selectCell(static_cast<size_t>(row), static_cast<size_t>(col));
    }
}

void SudokuBoardWidget::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return;
    }

    if (!selected_cell_.has_value()) {
        QWidget::keyPressEvent(event);
        return;
    }

    const auto& pos = selected_cell_.value();
    switch (event->key()) {
        case Qt::Key_Up:
            selectCell(pos.row > 0 ? pos.row - 1 : core::BOARD_SIZE - 1, pos.col);
            clearHoverHighlight();
            return;
        case Qt::Key_Down:
            selectCell(pos.row < core::BOARD_SIZE - 1 ? pos.row + 1 : 0, pos.col);
            clearHoverHighlight();
            return;
        case Qt::Key_Left:
            selectCell(pos.row, pos.col > 0 ? pos.col - 1 : core::BOARD_SIZE - 1);
            clearHoverHighlight();
            return;
        case Qt::Key_Right:
            selectCell(pos.row, pos.col < core::BOARD_SIZE - 1 ? pos.col + 1 : 0);
            clearHoverHighlight();
            return;
        default:
            break;
    }

    QWidget::keyPressEvent(event);
}

void SudokuBoardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!view_model_) {
        return;
    }

    QPointF origin = painter_.boardOrigin(width(), height());
    float cs = painter_.cellSize(width(), height());

    float mx = static_cast<float>(event->position().x()) - static_cast<float>(origin.x());
    float my = static_cast<float>(event->position().y()) - static_cast<float>(origin.y());

    int new_hover = 0;
    std::optional<core::Position> new_hovered_cell;

    if (mx >= 0 && my >= 0) {
        auto col = static_cast<size_t>(mx / cs);
        auto row = static_cast<size_t>(my / cs);

        if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
            new_hovered_cell = core::Position{.row = row, .col = col};

            const auto& game_state = view_model_->gameState.get();
            const auto cell = game_state.getCell(row, col);
            if (cell.value == 0 && !cell.notes.empty()) {
                float local_x = mx - (static_cast<float>(col) * cs);
                float local_y = my - (static_cast<float>(row) * cs);
                int candidate = BoardPainter::candidateAtPosition(local_x, local_y, cs);
                if (cell.notes.contains(candidate)) {
                    new_hover = candidate;
                }
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

void SudokuBoardWidget::selectCell(const core::Position& pos) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        selected_cell_ = pos;
        update();
    }
}

void SudokuBoardWidget::selectCell(size_t row, size_t col) {
    selectCell({.row = row, .col = col});
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

void SudokuBoardWidget::paintCell(QPainter& painter, const model::Cell& cell, size_t row, size_t col,
                                  const QPointF& origin, float cell_size, bool is_selected, bool is_region_highlight,
                                  bool is_same_value_highlight) {
    QRectF cell_rect(origin.x() + (static_cast<float>(col) * cell_size),
                     origin.y() + (static_cast<float>(row) * cell_size), cell_size, cell_size);

    // Cell background: analysis color → selection → default
    static constexpr std::array<QColor, 7> analysis_colors = {
        BoardColors::CELL_BACKGROUND,         // 0 = no color
        SudokuBoardColors::ANALYSIS_COLOR_1,  // 1 = blue
        SudokuBoardColors::ANALYSIS_COLOR_2,  // 2 = green
        SudokuBoardColors::ANALYSIS_COLOR_3,  // 3 = orange
        SudokuBoardColors::ANALYSIS_COLOR_4,  // 4 = purple
        SudokuBoardColors::ANALYSIS_COLOR_5,  // 5 = red
        SudokuBoardColors::ANALYSIS_COLOR_6,  // 6 = yellow
    };
    uint8_t color_idx = view_model_ ? view_model_->gameState.get().getCellColor(row, col) : 0;
    QColor bg = BoardColors::CELL_BACKGROUND;
    if (color_idx > 0 && color_idx < analysis_colors.size()) {
        bg = analysis_colors[color_idx];
        if (is_selected) {
            bg = bg.darker(120);
        }
    } else if (is_selected) {
        bg = BoardColors::CELL_SELECTED;
    } else if (is_same_value_highlight) {
        bg = BoardColors::HIGHLIGHT_SAME_VALUE;
    } else if (is_region_highlight) {
        bg = BoardColors::HIGHLIGHT_REGION;
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRect(cell_rect);

    // Hover highlight: subtle tint on cells sharing the hovered candidate
    if (hovered_candidate_ > 0 && !is_selected && cell.value == 0) {
        if (cell.notes.contains(hovered_candidate_)) {
            painter.setBrush(BoardColors::HOVER_TINT);
            painter.drawRect(cell_rect);
        }
    }

    // Selection outline
    if (is_selected) {
        painter_.paintSelectionOutline(painter, cell_rect);
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
        text_color = SudokuBoardColors::TEXT_HINT;
    } else if (!cell.is_given) {
        text_color = cell.has_conflict ? SudokuBoardColors::TEXT_ERROR : SudokuBoardColors::TEXT_USER;
    }

    painter.setPen(text_color);
    painter.drawText(cell_rect, Qt::AlignCenter, QString::number(cell.value));
}

void SudokuBoardWidget::paintCellNotes(QPainter& painter, const model::Cell& cell, const QRectF& cell_rect) {
    int note_font_size = std::max(8, static_cast<int>(cell_rect.height() / 3.0 * 0.55));
    QFont font("Sans", note_font_size);
    painter.setFont(font);

    float note_w = static_cast<float>(cell_rect.width()) / static_cast<float>(core::BOX_SIZE);
    float note_h = static_cast<float>(cell_rect.height()) / static_cast<float>(core::BOX_SIZE);

    for (int note : cell.notes) {
        if (note >= core::MIN_VALUE && note <= core::MAX_VALUE) {
            int note_row = (note - 1) / static_cast<int>(core::BOX_SIZE);
            int note_col = (note - 1) % static_cast<int>(core::BOX_SIZE);

            QRectF note_rect(cell_rect.x() + (static_cast<float>(note_col) * note_w),
                             cell_rect.y() + (static_cast<float>(note_row) * note_h), note_w, note_h);

            if (note == hovered_candidate_) {
                QFont bold_font("Sans", note_font_size, QFont::Bold);
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
