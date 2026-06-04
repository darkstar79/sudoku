// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "test_fixture.h"
#include "view/main_window.h"
#include "view/sudoku_board_widget.h"

#include <QTest>

using namespace sudoku;

class TestKeyboardHandling : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void arrowKeysNavigateSelection();
    void arrowUpWrapsAround();
    void deleteKeyClears();
    void normalModeNumberKeyPlacesValue();
    void notesModeNumberKeyTogglesNote();
    void spaceKeyCyclesInputMode();
    void ctrlDigitForcesValueWithToggleOff();
    void shiftDigitTogglesPencilAndGuardsFilledCell();
    void altDigitAppliesColorWithToggleOff();
    void colorModePlainDigitAppliesColor();
    void altDigitSevenToNineIsNoOp();
    void eraseFamilyModifierCombos();
    void overrideOnGivenCellIsNoOp();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    [[nodiscard]] std::optional<core::Position> selectedPos() const {
        return window_->board_widget_->selectedCell();
    }

    [[nodiscard]] uint8_t colorAt(const core::Position& pos) const {
        return ctx_->game_vm->gameState.get().getCellColor(pos.row, pos.col);
    }

    // Select the first empty cell and return it. Fails the test (and returns a default) if the
    // board has none — returning a concrete Position keeps the call sites free of optional
    // dereferences that clang-tidy's unchecked-optional-access cannot prove safe past QVERIFY.
    core::Position selectEmptyCell();
    [[nodiscard]] core::Position givenCellOrFail() const;
};

void TestKeyboardHandling::initTestCase() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));

    ctx_->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    // Select cell (4,4) as starting point
    window_->board_widget_->setSelectedCell(core::Position{.row = 4, .col = 4});
    QApplication::processEvents();
}

void TestKeyboardHandling::arrowKeysNavigateSelection() {
    window_->board_widget_->setSelectedCell(core::Position{.row = 4, .col = 4});
    QApplication::processEvents();

    QTest::keyClick(window_->board_widget_, Qt::Key_Right);
    QApplication::processEvents();

    auto pos = selectedPos();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->col, 5U);

    QTest::keyClick(window_->board_widget_, Qt::Key_Down);
    QApplication::processEvents();

    pos = selectedPos();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->row, 5U);
}

void TestKeyboardHandling::arrowUpWrapsAround() {
    window_->board_widget_->setSelectedCell(core::Position{.row = 0, .col = 0});
    QApplication::processEvents();

    QTest::keyClick(window_->board_widget_, Qt::Key_Up);
    QApplication::processEvents();

    auto pos = selectedPos();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->row, 8U);
}

void TestKeyboardHandling::deleteKeyClears() {
    selectEmptyCell();
    auto pos = selectedPos();
    QVERIFY(pos.has_value());

    // In Normal mode (default), single press places value (number keys go to board widget)
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
    QTest::keyClick(window_->board_widget_, Qt::Key_5);
    QApplication::processEvents();

    auto cell_after_enter = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell_after_enter.value, 5);

    // Delete key propagates to MainWindow
    QTest::keyClick(window_->board_widget_, Qt::Key_Delete);
    QApplication::processEvents();

    auto cell_after_delete = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell_after_delete.value, 0);
}

void TestKeyboardHandling::normalModeNumberKeyPlacesValue() {
    selectEmptyCell();
    auto pos = selectedPos();
    QVERIFY(pos.has_value());

    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);

    QTest::keyClick(window_->board_widget_, Qt::Key_7);
    QApplication::processEvents();

    auto cell = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell.value, 7);

    // Clean up
    QTest::keyClick(window_->board_widget_, Qt::Key_Delete);
    QApplication::processEvents();
}

void TestKeyboardHandling::notesModeNumberKeyTogglesNote() {
    selectEmptyCell();
    auto pos = selectedPos();
    QVERIFY(pos.has_value());

    // Switch to Notes mode
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Notes);

    QTest::keyClick(window_->board_widget_, Qt::Key_3);
    QApplication::processEvents();

    auto cell = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell.value, 0);
    QVERIFY(cell.notes.contains(3));

    // Press again to toggle off
    QTest::keyClick(window_->board_widget_, Qt::Key_3);
    QApplication::processEvents();

    cell = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QVERIFY(!cell.notes.contains(3));

    // Restore Normal mode
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
}

void TestKeyboardHandling::spaceKeyCyclesInputMode() {
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Normal);

    // Space key propagates from board to MainWindow
    QTest::keyClick(window_->board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Notes);

    QTest::keyClick(window_->board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Color);

    QTest::keyClick(window_->board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Normal);
}

void TestKeyboardHandling::ctrlDigitForcesValueWithToggleOff() {
    const auto pos = selectEmptyCell();

    // Force a final value while NOT in Normal mode — the override must win.
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Notes);
    QTest::keyClick(window_->board_widget_, Qt::Key_8, Qt::ControlModifier);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).value, 8);

    // Same digit again clears it (toggle-off).
    QTest::keyClick(window_->board_widget_, Qt::Key_8, Qt::ControlModifier);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).value, 0);

    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
}

