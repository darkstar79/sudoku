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

#include "view_model/game_view_model.h"  // viewmodel::InputMode

#include <optional>

#include <QtGlobal>
#include <qnamespace.h>

namespace sudoku::view {

/// Resolve a board digit (1..9) from a key event in a layout- and platform-robust way.
///
/// `key` is QKeyEvent::key(), `modifiers` is QKeyEvent::modifiers(), and
/// `native_virtual_key` is QKeyEvent::nativeVirtualKey(). The helper is pure so it can be
/// unit-tested directly without synthesizing native key events (which QTest cannot do).
///
/// Returns std::nullopt for non-digit keys — including `0`, which is the erase key handled
/// separately, not a placement digit.
[[nodiscard]] std::optional<int> resolveDigit(int key, Qt::KeyboardModifiers modifiers, quint32 native_virtual_key);

/// Map modifier flags to the layer a modified digit targets: Ctrl/Cmd → value (Normal),
/// Shift → pencil (Notes), Alt/Option → color (Color). std::nullopt means "no override —
/// act on the active input mode". Non-layer modifiers (e.g. KeypadModifier) are ignored, so
/// numpad digits still act on the active mode.
[[nodiscard]] std::optional<viewmodel::InputMode> overrideLayerFor(Qt::KeyboardModifiers modifiers);

}  // namespace sudoku::view
