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

/// CSS color constants for Qt stylesheet use.
/// Board-level paint colors (QColor) live in BoardColors (board_painter.h),
/// SudokuBoardColors (sudoku_board_widget.h), and AnnotationColors (board_painter.h).
namespace sudoku::view::StyleColors {

// Primary action (blue buttons, badges, highlights)
inline constexpr auto PRIMARY = "#2563eb";
inline constexpr auto PRIMARY_DARK = "#1d4ed8";

// Surfaces and chrome
inline constexpr auto BACKGROUND_WARM = "#FAF8F0";
inline constexpr auto SURFACE = "#f5f5f5";
inline constexpr auto SURFACE_STATUS = "#f0f0f0";
inline constexpr auto DIVIDER = "#ddd";

// Text
inline constexpr auto TEXT_MUTED = "#666";
inline constexpr auto TEXT_SUBTLE = "#555";
inline constexpr auto TEXT_NEAR_BLACK = "#333";

// Buttons (neutral)
inline constexpr auto BTN_BG = "#e5e7eb";
inline constexpr auto BTN_TEXT = "#374151";
inline constexpr auto BTN_BORDER = "#d1d5db";
inline constexpr auto BTN_DISABLED_TEXT = "#9ca3af";
inline constexpr auto BTN_DISABLED_BG = "#f3f4f6";

// Feedback states
inline constexpr auto SUCCESS = "#2e7d32";
inline constexpr auto WARNING = "#cc9900";
inline constexpr auto ERROR_COLOR = "#dc2626";

// Training color palette buttons
inline constexpr auto PALETTE_A = "#a0c4ff";
inline constexpr auto PALETTE_B = "#a0ffcc";

// Toast
inline constexpr auto TOAST_BG = "rgba(40, 40, 40, 230)";
inline constexpr auto TOAST_TEXT = "#00cc00";

// Hint text
inline constexpr auto HINT_TEXT = "#0052cc";

// Coaching panel
inline constexpr auto COACHING_BG = "#e8f0fe";
inline constexpr auto COACHING_BORDER = "#c2d7f2";
inline constexpr auto COACHING_TEXT = "#1a3a5c";

}  // namespace sudoku::view::StyleColors
