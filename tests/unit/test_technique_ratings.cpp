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

// Golden characterization pin for getTechniqueRating() — Story 0b.1.
//
// Pins the *current* Sudoku Explainer (SE) rating of every one of the 54 logical
// strategy techniques (NakedSingle(0) .. GroupedNiceLoop(53)). The persisted
// difficulty/rating of saved puzzles is derived from these values (UK2 — save-pinned
// values), so any change to getTechniqueRating() must be an explicit, reviewable
// line-item diff. This pin is the guard that 0b.3's deliberate corrections will update.
//
// The expected table below is an EXPLICIT literal copied from the switch at
// src/core/solving_technique.h — it is NOT derived by calling getTechniqueRating(),
// which would make the pin vacuously self-consistent. Ground truth is the switch;
// trust the switch over this copy if they ever disagree.
//
// This file is the *complete* save-compat pin and coexists with the partial
// spot-checks in test_solving_technique.cpp (distinct [solver][ratings] tag/intent).

#include "../../src/core/solve_step.h"  // RatingContext / RegionType — Story 0b.4a
#include "../../src/core/solving_technique.h"
#include "../../src/core/training_learning_path.h"

#include <array>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// Number of logical strategy techniques pinned here: NakedSingle(0) .. FullHouse(54).
// Backtracking (255) and the getTechniqueRating() 0.0 fallback are non-strategies and are
// deliberately excluded — matching the cardinality convention used across the solver suite.
constexpr int kPinnedStrategyCount = 55;

// Explicit expected ratings in enum order (0..53), copied verbatim from the switch at
// src/core/solving_technique.h:228-318. Watch the fall-through groups in the switch — several
// distinct techniques share one return value (e.g. FinnedXWing/SashimiXWing/HiddenPair all 3.4).
constexpr std::array<std::pair<SolvingTechnique, double>, kPinnedStrategyCount> kExpectedRatings = {{
    {SolvingTechnique::NakedSingle, 2.3},
    // HiddenSingle's flat value is the base/default (= SE line rating). Story 0b.4a widens the
    // *context* overload so a block hidden single rates 1.2 (asserted in the Class-A pin below);
    // the flat overload stays 1.5 as the single source for the line value.
    {SolvingTechnique::HiddenSingle, 1.5},
    {SolvingTechnique::NakedPair, 3.0},
    {SolvingTechnique::NakedTriple, 3.6},
    {SolvingTechnique::HiddenPair, 3.4},
    {SolvingTechnique::HiddenTriple, 4.0},
    {SolvingTechnique::PointingPair, 2.6},
    {SolvingTechnique::BoxLineReduction, 2.8},
    {SolvingTechnique::NakedQuad, 5.0},
    {SolvingTechnique::HiddenQuad, 5.4},
    {SolvingTechnique::XWing, 3.2},
    {SolvingTechnique::XYWing, 4.2},
    {SolvingTechnique::Swordfish, 3.8},
    {SolvingTechnique::Skyscraper, 4.0},
    {SolvingTechnique::TwoStringKite, 4.1},
    {SolvingTechnique::XYZWing, 4.4},
    {SolvingTechnique::UniqueRectangle, 4.5},
    {SolvingTechnique::WWing, 4.4},
    {SolvingTechnique::SimpleColoring, 4.0},
    {SolvingTechnique::FinnedXWing, 3.4},
    {SolvingTechnique::RemotePairs, 4.5},
    {SolvingTechnique::BUG, 5.6},
    {SolvingTechnique::Jellyfish, 5.2},
    {SolvingTechnique::FinnedSwordfish, 4.0},
    {SolvingTechnique::EmptyRectangle, 4.3},
    {SolvingTechnique::WXYZWing, 4.6},
    {SolvingTechnique::FinnedJellyfish, 5.4},
    {SolvingTechnique::XYChain, 6.6},
    {SolvingTechnique::MultiColoring, 4.2},
    {SolvingTechnique::ALSxZ, 6.8},
    {SolvingTechnique::SueDeCoq, 6.6},
    {SolvingTechnique::ForcingChain, 7.5},
    {SolvingTechnique::NiceLoop, 7.5},
    {SolvingTechnique::XCycles, 6.6},
    {SolvingTechnique::ThreeDMedusa, 4.4},
    {SolvingTechnique::HiddenUniqueRectangle, 4.8},
    {SolvingTechnique::AvoidableRectangle, 4.5},
    {SolvingTechnique::ALSXYWing, 7.8},
    {SolvingTechnique::DeathBlossom, 8.2},
    {SolvingTechnique::VWXYZWing, 4.8},
    {SolvingTechnique::FrankenFish, 4.2},
    {SolvingTechnique::GroupedXCycles, 6.8},
    {SolvingTechnique::SashimiXWing, 3.4},
    {SolvingTechnique::SashimiSwordfish, 4.2},
    {SolvingTechnique::SashimiJellyfish, 5.6},
    {SolvingTechnique::UnitForcingChain, 7.8},
    {SolvingTechnique::RegionForcingChain, 8.0},
    {SolvingTechnique::MutantFish, 5.4},
    {SolvingTechnique::KrakenFish, 8.5},
    {SolvingTechnique::ALSChain, 8.5},
    {SolvingTechnique::JuniorExocet, 9.4},
    {SolvingTechnique::UniqueLoop, 4.5},
    {SolvingTechnique::ContinuousNiceLoop, 7.0},
    {SolvingTechnique::GroupedNiceLoop, 8.0},
    // Story 0b.4b — Full House (SE "Last value in block/row/col"), the easiest technique on the scale.
    {SolvingTechnique::FullHouse, 1.0},
}};

}  // namespace

