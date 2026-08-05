// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Story 8-20: pure per-cell highlight decisions (BoardPainter::cellHighlightFlags /
// noteHighlightValue / valueTextColor) plus the widget-level startup/toggle behavior. Lives under
// tests/ui because these take core::Position / QColor from sudoku_view_lib, which the Qt-Core-only
// unit_tests / sudoku_lib target deliberately excludes (src/view is stripped there).

#include "core/settings_manager.h"
#include "test_fixture.h"
#include "view/board_painter.h"
#include "view/main_window.h"
#include "view/sudoku_board_widget.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>

#include <QTest>

using namespace sudoku;
using sudoku::core::Position;
using sudoku::view::BoardPainter;
using sudoku::view::CellHighlightFlags;
using sudoku::view::HighlightOptions;
namespace SudokuBoardColors = sudoku::view::SudokuBoardColors;

class TestBoardHighlighting : public QObject {
    Q_OBJECT

    std::unique_ptr<test::UITestContext> ctx_;
    std::filesystem::path settings_dir_;
    std::filesystem::path settings_file_;

private slots:
    // Pure decisions (Task 3 — RED before Task 4's BoardPainter helpers exist)
    void noFocusMeansNoFlagsInEveryCombination();
    void cellHighlightFlagsTruthTable();
    void noteHighlightValuePrecedenceAndGating();
    void valueTextColorConflictGate();  // AC5, RED-first
    void valueTextColorNonRegression();
    void crossAidIndependence();  // AC7

    // Widget/startup behavior (Task 6 — needs Task 5's MainWindow wiring)
    void initTestCase();
    void cleanupTestCase();
    void toggleEachSettingUpdatesBoardWidget();
    void startupAppliesPersistedHighlightOptions();
    void startupAppliesConflictsFalseFromPersistedSettings();
};

// --- Pure decisions -------------------------------------------------------

