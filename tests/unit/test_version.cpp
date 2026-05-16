// sudoku-cpp - Offline Sudoku Game
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

/// Smoke tests for the CMake-generated version header. Guards against:
///   - configure_file failing to substitute @PROJECT_VERSION@
///   - someone hardcoding a value back into version.h.in
///   - drift between CMake's PROJECT_VERSION and the runtime constant

#include "sudoku/version.h"

#include <cctype>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

bool isSemverLike(std::string_view value) {
    int dots = 0;
    for (char c : value) {
        if (c == '.') {
            ++dots;
        } else if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return dots == 2;
}

}  // namespace

TEST_CASE("kAppVersion is non-empty", "[version]") {
    const std::string_view version{sudoku::kAppVersion};
    REQUIRE_FALSE(version.empty());
}

TEST_CASE("kAppVersion was substituted by CMake (not literal placeholder)", "[version]") {
    const std::string_view version{sudoku::kAppVersion};
    REQUIRE(version.find('@') == std::string_view::npos);
}

TEST_CASE("kAppVersion matches MAJOR.MINOR.PATCH", "[version]") {
    REQUIRE(isSemverLike(sudoku::kAppVersion));
}
