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

#include "../../src/core/settings_manager.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

std::filesystem::path createTempDir() {
    auto tmp = std::filesystem::temp_directory_path() / "sudoku_test_settings";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    return tmp;
}

}  // namespace

TEST_CASE("SettingsManager - Default values when no file exists", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    SettingsManager mgr(path);
    const auto& s = mgr.getSettings();

    CHECK(s.max_hints == 10);
    CHECK(s.auto_save_interval_ms == 30000);
    CHECK(s.default_difficulty == Difficulty::Medium);
    CHECK(s.show_conflicts == true);
    CHECK(s.show_hints == true);
    CHECK(s.language == "en");

    std::filesystem::remove_all(tmp);
}

TEST_CASE("SettingsManager - Save and load round-trip", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    {
        SettingsManager mgr(path);
        mgr.setMaxHints(5);
        mgr.setAutoSaveInterval(60000);
        mgr.setDefaultDifficulty(Difficulty::Hard);
        mgr.setShowConflicts(false);
        mgr.setShowHints(false);
        mgr.setLanguage("de");
    }

    // Load from same file
    SettingsManager mgr2(path);
    const auto& s = mgr2.getSettings();

    CHECK(s.max_hints == 5);
    CHECK(s.auto_save_interval_ms == 60000);
    CHECK(s.default_difficulty == Difficulty::Hard);
    CHECK(s.show_conflicts == false);
    CHECK(s.show_hints == false);
    CHECK(s.language == "de");

    std::filesystem::remove_all(tmp);
}

TEST_CASE("SettingsManager - Corrupt YAML falls back to defaults", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    {
        std::ofstream out(path);
        out << "{{{{ invalid yaml !@#$";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings() == Settings{});

    std::filesystem::remove_all(tmp);
}

TEST_CASE("SettingsManager - Observable notifies on change", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    SettingsManager mgr(path);

    int notify_count = 0;
    Settings last_notified;
    mgr.settingsObservable().subscribe([&](const Settings& s) {
        ++notify_count;
        last_notified = s;
    });

    mgr.setMaxHints(3);
    CHECK(notify_count == 1);
    CHECK(last_notified.max_hints == 3);

    // Setting same value should NOT notify
    mgr.setMaxHints(3);
    CHECK(notify_count == 1);

    mgr.setShowConflicts(false);
    CHECK(notify_count == 2);
    CHECK(last_notified.show_conflicts == false);

    std::filesystem::remove_all(tmp);
}

TEST_CASE("SettingsManager - Values are clamped to valid ranges", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    SettingsManager mgr(path);

    mgr.setMaxHints(0);
    CHECK(mgr.getSettings().max_hints == 1);

    mgr.setMaxHints(100);
    CHECK(mgr.getSettings().max_hints == 50);

    mgr.setAutoSaveInterval(1000);
    CHECK(mgr.getSettings().auto_save_interval_ms == 10000);

    mgr.setAutoSaveInterval(999999);
    CHECK(mgr.getSettings().auto_save_interval_ms == 300000);

    std::filesystem::remove_all(tmp);
}

TEST_CASE("SettingsManager - Migrate language.txt on first run", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    // Create a legacy language.txt
    {
        std::ofstream out(tmp / "language.txt");
        out << "de";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings().language == "de");

    // Settings file should now exist
    CHECK(std::filesystem::exists(path));

    std::filesystem::remove_all(tmp);
}

TEST_CASE("SettingsManager - No migration if settings.yaml already exists", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    // Create settings file by changing a setting (triggers save)
    {
        SettingsManager mgr(path);
        mgr.setMaxHints(5);
    }

    // Create language.txt with different value
    {
        std::ofstream out(tmp / "language.txt");
        out << "fr";
    }

    // Should NOT migrate — settings.yaml already exists
    SettingsManager mgr(path);
    CHECK(mgr.getSettings().language == "en");

    std::filesystem::remove_all(tmp);
}

TEST_CASE("SettingsManager - All difficulty values round-trip", "[settings]") {
    auto tmp = createTempDir();
    auto path = tmp / "settings.yaml";

    for (auto d : {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert, Difficulty::Master}) {
        {
            SettingsManager mgr(path);
            mgr.setDefaultDifficulty(d);
        }
        SettingsManager mgr2(path);
        CHECK(mgr2.getSettings().default_difficulty == d);
    }

    std::filesystem::remove_all(tmp);
}
