// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "view/key_utils.h"
#include "view_model/game_view_model.h"

#include <optional>

#include <QTest>
#include <qnamespace.h>

using namespace sudoku;
using sudoku::view::overrideLayerFor;
using sudoku::view::resolveDigit;
using sudoku::viewmodel::InputMode;

// Direct, layout-robust unit tests for the pure digit/override helpers (AC-6, AC-8).
// These pass inputs in by hand rather than synthesizing native key events, because
// QTest::keyClick does not populate nativeVirtualKey() — the helper must be correct
// on the Key_1..Key_9 + Shift fallback path as well as the native physical-key path.
class TestResolveDigit : public QObject {
    Q_OBJECT

private slots:
    // resolveDigit -------------------------------------------------------------
    void plainDigitResolves();
    void ctrlDigitKeepsDigit();
    void altDigitKeepsDigit();
    void shiftDigitFallbackResolves();
    void shiftGlyphResolvesFromNativeKey();
    void nonDigitReturnsNullopt();
    void zeroIsNotResolvedAsDigit();

    // overrideLayerFor ---------------------------------------------------------
    void noModifierMeansNoOverride();
    void ctrlOverridesToValue();
    void shiftOverridesToPencil();
    void altOverridesToColor();
    void keypadAloneIsNoOverride();
};

void TestResolveDigit::plainDigitResolves() {
    QCOMPARE(resolveDigit(Qt::Key_5, Qt::NoModifier, 0), std::optional<int>(5));
    QCOMPARE(resolveDigit(Qt::Key_1, Qt::NoModifier, 0), std::optional<int>(1));
    QCOMPARE(resolveDigit(Qt::Key_9, Qt::NoModifier, 0), std::optional<int>(9));
}

void TestResolveDigit::ctrlDigitKeepsDigit() {
    // Ctrl does not remap the key — Key_1..Key_9 stays intact.
    QCOMPARE(resolveDigit(Qt::Key_3, Qt::ControlModifier, 0), std::optional<int>(3));
}

void TestResolveDigit::altDigitKeepsDigit() {
    QCOMPARE(resolveDigit(Qt::Key_6, Qt::AltModifier, 0), std::optional<int>(6));
}

void TestResolveDigit::shiftDigitFallbackResolves() {
    // The QTest path: synthesized Shift+digit keeps Key_1..Key_9 (no layout cooking),
    // so the helper must accept it even without a native virtual key.
    QCOMPARE(resolveDigit(Qt::Key_7, Qt::ShiftModifier, 0), std::optional<int>(7));
}

void TestResolveDigit::shiftGlyphResolvesFromNativeKey() {
    // A real US keyboard cooks Shift+1 into Key_Exclam; recover the digit from the
    // physical (native) key, which is not layout-dependent.
#if defined(Q_OS_MACOS)
    const quint32 native_one = 0x12;  // kVK_ANSI_1
#else
    const quint32 native_one = 0x31;  // Win VK '1' / X11 keysym XK_1 (both == ASCII '1')
#endif
    QCOMPARE(resolveDigit(Qt::Key_Exclam, Qt::ShiftModifier, native_one), std::optional<int>(1));
}

void TestResolveDigit::nonDigitReturnsNullopt() {
    QCOMPARE(resolveDigit(Qt::Key_A, Qt::NoModifier, 0), std::nullopt);
    QCOMPARE(resolveDigit(Qt::Key_Delete, Qt::NoModifier, 0), std::nullopt);
    // A shifted glyph without a recognizable native key cannot be resolved.
    QCOMPARE(resolveDigit(Qt::Key_Exclam, Qt::ShiftModifier, 0), std::nullopt);
}

void TestResolveDigit::zeroIsNotResolvedAsDigit() {
    // 0 is the erase key, handled separately — resolveDigit only yields 1..9.
    QCOMPARE(resolveDigit(Qt::Key_0, Qt::NoModifier, 0), std::nullopt);
    QCOMPARE(resolveDigit(Qt::Key_0, Qt::ControlModifier, 0), std::nullopt);
}

void TestResolveDigit::noModifierMeansNoOverride() {
    QCOMPARE(overrideLayerFor(Qt::NoModifier), std::optional<InputMode>(std::nullopt));
}

void TestResolveDigit::ctrlOverridesToValue() {
    QCOMPARE(overrideLayerFor(Qt::ControlModifier), std::optional<InputMode>(InputMode::Normal));
}

void TestResolveDigit::shiftOverridesToPencil() {
    QCOMPARE(overrideLayerFor(Qt::ShiftModifier), std::optional<InputMode>(InputMode::Notes));
}

void TestResolveDigit::altOverridesToColor() {
    QCOMPARE(overrideLayerFor(Qt::AltModifier), std::optional<InputMode>(InputMode::Color));
}

void TestResolveDigit::keypadAloneIsNoOverride() {
    // Numpad digits carry KeypadModifier but must still act on the active mode.
    QCOMPARE(overrideLayerFor(Qt::KeypadModifier), std::optional<InputMode>(std::nullopt));
}

QTEST_MAIN(TestResolveDigit)
#include "test_resolve_digit.moc"
