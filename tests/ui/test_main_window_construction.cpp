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

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTest>

using namespace sudoku;

class TestMainWindowConstruction : public QObject {
    Q_OBJECT

private slots:
    void constructsWithoutViewModel();
    void constructsWithViewModel();
    void centralStackDefaultsToGameBoard();
    void difficultyComboHasFiveEntries();
    void statusBarShowsReady();
    void boardWidgetExists();
    void trainingWidgetExists();
    void buttonPanelExists();
    void buttonPanelInitialState();
    void ratingButtonExists();
    void ratingButtonHiddenInitially();
};

void TestMainWindowConstruction::constructsWithoutViewModel() {
    view::MainWindow window;
    QVERIFY(window.central_stack_ != nullptr);
    QVERIFY(window.board_widget_ != nullptr);
}

void TestMainWindowConstruction::constructsWithViewModel() {
    test::UITestContext ctx;
    view::MainWindow window;
    ctx.setupMainWindow(window);

    QVERIFY(window.view_model_ != nullptr);
    QVERIFY(window.training_vm_ != nullptr);
}

void TestMainWindowConstruction::centralStackDefaultsToGameBoard() {
    view::MainWindow window;
    QCOMPARE(window.central_stack_->currentIndex(), 0);
}

void TestMainWindowConstruction::difficultyComboHasFiveEntries() {
    view::MainWindow window;
    QCOMPARE(window.difficulty_combo_->count(), 5);
    QCOMPARE(window.difficulty_combo_->itemText(0), QString("Easy"));
    QCOMPARE(window.difficulty_combo_->itemText(4), QString("Master"));
}

void TestMainWindowConstruction::statusBarShowsReady() {
    view::MainWindow window;
    QCOMPARE(window.status_label_->text(), QString("Ready"));
}

void TestMainWindowConstruction::boardWidgetExists() {
    view::MainWindow window;
    QVERIFY(window.board_widget_ != nullptr);
    QVERIFY(window.board_widget_->minimumSizeHint().width() > 0);
}

void TestMainWindowConstruction::trainingWidgetExists() {
    view::MainWindow window;
    QVERIFY(window.training_widget_ != nullptr);
}

void TestMainWindowConstruction::buttonPanelExists() {
    view::MainWindow window;
    QVERIFY(window.undo_btn_ != nullptr);
    QVERIFY(window.redo_btn_ != nullptr);
    QVERIFY(window.undo_valid_btn_ != nullptr);
    QVERIFY(window.auto_notes_btn_ != nullptr);
}

void TestMainWindowConstruction::buttonPanelInitialState() {
    view::MainWindow window;
    QCOMPARE(window.undo_btn_->text(), QString("Undo"));
    QCOMPARE(window.redo_btn_->text(), QString("Redo"));
    QCOMPARE(window.undo_valid_btn_->text(), QString("Undo Until Valid"));
    QCOMPARE(window.auto_notes_btn_->text(), QString("Fill Notes"));
    QVERIFY(window.auto_notes_btn_->isCheckable());
}

void TestMainWindowConstruction::ratingButtonExists() {
    view::MainWindow window;
    QVERIFY(window.rating_btn_ != nullptr);
    QVERIFY(window.rating_btn_->isFlat());
}

void TestMainWindowConstruction::ratingButtonHiddenInitially() {
    view::MainWindow window;
    QVERIFY(!window.rating_btn_->isVisible());
}

QTEST_MAIN(TestMainWindowConstruction)
#include "test_main_window_construction.moc"
