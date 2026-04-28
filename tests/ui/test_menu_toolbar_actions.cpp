// sudoku-cpp - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "test_fixture.h"
#include "view/main_window.h"

#include <QAction>
#include <QComboBox>
#include <QMenu>
#include <QMenuBar>
#include <QStackedWidget>
#include <QTest>

using namespace sudoku;

class TestMenuToolbarActions : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void undoMenuActionExists();
    void redoMenuActionExists();
    void trainingModeMenuSwitchesStack();
    void resumeGameMenuSwitchesBack();
    void difficultyComboDefaultIsMedium();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    QAction* findMenuAction(const QString& text) const;
};

void TestMenuToolbarActions::initTestCase() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

QAction* TestMenuToolbarActions::findMenuAction(const QString& text) const {
    for (auto* action : window_->menuBar()->findChildren<QAction*>()) {
        if (action->text().contains(text, Qt::CaseInsensitive)) {
            return action;
        }
    }
    return nullptr;
}

void TestMenuToolbarActions::undoMenuActionExists() {
    auto* action = findMenuAction("Undo");
    QVERIFY(action != nullptr);
    QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Z"));
}

void TestMenuToolbarActions::redoMenuActionExists() {
    auto* action = findMenuAction("Redo");
    QVERIFY(action != nullptr);
    QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Y"));
}

void TestMenuToolbarActions::trainingModeMenuSwitchesStack() {
    QCOMPARE(window_->central_stack_->currentIndex(), 0);

    auto* action = findMenuAction("Training Mode");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    QCOMPARE(window_->central_stack_->currentIndex(), 1);
}

void TestMenuToolbarActions::resumeGameMenuSwitchesBack() {
    // First switch to training
    window_->central_stack_->setCurrentIndex(1);
    QApplication::processEvents();
    QCOMPARE(window_->central_stack_->currentIndex(), 1);

    auto* action = findMenuAction("Resume Game");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    QCOMPARE(window_->central_stack_->currentIndex(), 0);
}

void TestMenuToolbarActions::difficultyComboDefaultIsMedium() {
    QCOMPARE(window_->difficulty_combo_->currentIndex(), 1);
    QCOMPARE(window_->difficulty_combo_->currentText(), QString("Medium"));
}

QTEST_MAIN(TestMenuToolbarActions)
#include "test_menu_toolbar_actions.moc"