void TestBoardHighlighting::noFocusMeansNoFlagsInEveryCombination() {
    for (bool regions : {false, true}) {
        for (bool same_numbers : {false, true}) {
            HighlightOptions opts{.regions = regions, .same_numbers = same_numbers, .conflicts = true};
            auto flags = BoardPainter::cellHighlightFlags(0, 0, std::nullopt, 5, 5, opts);
            QVERIFY(!flags.region);
            QVERIFY(!flags.same_value);
        }
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TestBoardHighlighting::cellHighlightFlagsTruthTable() {
    const Position focus{.row = 4, .col = 4};  // center cell, box (1,1) in 0-indexed box coords
    constexpr int focus_value = 7;

    struct Case {
        const char* name{""};
        Position cell{};
        int cell_value{0};
        bool expect_region{false};
        bool expect_same_value{false};
    };

    const std::array<Case, 6> cases{{
        {.name = "same row",
         .cell = Position{.row = 4, .col = 0},
         .cell_value = 0,
         .expect_region = true,
         .expect_same_value = false},
        {.name = "same column",
         .cell = Position{.row = 0, .col = 4},
         .cell_value = 0,
         .expect_region = true,
         .expect_same_value = false},
        {.name = "same box",
         .cell = Position{.row = 3, .col = 3},
         .cell_value = 0,
         .expect_region = true,
         .expect_same_value = false},
        {.name = "same value outside region",
         .cell = Position{.row = 8, .col = 8},
         .cell_value = focus_value,
         .expect_region = false,
         .expect_same_value = true},
        {.name = "both in-region and same-value",
         .cell = Position{.row = 4, .col = 5},
         .cell_value = focus_value,
         .expect_region = true,
         .expect_same_value = true},
        {.name = "unrelated",
         .cell = Position{.row = 0, .col = 8},
         .cell_value = 0,
         .expect_region = false,
         .expect_same_value = false},
    }};

    for (bool regions : {false, true}) {
        for (bool same_numbers : {false, true}) {
            HighlightOptions opts{.regions = regions, .same_numbers = same_numbers, .conflicts = true};
            for (const auto& c : cases) {
                auto flags =
                    BoardPainter::cellHighlightFlags(c.cell.row, c.cell.col, focus, focus_value, c.cell_value, opts);
                QCOMPARE(flags.region, regions && c.expect_region);
                QCOMPARE(flags.same_value, same_numbers && c.expect_same_value);
            }
        }
    }
}

void TestBoardHighlighting::noteHighlightValuePrecedenceAndGating() {
    HighlightOptions on{.regions = true, .same_numbers = true, .conflicts = true};
    HighlightOptions off{.regions = true, .same_numbers = false, .conflicts = true};

    // Focus value wins over hovered candidate.
    QCOMPARE(BoardPainter::noteHighlightValue(3, 7, on), 3);
    // Hovered candidate used when the focus cell is empty (focus_value == 0).
    QCOMPARE(BoardPainter::noteHighlightValue(0, 7, on), 7);
    // 0 when same_numbers is off, in both cases.
    QCOMPARE(BoardPainter::noteHighlightValue(3, 7, off), 0);
    QCOMPARE(BoardPainter::noteHighlightValue(0, 7, off), 0);
}

// RED-first: this is the live show_conflicts defect. The conflicts==false case must FAIL until
// Task 4 rewires paintCellValue through this helper.
void TestBoardHighlighting::valueTextColorConflictGate() {
    HighlightOptions conflicts_on{.regions = true, .same_numbers = true, .conflicts = true};
    HighlightOptions conflicts_off{.regions = true, .same_numbers = true, .conflicts = false};

    QCOMPARE(BoardPainter::valueTextColor(false, false, false, true, conflicts_on), SudokuBoardColors::TEXT_ERROR);
    QCOMPARE(BoardPainter::valueTextColor(false, false, false, true, conflicts_off), SudokuBoardColors::TEXT_USER);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TestBoardHighlighting::valueTextColorNonRegression() {
    for (bool conflicts : {false, true}) {
        HighlightOptions opts{.regions = true, .same_numbers = true, .conflicts = conflicts};

        // Hint-revealed always wins, regardless of conflict state.
        QCOMPARE(BoardPainter::valueTextColor(false, false, true, true, opts), SudokuBoardColors::TEXT_HINT);
        QCOMPARE(BoardPainter::valueTextColor(false, false, true, false, opts), SudokuBoardColors::TEXT_HINT);

        // Given stays TEXT_GIVEN even if (impossibly, but defensively) marked conflicted.
        QCOMPARE(BoardPainter::valueTextColor(true, false, false, true, opts), sudoku::view::BoardColors::TEXT_GIVEN);
        QCOMPARE(BoardPainter::valueTextColor(true, false, false, false, opts), sudoku::view::BoardColors::TEXT_GIVEN);

        // is_found falls through to TEXT_GIVEN, not a color of its own.
        QCOMPARE(BoardPainter::valueTextColor(false, true, false, true, opts), sudoku::view::BoardColors::TEXT_GIVEN);
        QCOMPARE(BoardPainter::valueTextColor(false, true, false, false, opts), sudoku::view::BoardColors::TEXT_GIVEN);

        // A plain user value with no conflict is always TEXT_USER.
        QCOMPARE(BoardPainter::valueTextColor(false, false, false, false, opts), SudokuBoardColors::TEXT_USER);
    }
}

void TestBoardHighlighting::crossAidIndependence() {
    const Position focus{.row = 4, .col = 4};
    constexpr int focus_value = 7;

    // Toggling `conflicts` changes no CellHighlightFlags.
    HighlightOptions base{.regions = true, .same_numbers = true, .conflicts = true};
    HighlightOptions conflicts_off = base;
    conflicts_off.conflicts = false;
    QCOMPARE(BoardPainter::cellHighlightFlags(4, 5, focus, focus_value, focus_value, base),
             BoardPainter::cellHighlightFlags(4, 5, focus, focus_value, focus_value, conflicts_off));

    // Toggling `regions`/`same_numbers` changes no valueTextColor.
    HighlightOptions regions_off = base;
    regions_off.regions = false;
    HighlightOptions same_numbers_off = base;
    same_numbers_off.same_numbers = false;
    for (bool has_conflict : {false, true}) {
        auto expected = BoardPainter::valueTextColor(false, false, false, has_conflict, base);
        QCOMPARE(BoardPainter::valueTextColor(false, false, false, has_conflict, regions_off), expected);
        QCOMPARE(BoardPainter::valueTextColor(false, false, false, has_conflict, same_numbers_off), expected);
    }
}

// --- Widget/startup behavior -----------------------------------------------

void TestBoardHighlighting::initTestCase() {
    settings_dir_ =
        std::filesystem::temp_directory_path() /
        ("ui_test_board_highlighting_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(settings_dir_);
    settings_file_ = settings_dir_ / "settings.yaml";

    ctx_ = std::make_unique<test::UITestContext>();
}

void TestBoardHighlighting::cleanupTestCase() {
    ctx_.reset();
    if (std::filesystem::exists(settings_dir_)) {
        std::filesystem::remove_all(settings_dir_);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TestBoardHighlighting::toggleEachSettingUpdatesBoardWidget() {
    auto settings = std::make_shared<core::SettingsManager>(settings_file_);

    view::MainWindow window;
    ctx_->setupMainWindow(window);
    window.setSettingsManager(settings);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* board_widget = window.findChild<view::SudokuBoardWidget*>();
    QVERIFY(board_widget != nullptr);

    // All-on by default.
    QVERIFY(board_widget->highlightOptions().regions);
    QVERIFY(board_widget->highlightOptions().same_numbers);
    QVERIFY(board_widget->highlightOptions().conflicts);

    settings->setHighlightRegions(false);
    QApplication::processEvents();
    QVERIFY(!board_widget->highlightOptions().regions);
    QVERIFY(board_widget->highlightOptions().same_numbers);
    QVERIFY(board_widget->highlightOptions().conflicts);
    settings->setHighlightRegions(true);
    QApplication::processEvents();

    settings->setHighlightSameNumbers(false);
    QApplication::processEvents();
    QVERIFY(!board_widget->highlightOptions().same_numbers);
    settings->setHighlightSameNumbers(true);
    QApplication::processEvents();

    settings->setShowConflicts(false);
    QApplication::processEvents();
    QVERIFY(!board_widget->highlightOptions().conflicts);
    settings->setShowConflicts(true);
    QApplication::processEvents();
}

// D1 regression class (story 8-19): assert the STARTUP direction too — bind an already-flipped
// settings manager to a FRESH MainWindow, not just the toggle.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TestBoardHighlighting::startupAppliesPersistedHighlightOptions() {
    auto settings = std::make_shared<core::SettingsManager>(settings_file_);
    settings->setHighlightRegions(false);
    settings->setHighlightSameNumbers(false);

    view::MainWindow window;
    ctx_->setupMainWindow(window);
    window.setSettingsManager(settings);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* board_widget = window.findChild<view::SudokuBoardWidget*>();
    QVERIFY(board_widget != nullptr);
    QVERIFY(!board_widget->highlightOptions().regions);
    QVERIFY(!board_widget->highlightOptions().same_numbers);

    settings->setHighlightRegions(true);
    settings->setHighlightSameNumbers(true);
}

// Same startup-direction proof for `conflicts` — the one of the three flags with a persisted
// `false` in real users' files today (the dead-toggle defect this story fixes).
void TestBoardHighlighting::startupAppliesConflictsFalseFromPersistedSettings() {
    auto settings = std::make_shared<core::SettingsManager>(settings_file_);
    settings->setShowConflicts(false);

    view::MainWindow window;
    ctx_->setupMainWindow(window);
    window.setSettingsManager(settings);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* board_widget = window.findChild<view::SudokuBoardWidget*>();
    QVERIFY(board_widget != nullptr);
    QVERIFY(!board_widget->highlightOptions().conflicts);

    settings->setShowConflicts(true);
}

QTEST_MAIN(TestBoardHighlighting)
#include "test_board_highlighting.moc"
