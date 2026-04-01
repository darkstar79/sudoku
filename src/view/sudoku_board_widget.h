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

#include "../core/i_localization_manager.h"
#include "../model/game_state.h"
#include "../view_model/game_view_model.h"
#include "board_painter.h"

#include <cstddef>
#include <memory>
#include <string_view>

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

/// Widget-specific color constants (shared colors are in BoardColors in board_painter.h).
namespace SudokuBoardColors {
inline constexpr QColor TEXT_USER{0, 82, 204};
inline constexpr QColor TEXT_ERROR{220, 38, 38};
inline constexpr QColor TEXT_HINT{255, 165, 0};
inline constexpr QColor TEXT_NOTE{107, 107, 107};

// Analysis colors for free-form cell coloring (1-6)
inline constexpr QColor ANALYSIS_COLOR_1{180, 210, 255};  // Soft blue
inline constexpr QColor ANALYSIS_COLOR_2{180, 235, 195};  // Soft green
inline constexpr QColor ANALYSIS_COLOR_3{255, 215, 170};  // Soft orange
inline constexpr QColor ANALYSIS_COLOR_4{215, 190, 255};  // Soft purple
inline constexpr QColor ANALYSIS_COLOR_5{255, 190, 190};  // Soft red
inline constexpr QColor ANALYSIS_COLOR_6{255, 245, 170};  // Soft yellow
}  // namespace SudokuBoardColors

class SudokuBoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit SudokuBoardWidget(QWidget* parent = nullptr);

    void setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model);
    void setLocalizationManager(std::shared_ptr<core::ILocalizationManager> loc_manager);

    [[nodiscard]] float cellSize() const;
    [[nodiscard]] QPointF boardOrigin() const;
    void clearHoverHighlight();

    void selectCell(const core::Position& pos);
    void selectCell(size_t row, size_t col);
    [[nodiscard]] std::optional<core::Position> selectedCell() const;
    void clearSelection();

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    std::shared_ptr<viewmodel::GameViewModel> view_model_;
    std::shared_ptr<core::ILocalizationManager> loc_manager_;
    BoardPainter painter_;

    [[nodiscard]] std::string_view loc(std::string_view key) const {
        return loc_manager_ ? loc_manager_->getString(key) : key;
    }
    int hovered_candidate_{0};                     ///< Currently hovered candidate value (0 = none)
    std::optional<core::Position> hovered_cell_;   ///< Currently hovered cell (nullopt = mouse outside board)
    std::optional<core::Position> selected_cell_;  ///< Currently selected cell for editing

    void paintCell(QPainter& painter, const model::Cell& cell, size_t row, size_t col, const QPointF& origin,
                   float cell_size, bool is_selected, bool is_region_highlight, bool is_same_value_highlight);
    void paintCellValue(QPainter& painter, const model::Cell& cell, const QRectF& cell_rect);
    void paintCellNotes(QPainter& painter, const model::Cell& cell, const QRectF& cell_rect);

#ifdef SUDOKU_UI_TESTING
    friend class ::TestBoardInteraction;
#endif
};

}  // namespace sudoku::view