// Catch2 TEST_CASE: the SECTIONs + the 54-row pin loop expand to nested conditionals, so the
// generated function trips the cognitive-complexity threshold; complexity is inherent to the pin.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("getTechniqueRating() golden pin — all 55 strategy ratings", "[solver][ratings]") {
    SECTION("Each strategy technique returns its pinned current rating") {
        for (const auto& [technique, expected] : kExpectedRatings) {
            // Name the offending technique if a future drift makes this go RED.
            CAPTURE(getTechniqueName(technique), static_cast<int>(technique), expected);
            // Ratings are exact double literals (multiples of 0.1) used identically on both
            // sides — direct == is intentional. No epsilon: it would weaken the pin against a
            // deliberate 0.1-step correction (the very change 0b.3 will make).
            REQUIRE(getTechniqueRating(technique) == expected);
        }
    }

    SECTION("Cardinality guard — exactly 55 strategies are pinned") {
        // Adding a new SolvingTechnique strategy without pinning its rating must fail here.
        REQUIRE(kExpectedRatings.size() == kPinnedStrategyCount);

        // Cross-witnesses that 54 is the live strategy count (independent of this table).
        REQUIRE(static_cast<int>(kLastLogicalTechnique) == kPinnedStrategyCount - 1);
        REQUIRE(kAllTechniques.size() == kPinnedStrategyCount);
    }

    SECTION("constexpr-evaluability is preserved (spot check)") {
        // getTechniqueRating() is constexpr; pin that property so it cannot silently regress.
        STATIC_REQUIRE(getTechniqueRating(SolvingTechnique::NakedSingle) == 2.3);
    }

    SECTION("Backtracking is documented but excluded from the 54-row pin") {
        // Backtracking is the brute-force fallback, not a logical strategy — outside the count.
        REQUIRE(getTechniqueRating(SolvingTechnique::Backtracking) == 12.0);
    }
}

