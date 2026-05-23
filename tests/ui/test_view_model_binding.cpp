// sudoku - Offline Sudoku Game
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

    // After starting a game, status should show playing text
    QCOMPARE(window.status_label_->text(), QString("Playing"));
}

void TestViewModelBinding::statusLabelShowsReadyBeforeGame() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);

    // Before starting any game, should show ready text
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

    // Before game: rating toolbar entry hidden
    QVERIFY(!window.rating_action_->isVisible());

    ctx.game_vm->startNewGame(core::Difficulty::Easy);

    // After game: rating toolbar entry should be visible (puzzle rating > 0).
    // Check the QAction, not rating_btn_->isVisible(): the QWidget visibility
    // of a toolbar-embedded button depends on the toolbar's own layout pass,
    // which on the Windows offscreen platform never completes for a
    // never-mapped widget — leaving isVisible() permanently false even
    // though the action is shown. rating_action_->isVisible() reflects the
    // semantic state that updateToolBar() actually controls.
    QTRY_VERIFY(window.rating_action_->isVisible());
    QVERIFY(!window.rating_btn_->text().isEmpty());
}

void TestViewModelBinding::ratingButtonShowsTechniqueCount() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ctx.game_vm->startNewGame(core::Difficulty::Medium);
    QApplication::processEvents();

    // Rating text should be non-empty (localized format)
    QString text = window.rating_btn_->text();
    QVERIFY(!text.isEmpty());
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

    // Fill Notes button is checkable (toggle: fill on first click, clear on second)
    QVERIFY(window.auto_notes_btn_->isCheckable());

    // Clicking fills notes
    ctx.game_vm->fillNotes();
    QApplication::processEvents();

    // Verify some notes were filled (at least one empty cell should have notes)
    const auto& state = ctx.game_vm->gameState.get();
    bool has_notes = false;
    for (size_t r = 0; r < 9 && !has_notes; ++r) {
        for (size_t c = 0; c < 9 && !has_notes; ++c) {
            if (!state.getCell(r, c).notes.empty()) {
                has_notes = true;
            }
        }
    }
    QVERIFY(has_notes);
}

QTEST_MAIN(TestViewModelBinding)
#include "test_view_model_binding.moc"
