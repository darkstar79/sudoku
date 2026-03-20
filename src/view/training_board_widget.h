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

#include "../core/training_types.h"
#include "board_painter.h"

#include <cstddef>
#include <optional>
#include <utility>

#include <QWidget>
#include <qcolor.h>
#include <qpoint.h>
#include <qtmetamacros.h>

namespace sudoku::view {

/// Training-specific color constants (shared colors are in BoardColors in board_painter.h).
namespace TrainingBoardColors {
inline constexpr QColor TEXT_CANDIDATE{107, 107, 107};
inline constexpr QColor TEXT_ELIMINATED{220, 38, 38};
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
}  // namespace TrainingBoardColors

/// Passive Sudoku board renderer for training exercises.
/// Displays a TrainingBoard and emits user interaction signals.
/// All mutable state is owned by TrainingViewModel.
class TrainingBoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrainingBoardWidget(QWidget* parent = nullptr);

    /// Update the board data and repaint
    void setBoard(const core::TrainingBoard& board);

    /// Update the selected cell highlight
    void setSelectedCell(std::optional<std::pair<size_t, size_t>> cell);

    /// Set read-only mode (disables interaction, used on feedback page)
    void setReadOnly(bool read_only);

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// Emitted when a cell is selected (mouse click or keyboard navigation)
    void cellSelected(size_t row, size_t col);

    /// Emitted when user presses a number key (1-9)
    void numberPressed(int value);

    /// Emitted when user presses a color key (A=1, B=2)
    void colorPressed(int color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    core::TrainingBoard board_{};
    std::optional<std::pair<size_t, size_t>> selected_cell_;
    bool read_only_{false};
    int hovered_candidate_{0};  ///< Currently hovered candidate value (0 = none)
    BoardPainter painter_;

    void paintCell(QPainter& painter, size_t row, size_t col, const QPointF& origin, float cell_size);
    void paintCellValue(QPainter& painter, const core::TrainingCellState& cell, const QRectF& cell_rect);
    void paintCellCandidates(QPainter& painter, const core::TrainingCellState& cell, const QRectF& cell_rect);

    [[nodiscard]] static QColor cellRoleColor(core::CellRole role);
    [[nodiscard]] static QColor playerColorBackground(int player_color);
};

}  // namespace sudoku::view