void TestKeyboardHandling::shiftDigitTogglesPencilAndGuardsFilledCell() {
    const auto pos = selectEmptyCell();

    // Shift forces a pencil mark while in Normal mode. QTest sends Key_4 + Shift
    // (no native cooking), exercising the resolveDigit fallback path.
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
    QTest::keyClick(window_->board_widget_, Qt::Key_4, Qt::ShiftModifier);
    QApplication::processEvents();
    QVERIFY(ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).notes.contains(4));

    // Re-press removes the mark.
    QTest::keyClick(window_->board_widget_, Qt::Key_4, Qt::ShiftModifier);
    QApplication::processEvents();
    QVERIFY(!ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).notes.contains(4));

    // Filled-cell guard: place a value, then Shift+digit is a no-op.
    QTest::keyClick(window_->board_widget_, Qt::Key_6);  // Normal mode places 6
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).value, 6);
    QTest::keyClick(window_->board_widget_, Qt::Key_2, Qt::ShiftModifier);
    QApplication::processEvents();
    QVERIFY(!ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).notes.contains(2));

    QTest::keyClick(window_->board_widget_, Qt::Key_Delete);  // cleanup
    QApplication::processEvents();
}

void TestKeyboardHandling::altDigitAppliesColorWithToggleOff() {
    const auto pos = selectEmptyCell();

    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);

    QTest::keyClick(window_->board_widget_, Qt::Key_3, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(3));

    // Different digit replaces the color.
    QTest::keyClick(window_->board_widget_, Qt::Key_5, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(5));

    // Same digit again removes it.
    QTest::keyClick(window_->board_widget_, Qt::Key_5, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(0));
}

void TestKeyboardHandling::colorModePlainDigitAppliesColor() {
    const auto pos = selectEmptyCell();

    // The previously-untested Color-mode application path: a plain digit in Color mode.
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Color);
    QTest::keyClick(window_->board_widget_, Qt::Key_2);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(2));

    // Plain Color mode is set-only (AC-1): re-pressing the same digit must NOT toggle the color off
    // (toggle-off is exclusive to the Alt override). This guards the trickiest behavioural split.
    QTest::keyClick(window_->board_widget_, Qt::Key_2);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(2));

    ctx_->game_vm->colorCell(pos, 0);  // cleanup
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
}

void TestKeyboardHandling::altDigitSevenToNineIsNoOp() {
    const auto pos = selectEmptyCell();

    QTest::keyClick(window_->board_widget_, Qt::Key_7, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(0));
    QTest::keyClick(window_->board_widget_, Qt::Key_9, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(0));
}

void TestKeyboardHandling::eraseFamilyModifierCombos() {
    const auto pos = selectEmptyCell();
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);

    // Ctrl+0 clears the value in any mode.
    QTest::keyClick(window_->board_widget_, Qt::Key_5);  // place a value
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).value, 5);
    QTest::keyClick(window_->board_widget_, Qt::Key_0, Qt::ControlModifier);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).value, 0);

    // Alt+0 clears the color.
    QTest::keyClick(window_->board_widget_, Qt::Key_4, Qt::AltModifier);  // set color 4
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(4));
    QTest::keyClick(window_->board_widget_, Qt::Key_0, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(colorAt(pos), static_cast<uint8_t>(0));

    // Shift+Delete clears all pencil marks in the cell.
    QTest::keyClick(window_->board_widget_, Qt::Key_1, Qt::ShiftModifier);  // pencil 1
    QTest::keyClick(window_->board_widget_, Qt::Key_3, Qt::ShiftModifier);  // pencil 3
    QApplication::processEvents();
    QVERIFY(ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).notes.contains(1));
    QTest::keyClick(window_->board_widget_, Qt::Key_Delete, Qt::ShiftModifier);
    QApplication::processEvents();
    QVERIFY(!ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).notes.contains(1));
    QVERIFY(!ctx_->game_vm->gameState.get().getCell(pos.row, pos.col).notes.contains(3));
}

void TestKeyboardHandling::overrideOnGivenCellIsNoOp() {
    const auto given = givenCellOrFail();
    window_->board_widget_->setSelectedCell(given);
    QApplication::processEvents();

    const int given_value = ctx_->game_vm->gameState.get().getCell(given.row, given.col).value;
    QTest::keyClick(window_->board_widget_, Qt::Key_8, Qt::ControlModifier);
    QApplication::processEvents();

    // The given is untouched by the value override.
    QCOMPARE(ctx_->game_vm->gameState.get().getCell(given.row, given.col).value, given_value);
}

core::Position TestKeyboardHandling::givenCellOrFail() const {
    const auto& state = ctx_->game_vm->gameState.get();
    for (size_t r = 0; r < core::BOARD_SIZE; ++r) {
        for (size_t c = 0; c < core::BOARD_SIZE; ++c) {
            if (state.getCell(r, c).is_given) {
                return core::Position{.row = r, .col = c};
            }
        }
    }
    QTest::qFail("No given cell found on Easy board", __FILE__, __LINE__);
    return core::Position{.row = 0, .col = 0};
}

core::Position TestKeyboardHandling::selectEmptyCell() {
    const auto& state = ctx_->game_vm->gameState.get();
    for (size_t r = 0; r < core::BOARD_SIZE; ++r) {
        for (size_t c = 0; c < core::BOARD_SIZE; ++c) {
            const auto& cell = state.getCell(r, c);
            if (cell.value == 0 && !cell.is_given) {
                const core::Position pos{.row = r, .col = c};
                window_->board_widget_->setSelectedCell(pos);
                QApplication::processEvents();
                return pos;
            }
        }
    }
    QTest::qFail("No empty cell found on Easy board", __FILE__, __LINE__);
    return core::Position{.row = 0, .col = 0};
}

QTEST_MAIN(TestKeyboardHandling)
#include "test_keyboard_handling.moc"
