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

#include "core/i_localization_manager.h"
#include "core/localization_manager.h"

#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

/// Helper to create a temporary directory with locale YAML files for testing.
class TempLocaleDir {
public:
    TempLocaleDir() : dir_(std::filesystem::temp_directory_path() / "sudoku_test_locales") {
        std::filesystem::create_directories(dir_);
    }

    ~TempLocaleDir() {
        std::filesystem::remove_all(dir_);
    }

    TempLocaleDir(const TempLocaleDir&) = delete;
    TempLocaleDir& operator=(const TempLocaleDir&) = delete;
    TempLocaleDir(TempLocaleDir&&) = delete;
    TempLocaleDir& operator=(TempLocaleDir&&) = delete;

    /// Write a YAML file to the temp directory.
    void writeFile(const std::string& filename, const std::string& content) const {
        std::ofstream out(dir_ / filename);
        out << content;
    }

    [[nodiscard]] const std::filesystem::path& path() const {
        return dir_;
    }

private:
    std::filesystem::path dir_;
};

const std::string ENGLISH_YAML = R"(locale: en
name: English

strings:
  menu.game: "Game"
  menu.new_game: "New Game"
  stats.games_played: "Games Played: {0}"
  stats.completion_rate: "Completion Rate: {0:.1f}%"
)";

const std::string GERMAN_YAML = R"(locale: de
name: Deutsch

strings:
  menu.game: "Spiel"
  menu.new_game: "Neues Spiel"
  stats.games_played: "Gespielte Spiele: {0}"
)";

const std::string INVALID_YAML = R"(this is not: [valid: yaml: {{)";

const std::string MISSING_STRINGS_YAML = R"(locale: fr
name: Français
)";

}  // namespace

TEST_CASE("LocalizationManager - Load and retrieve strings", "[localization]") {
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);

    LocalizationManager manager(temp.path());

    SECTION("getString returns localized value after setLocale") {
        auto result = manager.setLocale("en");

        REQUIRE(result.has_value());
        REQUIRE(manager.getString("menu.game") == "Game");
        REQUIRE(manager.getString("menu.new_game") == "New Game");
    }

    SECTION("getString returns parameterized template strings") {
        auto result = manager.setLocale("en");

        REQUIRE(result.has_value());
        REQUIRE(manager.getString("stats.games_played") == "Games Played: {0}");
    }

    SECTION("getCurrentLocale returns active locale code") {
        auto result = manager.setLocale("en");

        REQUIRE(result.has_value());
        REQUIRE(manager.getCurrentLocale() == "en");
    }
}

TEST_CASE("LocalizationManager - Missing key handling", "[localization]") {
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);

    LocalizationManager manager(temp.path());
    auto result = manager.setLocale("en");
    REQUIRE(result.has_value());

    SECTION("Missing key returns the key itself") {
        auto value = manager.getString("nonexistent.key");

        REQUIRE(value == "nonexistent.key");
    }

    SECTION("Missing key returns stable view on repeated calls") {
        auto first = manager.getString("missing.key");
        auto second = manager.getString("missing.key");

        REQUIRE(first == second);
    }
}

TEST_CASE("LocalizationManager - Locale switching", "[localization]") {
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);
    temp.writeFile("de.yaml", GERMAN_YAML);

    LocalizationManager manager(temp.path());

    SECTION("Switching locale changes returned strings") {
        auto en_result = manager.setLocale("en");
        REQUIRE(en_result.has_value());
        REQUIRE(manager.getString("menu.game") == "Game");

        auto de_result = manager.setLocale("de");
        REQUIRE(de_result.has_value());
        REQUIRE(manager.getString("menu.game") == "Spiel");
        REQUIRE(manager.getCurrentLocale() == "de");
    }

    SECTION("Switching to non-English locale uses English fallback for missing keys") {
        auto en_result = manager.setLocale("en");
        REQUIRE(en_result.has_value());

        auto de_result = manager.setLocale("de");
        REQUIRE(de_result.has_value());

        // German YAML is missing stats.completion_rate, should fall back to English
        REQUIRE(manager.getString("stats.completion_rate") == "Completion Rate: {0:.1f}%");
    }
}

