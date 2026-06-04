// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "test_fixture.h"
#include "view/keyboard_shortcuts.h"
#include "view/main_window.h"

#include <optional>
#include <set>

#include <QAction>
#include <QDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTest>

using namespace sudoku;

namespace {

// Native rendering of the Space key — what the mode-button affordance and the cycle row
// must surface. Checking the token (not a localized mode name) keeps assertions i18n-proof.
[[nodiscard]] QString nativeSpace() {
    return QKeySequence(Qt::Key_Space).toString(QKeySequence::NativeText);
}

// First shortcut entry with the given action, or nullopt. Keeps the test bodies flat (no
// loops) so each stays well under the cognitive-complexity threshold.
[[nodiscard]] std::optional<view::ShortcutEntry> entryFor(view::ShortcutAction action) {
    for (const auto& entry : view::keyboardShortcuts()) {
        if (entry.action == action) {
            return entry;
        }
    }
    return std::nullopt;
}

}  // namespace

class TestKeyboardShortcuts : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void sourceListCoversExpectedActions();
    void chordTextRendersNativeModifierAndDigitRange();
    void modifierHintNamesAllThreeModifiers();
    void buildShortcutsDialogIsPopulatedFromSourceList();
    void helpMenuActionOpensShortcutsDialog();
    void modeButtonLabelSurfacesSpaceAffordance();
    void statusBarExposesModifierHint();

    // Qt needs `private slots:` for the test methods, kept separate from the plain-`private:`
    // data members below.
    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;
};

void TestKeyboardShortcuts::initTestCase() {
    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));

    ctx_->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();
}

void TestKeyboardShortcuts::sourceListCoversExpectedActions() {
    const auto shortcuts = view::keyboardShortcuts();
    QVERIFY(!shortcuts.empty());

    std::set<view::ShortcutAction> actions;
    for (const auto& entry : shortcuts) {
        actions.insert(entry.action);
    }

    // The map the dialog/hint render must cover the full 0a.2 binding set: the three
    // modifier overrides, the cycle verb, and at least one erase action.
    QVERIFY(actions.contains(view::ShortcutAction::ValueOverride));
    QVERIFY(actions.contains(view::ShortcutAction::PencilOverride));
    QVERIFY(actions.contains(view::ShortcutAction::ColorOverride));
    QVERIFY(actions.contains(view::ShortcutAction::CycleMode));
    QVERIFY(actions.contains(view::ShortcutAction::ClearValue) || actions.contains(view::ShortcutAction::ClearColor) ||
            actions.contains(view::ShortcutAction::ClearPencilMarks) ||
            actions.contains(view::ShortcutAction::ClearActiveLayer));
}

void TestKeyboardShortcuts::chordTextRendersNativeModifierAndDigitRange() {
    const auto value = entryFor(view::ShortcutAction::ValueOverride);
    QVERIFY(value.has_value());
    if (!value.has_value()) {
        return;
    }
    const QString value_chord = view::shortcutChordText(value.value());
    // Modifier glyph comes from NativeText (no hard-coded "Ctrl" literal).
    QVERIFY(value_chord.contains(view::nativeModifierName(Qt::ControlModifier)));
    // Digit family renders a 1..9 range, not a single representative digit.
    QVERIFY(value_chord.contains(QString::number(core::MAX_VALUE)));

    // The Space cycle row renders the native Space key.
    const auto cycle = entryFor(view::ShortcutAction::CycleMode);
    QVERIFY(cycle.has_value());
    if (!cycle.has_value()) {
        return;
    }
    QVERIFY(view::shortcutChordText(cycle.value()).contains(nativeSpace()));
}

void TestKeyboardShortcuts::modifierHintNamesAllThreeModifiers() {
    const QString hint = view::modifierHintText();
    QVERIFY(!hint.isEmpty());
    QVERIFY(hint.contains(view::nativeModifierName(Qt::ControlModifier)));
    QVERIFY(hint.contains(view::nativeModifierName(Qt::ShiftModifier)));
    QVERIFY(hint.contains(view::nativeModifierName(Qt::AltModifier)));
}

void TestKeyboardShortcuts::buildShortcutsDialogIsPopulatedFromSourceList() {
    auto* dialog = window_->buildKeyboardShortcutsDialog();
    QVERIFY(dialog != nullptr);
    if (dialog == nullptr) {
        return;
    }

    auto* table = dialog->findChild<QTableWidget*>("keyboardShortcutsTable");
    QVERIFY(table != nullptr);
    if (table == nullptr) {
        delete dialog;
        return;
    }
    QCOMPARE(table->rowCount(), static_cast<int>(view::keyboardShortcuts().size()));

    // Every row carries both an action label and a chord — the no-drift surface.
    for (int row = 0; row < table->rowCount(); ++row) {
        const auto* action_item = table->item(row, 0);
        const auto* chord_item = table->item(row, 1);
        QVERIFY(action_item != nullptr && !action_item->text().isEmpty());
        QVERIFY(chord_item != nullptr && !chord_item->text().isEmpty());
    }

    delete dialog;
}

void TestKeyboardShortcuts::helpMenuActionOpensShortcutsDialog() {
    auto* action = window_->findChild<QAction*>("keyboardShortcutsAction");
    QVERIFY(action != nullptr);
    if (action == nullptr) {
        return;
    }

    action->trigger();
    QApplication::processEvents();

    QDialog* opened = nullptr;
    const auto widgets = QApplication::topLevelWidgets();
    for (auto* widget : widgets) {
        if (widget->objectName() == "keyboardShortcutsDialog") {
            opened = qobject_cast<QDialog*>(widget);
            break;
        }
    }
    QVERIFY(opened != nullptr);
    if (opened == nullptr) {
        return;
    }
    QVERIFY(opened->isVisible());

    opened->close();
    QApplication::processEvents();
}

void TestKeyboardShortcuts::modeButtonLabelSurfacesSpaceAffordance() {
    const QString space = nativeSpace();

    for (const auto mode : {viewmodel::InputMode::Normal, viewmodel::InputMode::Notes, viewmodel::InputMode::Color}) {
        ctx_->game_vm->setInputMode(mode);
        window_->updateButtonPanel();
        QVERIFY2(window_->mode_btn_->text().contains(space), "cycling-mode label must surface the Space cycle key");
    }

    // EditGivens is not a cycling mode — its label keeps its plain semantics (no Space hint).
    ctx_->game_vm->setInputMode(viewmodel::InputMode::EditGivens);
    window_->updateButtonPanel();
    QVERIFY(!window_->mode_btn_->text().contains(space));

    ctx_->game_vm->setInputMode(viewmodel::InputMode::Normal);
    window_->updateButtonPanel();
}

void TestKeyboardShortcuts::statusBarExposesModifierHint() {
    auto* hint = window_->findChild<QLabel*>("modifierHintLabel");
    QVERIFY(hint != nullptr);
    if (hint == nullptr) {
        return;
    }
    QVERIFY(!hint->text().isEmpty());
    QVERIFY(hint->text().contains(view::nativeModifierName(Qt::ControlModifier)));
}

QTEST_MAIN(TestKeyboardShortcuts)
#include "test_keyboard_shortcuts.moc"
