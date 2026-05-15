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
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QTest>

using namespace sudoku;

namespace {

// 81-char canonical "easy" puzzle, matching test_puzzle_analyzer.cpp::kEasyDigits.
constexpr const char* kImportableEasy = "034678912"
                                        "672195348"
                                        "198342567"
                                        "859761423"
                                        "426853791"
                                        "713924856"
                                        "961537284"
                                        "287419635"
                                        "345286179";

}  // namespace

class TestAnalyzeDifficulty : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void menuActionExists();
    void analyzingImportedPuzzlePopulatesRating();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    QAction* findMenuAction(const QString& text) const;
};

QAction* TestAnalyzeDifficulty::findMenuAction(const QString& text) const {
    for (auto* action : window_->menuBar()->findChildren<QAction*>()) {
        if (action->text().contains(text, Qt::CaseInsensitive)) {
            return action;
        }
    }
    return nullptr;
}

void TestAnalyzeDifficulty::init() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

void TestAnalyzeDifficulty::cleanup() {
    window_.reset();
    ctx_.reset();
}

void TestAnalyzeDifficulty::menuActionExists() {
    auto* action = findMenuAction("Analyze Difficulty");
    QVERIFY2(action != nullptr, "Game menu should have an 'Analyze Difficulty' entry");
}

void TestAnalyzeDifficulty::analyzingImportedPuzzlePopulatesRating() {
    // Imported puzzles start with rating == 0 (unrated until analyzed).
    ctx_->game_vm->importPuzzleFromString(kImportableEasy);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->uiState.get().puzzle_rating, 0.0);

    auto* action = findMenuAction("Analyze Difficulty");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    QVERIFY2(ctx_->game_vm->uiState.get().puzzle_rating > 0.0,
             "Analyze should publish a positive SE rating into uiState");
    QCOMPARE(QString::fromStdString(ctx_->game_vm->errorMessage.get()), QString());
}

QTEST_MAIN(TestAnalyzeDifficulty)
#include "test_analyze_difficulty.moc"
