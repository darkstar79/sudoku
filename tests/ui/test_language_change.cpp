// sudoku-cpp - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "view/main_window.h"
#include "view/training_widget.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QMenu>
#include <QMenuBar>
#include <QStackedWidget>
#include <QTest>
#include <QTranslator>

using namespace sudoku;

class TestLanguageChange : public QObject {
    Q_OBJECT

private slots:
    void menuBarRetranslatesOnLanguageChange();
    void trainingPagesRebuildOnLanguageChange();

private:
    [[nodiscard]] static bool loadGermanTranslator(QTranslator& translator);
};

bool TestLanguageChange::loadGermanTranslator(QTranslator& translator) {
    // SUDOKU_QM_DIR is set by tests/ui/CMakeLists.txt to the build directory
    // where qt_add_lrelease emits sudoku_<locale>.qm. Building the main
    // 'sudoku' target produces them.
    return translator.load(QString("sudoku_de"), QString::fromUtf8(SUDOKU_QM_DIR));
}

void TestLanguageChange::menuBarRetranslatesOnLanguageChange() {
    view::MainWindow window;

    auto initial_actions = window.menuBar()->actions();
    QVERIFY(!initial_actions.isEmpty());
    QCOMPARE(initial_actions.first()->text(), QString("&Game"));

    QTranslator translator;
    if (!loadGermanTranslator(translator)) {
        QSKIP("sudoku_de.qm not built; build the main 'sudoku' target first");
    }

    QVERIFY(QCoreApplication::installTranslator(&translator));
    QApplication::processEvents();

    auto translated_actions = window.menuBar()->actions();
    QVERIFY(!translated_actions.isEmpty());
    QCOMPARE(translated_actions.first()->text(), QString("&Spiel"));

    QCoreApplication::removeTranslator(&translator);
    QApplication::processEvents();

    auto restored_actions = window.menuBar()->actions();
    QCOMPARE(restored_actions.first()->text(), QString("&Game"));
}

void TestLanguageChange::trainingPagesRebuildOnLanguageChange() {
    view::MainWindow window;
    auto* training = window.findChild<view::TrainingWidget*>();
    QVERIFY(training != nullptr);

    auto* pages_before = training->findChild<QStackedWidget*>();
    QVERIFY(pages_before != nullptr);

    QTranslator translator;
    if (!loadGermanTranslator(translator)) {
        QSKIP("sudoku_de.qm not built; build the main 'sudoku' target first");
    }

    QVERIFY(QCoreApplication::installTranslator(&translator));
    QApplication::processEvents();

    auto* pages_after = training->findChild<QStackedWidget*>();
    QVERIFY(pages_after != nullptr);
    QVERIFY(pages_after->count() > 0);

    QCoreApplication::removeTranslator(&translator);
    QApplication::processEvents();
}

QTEST_MAIN(TestLanguageChange)
#include "test_language_change.moc"
