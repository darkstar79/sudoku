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

#include "i_localization_manager.h"
#include "solving_technique.h"
#include "string_keys.h"

#include <string_view>

namespace sudoku::core {

/// Localized theory text for a solving technique
struct TechniqueDescription {
    std::string_view title;
    std::string_view what_it_is;
    std::string_view what_to_look_for;
};

/// Get the localized description for a solving technique
/// @param loc Localization manager for translating description text
/// @param technique The technique to describe
/// @return Description with localized title, explanation, and identification tips
// NOLINTNEXTLINE(readability-function-size) — dispatch table over all SolvingTechnique values; each case returns one constant struct; cannot meaningfully split
[[nodiscard]] inline TechniqueDescription getTechniqueDescription(const ILocalizationManager& loc,
                                                                  SolvingTechnique technique) {
    using enum SolvingTechnique;
    using namespace StringKeys;
    switch (technique) {
        case NakedSingle:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescNakedSingleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescNakedSingleWhatToLookFor)};
        case HiddenSingle:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescHiddenSingleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescHiddenSingleWhatToLookFor)};
        case NakedPair:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescNakedPairWhatItIs),
                    .what_to_look_for = loc.getString(TechDescNakedPairWhatToLookFor)};
        case NakedTriple:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescNakedTripleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescNakedTripleWhatToLookFor)};
        case HiddenPair:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescHiddenPairWhatItIs),
                    .what_to_look_for = loc.getString(TechDescHiddenPairWhatToLookFor)};
        case HiddenTriple:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescHiddenTripleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescHiddenTripleWhatToLookFor)};
        case PointingPair:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescPointingPairWhatItIs),
                    .what_to_look_for = loc.getString(TechDescPointingPairWhatToLookFor)};
        case BoxLineReduction:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescBoxLineReductionWhatItIs),
                    .what_to_look_for = loc.getString(TechDescBoxLineReductionWhatToLookFor)};
        case NakedQuad:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescNakedQuadWhatItIs),
                    .what_to_look_for = loc.getString(TechDescNakedQuadWhatToLookFor)};
        case HiddenQuad:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescHiddenQuadWhatItIs),
                    .what_to_look_for = loc.getString(TechDescHiddenQuadWhatToLookFor)};
        case XWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescXWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescXWingWhatToLookFor)};
        case XYWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescXYWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescXYWingWhatToLookFor)};
        case Swordfish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescSwordfishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescSwordfishWhatToLookFor)};
        case Skyscraper:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescSkyscraperWhatItIs),
                    .what_to_look_for = loc.getString(TechDescSkyscraperWhatToLookFor)};
        case TwoStringKite:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescTwoStringKiteWhatItIs),
                    .what_to_look_for = loc.getString(TechDescTwoStringKiteWhatToLookFor)};
        case XYZWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescXYZWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescXYZWingWhatToLookFor)};
        case UniqueRectangle:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescUniqueRectangleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescUniqueRectangleWhatToLookFor)};
        case WWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescWWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescWWingWhatToLookFor)};
        case SimpleColoring:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescSimpleColoringWhatItIs),
                    .what_to_look_for = loc.getString(TechDescSimpleColoringWhatToLookFor)};
        case FinnedXWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescFinnedXWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescFinnedXWingWhatToLookFor)};
        case RemotePairs:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescRemotePairsWhatItIs),
                    .what_to_look_for = loc.getString(TechDescRemotePairsWhatToLookFor)};
        case BUG:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescBUGWhatItIs),
                    .what_to_look_for = loc.getString(TechDescBUGWhatToLookFor)};
        case Jellyfish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescJellyfishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescJellyfishWhatToLookFor)};
        case FinnedSwordfish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescFinnedSwordfishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescFinnedSwordfishWhatToLookFor)};
        case EmptyRectangle:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescEmptyRectangleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescEmptyRectangleWhatToLookFor)};
        case WXYZWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescWXYZWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescWXYZWingWhatToLookFor)};
        case FinnedJellyfish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescFinnedJellyfishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescFinnedJellyfishWhatToLookFor)};
        case XYChain:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescXYChainWhatItIs),
                    .what_to_look_for = loc.getString(TechDescXYChainWhatToLookFor)};
        case MultiColoring:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescMultiColoringWhatItIs),
                    .what_to_look_for = loc.getString(TechDescMultiColoringWhatToLookFor)};
        case ALSxZ:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescALSxZWhatItIs),
                    .what_to_look_for = loc.getString(TechDescALSxZWhatToLookFor)};
        case SueDeCoq:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescSueDeCoqWhatItIs),
                    .what_to_look_for = loc.getString(TechDescSueDeCoqWhatToLookFor)};
        case ForcingChain:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescForcingChainWhatItIs),
                    .what_to_look_for = loc.getString(TechDescForcingChainWhatToLookFor)};
        case NiceLoop:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescNiceLoopWhatItIs),
                    .what_to_look_for = loc.getString(TechDescNiceLoopWhatToLookFor)};
        case XCycles:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescXCyclesWhatItIs),
                    .what_to_look_for = loc.getString(TechDescXCyclesWhatToLookFor)};
        case ThreeDMedusa:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescThreeDMedusaWhatItIs),
                    .what_to_look_for = loc.getString(TechDescThreeDMedusaWhatToLookFor)};
        case HiddenUniqueRectangle:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescHiddenUniqueRectangleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescHiddenUniqueRectangleWhatToLookFor)};
        case AvoidableRectangle:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescAvoidableRectangleWhatItIs),
                    .what_to_look_for = loc.getString(TechDescAvoidableRectangleWhatToLookFor)};
        case ALSXYWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescALSXYWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescALSXYWingWhatToLookFor)};
        case DeathBlossom:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescDeathBlossomWhatItIs),
                    .what_to_look_for = loc.getString(TechDescDeathBlossomWhatToLookFor)};
        case VWXYZWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescVWXYZWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescVWXYZWingWhatToLookFor)};
        case FrankenFish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescFrankenFishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescFrankenFishWhatToLookFor)};
        case GroupedXCycles:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescGroupedXCyclesWhatItIs),
                    .what_to_look_for = loc.getString(TechDescGroupedXCyclesWhatToLookFor)};
        case SashimiXWing:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescSashimiXWingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescSashimiXWingWhatToLookFor)};
        case SashimiSwordfish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescSashimiSwordfishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescSashimiSwordfishWhatToLookFor)};
        case SashimiJellyfish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescSashimiJellyfishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescSashimiJellyfishWhatToLookFor)};
        case UnitForcingChain:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescUnitForcingChainWhatItIs),
                    .what_to_look_for = loc.getString(TechDescUnitForcingChainWhatToLookFor)};
        case RegionForcingChain:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescRegionForcingChainWhatItIs),
                    .what_to_look_for = loc.getString(TechDescRegionForcingChainWhatToLookFor)};
        case MutantFish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescMutantFishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescMutantFishWhatToLookFor)};
        case KrakenFish:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescKrakenFishWhatItIs),
                    .what_to_look_for = loc.getString(TechDescKrakenFishWhatToLookFor)};
        case ALSChain:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescALSChainWhatItIs),
                    .what_to_look_for = loc.getString(TechDescALSChainWhatToLookFor)};
        case UniqueLoop:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescUniqueLoopWhatItIs),
                    .what_to_look_for = loc.getString(TechDescUniqueLoopWhatToLookFor)};
        case JuniorExocet:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescJuniorExocetWhatItIs),
                    .what_to_look_for = loc.getString(TechDescJuniorExocetWhatToLookFor)};
        case ContinuousNiceLoop:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescContinuousNiceLoopWhatItIs),
                    .what_to_look_for = loc.getString(TechDescContinuousNiceLoopWhatToLookFor)};
        case GroupedNiceLoop:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescGroupedNiceLoopWhatItIs),
                    .what_to_look_for = loc.getString(TechDescGroupedNiceLoopWhatToLookFor)};
        case Backtracking:
            return {.title = getLocalizedTechniqueName(loc, technique),
                    .what_it_is = loc.getString(TechDescBacktrackingWhatItIs),
                    .what_to_look_for = loc.getString(TechDescBacktrackingWhatToLookFor)};
    }
    return {.title = getLocalizedTechniqueName(loc, technique),
            .what_it_is = loc.getString(TechDescUnknownWhatItIs),
            .what_to_look_for = loc.getString(TechDescUnknownWhatToLookFor)};
}

}  // namespace sudoku::core
