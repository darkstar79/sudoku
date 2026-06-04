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

#include "key_utils.h"

namespace sudoku::view {

namespace {

/// Map a platform native virtual key on the digit row to its digit (1..9), or 0 if the key is
/// not a digit-row key. Used only to recover Shift+digit, where the layout-cooked key() is a
/// glyph (e.g. Key_Exclam on US) but the physical key is stable per platform.
[[nodiscard]] int nativeDigitRow(quint32 native_virtual_key) {
#ifdef Q_OS_MACOS
    // Carbon kVK_ANSI_* virtual key codes (what the cocoa backend reports).
    switch (native_virtual_key) {
        case 0x12:
            return 1;
        case 0x13:
            return 2;
        case 0x14:
            return 3;
        case 0x15:
            return 4;
        case 0x17:
            return 5;
        case 0x16:
            return 6;
        case 0x1A:
            return 7;
        case 0x1C:
            return 8;
        case 0x19:
            return 9;
        default:
            return 0;
    }
#else
    // Windows VK codes ('1'..'9' == 0x31..0x39) and X11 keysyms (XK_1..XK_9 == 0x31..0x39)
    // both coincide with the ASCII digits.
    if (native_virtual_key >= 0x31 && native_virtual_key <= 0x39) {
        return static_cast<int>(native_virtual_key) - 0x30;
    }
    return 0;
#endif
}

}  // namespace

std::optional<int> resolveDigit(int key, Qt::KeyboardModifiers modifiers, quint32 native_virtual_key) {
    // Primary path: Qt::Key_1..Key_9 survive intact for plain digits, Ctrl+digit, Alt+digit,
    // numpad digits, AND synthesized Shift+digit (QTest sends Key_1 + Shift, not the cooked
    // glyph). This layout-stable core is the only path most callers ever exercise.
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        return key - Qt::Key_1 + 1;
    }
    // Shift on a physical keyboard cooks the digit into a layout glyph; recover it from the
    // native (physical) key. Only Shift remaps the key, so this is the sole case that needs it.
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        if (const int digit = nativeDigitRow(native_virtual_key); digit != 0) {
            return digit;
        }
    }
    return std::nullopt;
}

std::optional<viewmodel::InputMode> overrideLayerFor(Qt::KeyboardModifiers modifiers) {
    // On macOS Qt maps Cmd→ControlModifier and Option→AltModifier, so the same checks cover
    // both platforms' "value / pencil / color" chords.
    if (modifiers.testFlag(Qt::ControlModifier)) {
        return viewmodel::InputMode::Normal;  // value
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        return viewmodel::InputMode::Notes;  // pencil
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        return viewmodel::InputMode::Color;  // color
    }
    return std::nullopt;
}

}  // namespace sudoku::view
