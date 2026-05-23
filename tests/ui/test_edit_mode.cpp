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

#include <array>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QTest>
#include <QToolBar>

using namespace sudoku;

namespace {

// 81-char canonical "easy" puzzle, identical to test_puzzle_analyzer.cpp::kEasyDigits.
constexpr std::string_view kImportableEasy = "034678912"
                                             "672195348"
                                             "198342567"
                                             "859761423"
                                             "426853791"
                                             "713924856"
                                             "961537284"
                                             "287419635"
                                             "345286179";

// Load givens into edit-mode VM cell-by-cell, simulating the user typing them.
void placeGivens(viewmodel::GameViewModel& vm, std::string_view digits) {
    for (size_t i = 0; i < 81 && i < digits.size(); ++i) {
        const char c = digits[i];
        if (c >= '1' && c <= '9') {
            vm.setEditModeGiven(core::Position{.row = i / 9, .col = i % 9}, c - '0');
        }
    }
}

}  // namespace

class TestEditMode : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void menuActionExists();
    void doneEditingActionInitiallyHidden();
    void enterEditModeShowsDoneEditingAction();
    void commitValidPuzzleReturnsToNormalModeWithOrigin();
    void commitInvalidPuzzleStaysInEditModeWithError();
    void deleteKeyInEditModeClearsGiven();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    QAction* findMenuAction(const QString& text) const;
    QAction* findToolBarAction(const QString& text) const;
};

QAction* TestEditMode::findMenuAction(const QString& text) const {
    for (auto* action : window_->menuBar()->findChildren<QAction*>()) {
        if (action->text().contains(text, Qt::CaseInsensitive)) {
            return action;
        }
    }
    return nullptr;
}

QAction* TestEditMode::findToolBarAction(const QString& text) const {
    for (auto* toolbar : window_->findChildren<QToolBar*>()) {
        for (auto* action : toolbar->actions()) {
            if (action->text().contains(text, Qt::CaseInsensitive)) {
                return action;
            }
        }
    }
    return nullptr;
}

void TestEditMode::init() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

void TestEditMode::cleanup() {
    window_.reset();
    ctx_.reset();
}

void TestEditMode::menuActionExists() {
    auto* action = findMenuAction("Edit Custom Puzzle");
    QVERIFY2(action != nullptr, "Game menu should have an 'Edit Custom Puzzle' entry");
}

void TestEditMode::doneEditingActionInitiallyHidden() {
    auto* action = findToolBarAction("Done Editing");
    QVERIFY2(action != nullptr, "Toolbar should have a 'Done Editing' action defined");
    QVERIFY2(!action->isVisible(), "Done Editing action should be hidden when not in edit mode");
}

void TestEditMode::enterEditModeShowsDoneEditingAction() {
    auto* enter = findMenuAction("Edit Custom Puzzle");
    QVERIFY(enter != nullptr);
    enter->trigger();
    QApplication::processEvents();

    QCOMPARE(static_cast<int>(ctx_->game_vm->getInputMode()), static_cast<int>(viewmodel::InputMode::EditGivens));

    auto* done = findToolBarAction("Done Editing");
    QVERIFY(done != nullptr);
    QVERIFY2(done->isVisible(), "Done Editing action should be visible in edit mode");
}

void TestEditMode::commitValidPuzzleReturnsToNormalModeWithOrigin() {
    findMenuAction("Edit Custom Puzzle")->trigger();
    QApplication::processEvents();

    placeGivens(*ctx_->game_vm, kImportableEasy);

    findToolBarAction("Done Editing")->trigger();
    QApplication::processEvents();

    QCOMPARE(static_cast<int>(ctx_->game_vm->getInputMode()), static_cast<int>(viewmodel::InputMode::Normal));
    QCOMPARE(static_cast<int>(ctx_->game_vm->getCurrentPuzzleOrigin()),
             static_cast<int>(core::PuzzleOrigin::ImportedEditMode));
    QCOMPARE(QString::fromStdString(ctx_->game_vm->errorMessage.get()), QString());

    // Done action hidden again after exiting edit mode.
    QVERIFY(!findToolBarAction("Done Editing")->isVisible());
}

void TestEditMode::commitInvalidPuzzleStaysInEditModeWithError() {
    findMenuAction("Edit Custom Puzzle")->trigger();
    QApplication::processEvents();

    // Two 5s in row 0 — rule violation.
    ctx_->game_vm->setEditModeGiven({.row = 0, .col = 0}, 5);
    ctx_->game_vm->setEditModeGiven({.row = 0, .col = 5}, 5);

    findToolBarAction("Done Editing")->trigger();
    QApplication::processEvents();

    QCOMPARE(static_cast<int>(ctx_->game_vm->getInputMode()), static_cast<int>(viewmodel::InputMode::EditGivens));
    QVERIFY2(!ctx_->game_vm->errorMessage.get().empty(), "Commit should populate errorMessage on invalid puzzle");
}

void TestEditMode::deleteKeyInEditModeClearsGiven() {
    findMenuAction("Edit Custom Puzzle")->trigger();
    QApplication::processEvents();

    // Place a given via the VM.
    const core::Position pos{.row = 4, .col = 4};
    ctx_->game_vm->setEditModeGiven(pos, 7);
    QCOMPARE(static_cast<int>(ctx_->game_vm->gameState.get().getCell(pos).value), 7);

    // Multiple SudokuBoardWidget instances exist (game + training submenu); pick
    // the game one specifically. Friend access lets us bypass findChild ambiguity.
    window_->board_widget_->setSelectedCell(pos);

    // The Edit menu's "Clear Cell" action (Qt::Key_Delete shortcut) is the
    // user-facing binding for clearing in edit mode. Triggering it directly
    // is more deterministic than synthesising shortcut events in offscreen.
    auto* clear_action = findMenuAction("Clear Cell");
    QVERIFY(clear_action != nullptr);
    clear_action->trigger();
    QApplication::processEvents();

    QCOMPARE(static_cast<int>(ctx_->game_vm->gameState.get().getCell(pos).value), 0);
}

QTEST_MAIN(TestEditMode)
#include "test_edit_mode.moc"
