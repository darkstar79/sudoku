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

#include <QWidget>
#include <qcolor.h>
#include <qpoint.h>
#include <qrect.h>

class QPainter;

namespace sudoku::view {

/// Shared board color constants used by both SudokuBoardWidget and TrainingBoardWidget.
namespace BoardColors {
inline constexpr QColor BOARD_BORDER{44, 44, 44};
inline constexpr QColor BOARD_BACKGROUND{255, 255, 255};
inline constexpr QColor GRID_THICK_LINE{44, 44, 44};
inline constexpr QColor GRID_THIN_LINE{140, 140, 140};
inline constexpr QColor CELL_BACKGROUND{255, 255, 255};
inline constexpr QColor CELL_SELECTED{254, 243, 199};
inline constexpr QColor CELL_SELECTION_OUTLINE{20, 20, 20};
inline constexpr QColor TEXT_GIVEN{20, 20, 20};
inline constexpr QColor HOVER_TINT{200, 220, 255, 80};  // Subtle blue overlay for candidate hover
}  // namespace BoardColors

/// Shared board geometry and painting utilities for Sudoku board widgets.
/// Encapsulates cell size calculations, background painting, grid line rendering,
/// and selection outline drawing — parameterized by board dimensions.
class BoardPainter {
public:
    struct Config {
        float max_size;
        float min_size;
        float padding;
    };

    explicit BoardPainter(Config config);

    [[nodiscard]] float cellSize(int widget_width, int widget_height) const;
    [[nodiscard]] QPointF boardOrigin(int widget_width, int widget_height) const;
    [[nodiscard]] QSize sizeHint() const;
    [[nodiscard]] QSize minimumSizeHint() const;

    void paintBackground(QPainter& painter, const QPointF& origin, float board_size) const;
    void paintGridLines(QPainter& painter, const QPointF& origin, float board_size, float cell_size) const;
    void paintSelectionOutline(QPainter& painter, const QRectF& cell_rect) const;

    /// Determine which candidate (1-9) is under the given cell-relative position
    [[nodiscard]] static int candidateAtPosition(float local_x, float local_y, float cell_size);

private:
    Config config_;
};

}  // namespace sudoku::view
