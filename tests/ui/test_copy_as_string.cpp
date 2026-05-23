// sudoku - Offline Sudoku Game
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
// Note: position 0 is '0' (empty) in the source; serializeToString emits '.' for empties.
constexpr const char* kImportableEasy = "034678912"
                                        "672195348"
                                        "198342567"
                                        "859761423"
                                        "426853791"
                                        "713924856"
                                        "961537284"
                                        "287419635"
                                        "345286179";

constexpr const char* kExpectedSerialized = ".34678912"
                                            "672195348"
                                            "198342567"
                                            "859761423"
                                            "426853791"
                                            "713924856"
                                            "961537284"
                                            "287419635"
                                            "345286179";

}  // namespace

class TestCopyAsString : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void menuActionExists();
    void triggeringWritesSerializedGivensToClipboard();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    QAction* findMenuAction(const QString& text) const;
};

QAction* TestCopyAsString::findMenuAction(const QString& text) const {
    for (auto* action : window_->menuBar()->findChildren<QAction*>()) {
        if (action->text().contains(text, Qt::CaseInsensitive)) {
            return action;
        }
    }
    return nullptr;
}

void TestCopyAsString::init() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

void TestCopyAsString::cleanup() {
    window_.reset();
    ctx_.reset();
}

void TestCopyAsString::menuActionExists() {
    auto* action = findMenuAction("Copy Puzzle as Text");
    QVERIFY2(action != nullptr, "Game menu should have a 'Copy Puzzle as Text' entry");
}

void TestCopyAsString::triggeringWritesSerializedGivensToClipboard() {
    ctx_->game_vm->importPuzzleFromString(kImportableEasy);
    QApplication::processEvents();

    auto* action = findMenuAction("Copy Puzzle as Text");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    QCOMPARE(QString::fromStdString(ctx_->clipboard->getText()), QString::fromUtf8(kExpectedSerialized));
}

QTEST_MAIN(TestCopyAsString)
#include "test_copy_as_string.moc"
