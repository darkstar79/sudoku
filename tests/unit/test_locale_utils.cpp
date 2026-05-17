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

#include "../../src/core/locale_utils.h"
#include "../helpers/test_utils.h"

#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using sudoku::core::isValidLocaleCode;
using sudoku::core::scanInstalledLocales;

TEST_CASE("isValidLocaleCode accepts BCP 47 subset", "[locale]") {
    CHECK(isValidLocaleCode("en"));
    CHECK(isValidLocaleCode("de"));
    CHECK(isValidLocaleCode("ru"));
    CHECK(isValidLocaleCode("fil"));    // 3-letter language
    CHECK(isValidLocaleCode("zh_CN"));  // language + region
    CHECK(isValidLocaleCode("pt_BR"));
}

TEST_CASE("isValidLocaleCode rejects malformed codes", "[locale]") {
    CHECK_FALSE(isValidLocaleCode(""));
    CHECK_FALSE(isValidLocaleCode("e"));        // too short
    CHECK_FALSE(isValidLocaleCode("englsh"));   // too long (6 lowercase)
    CHECK_FALSE(isValidLocaleCode("EN"));       // wrong case
    CHECK_FALSE(isValidLocaleCode("en_de"));    // region must be uppercase
    CHECK_FALSE(isValidLocaleCode("en_"));      // trailing underscore
    CHECK_FALSE(isValidLocaleCode("en_C"));     // region too short
    CHECK_FALSE(isValidLocaleCode("en_USA"));   // region too long
    CHECK_FALSE(isValidLocaleCode("sr_Latn"));  // script subtags not supported
    CHECK_FALSE(isValidLocaleCode("en-US"));    // hyphen, not underscore
    CHECK_FALSE(isValidLocaleCode("en US"));    // space
    CHECK_FALSE(isValidLocaleCode("en.de"));    // dot
    CHECK_FALSE(isValidLocaleCode("en/de"));    // slash
}

TEST_CASE("isValidLocaleCode blocks path-traversal payloads", "[locale][security]") {
    CHECK_FALSE(isValidLocaleCode("../../etc/passwd"));
    CHECK_FALSE(isValidLocaleCode(".."));
    CHECK_FALSE(isValidLocaleCode("../en"));
    CHECK_FALSE(isValidLocaleCode("en;rm -rf /"));
    // String with embedded NUL: real attack would smuggle bytes past a C-string boundary.
    std::string with_nul = "en";
    with_nul.push_back('\0');
    with_nul += "evil";
    CHECK_FALSE(isValidLocaleCode(with_nul));
    CHECK_FALSE(isValidLocaleCode("\xff\xfe"));  // non-ASCII bytes
    std::string very_long(10000, 'a');
    CHECK_FALSE(isValidLocaleCode(very_long));
}

namespace {

void touch(const std::filesystem::path& p) {
    std::ofstream out(p);
}

}  // namespace

TEST_CASE("scanInstalledLocales returns empty list for missing dir", "[locale]") {
    sudoku::test::TempTestDir tmp;
    auto missing = tmp.path() / "does_not_exist";
    auto result = scanInstalledLocales(missing);
    CHECK(result.empty());
}

TEST_CASE("scanInstalledLocales returns empty list for empty dir", "[locale]") {
    sudoku::test::TempTestDir tmp;
    auto result = scanInstalledLocales(tmp.path());
    CHECK(result.empty());
}

TEST_CASE("scanInstalledLocales finds sudoku_<code>.qm files", "[locale]") {
    sudoku::test::TempTestDir tmp;
    touch(tmp.path() / "sudoku_en.qm");
    touch(tmp.path() / "sudoku_de.qm");
    touch(tmp.path() / "sudoku_ru.qm");
    touch(tmp.path() / "sudoku_zh_CN.qm");

    auto result = scanInstalledLocales(tmp.path());

    REQUIRE(result.size() == 4);
    // Output is sorted alphabetically for determinism
    CHECK(result[0] == "de");
    CHECK(result[1] == "en");
    CHECK(result[2] == "ru");
    CHECK(result[3] == "zh_CN");
}

TEST_CASE("scanInstalledLocales ignores non-matching files", "[locale]") {
    sudoku::test::TempTestDir tmp;
    touch(tmp.path() / "sudoku_en.qm");
    touch(tmp.path() / "sudoku.qm");         // no locale
    touch(tmp.path() / "sudoku_.qm");        // empty locale
    touch(tmp.path() / "readme.txt");        // wrong extension
    touch(tmp.path() / "other_en.qm");       // wrong prefix
    touch(tmp.path() / "sudoku_en.qm.bak");  // wrong suffix

    auto result = scanInstalledLocales(tmp.path());

    REQUIRE(result.size() == 1);
    CHECK(result[0] == "en");
}

TEST_CASE("scanInstalledLocales rejects invalid locale codes in filenames", "[locale][security]") {
    sudoku::test::TempTestDir tmp;
    touch(tmp.path() / "sudoku_en.qm");
    touch(tmp.path() / "sudoku_EN.qm");     // wrong case
    touch(tmp.path() / "sudoku_en-US.qm");  // hyphen
    touch(tmp.path() / "sudoku_evil$.qm");  // shell metacharacter

    auto result = scanInstalledLocales(tmp.path());

    REQUIRE(result.size() == 1);
    CHECK(result[0] == "en");
}

TEST_CASE("scanInstalledLocales is non-recursive", "[locale]") {
    sudoku::test::TempTestDir tmp;
    auto sub = tmp.path() / "sub";
    std::filesystem::create_directory(sub);
    touch(tmp.path() / "sudoku_en.qm");
    touch(sub / "sudoku_de.qm");

    auto result = scanInstalledLocales(tmp.path());

    REQUIRE(result.size() == 1);
    CHECK(result[0] == "en");
}
