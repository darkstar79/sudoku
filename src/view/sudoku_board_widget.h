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

#include "../model/game_state.h"
#include "../view_model/game_view_model.h"

#include <cstddef>
#include <memory>

#include <QWidget>
#include <qcolor.h>
#include <qicon.h>
#include <qpoint.h>
#include <qrect.h>
#include <qtmetamacros.h>

#ifdef SUDOKU_UI_TESTING
class TestBoardInteraction;
#endif

namespace sudoku::view {

// CPD-OFF — board color constants shared with training_board_widget
namespace BoardColors {
inline constexpr QColor BOARD_BORDER{44, 44, 44};
inline constexpr QColor BOARD_BACKGROUND{255, 255, 255};
inline constexpr QColor GRID_THICK_LINE{44, 44, 44};
inline constexpr QColor GRID_THIN_LINE{140, 140, 140};
inline constexpr QColor CELL_BACKGROUND{255, 255, 255};
inline constexpr QColor CELL_SELECTED{254, 243, 199};
inline constexpr QColor CELL_SELECTION_OUTLINE{20, 20, 20};
inline constexpr QColor TEXT_GIVEN{20, 20, 20};
inline constexpr QColor TEXT_USER{0, 82, 204};
inline constexpr QColor TEXT_ERROR{220, 38, 38};
inline constexpr QColor TEXT_HINT{255, 165, 0};
inline constexpr QColor TEXT_NOTE{107, 107, 107};
}  // namespace BoardColors

class SudokuBoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit SudokuBoardWidget(QWidget* parent = nullptr);

    void setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model);

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    std::shared_ptr<viewmodel::GameViewModel> view_model_;

    [[nodiscard]] float cellSize() const;
    [[nodiscard]] QPointF boardOrigin() const;

    void paintBackground(QPainter& painter, const QPointF& origin, float board_size);
    void paintCell(QPainter& painter, const model::Cell& cell, size_t row, size_t col, const QPointF& origin,
                   float cell_size, bool is_selected);
    void paintCellValue(QPainter& painter, const model::Cell& cell, const QRectF& cell_rect);
    void paintCellNotes(QPainter& painter, const model::Cell& cell, const QRectF& cell_rect);
    void paintGridLines(QPainter& painter, const QPointF& origin, float board_size, float cell_size);
    // CPD-ON

#ifdef SUDOKU_UI_TESTING
    friend class ::TestBoardInteraction;
#endif
};

}  // namespace sudoku::view
