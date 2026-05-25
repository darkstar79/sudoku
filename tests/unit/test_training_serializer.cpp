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

#include "../../src/core/training_learning_path.h"
#include "../../src/core/training_statistics_serializer.h"
#include "../helpers/test_utils.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::core::training_stats_serializer;

TEST_CASE("training_stats_serializer — round-trip serialize/deserialize", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "stats.yaml";

    std::unordered_map<SolvingTechnique, TechniqueStats> stats;
    TechniqueStats ts;
    ts.total_exercises_attempted = 10;
    ts.total_correct = 7;
    ts.best_score = 4;
    ts.best_session_hints = 2;
    ts.proficient_count = 1;
    ts.mastered_count = 0;
    ts.average_hints = 1.5;
    ts.last_practiced = std::chrono::system_clock::time_point(std::chrono::seconds(1700000000));
    stats[SolvingTechnique::NakedPair] = ts;

    auto write_result = serializeToYaml(stats, file);
    REQUIRE(write_result.has_value());

    auto read_result = deserializeFromYaml(file);
    REQUIRE(read_result.has_value());

    auto& loaded = *read_result;
    REQUIRE(loaded.contains(SolvingTechnique::NakedPair));
    auto& loaded_ts = loaded[SolvingTechnique::NakedPair];
    CHECK(loaded_ts.total_exercises_attempted == 10);
    CHECK(loaded_ts.total_correct == 7);
    CHECK(loaded_ts.best_score == 4);
    CHECK(loaded_ts.best_session_hints == 2);
    CHECK(loaded_ts.proficient_count == 1);
    CHECK(loaded_ts.mastered_count == 0);
    CHECK(loaded_ts.average_hints == 1.5);
    CHECK(loaded_ts.last_practiced == std::chrono::system_clock::time_point(std::chrono::seconds(1700000000)));
}

TEST_CASE("training_stats_serializer — deserialize non-existent file returns empty", "[TrainingSerializer]") {
    auto result = deserializeFromYaml("/tmp/sudoku_test_serializer_nonexistent/stats.yaml");
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("training_stats_serializer — deserialize empty YAML (no techniques key)", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "empty_stats.yaml";
    {
        std::ofstream f(file);
        f << "version: '1.0'\n";
    }

    auto result = deserializeFromYaml(file);
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("training_stats_serializer — deserialize unknown technique is skipped", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "unknown_tech.yaml";
    {
        std::ofstream f(file);
        f << "version: '1.0'\n"
          << "techniques:\n"
          << "  FakeTechnique:\n"
          << "    total_exercises_attempted: 5\n"
          << "    total_correct: 3\n"
          << "    best_score: 3\n"
          << "    best_session_hints: 1\n"
          << "    proficient_count: 0\n"
          << "    mastered_count: 0\n"
          << "    average_hints: 0.5\n"
          << "    last_practiced: 0\n";
    }

    auto result = deserializeFromYaml(file);
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("training_stats_serializer — deserialize malformed YAML returns error", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "bad.yaml";
    {
        std::ofstream f(file);
        f << "{{{{not valid yaml at all";
    }

    auto result = deserializeFromYaml(file);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == TrainingStatsError::SerializationError);
}

TEST_CASE("training_stats_serializer — serialize to read-only path returns error", "[TrainingSerializer]") {
    // Use a path that can't be written to
    auto result = serializeToYaml({}, "/proc/sudoku_test_readonly_stats.yaml");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == TrainingStatsError::FileAccessError);
}

TEST_CASE("training_stats_serializer — multiple techniques round-trip", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "multi.yaml";

    std::unordered_map<SolvingTechnique, TechniqueStats> stats;
    stats[SolvingTechnique::NakedSingle].total_correct = 5;
    stats[SolvingTechnique::XWing].total_correct = 3;
    stats[SolvingTechnique::ForcingChain].total_correct = 1;

    REQUIRE(serializeToYaml(stats, file).has_value());

    auto result = deserializeFromYaml(file);
    REQUIRE(result.has_value());
    CHECK(result->size() == 3);
    CHECK(result->at(SolvingTechnique::NakedSingle).total_correct == 5);
    CHECK(result->at(SolvingTechnique::XWing).total_correct == 3);
    CHECK(result->at(SolvingTechnique::ForcingChain).total_correct == 1);
}

// Regression for #13 (BL-10): the prior loop bound stopped at GroupedXCycles (41),
// silently dropping every technique with enum value 42..53 on reload. Round-trip
// representatives from that range to ensure each survives a save/load cycle.
TEST_CASE("training_stats_serializer — advanced techniques (≥42) round-trip", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "advanced.yaml";

    std::unordered_map<SolvingTechnique, TechniqueStats> stats;
    const std::array advanced = {
        SolvingTechnique::SashimiXWing,        // 42
        SolvingTechnique::SashimiSwordfish,    // 43
        SolvingTechnique::SashimiJellyfish,    // 44
        SolvingTechnique::UnitForcingChain,    // 45
        SolvingTechnique::RegionForcingChain,  // 46
        SolvingTechnique::MutantFish,          // 47
        SolvingTechnique::KrakenFish,          // 48
        SolvingTechnique::ALSChain,            // 49
        SolvingTechnique::JuniorExocet,        // 50
        SolvingTechnique::UniqueLoop,          // 51
        SolvingTechnique::ContinuousNiceLoop,  // 52
        SolvingTechnique::GroupedNiceLoop,     // 53
    };
    int n = 1;
    for (auto tech : advanced) {
        stats[tech].total_correct = n++;
    }

    REQUIRE(serializeToYaml(stats, file).has_value());

    auto result = deserializeFromYaml(file);
    REQUIRE(result.has_value());
    CHECK(result->size() == advanced.size());
    int expected = 1;
    for (auto tech : advanced) {
        INFO("technique = " << getTechniqueName(tech));
        REQUIRE(result->contains(tech));
        CHECK(result->at(tech).total_correct == expected++);
    }
}

// Defense-in-depth: every technique in kAllTechniques must round-trip by name,
// so a future addition that forgets to wire up getTechniqueName() fails loudly.
TEST_CASE("training_stats_serializer — every kAllTechniques entry round-trips", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "all.yaml";

    std::unordered_map<SolvingTechnique, TechniqueStats> stats;
    int n = 1;
    for (auto tech : kAllTechniques) {
        stats[tech].total_correct = n++;
    }

    REQUIRE(serializeToYaml(stats, file).has_value());
    auto result = deserializeFromYaml(file);
    REQUIRE(result.has_value());
    CHECK(result->size() == kAllTechniques.size());
    for (auto tech : kAllTechniques) {
        INFO("technique = " << getTechniqueName(tech));
        CHECK(result->contains(tech));
    }
}

TEST_CASE("training_stats_serializer — Backtracking technique round-trip", "[TrainingSerializer]") {
    sudoku::test::TempTestDir tmp;
    auto file = tmp.path() / "backtracking.yaml";

    std::unordered_map<SolvingTechnique, TechniqueStats> stats;
    stats[SolvingTechnique::Backtracking].total_correct = 1;

    REQUIRE(serializeToYaml(stats, file).has_value());

    auto result = deserializeFromYaml(file);
    REQUIRE(result.has_value());
    CHECK(result->contains(SolvingTechnique::Backtracking));
    CHECK(result->at(SolvingTechnique::Backtracking).total_correct == 1);
}
