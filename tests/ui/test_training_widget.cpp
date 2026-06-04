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
#include "view/training_widget.h"

#include <optional>

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTest>

using namespace sudoku;

class TestTrainingWidget : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testInitialPage();
    void testSelectTechniqueShowsTheory();
    void testStartExercisesShowsExercise();
    void testSharedBoardInputStillFunctions();
    void testSubmitAnswerShowsFeedback();
    void testFeedbackButtonsExist();
    void testReturnToSelection();
    void testBackToGameSignal();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    [[nodiscard]] view::TrainingWidget* trainingWidget() const {
        return window_->training_widget_;
    }

    [[nodiscard]] QStackedWidget* pages() const {
        return trainingWidget()->pages_;
    }
};

void TestTrainingWidget::initTestCase() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));

    // Switch to training mode
    window_->central_stack_->setCurrentIndex(1);
    QApplication::processEvents();
}

void TestTrainingWidget::testInitialPage() {
    QCOMPARE(pages()->currentIndex(), 0);
}

void TestTrainingWidget::testSelectTechniqueShowsTheory() {
    ctx_->training_vm->selectTechnique(core::SolvingTechnique::NakedSingle);
    QApplication::processEvents();

    QCOMPARE(pages()->currentIndex(), 1);

    auto* title = pages()->widget(1)->findChild<QLabel*>("theoryTitle");
    QVERIFY(title != nullptr);
    QVERIFY(!title->text().isEmpty());
}

void TestTrainingWidget::testStartExercisesShowsExercise() {
    ctx_->training_vm->startExercises();
    QApplication::processEvents();

    QCOMPARE(pages()->currentIndex(), 2);
    QVERIFY(trainingWidget()->training_board_ != nullptr);
}

void TestTrainingWidget::testSharedBoardInputStillFunctions() {
    // Shared-widget guard (AC-12): SudokuBoardWidget is reused by Training. After 0a.2 unified
    // its key handling onto the single digitKeyPressed(digit, modifiers) signal, Training's board
    // must still receive keyboard input AND apply it. Verify both the unified intent signal fires
    // and the ViewModel actually mutates the board: an Alt+digit colors the cell (migrated from the
    // retired A/B keys), and an out-of-palette Alt digit is rejected (no unrenderable player_color).
    QCOMPARE(pages()->currentIndex(), 2);
    auto* board = trainingWidget()->training_board_;
    QVERIFY(board != nullptr);
    auto* vm = trainingWidget()->training_vm_.get();
    QVERIFY(vm != nullptr);

    // Find an editable (non-given, non-found) cell to receive input, and select it on both the
    // ViewModel (drives applyNumber/applyColor) and the widget.
    std::optional<core::Position> editable;
    const auto& initial = vm->trainingBoard.get();
    for (size_t r = 0; r < core::BOARD_SIZE && !editable; ++r) {
        for (size_t c = 0; c < core::BOARD_SIZE && !editable; ++c) {
            if (!initial[r][c].is_given && !initial[r][c].is_found) {
                editable = core::Position{.row = r, .col = c};
            }
        }
    }
    QVERIFY(editable.has_value());
    vm->selectCell(editable->row, editable->col);
    board->setSelectedCell(*editable);

    QSignalSpy spy(board, &view::SudokuBoardWidget::digitKeyPressed);

    // Plain digit still reaches the widget's unified intent signal.
    QTest::keyClick(board, Qt::Key_5);
    QApplication::processEvents();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 5);

    // Alt+1 colors the cell (Color A) — assert the applied effect, not just the signal.
    QTest::keyClick(board, Qt::Key_1, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(vm->trainingBoard.get()[editable->row][editable->col].player_color, core::kMinPlayerColor);

    // Alt+5 is outside Training's two-color palette — it must be a no-op, leaving Color A intact.
    QTest::keyClick(board, Qt::Key_5, Qt::AltModifier);
    QApplication::processEvents();
    QCOMPARE(vm->trainingBoard.get()[editable->row][editable->col].player_color, core::kMinPlayerColor);
}

void TestTrainingWidget::testSubmitAnswerShowsFeedback() {
    // Submit the current board as-is (will likely be incorrect, but that's fine for UI test)
    ctx_->training_vm->submitAnswer();
    QApplication::processEvents();

    QCOMPARE(pages()->currentIndex(), 3);

    auto* result_label = pages()->widget(3)->findChild<QLabel*>("feedbackResult");
    QVERIFY(result_label != nullptr);
    QVERIFY(!result_label->text().isEmpty());
}

void TestTrainingWidget::testFeedbackButtonsExist() {
    // We should still be on feedback page from previous test
    QCOMPARE(pages()->currentIndex(), 3);

    auto* feedback_page = pages()->widget(3);
    auto buttons = feedback_page->findChildren<QPushButton*>();

    QStringList button_texts;
    for (auto* btn : buttons) {
        button_texts << btn->text();
    }

    QVERIFY(button_texts.contains("Next Exercise"));
    QVERIFY(button_texts.contains("Retry"));
    QVERIFY(button_texts.contains("Show Solution"));
    QVERIFY(button_texts.contains("Quit Lesson"));
}

void TestTrainingWidget::testReturnToSelection() {
    ctx_->training_vm->returnToSelection();
    QApplication::processEvents();

    QCOMPARE(pages()->currentIndex(), 0);
}

void TestTrainingWidget::testBackToGameSignal() {
    QSignalSpy spy(trainingWidget(), &view::TrainingWidget::backToGame);

    // Find the "Back to Game" button on the selection page (page 0)
    auto* selection_page = pages()->widget(0);
    auto buttons = selection_page->findChildren<QPushButton*>();

    QPushButton* back_btn = nullptr;
    for (auto* btn : buttons) {
        if (btn->text() == "Back to Game") {
            back_btn = btn;
            break;
        }
    }

    QVERIFY(back_btn != nullptr);
    back_btn->click();
    QApplication::processEvents();

    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestTrainingWidget)
#include "test_training_widget.moc"
