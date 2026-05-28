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

/// Tests for the shared atomic file-write utility (file_utils::atomicWriteFile).
/// Atomicity guarantee: the target is only ever replaced by a fully-written,
/// fsync'd temp file via rename. A failed/partial write must never touch an
/// existing target, and no .tmp must be left behind on success.

#include "../../src/core/file_utils.h"
#include "../helpers/test_utils.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> toBytes(const std::string& s) {
    return {s.begin(), s.end()};
}

std::string readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("file_utils::atomicWriteFile writes content and leaves no temp", "[file_utils]") {
    TempTestDir tmp;
    auto target = tmp.path() / "out.bin";

    auto result = sudoku::core::file_utils::atomicWriteFile(target, toBytes("hello world"));

    REQUIRE(result.has_value());
    REQUIRE(fs::exists(target));
    REQUIRE(readAll(target) == "hello world");
    // The .tmp scratch file must be gone after a successful write.
    REQUIRE_FALSE(fs::exists(fs::path(target) += ".tmp"));
}

TEST_CASE("file_utils::atomicWriteFile overwrites a longer target without stale tail", "[file_utils]") {
    TempTestDir tmp;
    auto target = tmp.path() / "out.bin";

    REQUIRE(sudoku::core::file_utils::atomicWriteFile(target, toBytes("a long initial payload")).has_value());
    REQUIRE(sudoku::core::file_utils::atomicWriteFile(target, toBytes("short")).has_value());

    // rename-based replacement must not leave trailing bytes from the longer file.
    REQUIRE(readAll(target) == "short");
}

TEST_CASE("file_utils::atomicWriteFile overwrites a pre-existing stray .tmp", "[file_utils]") {
    TempTestDir tmp;
    auto target = tmp.path() / "out.bin";
    auto stray_tmp = fs::path(target) += ".tmp";

    // Simulate a crashed previous run that left a partial .tmp behind.
    {
        std::ofstream f(stray_tmp, std::ios::binary);
        f << "garbage from a crash";
    }

    auto result = sudoku::core::file_utils::atomicWriteFile(target, toBytes("clean data"));

    REQUIRE(result.has_value());
    REQUIRE(readAll(target) == "clean data");
    REQUIRE_FALSE(fs::exists(stray_tmp));
}

TEST_CASE("file_utils::atomicWriteFile fails cleanly and leaves existing target intact", "[file_utils]") {
    TempTestDir tmp;
    auto target = tmp.path() / "out.bin";
    REQUIRE(sudoku::core::file_utils::atomicWriteFile(target, toBytes("original content")).has_value());

    // Writing into a non-existent subdirectory cannot succeed: the .tmp open fails.
    auto bad_target = tmp.path() / "no_such_dir" / "out.bin";
    auto result = sudoku::core::file_utils::atomicWriteFile(bad_target, toBytes("won't land"));

    REQUIRE_FALSE(result.has_value());
    // The original, unrelated target is untouched.
    REQUIRE(readAll(target) == "original content");
    REQUIRE_FALSE(fs::exists(bad_target));
}

TEST_CASE("file_utils::atomicWriteFile writes empty data", "[file_utils]") {
    TempTestDir tmp;
    auto target = tmp.path() / "empty.bin";

    auto result = sudoku::core::file_utils::atomicWriteFile(target, {});

    REQUIRE(result.has_value());
    REQUIRE(fs::exists(target));
    REQUIRE(readAll(target).empty());
    REQUIRE_FALSE(fs::exists(fs::path(target) += ".tmp"));
}
