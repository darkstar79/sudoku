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

#pragma once

#include "i_training_statistics_manager.h"
#include "solving_technique.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace sudoku::core {

/// Prerequisite relationship: technique requires prerequisite to reach min_level
struct TechniquePrerequisite {
    SolvingTechnique prerequisite;
    MasteryLevel min_level{MasteryLevel::Intermediate};
};

/// Returns the prerequisites for a given technique.
/// Prerequisites are based on natural skill progressions:
///   Naked Pair → Naked Triple → Naked Quad
///   Hidden Pair → Hidden Triple → Hidden Quad
///   X-Wing → Swordfish → Jellyfish (+ finned variants)
///   Simple Coloring → Multi-Coloring → 3D Medusa
///   XY-Wing → XYZ-Wing → WXYZ-Wing → VWXYZ-Wing
///   ALS-XZ → ALS-XY-Wing, Death Blossom, Sue de Coq
[[nodiscard]] inline std::vector<TechniquePrerequisite> getPrerequisites(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        // Subset chains
        case NakedTriple:
            return {{.prerequisite = NakedPair}};
        case NakedQuad:
            return {{.prerequisite = NakedTriple}};
        case HiddenTriple:
            return {{.prerequisite = HiddenPair}};
        case HiddenQuad:
            return {{.prerequisite = HiddenTriple}};

        // Intersections build on singles
        case PointingPair:
        case BoxLineReduction:
            return {{.prerequisite = HiddenSingle}};

        // Fish chain
        case Swordfish:
            return {{.prerequisite = XWing}};
        case Jellyfish:
            return {{.prerequisite = Swordfish}};
        case FinnedXWing:
            return {{.prerequisite = XWing}};
        case FinnedSwordfish:
            return {{.prerequisite = Swordfish}, {.prerequisite = FinnedXWing}};
        case FinnedJellyfish:
            return {{.prerequisite = Jellyfish}, {.prerequisite = FinnedSwordfish}};
        case FrankenFish:
            return {{.prerequisite = XWing}};
        case MutantFish:
            return {{.prerequisite = FrankenFish}};
        case SashimiXWing:
            return {{.prerequisite = FinnedXWing}};
        case SashimiSwordfish:
            return {{.prerequisite = FinnedSwordfish}, {.prerequisite = SashimiXWing}};
        case SashimiJellyfish:
            return {{.prerequisite = FinnedJellyfish}, {.prerequisite = SashimiSwordfish}};

        // Wing chain
        case XYZWing:
            return {{.prerequisite = XYWing}};
        case WXYZWing:
            return {{.prerequisite = XYZWing}};
        case VWXYZWing:
            return {{.prerequisite = WXYZWing}};
        case WWing:
            return {{.prerequisite = XYWing}};

        // Coloring chain
        case MultiColoring:
            return {{.prerequisite = SimpleColoring}};
        case ThreeDMedusa:
            return {{.prerequisite = MultiColoring}};

        // Chain techniques
        case XYChain:
            return {{.prerequisite = XYWing}};
        case XCycles:
            return {{.prerequisite = SimpleColoring}};
        case GroupedXCycles:
            return {{.prerequisite = XCycles}};
        case ForcingChain:
            return {{.prerequisite = XYChain}};
        case UnitForcingChain:
            return {{.prerequisite = ForcingChain}};
        case RegionForcingChain:
            return {{.prerequisite = UnitForcingChain}};
        case NiceLoop:
            return {{.prerequisite = ForcingChain}};
        case ContinuousNiceLoop:
            return {{.prerequisite = NiceLoop}};
        case GroupedNiceLoop:
            return {{.prerequisite = NiceLoop}, {.prerequisite = GroupedXCycles}};

        // Kraken Fish requires finned fish knowledge + chain propagation
        case KrakenFish:
            return {{.prerequisite = FinnedJellyfish}, {.prerequisite = ForcingChain}};

        // Junior Exocet requires forcing chain-level reasoning
        case JuniorExocet:
            return {{.prerequisite = ForcingChain}};

        // Unique rectangles
        case HiddenUniqueRectangle:
        case AvoidableRectangle:
        case UniqueLoop:
            return {{.prerequisite = UniqueRectangle}};

        // ALS family
        case ALSXYWing:
        case DeathBlossom:
        case SueDeCoq:
            return {{.prerequisite = ALSxZ}};
        case ALSChain:
            return {{.prerequisite = ALSXYWing}};

        // Remote Pairs builds on naked pairs
        case RemotePairs:
            return {{.prerequisite = NakedPair}};

        default:
            return {};
    }
}

