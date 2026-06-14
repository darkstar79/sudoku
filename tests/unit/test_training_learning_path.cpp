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
#include "../../src/core/training_statistics_manager.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

void masterTechnique(TrainingStatisticsManager& mgr, SolvingTechnique technique) {
    TrainingLessonResult result;
    result.technique = technique;
    result.correct_count = 5;
    result.total_exercises = 5;
    result.hints_used = 0;
    REQUIRE(mgr.recordLesson(result).has_value());
    REQUIRE(mgr.recordLesson(result).has_value());
    REQUIRE(mgr.getMastery(technique) == MasteryLevel::Mastered);
}

void reachIntermediate(TrainingStatisticsManager& mgr, SolvingTechnique technique) {
    TrainingLessonResult result;
    result.technique = technique;
    result.correct_count = 3;
    result.total_exercises = 5;
    result.hints_used = 10;
    REQUIRE(mgr.recordLesson(result).has_value());
    REQUIRE(mgr.getMastery(technique) == MasteryLevel::Intermediate);
}

}  // namespace

TEST_CASE("getPrerequisites — techniques with no prerequisites", "[LearningPath]") {
    CHECK(getPrerequisites(SolvingTechnique::NakedSingle).empty());
    CHECK(getPrerequisites(SolvingTechnique::HiddenSingle).empty());
    CHECK(getPrerequisites(SolvingTechnique::NakedPair).empty());
    CHECK(getPrerequisites(SolvingTechnique::XWing).empty());
    CHECK(getPrerequisites(SolvingTechnique::XYWing).empty());
    CHECK(getPrerequisites(SolvingTechnique::SimpleColoring).empty());
    CHECK(getPrerequisites(SolvingTechnique::UniqueRectangle).empty());
    CHECK(getPrerequisites(SolvingTechnique::ALSxZ).empty());
}

TEST_CASE("getPrerequisites — subset chains", "[LearningPath]") {
    auto np_prereqs = getPrerequisites(SolvingTechnique::NakedTriple);
    REQUIRE(np_prereqs.size() == 1);
    CHECK(np_prereqs[0].prerequisite == SolvingTechnique::NakedPair);

    auto nq_prereqs = getPrerequisites(SolvingTechnique::NakedQuad);
    REQUIRE(nq_prereqs.size() == 1);
    CHECK(nq_prereqs[0].prerequisite == SolvingTechnique::NakedTriple);

    auto ht_prereqs = getPrerequisites(SolvingTechnique::HiddenTriple);
    REQUIRE(ht_prereqs.size() == 1);
    CHECK(ht_prereqs[0].prerequisite == SolvingTechnique::HiddenPair);
}

TEST_CASE("getPrerequisites — fish chain", "[LearningPath]") {
    auto sf_prereqs = getPrerequisites(SolvingTechnique::Swordfish);
    REQUIRE(sf_prereqs.size() == 1);
    CHECK(sf_prereqs[0].prerequisite == SolvingTechnique::XWing);

    auto jf_prereqs = getPrerequisites(SolvingTechnique::Jellyfish);
    REQUIRE(jf_prereqs.size() == 1);
    CHECK(jf_prereqs[0].prerequisite == SolvingTechnique::Swordfish);

    auto fsf_prereqs = getPrerequisites(SolvingTechnique::FinnedSwordfish);
    REQUIRE(fsf_prereqs.size() == 2);
}

TEST_CASE("getPrerequisites — coloring chain", "[LearningPath]") {
    auto mc_prereqs = getPrerequisites(SolvingTechnique::MultiColoring);
    REQUIRE(mc_prereqs.size() == 1);
    CHECK(mc_prereqs[0].prerequisite == SolvingTechnique::SimpleColoring);

    auto med_prereqs = getPrerequisites(SolvingTechnique::ThreeDMedusa);
    REQUIRE(med_prereqs.size() == 1);
    CHECK(med_prereqs[0].prerequisite == SolvingTechnique::MultiColoring);
}

TEST_CASE("arePrerequisitesMet — fresh stats", "[LearningPath]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    SECTION("techniques with no prerequisites are always available") {
        CHECK(arePrerequisitesMet(SolvingTechnique::NakedSingle, mgr));
        CHECK(arePrerequisitesMet(SolvingTechnique::NakedPair, mgr));
        CHECK(arePrerequisitesMet(SolvingTechnique::XWing, mgr));
    }

    SECTION("techniques with prerequisites are blocked when unpracticed") {
        CHECK_FALSE(arePrerequisitesMet(SolvingTechnique::NakedTriple, mgr));
        CHECK_FALSE(arePrerequisitesMet(SolvingTechnique::Swordfish, mgr));
        CHECK_FALSE(arePrerequisitesMet(SolvingTechnique::MultiColoring, mgr));
    }
}

