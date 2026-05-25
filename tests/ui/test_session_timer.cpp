// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "core/settings_manager.h"
#include "test_fixture.h"
#include "view/main_window.h"

#include <chrono>
#include <filesystem>
#include <memory>

#include <QApplication>
#include <QLabel>
#include <QList>
#include <QStatusBar>
#include <QTest>

using namespace sudoku;

// Verifies the Settings -> Display -> "Show session timer" checkbox actually
// gates the right-side status-bar label. The label widget is always created
// (so its updateStatusBar() code path stays compiled and reachable), but its
// visibility — and the cost of formatDuration() each tick — is driven by the
// show_session_timer flag in settings.yaml.
class TestSessionTimer : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void defaultHidesLabel();
    void enablingShowsLabel();
    void disablingHidesAgain();
    void enabledLabelHasNonEmptyTimestamp();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;
    std::shared_ptr<core::SettingsManager> settings_;
    std::filesystem::path settings_dir_;
    std::filesystem::path settings_file_;

    [[nodiscard]] QLabel* findSessionTimerLabel() const;
};

void TestSessionTimer::initTestCase() {
    settings_dir_ =
        std::filesystem::temp_directory_path() /
        ("ui_test_session_timer_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(settings_dir_);
    settings_file_ = settings_dir_ / "settings.yaml";

    settings_ = std::make_shared<core::SettingsManager>(settings_file_);

    ctx_ = std::make_unique<test::UITestContext>();
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->setSettingsManager(settings_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
}

void TestSessionTimer::cleanupTestCase() {
    window_.reset();
    settings_.reset();
    ctx_.reset();
    if (std::filesystem::exists(settings_dir_)) {
        std::filesystem::remove_all(settings_dir_);
    }
}

void TestSessionTimer::init() {
    settings_->setShowSessionTimer(false);
    QApplication::processEvents();
}

// The session-timer label is the QLabel attached via addPermanentWidget(),
// which makes it a child of QStatusBar. The left-side timer_label_ and
// status_label_ are attached via addWidget(); they're also QLabels but only
// the permanent (right-anchored) one is the session timer. We disambiguate
// by matching the addPermanentWidget pattern: the label whose parent is the
// status bar AND that isn't one of the addWidget()-attached ones. Since
// timer_label_ is empty and status_label_ has translated text like "Ready"
// or "Playing", finding the third QLabel child works in practice — but to
// be robust we look for the one that's visible-or-empty when toggled.
//
// Simpler: there are exactly three QLabels in the status bar in this build.
// The session timer is the only one anchored via addPermanentWidget(), which
// means QStatusBar lays it out on the right. We can't observe the layout
// directly, but we can rely on identity: when show_session_timer is false
// the session-timer label is the only one with isVisible() == false among
// the three (the other two are always visible).
QLabel* TestSessionTimer::findSessionTimerLabel() const {
    auto* bar = window_->statusBar();
    auto labels = bar->findChildren<QLabel*>();
    // Heuristic: the session timer is the only QLabel child of the status bar
    // whose visibility we drive from settings; on a fresh window with the
    // setting at its default (false) it is the unique hidden label.
    for (auto* label : labels) {
        if (label->parent() == bar && !label->isVisible()) {
            return label;
        }
    }
    // If the setting is currently on, fall back to "the only non-empty label
    // whose text matches HH:MM:SS" — but for our test sequencing we always
    // start from the false state in init(), so the loop above suffices.
    return nullptr;
}

void TestSessionTimer::defaultHidesLabel() {
    auto* label = findSessionTimerLabel();
    QVERIFY(label != nullptr);
    QVERIFY(!label->isVisible());
}

void TestSessionTimer::enablingShowsLabel() {
    auto* label = findSessionTimerLabel();
    QVERIFY(label != nullptr);
    QVERIFY(!label->isVisible());

    settings_->setShowSessionTimer(true);
    QApplication::processEvents();

    QVERIFY(label->isVisible());
}

void TestSessionTimer::disablingHidesAgain() {
    auto* label = findSessionTimerLabel();
    QVERIFY(label != nullptr);

    settings_->setShowSessionTimer(true);
    QApplication::processEvents();
    QVERIFY(label->isVisible());

    settings_->setShowSessionTimer(false);
    QApplication::processEvents();
    QVERIFY(!label->isVisible());
}

void TestSessionTimer::enabledLabelHasNonEmptyTimestamp() {
    auto* label = findSessionTimerLabel();
    QVERIFY(label != nullptr);

    settings_->setShowSessionTimer(true);
    QApplication::processEvents();
    // The observer callback in setSettingsManager() triggers updateStatusBar()
    // when the flag flips on, so the label should be populated immediately —
    // no need to wait for the 1Hz clock tick.
    QVERIFY(!label->text().isEmpty());
}

QTEST_MAIN(TestSessionTimer)
#include "test_session_timer.moc"
