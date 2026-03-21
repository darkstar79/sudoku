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

    auto selected_pos_opt = game_state.getSelectedPosition();

    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            const auto cell = game_state.getCell(row, col);
            bool is_selected =
                selected_pos_opt.has_value() && selected_pos_opt->row == row && selected_pos_opt->col == col;
            paintCell(painter, cell, row, col, origin, cs, is_selected);
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
        view_model_->selectCell(row, col);
        update();
    }
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

    if (mx >= 0 && my >= 0) {
        auto col = static_cast<size_t>(mx / cs);
        auto row = static_cast<size_t>(my / cs);

        if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
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

    if (new_hover != hovered_candidate_) {
        hovered_candidate_ = new_hover;
        update();
    }
}

void SudokuBoardWidget::paintCell(QPainter& painter, const model::Cell& cell, size_t row, size_t col,
                                  const QPointF& origin, float cell_size, bool is_selected) {
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
