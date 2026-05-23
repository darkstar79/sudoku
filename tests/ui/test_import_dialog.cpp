// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "test_fixture.h"
#include "view/main_window.h"
#include "view/puzzle_import_dialog.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTest>
#include <QTextEdit>

using namespace sudoku;

namespace {

// 81-char canonical "easy" puzzle (matches kEasyDigits from test_puzzle_analyzer.cpp).
constexpr const char* kImportableEasy = "034678912"
                                        "672195348"
                                        "198342567"
                                        "859761423"
                                        "426853791"
                                        "713924856"
                                        "961537284"
                                        "287419635"
                                        "345286179";

// Pre-feature puzzle with two cells violating row uniqueness on row 0 (two 5s).
constexpr const char* kInvalidPuzzle = "5.0..5..."
                                       "........."
                                       "........."
                                       "........."
                                       "........."
                                       "........."
                                       "........."
                                       "........."
                                       ".........";

}  // namespace

class TestImportDialog : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void menuActionExists();
    void menuActionOpensDialog();
    void importingValidPuzzleLoadsBoardAndCloses();
    void importingInvalidPuzzleShowsErrorAndStaysOpen();
    void cancelButtonClosesDialogWithoutChangingGame();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    QAction* findMenuAction(const QString& text) const;
    view::PuzzleImportDialog* findOpenDialog() const;
};

QAction* TestImportDialog::findMenuAction(const QString& text) const {
    for (auto* action : window_->menuBar()->findChildren<QAction*>()) {
        if (action->text().contains(text, Qt::CaseInsensitive)) {
            return action;
        }
    }
    return nullptr;
}

view::PuzzleImportDialog* TestImportDialog::findOpenDialog() const {
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (auto* dialog = qobject_cast<view::PuzzleImportDialog*>(widget)) {
            if (dialog->isVisible()) {
                return dialog;
            }
        }
    }
    return nullptr;
}

void TestImportDialog::init() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

void TestImportDialog::cleanup() {
    // Close any leftover dialog before tearing down the window.
    if (auto* dialog = findOpenDialog()) {
        dialog->reject();
        QApplication::processEvents();
    }
    window_.reset();
    ctx_.reset();
}

void TestImportDialog::menuActionExists() {
    auto* action = findMenuAction("Import Custom Puzzle");
    QVERIFY2(action != nullptr, "File/Game menu should have an 'Import Custom Puzzle…' entry");
}

void TestImportDialog::menuActionOpensDialog() {
    auto* action = findMenuAction("Import Custom Puzzle");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    auto* dialog = findOpenDialog();
    QVERIFY2(dialog != nullptr, "Triggering the menu action should open a PuzzleImportDialog");
}

void TestImportDialog::importingValidPuzzleLoadsBoardAndCloses() {
    auto* action = findMenuAction("Import Custom Puzzle");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    auto* dialog = findOpenDialog();
    QVERIFY(dialog != nullptr);

    dialog->text_edit_->setPlainText(QString::fromUtf8(kImportableEasy));
    dialog->import_btn_->click();
    QApplication::processEvents();

    // Dialog should close on success.
    QVERIFY2(findOpenDialog() == nullptr, "Dialog should close after a successful import");

    // GameState now holds the imported board.
    const auto& state = ctx_->game_vm->gameState.get();
    QCOMPARE(static_cast<int>(state.getCell({.row = 0, .col = 1}).value), 3);
    QCOMPARE(static_cast<int>(state.getCell({.row = 8, .col = 8}).value), 9);

    // Origin tracked as ImportedString.
    QCOMPARE(static_cast<int>(ctx_->game_vm->getCurrentPuzzleOrigin()),
             static_cast<int>(core::PuzzleOrigin::ImportedString));

    // No error message.
    QCOMPARE(QString::fromStdString(ctx_->game_vm->errorMessage.get()), QString());
}

void TestImportDialog::importingInvalidPuzzleShowsErrorAndStaysOpen() {
    auto* action = findMenuAction("Import Custom Puzzle");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    auto* dialog = findOpenDialog();
    QVERIFY(dialog != nullptr);

    dialog->text_edit_->setPlainText(QString::fromUtf8(kInvalidPuzzle));
    dialog->import_btn_->click();
    QApplication::processEvents();

    // Dialog stays open so user can correct the input.
    QVERIFY2(findOpenDialog() != nullptr, "Dialog should stay open after a failed import");

    // Error label populated.
    QVERIFY2(!dialog->error_label_->text().isEmpty(), "Error label should display the analyzer error");
}

void TestImportDialog::cancelButtonClosesDialogWithoutChangingGame() {
    const auto state_before = ctx_->game_vm->gameState.get();

    auto* action = findMenuAction("Import Custom Puzzle");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    auto* dialog = findOpenDialog();
    QVERIFY(dialog != nullptr);

    dialog->cancel_btn_->click();
    QApplication::processEvents();

    QVERIFY2(findOpenDialog() == nullptr, "Cancel should close the dialog");
    // GameState unchanged.
    const auto& state_after = ctx_->game_vm->gameState.get();
    QCOMPARE(state_after.getMoveCount(), state_before.getMoveCount());
}

QTEST_MAIN(TestImportDialog)
#include "test_import_dialog.moc"
