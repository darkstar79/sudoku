// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// Custom Catch2 entry point that owns a QCoreApplication for the lifetime of
// the test run. Without this, calls to QCoreApplication::translate() in unit
// tests fall through Qt's null-qApp path — an undocumented short-circuit that
// has historically returned the source string but is not part of Qt's public
// contract. With qApp present and no translator installed, translate() runs
// the documented "no installed translators" branch and returns the source
// string deterministically, the way the production code expects.
//
// Tests that want to exercise translation can installTranslator() on this
// qApp and removeTranslator() before the test ends.

#include <QCoreApplication>
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    QCoreApplication qt_app(argc, argv);
    return Catch::Session().run(argc, argv);
}