TEST_CASE("LocalizationManager - Error handling", "[localization]") {
    SECTION("setLocale with non-existent locale returns error") {
        TempLocaleDir temp;
        temp.writeFile("en.yaml", ENGLISH_YAML);

        LocalizationManager manager(temp.path());

        auto result = manager.setLocale("xx");

        REQUIRE(!result.has_value());
        REQUIRE(result.error().find("not found") != std::string::npos);
    }

    SECTION("Construction with non-existent directory does not throw") {
        auto nonexistent = std::filesystem::temp_directory_path() / "sudoku_nonexistent_dir_12345";

        LocalizationManager manager(nonexistent);

        REQUIRE(manager.getAvailableLocales().empty());
    }

    SECTION("setLocale with invalid YAML returns error") {
        TempLocaleDir temp;
        temp.writeFile("bad.yaml", INVALID_YAML);

        LocalizationManager manager(temp.path());
        auto result = manager.setLocale("bad");

        REQUIRE(!result.has_value());
    }

    SECTION("setLocale with YAML missing strings section returns error") {
        TempLocaleDir temp;
        temp.writeFile("fr.yaml", MISSING_STRINGS_YAML);

        LocalizationManager manager(temp.path());
        auto result = manager.setLocale("fr");

        REQUIRE(!result.has_value());
        REQUIRE(result.error().find("missing") != std::string::npos);
    }
}

TEST_CASE("LocalizationManager - Available locales discovery", "[localization]") {
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);
    temp.writeFile("de.yaml", GERMAN_YAML);

    LocalizationManager manager(temp.path());

    SECTION("Discovers all YAML locale files") {
        auto locales = manager.getAvailableLocales();

        REQUIRE(locales.size() == 2);
    }

    SECTION("Available locales are sorted by code") {
        auto locales = manager.getAvailableLocales();

        REQUIRE(locales[0].first == "de");
        REQUIRE(locales[0].second == "Deutsch");
        REQUIRE(locales[1].first == "en");
        REQUIRE(locales[1].second == "English");
    }

    SECTION("Invalid YAML files are skipped during discovery") {
        temp.writeFile("broken.yaml", INVALID_YAML);

        // Re-create manager to re-discover
        LocalizationManager manager2(temp.path());
        auto locales = manager2.getAvailableLocales();

        // Only valid locale files are discovered
        REQUIRE(locales.size() == 2);
    }
}

TEST_CASE("LocalizationManager - English fallback loading", "[localization]") {
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);
    temp.writeFile("de.yaml", GERMAN_YAML);

    SECTION("Direct non-English setLocale loads English fallback automatically") {
        LocalizationManager manager(temp.path());

        // Set German directly without loading English first
        auto result = manager.setLocale("de");
        REQUIRE(result.has_value());

        // German key works
        REQUIRE(manager.getString("menu.game") == "Spiel");

        // Missing key in German falls back to English
        REQUIRE(manager.getString("stats.completion_rate") == "Completion Rate: {0:.1f}%");
    }

    SECTION("English locale is used as its own fallback") {
        LocalizationManager manager(temp.path());

        auto result = manager.setLocale("en");
        REQUIRE(result.has_value());

        // All English keys should resolve
        REQUIRE(manager.getString("menu.game") == "Game");
        REQUIRE(manager.getString("stats.completion_rate") == "Completion Rate: {0:.1f}%");
    }
}

TEST_CASE("LocalizationManager - Fallback edge cases", "[localization]") {
    SECTION("Non-English locale without en.yaml: no English fallback loaded") {
        // Only German, no English — the 'exists(en_file)' false branch
        TempLocaleDir temp;
        temp.writeFile("de.yaml", GERMAN_YAML);

        LocalizationManager manager(temp.path());
        auto result = manager.setLocale("de");

        // Load should succeed (German locale still loads)
        REQUIRE(result.has_value());
        REQUIRE(manager.getString("menu.game") == "Spiel");
        // Missing key in German falls back to key name (no English fallback available)
        auto missing = manager.getString("stats.completion_rate");
        REQUIRE(missing == "stats.completion_rate");
    }

    SECTION("getString key missing in both active and fallback returns key name") {
        TempLocaleDir temp;
        temp.writeFile("en.yaml", ENGLISH_YAML);
        temp.writeFile("de.yaml", GERMAN_YAML);

        LocalizationManager manager(temp.path());
        auto result = manager.setLocale("de");
        REQUIRE(result.has_value());

        // This key is in neither German nor English YAML
        auto val = manager.getString("totally.unknown.key");
        REQUIRE(val == "totally.unknown.key");
    }
}

