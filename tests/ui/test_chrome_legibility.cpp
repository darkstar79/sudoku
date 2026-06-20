// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Regression pin for story 6-6a [regression][bug-darkmode]: in a dark OS color scheme the
// always-visible chrome text (toolbar labels, the difficulty combo, the "Done Editing" button, the
// rating link, and the status-bar labels) ghosted light-on-light against the forced-light chrome.
//
// The fix gives chrome text an explicit, static dark color via ONE mechanism per region — a scoped
// stylesheet `color` rule (the toolbar colors its labels/combo/toolbuttons; the status bar colors
// its labels). These tests assert that rule is present (it is the mechanism that actually renders the
// color — Qt does not reflect stylesheet `color` back into a widget's palette). The colors are static
// and scheme-independent, so the load-bearing guarantees are: (a) the chrome backgrounds stay forced
// light in both schemes, and (b) those text/background pairs clear the WCAG 4.5:1 AA bar — the latter
// pinned purely in test_color_utils.cpp. The toolbar rule is RED-first: before the fix the toolbar
// stylesheet carried no `color`, so the dark-color assertion failed.

#include "view/main_window.h"
#include "view/style_colors.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTest>
#include <QToolBar>
#include <QToolButton>

using sudoku::view::MainWindow;
namespace StyleColors = sudoku::view::StyleColors;

namespace {
// Style sheet of a widget, "" if null — keeps the static analyzer happy about findChild()'s null
// path without scattering guards (QVERIFY's return-on-failure isn't modelled by clang-analyzer).
[[nodiscard]] QString sheetOf(const QWidget* widget) {
    return widget != nullptr ? widget->styleSheet() : QString();
}
}  // namespace

class TestChromeLegibility : public QObject {
    Q_OBJECT

private slots:
    void toolbarTextHasExplicitDarkColor();
    void doneEditingButtonIsAToolButtonTheRuleCovers();
    void statusBarTextHasExplicitMutedColor();
    void chromeBackgroundsStayLight();
    void hintsBadgeKeepsItsWhiteText();
};

void TestChromeLegibility::toolbarTextHasExplicitDarkColor() {
    MainWindow window;

    auto* toolbar = window.findChild<QToolBar*>();
    QVERIFY(toolbar != nullptr);
    // The scoped rule colors the toolbar's labels, combo, and toolbuttons dark.
    QVERIFY(sheetOf(toolbar).contains("QToolBar QLabel"));
    QVERIFY(sheetOf(toolbar).contains(StyleColors::TEXT_NEAR_BLACK));

    // The flat rating link carries its dark resting color in its own stylesheet (it wins over the
    // ancestor rule because it has other rules of its own).
    auto* rating = window.findChild<QPushButton*>("ratingButton");
    QVERIFY(rating != nullptr);
    QVERIFY(sheetOf(rating).contains(StyleColors::TEXT_NEAR_BLACK));
}

void TestChromeLegibility::doneEditingButtonIsAToolButtonTheRuleCovers() {
    MainWindow window;

    // "Done Editing" is a toolbar QAction; it must render as a QToolButton for the scoped
    // QToolBar QToolButton color rule to reach it (always-visible chrome while editing a puzzle).
    auto* done_editing = window.findChild<QToolButton*>("doneEditingButton");
    QVERIFY(done_editing != nullptr);

    auto* toolbar = window.findChild<QToolBar*>();
    QVERIFY(toolbar != nullptr);
    QVERIFY(sheetOf(toolbar).contains("QToolButton"));
}

void TestChromeLegibility::statusBarTextHasExplicitMutedColor() {
    MainWindow window;
    // The QStatusBar `color` cascades to its child labels (timer/status/modifier-hint/session-timer).
    QVERIFY(sheetOf(window.statusBar()).contains(StyleColors::TEXT_MUTED));
}

void TestChromeLegibility::chromeBackgroundsStayLight() {
    // The whole fix relies on the chrome backgrounds being forced light in BOTH OS schemes (a single
    // static dark text color is only legible if the background never goes dark). Pin that load-bearing
    // assumption so a future change that drops the background-color can't silently re-ghost the text.
    MainWindow window;

    auto* toolbar = window.findChild<QToolBar*>();
    QVERIFY(toolbar != nullptr);
    QVERIFY(sheetOf(toolbar).contains(StyleColors::SURFACE));
    QVERIFY(sheetOf(window.statusBar()).contains(StyleColors::SURFACE_STATUS));
}

void TestChromeLegibility::hintsBadgeKeepsItsWhiteText() {
    MainWindow window;

    // The blue "10" badge sets its own color: white via stylesheet; that own rule wins over the
    // ancestor QToolBar QLabel dark rule, so the badge text stays white on the blue background.
    auto* badge = window.findChild<QLabel*>("hintsBadgeLabel");
    QVERIFY(badge != nullptr);
    QVERIFY(sheetOf(badge).contains("color: white"));
}

QTEST_MAIN(TestChromeLegibility)
#include "test_chrome_legibility.moc"
