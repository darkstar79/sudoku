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

#pragma once

#include "../core/i_game_validator.h"
#include "../core/solve_step.h"

#include <optional>

#include <QWidget>
#include <qcolor.h>
#include <qpoint.h>
#include <qrect.h>

class QPainter;
class QMouseEvent;

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
inline constexpr QColor HOVER_TINT{200, 220, 255, 80};        // Subtle blue overlay for candidate hover
inline constexpr QColor HIGHLIGHT_REGION{220, 232, 245};      // Row/col/block of focus cell
inline constexpr QColor HIGHLIGHT_SAME_VALUE{195, 218, 245};  // Same placed value as focus cell
}  // namespace BoardColors

/// Annotation color constants for technique highlights and training feedback.
namespace AnnotationColors {
inline constexpr QColor TEXT_CANDIDATE{107, 107, 107};
inline constexpr QColor HIGHLIGHT_PATTERN{200, 220, 255};
inline constexpr QColor HIGHLIGHT_PIVOT{255, 220, 180};
inline constexpr QColor HIGHLIGHT_WING{220, 255, 220};
inline constexpr QColor HIGHLIGHT_FIN{255, 200, 200};
inline constexpr QColor HIGHLIGHT_CHAIN_A{180, 210, 255};
inline constexpr QColor HIGHLIGHT_CHAIN_B{180, 255, 200};
inline constexpr QColor HIGHLIGHT_SET_A{230, 200, 255};
inline constexpr QColor HIGHLIGHT_SET_B{255, 230, 200};
inline constexpr QColor HIGHLIGHT_SET_C{200, 255, 230};
inline constexpr QColor HIGHLIGHT_CORRECT{198, 246, 213};  // Soft green — correct answer
inline constexpr QColor HIGHLIGHT_WRONG{254, 215, 215};    // Soft red — wrong answer
inline constexpr QColor HIGHLIGHT_MISSED{254, 252, 191};   // Soft yellow — missed answer
inline constexpr QColor COLOR_A{100, 149, 237};            // Cornflower blue
inline constexpr QColor COLOR_B{60, 179, 113};             // Medium sea green
}  // namespace AnnotationColors

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

    /// Convert a mouse event position to board cell coordinates, or nullopt if outside the board
    [[nodiscard]] std::optional<core::Position> hitTestCell(const QMouseEvent* event, int widget_width,
                                                            int widget_height) const;

    /// Compute the next cell position after an arrow key press (with wrapping)
    [[nodiscard]] static std::optional<core::Position> arrowNavigate(const core::Position& current, int qt_key);

    /// Font size for placed cell values, scaled to cell height
    [[nodiscard]] static int valueFontSize(float cell_height);

    /// Layout metrics for the 3x3 candidate grid within a cell
    struct CandidateLayout {
        float note_w;
        float note_h;
        int font_size;

        /// Rectangle for a specific candidate value (1-9) within the cell
        [[nodiscard]] QRectF noteRect(const QRectF& cell_rect, int value) const;
    };

    /// Compute candidate layout metrics for a cell of the given height
    [[nodiscard]] static CandidateLayout candidateLayout(float cell_height);

    /// Paint a subtle hover tint overlay on a cell
    void paintHoverTint(QPainter& painter, const QRectF& cell_rect) const;

    /// Map a CellRole to its display color
    [[nodiscard]] static QColor cellRoleColor(core::CellRole role);

    /// Map a player color index (1=A, 2=B) to a lightened background color
    [[nodiscard]] static QColor playerColorBackground(int player_color);

private:
    Config config_;
};

}  // namespace sudoku::view
