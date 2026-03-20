// sudoku-cpp - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "test_fixture.h"
#include "view/main_window.h"

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

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    [[nodiscard]] std::optional<core::Position> selectedPos() const {
        return ctx_->game_vm->gameState.get().getSelectedPosition();
    }

    void selectEmptyCell();
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
    ctx_->game_vm->selectCell(4, 4);
    QApplication::processEvents();
}

void TestKeyboardHandling::arrowKeysNavigateSelection() {
    ctx_->game_vm->selectCell(4, 4);
    QApplication::processEvents();

    QTest::keyClick(window_.get(), Qt::Key_Right);
    QApplication::processEvents();

    auto pos = selectedPos();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->col, 5U);

    QTest::keyClick(window_.get(), Qt::Key_Down);
    QApplication::processEvents();

    pos = selectedPos();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->row, 5U);
}

void TestKeyboardHandling::arrowUpWrapsAround() {
    ctx_->game_vm->selectCell(0, 0);
    QApplication::processEvents();

    QTest::keyClick(window_.get(), Qt::Key_Up);
    QApplication::processEvents();

    auto pos = selectedPos();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->row, 8U);
}

void TestKeyboardHandling::deleteKeyClears() {
    selectEmptyCell();
    auto pos = selectedPos();
    QVERIFY(pos.has_value());

    // In Normal mode (default), single press places value
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
    QTest::keyClick(window_.get(), Qt::Key_5);
    QApplication::processEvents();

    auto cell_after_enter = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell_after_enter.value, 5);

    QTest::keyClick(window_.get(), Qt::Key_Delete);
    QApplication::processEvents();

    auto cell_after_delete = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell_after_delete.value, 0);
}

void TestKeyboardHandling::normalModeNumberKeyPlacesValue() {
    selectEmptyCell();
    auto pos = selectedPos();
    QVERIFY(pos.has_value());

    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);

    QTest::keyClick(window_.get(), Qt::Key_7);
    QApplication::processEvents();

    auto cell = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell.value, 7);

    // Clean up
    QTest::keyClick(window_.get(), Qt::Key_Delete);
    QApplication::processEvents();
}

void TestKeyboardHandling::notesModeNumberKeyTogglesNote() {
    selectEmptyCell();
    auto pos = selectedPos();
    QVERIFY(pos.has_value());

    // Switch to Notes mode
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Notes);

    QTest::keyClick(window_.get(), Qt::Key_3);
    QApplication::processEvents();

    auto cell = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QCOMPARE(cell.value, 0);
    QVERIFY(cell.notes.contains(3));

    // Press again to toggle off
    QTest::keyClick(window_.get(), Qt::Key_3);
    QApplication::processEvents();

    cell = ctx_->game_vm->gameState.get().getCell(pos->row, pos->col);
    QVERIFY(!cell.notes.contains(3));

    // Restore Normal mode
    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
}

void TestKeyboardHandling::spaceKeyCyclesInputMode() {
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Normal);

    QTest::keyClick(window_.get(), Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Notes);

    QTest::keyClick(window_.get(), Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Color);

    QTest::keyClick(window_.get(), Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Normal);
}

void TestKeyboardHandling::selectEmptyCell() {
    const auto& state = ctx_->game_vm->gameState.get();
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            const auto& cell = state.getCell(r, c);
            if (cell.value == 0 && !cell.is_given) {
                ctx_->game_vm->selectCell(r, c);
                QApplication::processEvents();
                return;
            }
        }
    }
    QFAIL("No empty cell found on Easy board");
}

QTEST_MAIN(TestKeyboardHandling)
#include "test_keyboard_handling.moc"
