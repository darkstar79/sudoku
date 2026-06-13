// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "core/solving_technique.h"
#include "test_fixture.h"
#include "view/main_window.h"
#include "view/puzzle_technique_dialog.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTest>

using namespace sudoku;

namespace {

// 81-char canonical "easy" puzzle, matching test_puzzle_analyzer.cpp::kEasyDigits.
// Note: this puzzle is fully solvable by Naked Singles — perfect for L-D testing.
constexpr const char* kEasyOneCellGap = ".34678912"
                                        "672195348"
                                        "198342567"
                                        "859761423"
                                        "426853791"
                                        "713924856"
                                        "961537284"
                                        "287419635"
                                        "345286179";

}  // namespace

class TestFindByTechnique : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void menuActionExists();
    void menuActionOpensDialog();
    void comboCoversEveryLogicalTechnique();
    void findingApplicableTechniqueSetsHintMessage();
    void findingMissingTechniqueSetsErrorMessage();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    QAction* findMenuAction(const QString& text) const;
    view::PuzzleTechniqueDialog* findOpenDialog() const;
};

QAction* TestFindByTechnique::findMenuAction(const QString& text) const {
    for (auto* action : window_->menuBar()->findChildren<QAction*>()) {
        if (action->text().contains(text, Qt::CaseInsensitive)) {
            return action;
        }
    }
    return nullptr;
}

view::PuzzleTechniqueDialog* TestFindByTechnique::findOpenDialog() const {
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (auto* dialog = qobject_cast<view::PuzzleTechniqueDialog*>(widget)) {
            if (dialog->isVisible()) {
                return dialog;
            }
        }
    }
    return nullptr;
}

void TestFindByTechnique::init() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

void TestFindByTechnique::cleanup() {
    if (auto* dialog = findOpenDialog()) {
        dialog->reject();
        QApplication::processEvents();
    }
    window_.reset();
    ctx_.reset();
}

void TestFindByTechnique::menuActionExists() {
    auto* action = findMenuAction("Find Step by Technique");
    QVERIFY2(action != nullptr, "Help menu should have a 'Find Step by Technique…' entry");
}

void TestFindByTechnique::menuActionOpensDialog() {
    auto* action = findMenuAction("Find Step by Technique");
    QVERIFY(action != nullptr);
    action->trigger();
    QApplication::processEvents();

    auto* dialog = findOpenDialog();
    QVERIFY2(dialog != nullptr, "Triggering the menu should open a PuzzleTechniqueDialog");
}

// Exhaustiveness guard (story 0b.4b, AC3c): the "Find Step by Technique" combo is built from a
// hand-maintained kSelectableTechniques array (puzzle_technique_dialog.cpp) with no -Wswitch net, so a
// newly appended logical technique (e.g. FullHouse) could be silently omitted from the dialog. Tie the
// combo to the enum: the contiguous logical range [0, kLastLogicalTechnique] IS the selectable set;
// Backtracking (the id-255 sentinel) is excluded by living outside that range. Adding a logical technique
// without listing it here turns this red.
void TestFindByTechnique::comboCoversEveryLogicalTechnique() {
    view::PuzzleTechniqueDialog dialog;

    const int logical_count = static_cast<int>(core::kLastLogicalTechnique) + 1;
    QCOMPARE(dialog.technique_combo_->count(), logical_count);

    for (int id = 0; id <= static_cast<int>(core::kLastLogicalTechnique); ++id) {
        QVERIFY2(dialog.technique_combo_->findData(id) >= 0,
                 qPrintable(QString("Logical technique id %1 missing from Find-by-Technique combo").arg(id)));
    }
}

void TestFindByTechnique::findingApplicableTechniqueSetsHintMessage() {
    // Import a puzzle where Naked Single is trivially applicable.
    ctx_->game_vm->importPuzzleFromString(kEasyOneCellGap);
    QApplication::processEvents();

    findMenuAction("Find Step by Technique")->trigger();
    QApplication::processEvents();

    auto* dialog = findOpenDialog();
    QVERIFY(dialog != nullptr);

    // Select Naked Single in the combobox.
    const auto naked_single_id = static_cast<int>(core::SolvingTechnique::NakedSingle);
    const int idx = dialog->technique_combo_->findData(naked_single_id);
    QVERIFY2(idx >= 0, "Naked Single should be in the technique combo box");
    dialog->technique_combo_->setCurrentIndex(idx);
    dialog->find_btn_->click();
    QApplication::processEvents();

    QVERIFY2(!ctx_->game_vm->hintMessage.get().empty(), "Found step should populate hintMessage");
    QCOMPARE(QString::fromStdString(ctx_->game_vm->errorMessage.get()), QString());
    QVERIFY2(findOpenDialog() == nullptr, "Dialog should close after a successful find");
}

void TestFindByTechnique::findingMissingTechniqueSetsErrorMessage() {
    // Same easy puzzle (no AI-grade techniques will apply).
    ctx_->game_vm->importPuzzleFromString(kEasyOneCellGap);
    QApplication::processEvents();

    findMenuAction("Find Step by Technique")->trigger();
    QApplication::processEvents();

    auto* dialog = findOpenDialog();
    QVERIFY(dialog != nullptr);

    const auto deadblossom_id = static_cast<int>(core::SolvingTechnique::DeathBlossom);
    const int idx = dialog->technique_combo_->findData(deadblossom_id);
    QVERIFY(idx >= 0);
    dialog->technique_combo_->setCurrentIndex(idx);
    dialog->find_btn_->click();
    QApplication::processEvents();

    QVERIFY2(!ctx_->game_vm->errorMessage.get().empty(), "Missing technique should populate errorMessage");
}

QTEST_MAIN(TestFindByTechnique)
#include "test_find_by_technique.moc"
