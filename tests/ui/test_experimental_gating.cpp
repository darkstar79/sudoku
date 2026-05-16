// sudoku-cpp - Offline Sudoku Game
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

#include <QAction>
#include <QApplication>
#include <QMenuBar>
#include <QTest>

using namespace sudoku;

// Verifies the Settings -> Experimental tab actually gates the Training Mode and
// Get Coaching Hint menu entries. The actions are always added to the menu bar
// (so their code paths stay compiled and reachable from tests), but visibility
// is driven by the two experimental_* flags in settings.yaml.
//
// Regression sentinel for the 1.0 conservative-scope decision: training and
// coaching hints ship hidden, only revealed when the user opts in.
class TestExperimentalGating : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void defaultsHideBothActions();
    void onlyTrainingShowsOnlyTraining();
    void onlyCoachingShowsOnlyCoaching();
    void bothFlagsOnShowsBoth();
    void togglingBackToFalseHidesAgain();

private:
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;
    std::shared_ptr<core::SettingsManager> settings_;
    std::filesystem::path settings_dir_;
    std::filesystem::path settings_file_;

    [[nodiscard]] QAction* findMenuAction(const QString& text) const;
};

void TestExperimentalGating::initTestCase() {
    // Dedicated temp settings dir so we don't collide with the maintainer's real settings.
    settings_dir_ =
        std::filesystem::temp_directory_path() /
        ("ui_test_experimental_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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

void TestExperimentalGating::cleanupTestCase() {
    window_.reset();
    settings_.reset();
    ctx_.reset();
    if (std::filesystem::exists(settings_dir_)) {
        std::filesystem::remove_all(settings_dir_);
    }
}

// Reset both experimental flags to a known-false state before each test slot
// so individual tests don't depend on declaration order or what a prior slot
// left behind. Each test then drives the exact transition it wants to assert.
void TestExperimentalGating::init() {
    settings_->setExperimentalTrainingMode(false);
    settings_->setExperimentalCoachingHints(false);
    QApplication::processEvents();
}

QAction* TestExperimentalGating::findMenuAction(const QString& text) const {
    for (auto* action : window_->menuBar()->findChildren<QAction*>()) {
        if (action->text().contains(text, Qt::CaseInsensitive)) {
            return action;
        }
    }
    return nullptr;
}

void TestExperimentalGating::defaultsHideBothActions() {
    auto* training = findMenuAction("Training Mode");
    auto* coaching = findMenuAction("Coaching Hint");
    QVERIFY(training != nullptr);
    QVERIFY(coaching != nullptr);
    QVERIFY(!training->isVisible());
    QVERIFY(!coaching->isVisible());
}

void TestExperimentalGating::onlyTrainingShowsOnlyTraining() {
    settings_->setExperimentalTrainingMode(true);
    QApplication::processEvents();

    auto* training = findMenuAction("Training Mode");
    auto* coaching = findMenuAction("Coaching Hint");
    QVERIFY(training != nullptr);
    QVERIFY(coaching != nullptr);
    QVERIFY(training->isVisible());
    QVERIFY(!coaching->isVisible());
}

void TestExperimentalGating::onlyCoachingShowsOnlyCoaching() {
    settings_->setExperimentalCoachingHints(true);
    QApplication::processEvents();

    auto* training = findMenuAction("Training Mode");
    auto* coaching = findMenuAction("Coaching Hint");
    QVERIFY(training != nullptr);
    QVERIFY(coaching != nullptr);
    QVERIFY(!training->isVisible());
    QVERIFY(coaching->isVisible());
}

void TestExperimentalGating::bothFlagsOnShowsBoth() {
    settings_->setExperimentalTrainingMode(true);
    settings_->setExperimentalCoachingHints(true);
    QApplication::processEvents();

    auto* training = findMenuAction("Training Mode");
    auto* coaching = findMenuAction("Coaching Hint");
    QVERIFY(training != nullptr);
    QVERIFY(coaching != nullptr);
    QVERIFY(training->isVisible());
    QVERIFY(coaching->isVisible());
}

void TestExperimentalGating::togglingBackToFalseHidesAgain() {
    // Turn on, then off, and verify the menu actions hide again.
    settings_->setExperimentalTrainingMode(true);
    settings_->setExperimentalCoachingHints(true);
    QApplication::processEvents();

    settings_->setExperimentalTrainingMode(false);
    settings_->setExperimentalCoachingHints(false);
    QApplication::processEvents();

    auto* training = findMenuAction("Training Mode");
    auto* coaching = findMenuAction("Coaching Hint");
    QVERIFY(training != nullptr);
    QVERIFY(coaching != nullptr);
    QVERIFY(!training->isVisible());
    QVERIFY(!coaching->isVisible());
}

QTEST_MAIN(TestExperimentalGating)
#include "test_experimental_gating.moc"