TEST_CASE("arePrerequisitesMet — unlocks after reaching Intermediate", "[LearningPath]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    reachIntermediate(mgr, SolvingTechnique::NakedPair);
    CHECK(arePrerequisitesMet(SolvingTechnique::NakedTriple, mgr));

    // NakedQuad still blocked (needs NakedTriple)
    CHECK_FALSE(arePrerequisitesMet(SolvingTechnique::NakedQuad, mgr));

    reachIntermediate(mgr, SolvingTechnique::NakedTriple);
    CHECK(arePrerequisitesMet(SolvingTechnique::NakedQuad, mgr));
}

TEST_CASE("getRecommendedTechnique — fresh stats recommends FullHouse", "[LearningPath]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    auto recommended = getRecommendedTechnique(mgr);
    REQUIRE(recommended.has_value());
    // Full House (SE 1.0) is the easiest technique on the SE scale and the root of the path (story 0b.4b).
    // has_value() short-circuit guards the deref for clang-tidy bugprone-unchecked-optional-access.
    CHECK((recommended.has_value() && *recommended == SolvingTechnique::FullHouse));
}

TEST_CASE("getRecommendedTechnique — advances after mastering", "[LearningPath]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    masterTechnique(mgr, SolvingTechnique::FullHouse);
    masterTechnique(mgr, SolvingTechnique::HiddenSingle);

    auto recommended = getRecommendedTechnique(mgr);
    REQUIRE(recommended.has_value());
    // After Full House (SE 1.0) and HiddenSingle (SE 1.5), NakedSingle (SE 2.3) is next
    CHECK(*recommended == SolvingTechnique::NakedSingle);
}

TEST_CASE("getRecommendedTechnique — skips techniques with unmet prerequisites", "[LearningPath]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    // Master the easy singles (Full House, HiddenSingle, NakedSingle)
    masterTechnique(mgr, SolvingTechnique::FullHouse);
    masterTechnique(mgr, SolvingTechnique::HiddenSingle);
    masterTechnique(mgr, SolvingTechnique::NakedSingle);

    // Next should be PointingPair (SE 2.6) — it requires HiddenSingle (mastered)
    // NakedPair (SE 3.0) is higher and comes later
    auto recommended = getRecommendedTechnique(mgr);
    REQUIRE(recommended.has_value());
    CHECK(*recommended == SolvingTechnique::PointingPair);
}

TEST_CASE("getRecommendedTechnique — prefers least recently practiced at same difficulty", "[LearningPath]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    // Practice both PointingPair and BoxLineReduction (both 100 pts, no prereqs blocked)
    // Master lower-difficulty techniques first
    masterTechnique(mgr, SolvingTechnique::FullHouse);
    masterTechnique(mgr, SolvingTechnique::NakedSingle);
    masterTechnique(mgr, SolvingTechnique::HiddenSingle);
    masterTechnique(mgr, SolvingTechnique::NakedPair);
    masterTechnique(mgr, SolvingTechnique::NakedTriple);
    masterTechnique(mgr, SolvingTechnique::HiddenPair);
    masterTechnique(mgr, SolvingTechnique::HiddenTriple);

    // PointingPair (SE 2.6) comes before BoxLineReduction (SE 2.8)
    // Since they have different SE ratings, PointingPair is recommended first
    auto recommended = getRecommendedTechnique(mgr);
    REQUIRE(recommended.has_value());
    CHECK(*recommended == SolvingTechnique::PointingPair);
}

TEST_CASE("getRecommendedTechnique — returns nullopt when all mastered", "[LearningPath]") {
    sudoku::test::TempTestDir tmp;
    TrainingStatisticsManager mgr(tmp.path());

    for (auto tech : kAllTechniques) {
        masterTechnique(mgr, tech);
    }

    auto recommended = getRecommendedTechnique(mgr);
    CHECK_FALSE(recommended.has_value());
}

TEST_CASE("kAllTechniques — contains all logical techniques", "[LearningPath]") {
    CHECK(kAllTechniques.size() == 55);  // story 0b.4b — FullHouse added as the root

    // Verify Backtracking is excluded
    for (auto tech : kAllTechniques) {
        CHECK(tech != SolvingTechnique::Backtracking);
    }
}