TEST_CASE("LocalizationManager - locales path is a file not a directory", "[localization]") {
    // Covers the !is_directory() branch at discoverLocales line 110
    auto file_path = std::filesystem::temp_directory_path() / "sudoku_test_not_a_dir.yaml";
    {
        std::ofstream f(file_path);
        f << "not_a_dir: true\n";
    }

    // Pass a FILE path where a directory is expected
    LocalizationManager manager(file_path);

    // discoverLocales should warn and return; no locales discovered
    REQUIRE(manager.getAvailableLocales().empty());

    std::filesystem::remove(file_path);
}

TEST_CASE("LocalizationManager - Polymorphic usage through interface", "[localization]") {
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);

    SECTION("Can use through ILocalizationManager pointer") {
        std::shared_ptr<ILocalizationManager> manager = std::make_shared<LocalizationManager>(temp.path());

        auto result = manager->setLocale("en");
        REQUIRE(result.has_value());
        REQUIRE(manager->getString("menu.game") == "Game");
    }
}

TEST_CASE("LocalizationManager - Failed English fallback load is handled gracefully", "[localization]") {
    // Covers localization_manager.cpp line 50: warn branch when en.yaml fails to parse
    // while loading a non-English locale.
    // Setup: en.yaml is present but contains invalid YAML (parse fails → fallback warn)
    TempLocaleDir temp;
    temp.writeFile("en.yaml", INVALID_YAML);  // broken en.yaml
    temp.writeFile("de.yaml", GERMAN_YAML);   // valid German locale

    LocalizationManager manager(temp.path());

    // setLocale("de") sees en.yaml exists but loadYamlFile fails → warns, proceeds
    // The German locale itself may also fail (depends on broken en.yaml not affecting it)
    auto result = manager.setLocale("de");

    // German YAML is valid so setLocale should succeed; just no fallback available
    REQUIRE(result.has_value());
    // German key loads correctly
    REQUIRE(manager.getString("menu.game") == "Spiel");
}

TEST_CASE("LocalizationManager - Available locales sorted with many locales", "[localization]") {
    // Add 4 locales to exercise the sort lambda more thoroughly
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);
    temp.writeFile("de.yaml", GERMAN_YAML);
    temp.writeFile("fr.yaml", R"(locale: fr
name: Français

strings:
  menu.game: "Jeu"
  menu.new_game: "Nouveau jeu"
)");
    temp.writeFile("es.yaml", R"(locale: es
name: Español

strings:
  menu.game: "Juego"
  menu.new_game: "Nuevo juego"
)");

    LocalizationManager manager(temp.path());
    auto locales = manager.getAvailableLocales();

    REQUIRE(locales.size() == 4);
    // Should be sorted: de, en, es, fr
    REQUIRE(locales[0].first == "de");
    REQUIRE(locales[1].first == "en");
    REQUIRE(locales[2].first == "es");
    REQUIRE(locales[3].first == "fr");
}

TEST_CASE("LocalizationManager - Non-yaml files in locale directory are skipped", "[localization]") {
    // Covers the extension() != ".yaml" false branch (line 116) in discoverLocales.
    TempLocaleDir temp;
    temp.writeFile("en.yaml", ENGLISH_YAML);
    temp.writeFile("readme.txt", "This is not a locale file");

    LocalizationManager manager(temp.path());
    auto locales = manager.getAvailableLocales();

    // Only the .yaml file is discovered; readme.txt is silently skipped
    REQUIRE(locales.size() == 1);
    REQUIRE(locales[0].first == "en");
}
