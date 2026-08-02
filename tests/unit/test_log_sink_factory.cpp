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

#include "infrastructure/log_sink_factory.h"

#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using sudoku::infrastructure::makeTruncatingFileSink;

// Guards the rule that a log file the process cannot open must never cost the user the application:
// the caller falls back to console-only logging, so this factory reports failure by returning an
// empty pointer rather than by throwing.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("Truncating file sink factory", "[log_sink_factory]") {
    const auto tmp = std::filesystem::temp_directory_path() / "sudoku_log_sink_factory_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    SECTION("returns a sink for a writable path") {
        const auto log_path = tmp / "sudoku_debug.log";
        const auto sink = makeTruncatingFileSink(log_path);

        REQUIRE(sink != nullptr);
        REQUIRE(std::filesystem::exists(log_path));
    }

    SECTION("truncates an existing file") {
        const auto log_path = tmp / "existing.log";
        {
            std::ofstream seed(log_path);
            seed << "stale content from the previous launch";
        }
        REQUIRE(std::filesystem::file_size(log_path) > 0);

        const auto sink = makeTruncatingFileSink(log_path);

        REQUIRE(sink != nullptr);
        REQUIRE(std::filesystem::file_size(log_path) == 0);
    }

    SECTION("a merely missing parent directory is not a failure — spdlog creates it") {
        // Worth pinning: spdlog's file_helper mkdir -p's the parent, so "the directory does not
        // exist yet" must NOT be treated as the unopenable case, and the caller's own
        // create_directories call is belt-and-braces rather than a precondition.
        const auto log_path = tmp / "not_yet_created" / "sudoku_debug.log";

        const auto sink = makeTruncatingFileSink(log_path);

        REQUIRE(sink != nullptr);
        REQUIRE(std::filesystem::exists(log_path));
    }

    SECTION("returns nullptr instead of throwing when a path component is a file") {
        // Cannot be created at any depth: the parent names an existing regular file. Stands in for
        // the real-world unopenable cases — a read-only install tree, a sandbox, or a path the
        // active code page cannot represent on Windows.
        const auto blocker = tmp / "blocker";
        {
            std::ofstream create(blocker);
        }
        REQUIRE(std::filesystem::is_regular_file(blocker));
        const auto log_path = blocker / "sub" / "sudoku_debug.log";

        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sink;
        REQUIRE_NOTHROW(sink = makeTruncatingFileSink(log_path));
        REQUIRE(sink == nullptr);
    }

    SECTION("returns nullptr when the path names an existing directory") {
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sink;
        REQUIRE_NOTHROW(sink = makeTruncatingFileSink(tmp));
        REQUIRE(sink == nullptr);
    }

    std::filesystem::remove_all(tmp);
}
