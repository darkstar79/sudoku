// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "infrastructure/path_utils.h"

#include <filesystem>
#include <string>

#include <QString>
#include <catch2/catch_test_macros.hpp>

using sudoku::infrastructure::toFilesystemPath;

// The regression this guards is Windows-only: MSVC's std::filesystem::path interprets a narrow
// string in the active code page, so handing it the UTF-8 bytes of QString::toStdString() mangles
// any non-ASCII component — e.g. the per-user install path under a profile named "Müller", which
// then makes the debug-log sink fail to open. The Windows CI job runs unit_tests.exe, so this case
// is executed on the platform where it can actually fail.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("QString to filesystem path conversion", "[path_utils]") {
    SECTION("ASCII path round-trips") {
        const auto path = toFilesystemPath(QStringLiteral("C:/Users/dev/AppData/Local/Programs/Sudoku"));
        REQUIRE(path.filename() == "Sudoku");
    }

    SECTION("non-ASCII path component survives the conversion") {
        // A German profile name is the realistic case: %LOCALAPPDATA% carries the user name.
        const QString qt_path = QString::fromUtf8("/home/Müller/Sudoku");
        const auto path = toFilesystemPath(qt_path);

        // Comparing against the wide/UTF-8 literal in the platform's own native encoding is the
        // point: a code-page misinterpretation changes the byte sequence and fails here.
        REQUIRE(path.filename() == "Sudoku");
        REQUIRE(path.parent_path().filename().wstring() == std::wstring(L"Müller"));
    }

    SECTION("path with several non-ASCII components is preserved end to end") {
        const QString qt_path = QString::fromUtf8("/tmp/Ünïcödé/日本語/Sudoku");
        const auto path = toFilesystemPath(qt_path);

        REQUIRE(path.wstring() == std::wstring(L"/tmp/Ünïcödé/日本語/Sudoku"));
    }

    SECTION("empty string yields an empty path") {
        REQUIRE(toFilesystemPath(QString()).empty());
    }
}
