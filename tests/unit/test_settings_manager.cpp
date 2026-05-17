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
#include "../helpers/test_utils.h"

#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("SettingsManager - Default values when no file exists", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    SettingsManager mgr(path);
    const auto& s = mgr.getSettings();

    CHECK(s.max_hints == 10);
    CHECK(s.auto_save_interval_ms == 30000);
    CHECK(s.default_difficulty == Difficulty::Medium);
    CHECK(s.show_conflicts == true);
    CHECK(s.show_hints == true);
    CHECK(s.language == "en");
    CHECK(s.experimental_training_mode == false);
    CHECK(s.experimental_coaching_hints == false);
}

TEST_CASE("SettingsManager - Experimental flags load from YAML when present", "[settings][experimental]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    {
        std::ofstream out(path);
        out << "gameplay:\n"
            << "  max_hints: 10\n"
            << "experimental:\n"
            << "  training_mode: true\n"
            << "  coaching_hints: true\n";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings().experimental_training_mode == true);
    CHECK(mgr.getSettings().experimental_coaching_hints == true);
}

TEST_CASE("SettingsManager - Missing experimental section defaults to off (backward compat)",
          "[settings][experimental]") {
    // Simulates a settings.yaml written by a pre-1.0 binary that doesn't know
    // about the experimental section. Loading it must default both flags to false,
    // not crash, not throw.
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    {
        std::ofstream out(path);
        out << "gameplay:\n"
            << "  max_hints: 7\n"
            << "  default_difficulty: Hard\n"
            << "display:\n"
            << "  show_conflicts: false\n"
            << "language: de\n";
    }

    SettingsManager mgr(path);
    const auto& s = mgr.getSettings();

    // Sanity check that the file did load (non-default values present)
    CHECK(s.max_hints == 7);
    CHECK(s.default_difficulty == Difficulty::Hard);
    CHECK(s.show_conflicts == false);
    CHECK(s.language == "de");

    // Experimental flags default to false when the section is absent
    CHECK(s.experimental_training_mode == false);
    CHECK(s.experimental_coaching_hints == false);
}

TEST_CASE("SettingsManager - Experimental flags persist across save/load", "[settings][experimental]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    // Write a YAML file with both flags on, simulating a save by a 1.0 binary.
    {
        std::ofstream out(path);
        out << "experimental:\n"
            << "  training_mode: true\n"
            << "  coaching_hints: false\n";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings().experimental_training_mode == true);
    CHECK(mgr.getSettings().experimental_coaching_hints == false);

    // Force a save by toggling an orthogonal field, then reload — the experimental
    // values must round-trip through save() unchanged.
    mgr.setMaxHints(11);
    SettingsManager mgr2(path);
    CHECK(mgr2.getSettings().experimental_training_mode == true);
    CHECK(mgr2.getSettings().experimental_coaching_hints == false);
    CHECK(mgr2.getSettings().max_hints == 11);
}

TEST_CASE("SettingsManager - setExperimentalTrainingMode persists value", "[settings][experimental]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    {
        SettingsManager mgr(path);
        REQUIRE(mgr.getSettings().experimental_training_mode == false);
        mgr.setExperimentalTrainingMode(true);
        CHECK(mgr.getSettings().experimental_training_mode == true);
    }

    SettingsManager mgr2(path);
    CHECK(mgr2.getSettings().experimental_training_mode == true);
}

TEST_CASE("SettingsManager - setExperimentalCoachingHints persists value", "[settings][experimental]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    {
        SettingsManager mgr(path);
        REQUIRE(mgr.getSettings().experimental_coaching_hints == false);
        mgr.setExperimentalCoachingHints(true);
        CHECK(mgr.getSettings().experimental_coaching_hints == true);
    }

    SettingsManager mgr2(path);
    CHECK(mgr2.getSettings().experimental_coaching_hints == true);
}

