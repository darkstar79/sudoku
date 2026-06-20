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

#include "view/color_utils.h"

#include <algorithm>
#include <cmath>

namespace sudoku::view {

namespace {

/// Linearize a single 0..255 sRGB channel, then weight per WCAG relative-luminance.
[[nodiscard]] double linearizeChannel(int channel_0_255) {
    const double srgb = static_cast<double>(channel_0_255) / 255.0;
    return srgb <= 0.03928 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
}

/// WCAG relative luminance of an opaque color in [0.0, 1.0].
[[nodiscard]] double relativeLuminance(const QColor& color) {
    return (0.2126 * linearizeChannel(color.red())) + (0.7152 * linearizeChannel(color.green())) +
           (0.0722 * linearizeChannel(color.blue()));
}

}  // namespace

double contrastRatio(const QColor& a, const QColor& b) {
    const double lum_a = relativeLuminance(a);
    const double lum_b = relativeLuminance(b);
    const double lighter = std::max(lum_a, lum_b);
    const double darker = std::min(lum_a, lum_b);
    return (lighter + 0.05) / (darker + 0.05);
}

}  // namespace sudoku::view
