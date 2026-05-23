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

#include "../../src/core/i_training_statistics_manager.h"
#include "../../src/core/training_statistics_manager.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("TrainingStatisticsManager — fresh state", "[TrainingStatistics]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    SECTION("getStats returns empty for unpracticed technique") {
        auto stats = mgr.getStats(SolvingTechnique::NakedSingle);
        CHECK(stats.total_exercises_attempted == 0);
        CHECK(stats.total_correct == 0);
        CHECK(stats.best_score == 0);
        CHECK(stats.best_session_hints == -1);
    }

    SECTION("getAllStats is empty") {
        auto all = mgr.getAllStats();
        CHECK(all.empty());
    }

    SECTION("getMastery returns Beginner for unpracticed") {
        CHECK(mgr.getMastery(SolvingTechnique::XWing) == MasteryLevel::Beginner);
    }
}

TEST_CASE("TrainingStatisticsManager — recordLesson", "[TrainingStatistics]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    TrainingLessonResult result;
    result.technique = SolvingTechnique::NakedPair;
    result.correct_count = 3;
    result.total_exercises = 5;
    result.hints_used = 2;

    SECTION("records basic lesson data") {
        auto res = mgr.recordLesson(result);
        REQUIRE(res.has_value());

        auto stats = mgr.getStats(SolvingTechnique::NakedPair);
        CHECK(stats.total_exercises_attempted == 5);
        CHECK(stats.total_correct == 3);
        CHECK(stats.best_score == 3);
        CHECK(stats.best_session_hints == 2);
    }

    SECTION("accumulates across multiple lessons") {
        REQUIRE(mgr.recordLesson(result).has_value());

        TrainingLessonResult second;
        second.technique = SolvingTechnique::NakedPair;
        second.correct_count = 4;
        second.total_exercises = 5;
        second.hints_used = 1;
        REQUIRE(mgr.recordLesson(second).has_value());

        auto stats = mgr.getStats(SolvingTechnique::NakedPair);
        CHECK(stats.total_exercises_attempted == 10);
        CHECK(stats.total_correct == 7);
        CHECK(stats.best_score == 4);
        CHECK(stats.best_session_hints == 1);
    }

    SECTION("tracks different techniques independently") {
        REQUIRE(mgr.recordLesson(result).has_value());

        TrainingLessonResult other;
        other.technique = SolvingTechnique::XWing;
        other.correct_count = 5;
        other.total_exercises = 5;
        other.hints_used = 0;
        REQUIRE(mgr.recordLesson(other).has_value());

        CHECK(mgr.getStats(SolvingTechnique::NakedPair).total_correct == 3);
        CHECK(mgr.getStats(SolvingTechnique::XWing).total_correct == 5);
    }
}

TEST_CASE("TrainingStatisticsManager — mastery levels", "[TrainingStatistics]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    SECTION("Beginner: best_score < 3") {
        TrainingLessonResult result;
        result.technique = SolvingTechnique::HiddenSingle;
        result.correct_count = 2;
        result.total_exercises = 5;
        result.hints_used = 0;
        REQUIRE(mgr.recordLesson(result).has_value());
        CHECK(mgr.getMastery(SolvingTechnique::HiddenSingle) == MasteryLevel::Beginner);
    }

    SECTION("Intermediate: best_score >= 3") {
        TrainingLessonResult result;
        result.technique = SolvingTechnique::HiddenSingle;
        result.correct_count = 3;
        result.total_exercises = 5;
        result.hints_used = 10;
        REQUIRE(mgr.recordLesson(result).has_value());
        CHECK(mgr.getMastery(SolvingTechnique::HiddenSingle) == MasteryLevel::Intermediate);
    }

    SECTION("Proficient: 5/5 with <= 5 hints") {
        TrainingLessonResult result;
        result.technique = SolvingTechnique::HiddenSingle;
        result.correct_count = 5;
        result.total_exercises = 5;
        result.hints_used = 3;
        REQUIRE(mgr.recordLesson(result).has_value());
        CHECK(mgr.getMastery(SolvingTechnique::HiddenSingle) == MasteryLevel::Proficient);
    }

    SECTION("Mastered: 5/5 with 0 hints, twice") {
        TrainingLessonResult result;
        result.technique = SolvingTechnique::HiddenSingle;
        result.correct_count = 5;
        result.total_exercises = 5;
        result.hints_used = 0;
        REQUIRE(mgr.recordLesson(result).has_value());
        CHECK(mgr.getMastery(SolvingTechnique::HiddenSingle) == MasteryLevel::Proficient);

        REQUIRE(mgr.recordLesson(result).has_value());
        CHECK(mgr.getMastery(SolvingTechnique::HiddenSingle) == MasteryLevel::Mastered);
    }
}

TEST_CASE("TrainingStatisticsManager — persistence", "[TrainingStatistics]") {
    sudoku::test::TempTestDir tmp;

    {
        TrainingStatisticsManager mgr(tmp.path());
        TrainingLessonResult result;
        result.technique = SolvingTechnique::Swordfish;
        result.correct_count = 4;
        result.total_exercises = 5;
        result.hints_used = 1;
        REQUIRE(mgr.recordLesson(result).has_value());
    }

    // Reload from disk
    TrainingStatisticsManager mgr2(tmp.path());
    auto stats = mgr2.getStats(SolvingTechnique::Swordfish);
    CHECK(stats.total_exercises_attempted == 5);
    CHECK(stats.total_correct == 4);
    CHECK(stats.best_score == 4);
}

TEST_CASE("TrainingStatisticsManager — resetAllStats", "[TrainingStatistics]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    TrainingLessonResult result;
    result.technique = SolvingTechnique::NakedSingle;
    result.correct_count = 5;
    result.total_exercises = 5;
    result.hints_used = 0;
    REQUIRE(mgr.recordLesson(result).has_value());

    mgr.resetAllStats();

    CHECK(mgr.getAllStats().empty());
    CHECK(mgr.getMastery(SolvingTechnique::NakedSingle) == MasteryLevel::Beginner);

    // Reload — should also be empty
    TrainingStatisticsManager mgr2(tmp.path());
    CHECK(mgr2.getAllStats().empty());
}

TEST_CASE("computeMastery — static method", "[TrainingStatistics]") {
    SECTION("Beginner") {
        TechniqueStats ts;
        ts.best_score = 2;
        CHECK(ITrainingStatisticsManager::computeMastery(ts) == MasteryLevel::Beginner);
    }

    SECTION("Intermediate") {
        TechniqueStats ts;
        ts.best_score = 4;
        CHECK(ITrainingStatisticsManager::computeMastery(ts) == MasteryLevel::Intermediate);
    }

    SECTION("Proficient") {
        TechniqueStats ts;
        ts.best_score = 5;
        ts.proficient_count = 1;
        CHECK(ITrainingStatisticsManager::computeMastery(ts) == MasteryLevel::Proficient);
    }

    SECTION("Mastered") {
        TechniqueStats ts;
        ts.best_score = 5;
        ts.proficient_count = 2;
        ts.mastered_count = 2;
        CHECK(ITrainingStatisticsManager::computeMastery(ts) == MasteryLevel::Mastered);
    }
}