TEST_CASE("SettingsManager - Experimental setters notify observers on change only", "[settings][experimental]") {
    // Matches the existing notifyIfChanged convention: setting the same value
    // must NOT fire the observable, only an actual transition does.
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    SettingsManager mgr(path);

    int notify_count = 0;
    Settings last_notified;
    mgr.settingsObservable().subscribe([&](const Settings& s) {
        ++notify_count;
        last_notified = s;
    });

    mgr.setExperimentalTrainingMode(true);
    CHECK(notify_count == 1);
    CHECK(last_notified.experimental_training_mode == true);

    // No-op transition: same value, no notification.
    mgr.setExperimentalTrainingMode(true);
    CHECK(notify_count == 1);

    mgr.setExperimentalCoachingHints(true);
    CHECK(notify_count == 2);
    CHECK(last_notified.experimental_coaching_hints == true);

    mgr.setExperimentalCoachingHints(true);
    CHECK(notify_count == 2);

    // Both flags off again — two more notifications.
    mgr.setExperimentalTrainingMode(false);
    mgr.setExperimentalCoachingHints(false);
    CHECK(notify_count == 4);
    CHECK(last_notified.experimental_training_mode == false);
    CHECK(last_notified.experimental_coaching_hints == false);
}

TEST_CASE("SettingsManager - Save and load round-trip", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

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
}

TEST_CASE("SettingsManager - Corrupt YAML falls back to defaults", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    {
        std::ofstream out(path);
        out << "{{{{ invalid yaml !@#$";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings() == Settings{});
}

TEST_CASE("SettingsManager - Observable notifies on change", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

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
}

TEST_CASE("SettingsManager - Values are clamped to valid ranges", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    SettingsManager mgr(path);

    mgr.setMaxHints(0);
    CHECK(mgr.getSettings().max_hints == 1);

    mgr.setMaxHints(100);
    CHECK(mgr.getSettings().max_hints == 50);

    mgr.setAutoSaveInterval(1000);
    CHECK(mgr.getSettings().auto_save_interval_ms == 10000);

    mgr.setAutoSaveInterval(999999);
    CHECK(mgr.getSettings().auto_save_interval_ms == 300000);
}

TEST_CASE("SettingsManager - Migrate language.txt on first run", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    // Create a legacy language.txt
    {
        std::ofstream out(tmp.path() / "language.txt");
        out << "de";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings().language == "de");

    // Settings file should now exist
    CHECK(std::filesystem::exists(path));
}

TEST_CASE("SettingsManager - No migration if settings.yaml already exists", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    // Create settings file by changing a setting (triggers save)
    {
        SettingsManager mgr(path);
        mgr.setMaxHints(5);
    }

    // Create language.txt with different value
    {
        std::ofstream out(tmp.path() / "language.txt");
        out << "fr";
    }

    // Should NOT migrate — settings.yaml already exists
    SettingsManager mgr(path);
    CHECK(mgr.getSettings().language == "en");
}

TEST_CASE("SettingsManager - setLanguage rejects invalid codes", "[settings][security]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    SettingsManager mgr(path);
    REQUIRE(mgr.getSettings().language == "en");

    mgr.setLanguage("../../tmp/evil");
    CHECK(mgr.getSettings().language == "en");

    mgr.setLanguage("EN");
    CHECK(mgr.getSettings().language == "en");

    mgr.setLanguage("");
    CHECK(mgr.getSettings().language == "en");

    // Valid code is still accepted.
    mgr.setLanguage("de");
    CHECK(mgr.getSettings().language == "de");
}

TEST_CASE("SettingsManager - YAML with bogus language falls back to default", "[settings][security]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    {
        std::ofstream out(path);
        out << "language: \"../etc/passwd\"\n";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings().language == "en");
}

TEST_CASE("SettingsManager - language.txt migration rejects invalid code", "[settings][security]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    {
        std::ofstream out(tmp.path() / "language.txt");
        out << "../../";
    }

    SettingsManager mgr(path);
    CHECK(mgr.getSettings().language == "en");
}

TEST_CASE("SettingsManager - All difficulty values round-trip", "[settings]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "settings.yaml";

    for (auto d : {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert, Difficulty::Master}) {
        {
            SettingsManager mgr(path);
            mgr.setDefaultDifficulty(d);
        }
        SettingsManager mgr2(path);
        CHECK(mgr2.getSettings().default_difficulty == d);
    }
}
