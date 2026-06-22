// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Story 6.8 — Pause mode. Exercises the View wiring that turns the existing (but previously
// unwired) Pause/Resume verbs into a real feature: a toolbar button + P shortcut that toggle
// the timer, an opaque board overlay that HIDES the grid while paused, input gating, and a
// paused click that resumes. Offscreen QTest (QT_QPA_PLATFORM=offscreen).

#include "test_fixture.h"
#include "view/main_window.h"
#include "view/sudoku_board_widget.h"

#include <QImage>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

using namespace sudoku;
using viewmodel::GameCommand;

class TestPauseMode : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void pauseButtonTogglesPauseState();
    void shortcutTogglesPauseState();
    void overlayHidesGridWhilePaused();
    void inputIsGatedWhilePaused();
    void clickWhilePausedResumes();
    void pauseDisabledWhenNoGame();

    // Qt needs `private slots:` for the test methods; the data members live in a plain private
    // block below.
    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    void freshRunningGame();
    [[nodiscard]] QPoint cellCenter(size_t row, size_t col) const;
};

void TestPauseMode::initTestCase() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

void TestPauseMode::freshRunningGame() {
    ctx_->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();
    QVERIFY(!window_->board_widget_->isPaused());
}

QPoint TestPauseMode::cellCenter(size_t row, size_t col) const {
    auto* board = window_->board_widget_;
    float cell_sz = board->cellSize();
    QPointF origin = board->boardOrigin();
    int x = static_cast<int>(origin.x() + ((static_cast<float>(col) + 0.5F) * cell_sz));
    int y = static_cast<int>(origin.y() + ((static_cast<float>(row) + 0.5F) * cell_sz));
    return {x, y};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — flat QVERIFY/QCOMPARE sequence, no logic
void TestPauseMode::pauseButtonTogglesPauseState() {
    freshRunningGame();
    auto* btn = window_->pause_btn_;
    QVERIFY(btn != nullptr);
    if (btn == nullptr) {
        return;  // clang-analyzer: QVERIFY doesn't abort in its view, so guard the deref explicitly
    }
    QVERIFY(btn->isEnabled());
    const QString running_label = btn->text();

    btn->click();
    QApplication::processEvents();
    QVERIFY(window_->board_widget_->isPaused());
    QVERIFY(btn->isEnabled());  // still enabled — now it resumes
    QVERIFY(btn->text() != running_label);
    QVERIFY(ctx_->game_vm->canExecuteCommand(GameCommand::ResumeGame));

    btn->click();
    QApplication::processEvents();
    QVERIFY(!window_->board_widget_->isPaused());
    QCOMPARE(btn->text(), running_label);
    QVERIFY(ctx_->game_vm->canExecuteCommand(GameCommand::PauseGame));
}

void TestPauseMode::shortcutTogglesPauseState() {
    freshRunningGame();

    QTest::keyClick(window_.get(), Qt::Key_P);
    QApplication::processEvents();
    QVERIFY(window_->board_widget_->isPaused());

    QTest::keyClick(window_.get(), Qt::Key_P);
    QApplication::processEvents();
    QVERIFY(!window_->board_widget_->isPaused());
}

void TestPauseMode::overlayHidesGridWhilePaused() {
    freshRunningGame();
    auto* board = window_->board_widget_;

    const QImage running = board->grab().toImage();

    ctx_->game_vm->executeCommand(GameCommand::PauseGame);
    QApplication::processEvents();
    QVERIFY(board->isPaused());

    const QImage paused = board->grab().toImage();
    QVERIFY(paused != running);  // the overlay drew over the grid

    // The overlay is opaque: four corner-cell centres (clear of the centred "Paused" text) must
    // all be the SAME colour — i.e. givens/values/notes/highlights are no longer visible.
    const QColor c00 = paused.pixelColor(cellCenter(0, 0));
    const QColor c08 = paused.pixelColor(cellCenter(0, core::BOARD_SIZE - 1));
    const QColor c80 = paused.pixelColor(cellCenter(core::BOARD_SIZE - 1, 0));
    const QColor c88 = paused.pixelColor(cellCenter(core::BOARD_SIZE - 1, core::BOARD_SIZE - 1));
    QCOMPARE(c00, c08);
    QCOMPARE(c00, c80);
    QCOMPARE(c00, c88);

    ctx_->game_vm->executeCommand(GameCommand::ResumeGame);
    QApplication::processEvents();
}

void TestPauseMode::inputIsGatedWhilePaused() {
    freshRunningGame();
    auto* board = window_->board_widget_;
    board->setSelectedCell(core::Position{.row = 4, .col = 4});

    ctx_->game_vm->executeCommand(GameCommand::PauseGame);
    QApplication::processEvents();

    QSignalSpy digit_spy(board, &view::SudokuBoardWidget::digitKeyPressed);
    QTest::keyClick(board, Qt::Key_5);
    QApplication::processEvents();

    const auto selected = board->selectedCell();
    QCOMPARE(digit_spy.count(), 0);                                             // no digit reaches the board
    QVERIFY(selected.has_value() && selected->row == 4 && selected->col == 4);  // selection untouched
    QVERIFY(board->isPaused());                                                 // a digit key does not resume

    ctx_->game_vm->executeCommand(GameCommand::ResumeGame);
    QApplication::processEvents();
}

void TestPauseMode::clickWhilePausedResumes() {
    freshRunningGame();
    auto* board = window_->board_widget_;
    board->setSelectedCell(core::Position{.row = 1, .col = 1});

    ctx_->game_vm->executeCommand(GameCommand::PauseGame);
    QApplication::processEvents();
    QVERIFY(board->isPaused());

    QSignalSpy resume_spy(board, &view::SudokuBoardWidget::resumeRequested);
    QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, cellCenter(6, 6));
    QApplication::processEvents();

    const auto selected = board->selectedCell();
    QCOMPARE(resume_spy.count(), 1);                                            // the paused click asks to resume
    QVERIFY(!board->isPaused());                                                // and the game resumes
    QVERIFY(selected.has_value() && selected->row == 1 && selected->col == 1);  // selection NOT moved to (6,6)
}

void TestPauseMode::pauseDisabledWhenNoGame() {
    // A fresh window with no game loaded: nothing to pause.
    test::UITestContext fresh;
    view::MainWindow window;
    fresh.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QApplication::processEvents();

    QVERIFY(window.pause_btn_ != nullptr);
    QVERIFY(!window.pause_btn_->isEnabled());
    QVERIFY(!fresh.game_vm->canExecuteCommand(GameCommand::PauseGame));
    QVERIFY(!fresh.game_vm->canExecuteCommand(GameCommand::ResumeGame));
}

QTEST_MAIN(TestPauseMode)
#include "test_pause_mode.moc"