// Class-A re-base (Story 0b.4a): the rating surface is now a function of (technique, RatingContext).
// Hidden Single splits by resolving region — block 1.2 (SE "easiest region wins") vs line 1.5. This
// is the canonical RED→GREEN of the split: the context overload does not exist until 0b.4a lands, so
// this whole case is RED on the first build and GREEN once the overload is implemented. The flat
// 54-row pin above is the *base/default* surface and is unchanged (HS line value 1.5 lives there).
// The SECTIONs + the base-equivalence loop expand to nested conditionals, so the generated function
// trips the cognitive-complexity threshold; complexity is inherent to the pin (same as the golden pin
// above). The directive MUST be the line immediately above TEST_CASE to bind to it.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("getTechniqueRating() Class-A context split — Hidden Single block 1.2 / line 1.5", "[solver][ratings]") {
    using enum RegionType;

    SECTION("Block hidden single rates 1.2, line (row/col) rates 1.5") {
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle, RatingContext{.region = Box}) == 1.2);
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle, RatingContext{.region = Row}) == 1.5);
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle, RatingContext{.region = Col}) == 1.5);
    }

    SECTION("Two contexts of the same technique yield two distinct ratings (AC1)") {
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle, RatingContext{.region = Box}) !=
                getTechniqueRating(SolvingTechnique::HiddenSingle, RatingContext{.region = Row}));
    }

    SECTION("Default context reproduces the flat base — single source of truth, not a second table") {
        // A null context (region None) must equal the legacy flat overload for every technique, so the
        // numbers never silently diverge between the two surfaces.
        for (const auto& [technique, expected] : kExpectedRatings) {
            CAPTURE(getTechniqueName(technique));
            REQUIRE(getTechniqueRating(technique, RatingContext{}) == getTechniqueRating(technique));
        }
    }

    SECTION("Context overload is constexpr (spot check)") {
        STATIC_REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle, RatingContext{.region = Box}) == 1.2);
    }
}

// Class-B re-base (Story 0b.4b): the "Direct" forms rate lower than the elimination-only form when the
// step's own eliminations force a placement (forces_placement). SE v1.2.1: Direct Pointing 1.7 (vs
// 2.6), Direct Claiming 1.9 (vs 2.8), Direct Hidden Pair 2.0 (vs 3.4), Direct Hidden Triplet 2.5 (vs
// 4.0). This pins the rating mapping; the strategy/helper tests prove the flag is reachable on real
// boards (test_full_house_strategy.cpp / test_strategy_base_direct.cpp). The forced/unforced pairing
// here is the canonical "the flag distinguishes" guard.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("getTechniqueRating() Class-B Direct forms — forces_placement lowers the rating", "[solver][ratings]") {
    struct Case {
        SolvingTechnique technique;
        double base;
        double direct;
    };
    const std::array<Case, 4> cases = {{
        {.technique = SolvingTechnique::PointingPair, .base = 2.6, .direct = 1.7},      // Direct Pointing
        {.technique = SolvingTechnique::BoxLineReduction, .base = 2.8, .direct = 1.9},  // Direct Claiming
        {.technique = SolvingTechnique::HiddenPair, .base = 3.4, .direct = 2.0},        // Direct Hidden Pair
        {.technique = SolvingTechnique::HiddenTriple, .base = 4.0, .direct = 2.5},      // Direct Hidden Triplet
    }};

    SECTION("Forced (forces_placement) → Direct rating; unforced → base rating") {
        for (const auto& c : cases) {
            CAPTURE(getTechniqueName(c.technique));
            REQUIRE(getTechniqueRating(c.technique, RatingContext{.forces_placement = true}) == c.direct);
            REQUIRE(getTechniqueRating(c.technique, RatingContext{.forces_placement = false}) == c.base);
            // The Direct value must actually differ from the base (the flag is not a no-op).
            REQUIRE(c.direct != c.base);
        }
    }

    SECTION("forces_placement does not perturb techniques outside the Direct family") {
        // A non-Direct technique ignores the flag (single source of truth — no silent drift).
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedPair, RatingContext{.forces_placement = true}) ==
                getTechniqueRating(SolvingTechnique::NakedPair));
        REQUIRE(getTechniqueRating(SolvingTechnique::FullHouse, RatingContext{.forces_placement = true}) == 1.0);
    }

    SECTION("Context overload is constexpr for Direct forms (spot check)") {
        STATIC_REQUIRE(getTechniqueRating(SolvingTechnique::PointingPair, RatingContext{.forces_placement = true}) ==
                       1.7);
    }
}
