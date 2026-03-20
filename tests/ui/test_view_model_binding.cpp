// sudoku-cpp - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "test_fixture.h"
#include "view/main_window.h"
#include "view/toast_widget.h"

#include <QLabel>
#include <QPushButton>
#include <QTest>

using namespace sudoku;

class TestViewModelBinding : public QObject {
    Q_OBJECT

private slots:
    void statusLabelUpdatesOnNewGame();
    void statusLabelShowsReadyBeforeGame();
    void hintsLabelUpdatesOnNewGame();
    void toastWidgetExists();
    void ratingButtonVisibleAfterNewGame();
    void ratingButtonShowsTechniqueCount();
    void undoButtonDisabledBeforeAnyMove();
    void autoNotesButtonToggles();
};

void TestViewModelBinding::statusLabelUpdatesOnNewGame() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ctx.game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    // After starting a game, status should show "Playing..."
    QCOMPARE(window.status_label_->text(), QString("Playing..."));
}

void TestViewModelBinding::statusLabelShowsReadyBeforeGame() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);

    // Before starting any game, should show "Ready"
    QCOMPARE(window.status_label_->text(), QString("Ready"));
}

void TestViewModelBinding::hintsLabelUpdatesOnNewGame() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ctx.game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    // Hint count should be a number >= 0
    bool ok = false;
    int hint_count = window.hints_label_->text().toInt(&ok);
    QVERIFY(ok);
    QVERIFY(hint_count >= 0);
}

void TestViewModelBinding::toastWidgetExists() {
    view::MainWindow window;
    QVERIFY(window.toast_widget_ != nullptr);
}

void TestViewModelBinding::ratingButtonVisibleAfterNewGame() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Before game: rating button hidden
    QVERIFY(!window.rating_btn_->isVisible());

    ctx.game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    // After game: rating button should be visible (puzzle rating > 0)
    QVERIFY(window.rating_btn_->isVisible());
    QVERIFY(window.rating_btn_->text().contains("SE "));
}

void TestViewModelBinding::ratingButtonShowsTechniqueCount() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ctx.game_vm->startNewGame(core::Difficulty::Medium);
    QApplication::processEvents();

    // Rating text should mention technique count
    QString text = window.rating_btn_->text();
    QVERIFY(text.contains("SE "));
    QVERIFY(text.contains("techniques"));
}

void TestViewModelBinding::undoButtonDisabledBeforeAnyMove() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ctx.game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    // No moves yet — undo/redo should be disabled
    QVERIFY(!window.undo_btn_->isEnabled());
    QVERIFY(!window.redo_btn_->isEnabled());
}

void TestViewModelBinding::autoNotesButtonToggles() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ctx.game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    // Initially off
    QVERIFY(!window.auto_notes_btn_->isChecked());

    // Toggle on
    ctx.game_vm->toggleAutoNotes();
    QApplication::processEvents();
    QVERIFY(window.auto_notes_btn_->isChecked());

    // Toggle off
    ctx.game_vm->toggleAutoNotes();
    QApplication::processEvents();
    QVERIFY(!window.auto_notes_btn_->isChecked());
}

QTEST_MAIN(TestViewModelBinding)
#include "test_view_model_binding.moc"
