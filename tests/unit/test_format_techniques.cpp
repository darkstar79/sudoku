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

#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/solving_technique.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"

#include <filesystem>
#include <memory>
#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

/// Helper to create a minimal GameViewModel for formatTechniques testing.
/// The test runner (tests/helpers/qt_test_main.cpp) instantiates a
/// QCoreApplication with no QTranslator installed, so
/// QCoreApplication::translate returns the source-language English strings.
[[nodiscard]] GameViewModel createTestViewModel() {
    auto validator = std::make_shared<GameValidator>();
    return GameViewModel(validator, std::make_shared<PuzzleGenerator>(), std::make_shared<SudokuSolver>(validator),
                         std::make_shared<StatisticsManager>(), std::make_shared<SaveManager>());
}

}  // namespace

TEST_CASE("GameViewModel::formatTechniques - formats technique names for display", "[game_view_model][techniques]") {
    auto view_model = createTestViewModel();

    SECTION("Empty technique set with no backtracking returns empty vector") {
        std::set<SolvingTechnique> techniques;
        auto result = view_model.formatTechniques(techniques, false);
        REQUIRE(result.empty());
    }

    SECTION("Single technique returns formatted name with points") {
        std::set<SolvingTechnique> techniques{SolvingTechnique::NakedSingle};
        auto result = view_model.formatTechniques(techniques, false);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "Naked Single (SE 2.3)");
    }

    SECTION("Multiple techniques are sorted by SE rating ascending") {
        std::set<SolvingTechnique> techniques{
            SolvingTechnique::XWing,        // SE 3.2
            SolvingTechnique::NakedSingle,  // SE 2.3
            SolvingTechnique::HiddenPair,   // SE 3.4
        };
        auto result = view_model.formatTechniques(techniques, false);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "Naked Single (SE 2.3)");
        REQUIRE(result[1] == "X-Wing (SE 3.2)");
        REQUIRE(result[2] == "Hidden Pair (SE 3.4)");
    }

    SECTION("Backtracking only (pure backtracking puzzle)") {
        std::set<SolvingTechnique> techniques;
        auto result = view_model.formatTechniques(techniques, true);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "Backtracking (trial & error)");
    }

    SECTION("Mixed puzzle: techniques plus backtracking") {
        std::set<SolvingTechnique> techniques{
            SolvingTechnique::NakedSingle,   // SE 2.3
            SolvingTechnique::HiddenSingle,  // SE 1.5
        };
        auto result = view_model.formatTechniques(techniques, true);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "Hidden Single (SE 1.5)");  // HiddenSingle < NakedSingle in SE
        REQUIRE(result[1] == "Naked Single (SE 2.3)");
        REQUIRE(result[2] == "Backtracking (trial & error)");
    }

    SECTION("All logical techniques are formatted correctly") {
        std::set<SolvingTechnique> techniques{
            SolvingTechnique::NakedSingle,  SolvingTechnique::HiddenSingle,
            SolvingTechnique::NakedPair,    SolvingTechnique::NakedTriple,
            SolvingTechnique::HiddenPair,   SolvingTechnique::HiddenTriple,
            SolvingTechnique::PointingPair, SolvingTechnique::BoxLineReduction,
            SolvingTechnique::NakedQuad,    SolvingTechnique::HiddenQuad,
            SolvingTechnique::XWing,        SolvingTechnique::XYWing,
        };
        auto result = view_model.formatTechniques(techniques, false);
        REQUIRE(result.size() == 12);
        REQUIRE(result[0] == "Hidden Single (SE 1.5)");  // Easiest in SE scale
        REQUIRE(result[11] == "Hidden Quad (SE 5.4)");   // Hardest in this set
    }

    SECTION("Techniques sorted by SE rating") {
        std::set<SolvingTechnique> techniques{
            SolvingTechnique::PointingPair,      // SE 2.6
            SolvingTechnique::BoxLineReduction,  // SE 2.8
        };
        auto result = view_model.formatTechniques(techniques, false);
        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == "Pointing Pair (SE 2.6)");
        REQUIRE(result[1] == "Box/Line Reduction (SE 2.8)");
    }
}
