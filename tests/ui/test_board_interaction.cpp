// sudoku-cpp - Offline Sudoku Game
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

class TestBoardInteraction : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void clickSelectsCell();
    void clickDifferentCellMovesSelection();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    [[nodiscard]] QPoint cellCenter(size_t row, size_t col) const;
};

void TestBoardInteraction::initTestCase() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));

    // Start a game so the board has cells to interact with
    ctx_->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();
}

QPoint TestBoardInteraction::cellCenter(size_t row, size_t col) const {
    auto* board = window_->board_widget_;
    float cell_sz = board->cellSize();
    QPointF origin = board->boardOrigin();
    int x = static_cast<int>(origin.x() + (static_cast<float>(col) + 0.5F) * cell_sz);
    int y = static_cast<int>(origin.y() + (static_cast<float>(row) + 0.5F) * cell_sz);
    return {x, y};
}

void TestBoardInteraction::clickSelectsCell() {
    auto* board = window_->board_widget_;

    QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, cellCenter(3, 4));
    QApplication::processEvents();

    auto pos = window_->board_widget_->selectedCell();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->row, 3U);
    QCOMPARE(pos->col, 4U);
}

void TestBoardInteraction::clickDifferentCellMovesSelection() {
    auto* board = window_->board_widget_;

    QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, cellCenter(7, 2));
    QApplication::processEvents();

    auto pos = window_->board_widget_->selectedCell();
    QVERIFY(pos.has_value());
    QCOMPARE(pos->row, 7U);
    QCOMPARE(pos->col, 2U);
}

QTEST_MAIN(TestBoardInteraction)
#include "test_board_interaction.moc"