/// All logical techniques in difficulty order (excludes Backtracking)
inline constexpr std::array kAllTechniques = {
    SolvingTechnique::NakedSingle,
    SolvingTechnique::HiddenSingle,
    SolvingTechnique::NakedPair,
    SolvingTechnique::NakedTriple,
    SolvingTechnique::HiddenPair,
    SolvingTechnique::HiddenTriple,
    SolvingTechnique::PointingPair,
    SolvingTechnique::BoxLineReduction,
    SolvingTechnique::NakedQuad,
    SolvingTechnique::HiddenQuad,
    SolvingTechnique::XWing,
    SolvingTechnique::XYWing,
    SolvingTechnique::Swordfish,
    SolvingTechnique::Skyscraper,
    SolvingTechnique::TwoStringKite,
    SolvingTechnique::XYZWing,
    SolvingTechnique::UniqueRectangle,
    SolvingTechnique::UniqueLoop,
    SolvingTechnique::WWing,
    SolvingTechnique::SimpleColoring,
    SolvingTechnique::FinnedXWing,
    SolvingTechnique::SashimiXWing,
    SolvingTechnique::RemotePairs,
    SolvingTechnique::BUG,
    SolvingTechnique::XCycles,
    SolvingTechnique::HiddenUniqueRectangle,
    SolvingTechnique::AvoidableRectangle,
    SolvingTechnique::Jellyfish,
    SolvingTechnique::FinnedSwordfish,
    SolvingTechnique::SashimiSwordfish,
    SolvingTechnique::EmptyRectangle,
    SolvingTechnique::WXYZWing,
    SolvingTechnique::MultiColoring,
    SolvingTechnique::ThreeDMedusa,
    SolvingTechnique::FinnedJellyfish,
    SolvingTechnique::SashimiJellyfish,
    SolvingTechnique::XYChain,
    SolvingTechnique::VWXYZWing,
    SolvingTechnique::FrankenFish,
    SolvingTechnique::MutantFish,
    SolvingTechnique::GroupedXCycles,
    SolvingTechnique::ALSxZ,
    SolvingTechnique::SueDeCoq,
    SolvingTechnique::ALSXYWing,
    SolvingTechnique::DeathBlossom,
    SolvingTechnique::ALSChain,
    SolvingTechnique::ForcingChain,
    SolvingTechnique::UnitForcingChain,
    SolvingTechnique::RegionForcingChain,
    SolvingTechnique::KrakenFish,
    SolvingTechnique::JuniorExocet,
    SolvingTechnique::NiceLoop,
    SolvingTechnique::ContinuousNiceLoop,
    SolvingTechnique::GroupedNiceLoop,
};

/// Check whether all prerequisites for a technique are met at the required mastery level
[[nodiscard]] inline bool arePrerequisitesMet(SolvingTechnique technique, const ITrainingStatisticsManager& stats_mgr) {
    auto prereqs = getPrerequisites(technique);
    return std::ranges::all_of(prereqs, [&stats_mgr](const TechniquePrerequisite& p) {
        return stats_mgr.getMastery(p.prerequisite) >= p.min_level;
    });
}

/// Recommend the next technique to practice.
///
/// Algorithm:
/// 1. Find the lowest-difficulty technique not yet Mastered
/// 2. Among techniques at the same difficulty, prefer the least recently practiced
/// 3. Skip techniques whose prerequisites are not met
/// 4. Returns nullopt if all techniques are Mastered
[[nodiscard]] inline std::optional<SolvingTechnique>
getRecommendedTechnique(const ITrainingStatisticsManager& stats_mgr) {
    // Sort techniques by SE difficulty rating, then by enum order for stability
    struct TechCandidate {
        SolvingTechnique technique;
        double rating;
        std::chrono::system_clock::time_point last_practiced;
    };

    std::vector<TechCandidate> candidates;
    candidates.reserve(kAllTechniques.size());

    for (auto tech : kAllTechniques) {
        if (stats_mgr.getMastery(tech) == MasteryLevel::Mastered) {
            continue;
        }
        if (!arePrerequisitesMet(tech, stats_mgr)) {
            continue;
        }
        auto stats = stats_mgr.getStats(tech);
        candidates.push_back({tech, getTechniqueRating(tech), stats.last_practiced});
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    // Sort by: lowest difficulty first, then least recently practiced
    std::ranges::sort(candidates, [](const TechCandidate& a, const TechCandidate& b) {
        if (a.rating != b.rating) {
            return a.rating < b.rating;
        }
        return a.last_practiced < b.last_practiced;
    });

    return candidates.front().technique;
}

}  // namespace sudoku::core
