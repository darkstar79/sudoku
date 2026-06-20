// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Unit tests for the pure WCAG contrastRatio() helper (story 6-6a). Lives under
// tests/ui because contrastRatio takes QColor (Qt6::Gui), which the Qt-Core-only
// unit_tests / sudoku_lib target deliberately excludes (src/view is stripped there).
// Tagged conceptually [regression][bug-darkmode]: the chrome-pair cases below pin
// that the chosen chrome text/background pairs clear the WCAG 4.5:1 legibility bar
// that the dark-mode ghosting violated.

#include "view/color_utils.h"
#include "view/style_colors.h"

#include <QColor>
#include <QTest>
#include <QtGlobal>

using sudoku::view::contrastRatio;
namespace StyleColors = sudoku::view::StyleColors;

namespace {
// WCAG 4.5:1 is the AA threshold for normal-size text.
constexpr double kMinAaContrast = 4.5;
}  // namespace

class TestColorUtils : public QObject {
    Q_OBJECT

private slots:
    void blackOnWhiteIsMaxContrast();
    void contrastIsSymmetric();
    void identicalColorsAreMinContrast();
    void chromeTextPairsClearAaThreshold();
};

void TestColorUtils::blackOnWhiteIsMaxContrast() {
    // Known WCAG anchor: pure black vs pure white is exactly 21:1.
    QVERIFY(qAbs(contrastRatio(QColor("#000"), QColor("#fff")) - 21.0) < 1e-9);
}

void TestColorUtils::contrastIsSymmetric() {
    const QColor fg("#333");
    const QColor bg("#f5f5f5");
    QVERIFY(qAbs(contrastRatio(fg, bg) - contrastRatio(bg, fg)) < 1e-12);
}

void TestColorUtils::identicalColorsAreMinContrast() {
    QVERIFY(qAbs(contrastRatio(QColor("#666"), QColor("#666")) - 1.0) < 1e-9);
}

void TestColorUtils::chromeTextPairsClearAaThreshold() {
    // The previously-ghosted chrome text colors, on their forced-light chrome
    // backgrounds, must clear the AA bar — this is the objective, non-visual
    // proof that the light-on-light ghosting is gone.
    QVERIFY(contrastRatio(QColor(StyleColors::TEXT_NEAR_BLACK), QColor(StyleColors::SURFACE)) >= kMinAaContrast);
    QVERIFY(contrastRatio(QColor(StyleColors::TEXT_NEAR_BLACK), QColor(StyleColors::BACKGROUND_WARM)) >=
            kMinAaContrast);
    QVERIFY(contrastRatio(QColor(StyleColors::TEXT_MUTED), QColor(StyleColors::SURFACE_STATUS)) >= kMinAaContrast);
}

QTEST_MAIN(TestColorUtils)
#include "test_color_utils.moc"
