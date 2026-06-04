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

#include "keyboard_shortcuts.h"

#include "core/constants.h"
#include "core/i18n_helpers.h"
#include "view_model/game_view_model.h"  // viewmodel::kColorPaletteSize

#include <QKeyCombination>
#include <QKeySequence>

namespace sudoku::view {

namespace {

// Render just the modifier portion of a chord via NativeText. QKeySequence needs a key to
// format, so we attach a representative digit and strip its native rendering off the end —
// the modifier glyph(s) themselves always come from Qt (⌘/⌥/⇧ on macOS, Ctrl/Alt/Shift else).
[[nodiscard]] QString modifierPrefixNative(Qt::KeyboardModifiers modifiers) {
    if (modifiers == Qt::NoModifier) {
        return {};
    }
    const QString full = QKeySequence(QKeyCombination(modifiers, Qt::Key_1)).toString(QKeySequence::NativeText);
    const QString key = QKeySequence(QKeyCombination(Qt::NoModifier, Qt::Key_1)).toString(QKeySequence::NativeText);
    if (full.endsWith(key)) {
        return full.left(full.size() - key.size());
    }
    return full;
}

}  // namespace

std::vector<ShortcutEntry> keyboardShortcuts() {
    return {
        {.action = ShortcutAction::ActiveModeDigit,
         .modifiers = Qt::NoModifier,
         .key = Qt::Key_1,
         .digit_family = true,
         .digit_max = core::MAX_VALUE},
        {.action = ShortcutAction::ValueOverride,
         .modifiers = Qt::ControlModifier,
         .key = Qt::Key_1,
         .digit_family = true,
         .digit_max = core::MAX_VALUE},
        {.action = ShortcutAction::PencilOverride,
         .modifiers = Qt::ShiftModifier,
         .key = Qt::Key_1,
         .digit_family = true,
         .digit_max = core::MAX_VALUE},
        {.action = ShortcutAction::ColorOverride,
         .modifiers = Qt::AltModifier,
         .key = Qt::Key_1,
         .digit_family = true,
         .digit_max = viewmodel::kColorPaletteSize},
        {.action = ShortcutAction::CycleMode,
         .modifiers = Qt::NoModifier,
         .key = Qt::Key_Space,
         .digit_family = false,
         .digit_max = 0},
        {.action = ShortcutAction::ClearActiveLayer,
         .modifiers = Qt::NoModifier,
         .key = Qt::Key_Delete,
         .digit_family = false,
         .digit_max = 0},
        {.action = ShortcutAction::ClearValue,
         .modifiers = Qt::ControlModifier,
         .key = Qt::Key_Delete,
         .digit_family = false,
         .digit_max = 0},
        {.action = ShortcutAction::ClearColor,
         .modifiers = Qt::AltModifier,
         .key = Qt::Key_Delete,
         .digit_family = false,
         .digit_max = 0},
        {.action = ShortcutAction::ClearPencilMarks,
         .modifiers = Qt::ShiftModifier,
         .key = Qt::Key_Delete,
         .digit_family = false,
         .digit_max = 0},
    };
}

QString shortcutActionLabel(ShortcutAction action) {
    // Literal core::loc("Sudoku", "...") calls so Qt's lupdate extracts each string.
    switch (action) {
        case ShortcutAction::ActiveModeDigit:
            return QString::fromStdString(core::loc("Sudoku", "Enter a digit in the active mode"));
        case ShortcutAction::ValueOverride:
            return QString::fromStdString(core::loc("Sudoku", "Enter a final value (any mode)"));
        case ShortcutAction::PencilOverride:
            return QString::fromStdString(core::loc("Sudoku", "Toggle a pencil mark (any mode)"));
        case ShortcutAction::ColorOverride:
            return QString::fromStdString(core::loc("Sudoku", "Apply a color (any mode)"));
        case ShortcutAction::CycleMode:
            return QString::fromStdString(core::loc("Sudoku", "Cycle input mode (Normal → Notes → Color)"));
        case ShortcutAction::ClearActiveLayer:
            return QString::fromStdString(core::loc("Sudoku", "Clear the active layer"));
        case ShortcutAction::ClearValue:
            return QString::fromStdString(core::loc("Sudoku", "Clear the value"));
        case ShortcutAction::ClearColor:
            return QString::fromStdString(core::loc("Sudoku", "Clear the color"));
        case ShortcutAction::ClearPencilMarks:
            return QString::fromStdString(core::loc("Sudoku", "Clear all pencil marks"));
    }
    return {};
}

QString nativeModifierName(Qt::KeyboardModifiers modifiers) {
    QString prefix = modifierPrefixNative(modifiers);
    if (prefix.endsWith(QLatin1Char('+'))) {
        prefix.chop(1);
    }
    return prefix;
}

QString shortcutChordText(const ShortcutEntry& entry) {
    if (entry.digit_family) {
        // En-dash range, e.g. "Ctrl+1–9" / "⌥1–6" / "1–9". The trailing digit comes after the
        // native modifier prefix; digits 1 and N are universal glyphs (no localization needed).
        return modifierPrefixNative(entry.modifiers) + QStringLiteral("1") + QChar(0x2013) +
               QString::number(entry.digit_max);
    }
    return QKeySequence(QKeyCombination(entry.modifiers, static_cast<Qt::Key>(entry.key)))
        .toString(QKeySequence::NativeText);
}

QString modifierHintText() {
    const QString separator = QStringLiteral(" · ");  // middle dot with spaces
    // Literal core::loc(...) calls (one per action word) so lupdate extracts each string.
    return nativeModifierName(Qt::ControlModifier) + QStringLiteral("=") +
           QString::fromStdString(core::loc("Sudoku", "value")) + separator + nativeModifierName(Qt::ShiftModifier) +
           QStringLiteral("=") + QString::fromStdString(core::loc("Sudoku", "pencil")) + separator +
           nativeModifierName(Qt::AltModifier) + QStringLiteral("=") +
           QString::fromStdString(core::loc("Sudoku", "color"));
}

}  // namespace sudoku::view
