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

#include <QColor>

/// Pure, view-layer color math. Lives in src/view (not src/core) because QColor is
/// a Qt type and Core/Model never depend on Qt. Mirrors the flat key_utils/locale_utils
/// helper convention; extensible — further pure color math (luminance, mix/darken) can
/// live beside contrastRatio. Scheme logic (e.g. isDarkPalette) is NOT color math and
/// belongs with the theme unit (story 6-6b), not here.
namespace sudoku::view {

/// WCAG 2.x contrast ratio between two opaque colors, in the range [1.0, 21.0].
///
/// Computes each color's relative luminance from sRGB channels (per the WCAG
/// definition) and returns (L_lighter + 0.05) / (L_darker + 0.05). The result is
/// symmetric in its arguments. Known anchor: black vs white ⇒ 21:1; identical
/// colors ⇒ 1:1. Alpha is ignored (chrome colors are opaque).
[[nodiscard]] double contrastRatio(const QColor& a, const QColor& b);

}  // namespace sudoku::view
