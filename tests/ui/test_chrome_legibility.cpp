// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Regression pin for story 6-6a [regression][bug-darkmode]: in a dark OS color
// scheme the always-visible chrome text (toolbar labels, the difficulty combo,
// the rating link, and the status-bar labels) ghosted light-on-light against the
// forced-light chrome. The fix gives each ghosted widget an explicit, static dark
// text color. Written RED-first: against the unfixed widgets (which inherit the
// dark palette's near-white text) the dark-color assertions below fail.
//
// The chrome backgrounds are already forced light in BOTH schemes, so a single
// static dark text color is correct in both — this test sets a dark QApplication
// palette to simulate dark mode and asserts the resolved per-widget text color is
// the explicit dark color, not the inherited near-white.

#include "view/color_utils.h"
#include "view/main_window.h"
#include "view/style_colors.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QStatusBar>
#include <QTest>
#include <QToolBar>
#include <QToolButton>

using sudoku::view::contrastRatio;
using sudoku::view::MainWindow;
namespace StyleColors = sudoku::view::StyleColors;

namespace {

constexpr double kMinAaContrast = 4.5;

[[nodiscard]] QPalette darkScheme() {
    QPalette pal;
    const QColor bg(30, 30, 30);
    const QColor fg(240, 240, 240);  // near-white — the ghosting source
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Text, fg);
    pal.setColor(QPalette::Button, bg);
    pal.setColor(QPalette::ButtonText, fg);
    return pal;
}

[[nodiscard]] QPalette lightScheme() {
    QPalette pal;
    const QColor bg(255, 255, 255);
    const QColor fg(0, 0, 0);
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Text, fg);
    pal.setColor(QPalette::Button, bg);
    pal.setColor(QPalette::ButtonText, fg);
    return pal;
}

// Resolved foreground color of a named QLabel in the window (invalid QColor if not found).
[[nodiscard]] QColor labelColor(const MainWindow& window, const char* name) {
    auto* label = window.findChild<QLabel*>(name);
    return label != nullptr ? label->palette().color(QPalette::WindowText) : QColor();
}

}  // namespace

class TestChromeLegibility : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void toolbarTextIsExplicitDarkInDarkMode();
    void doneEditingButtonTextIsExplicitDarkInDarkMode();
    void statusBarTextIsExplicitDarkInDarkMode();
    void chromeBackgroundsStayLightInDarkMode();
    void chromeTextClearsAaContrastInDarkMode();
    void lightModeRendersTheSameStaticColors();
    void hintsBadgeKeepsItsWhiteText();

    // NOLINTNEXTLINE(readability-redundant-access-specifiers) — Qt needs `private slots:` above.
private:
    QPalette original_palette_;
};

void TestChromeLegibility::initTestCase() {
    original_palette_ = QApplication::palette();
}

void TestChromeLegibility::cleanupTestCase() {
    QApplication::setPalette(original_palette_);
}

void TestChromeLegibility::toolbarTextIsExplicitDarkInDarkMode() {
    QApplication::setPalette(darkScheme());
    MainWindow window;

    const QColor expected(StyleColors::TEXT_NEAR_BLACK);
    QCOMPARE(labelColor(window, "difficultyLabel"), expected);
    QCOMPARE(labelColor(window, "hintsTextLabel"), expected);

    auto* rating = window.findChild<QPushButton*>("ratingButton");
    QVERIFY(rating != nullptr);
    QCOMPARE(rating->palette().color(QPalette::ButtonText), expected);

    auto* combo = window.findChild<QComboBox*>("difficultyCombo");
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->palette().color(QPalette::Text), expected);
    // The combo's resting text renders via its stylesheet on native styles (palette can be ignored),
    // so pin the stylesheet path too — not just the palette read above.
    QVERIFY(combo->styleSheet().contains(StyleColors::TEXT_NEAR_BLACK));
}

void TestChromeLegibility::doneEditingButtonTextIsExplicitDarkInDarkMode() {
    QApplication::setPalette(darkScheme());
    MainWindow window;

    // "Done Editing" is a toolbar QAction rendered as a QToolButton — always-visible chrome while
    // editing a custom puzzle, on the same forced-light toolbar.
    auto* done_editing = window.findChild<QToolButton*>("doneEditingButton");
    QVERIFY(done_editing != nullptr);
    QCOMPARE(done_editing->palette().color(QPalette::ButtonText), QColor(StyleColors::TEXT_NEAR_BLACK));
}

void TestChromeLegibility::statusBarTextIsExplicitDarkInDarkMode() {
    QApplication::setPalette(darkScheme());
    MainWindow window;

    const QColor expected(StyleColors::TEXT_MUTED);
    QCOMPARE(labelColor(window, "timerLabel"), expected);
    QCOMPARE(labelColor(window, "statusLabel"), expected);
    QCOMPARE(labelColor(window, "modifierHintLabel"), expected);
    QCOMPARE(labelColor(window, "sessionTimerLabel"), expected);
}

void TestChromeLegibility::chromeBackgroundsStayLightInDarkMode() {
    // The whole fix relies on the chrome backgrounds being forced light in BOTH OS schemes (a single
    // static dark text color is only legible if the background never goes dark). Pin that load-bearing
    // assumption so a future change that drops the background-color can't silently re-ghost the text
    // while the contrast test below still passes against the (now-stale) light constant.
    QApplication::setPalette(darkScheme());
    MainWindow window;

    auto* toolbar = window.findChild<QToolBar*>();
    QVERIFY(toolbar != nullptr);
    QVERIFY(toolbar->styleSheet().contains(StyleColors::SURFACE));
    QVERIFY(window.statusBar()->styleSheet().contains(StyleColors::SURFACE_STATUS));
}

void TestChromeLegibility::chromeTextClearsAaContrastInDarkMode() {
    QApplication::setPalette(darkScheme());
    MainWindow window;

    // Toolbar label text vs the forced-light toolbar surface (its lightness is pinned by
    // chromeBackgroundsStayLightInDarkMode, so this contrast check is not vacuous).
    QVERIFY(contrastRatio(labelColor(window, "difficultyLabel"), QColor(StyleColors::SURFACE)) >= kMinAaContrast);
    // Status label text vs the forced-light status surface.
    QVERIFY(contrastRatio(labelColor(window, "statusLabel"), QColor(StyleColors::SURFACE_STATUS)) >= kMinAaContrast);
}

void TestChromeLegibility::lightModeRendersTheSameStaticColors() {
    // The colors are static, so light mode renders the identical explicit colors —
    // legible there too, and a no-op versus what light mode already showed.
    QApplication::setPalette(lightScheme());
    MainWindow window;

    QCOMPARE(labelColor(window, "difficultyLabel"), QColor(StyleColors::TEXT_NEAR_BLACK));
    QCOMPARE(labelColor(window, "statusLabel"), QColor(StyleColors::TEXT_MUTED));
}

void TestChromeLegibility::hintsBadgeKeepsItsWhiteText() {
    QApplication::setPalette(darkScheme());
    MainWindow window;

    // The blue "10" badge sets its own color: white via stylesheet; the scoped
    // chrome fix must not touch it (its palette is left untouched, not recolored dark).
    auto* badge = window.findChild<QLabel*>("hintsBadgeLabel");
    QVERIFY(badge != nullptr);
    QVERIFY(badge->styleSheet().contains("color: white"));
    QVERIFY(badge->palette().color(QPalette::WindowText) != QColor(StyleColors::TEXT_NEAR_BLACK));
}

QTEST_MAIN(TestChromeLegibility)
#include "test_chrome_legibility.moc"
