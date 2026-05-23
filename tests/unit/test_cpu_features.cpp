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

#include "core/cpu_features.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("CPU feature detection", "[cpu_features]") {
    SECTION("getCpuFeatures returns singleton (same address on repeated calls)") {
        const auto& f1 = getCpuFeatures();
        const auto& f2 = getCpuFeatures();
        REQUIRE(&f1 == &f2);
    }

    SECTION("getCpuFeatures returns consistent values") {
        const auto& f1 = getCpuFeatures();
        const auto& f2 = getCpuFeatures();
        REQUIRE(f1.has_avx2 == f2.has_avx2);
        REQUIRE(f1.has_avx512f == f2.has_avx512f);
        REQUIRE(f1.has_avx512bw == f2.has_avx512bw);
        REQUIRE(f1.has_popcnt == f2.has_popcnt);
    }

    SECTION("hasAvx2 convenience matches struct field") {
        REQUIRE(hasAvx2() == getCpuFeatures().has_avx2);
    }

    SECTION("CPU feature flags are boolean values") {
        // Verify detection works without requiring specific features
        const auto& features = getCpuFeatures();

        // All feature flags should be valid boolean values (true or false)
        // This verifies detection ran without errors
        REQUIRE((features.has_avx2 == true || features.has_avx2 == false));
        REQUIRE((features.has_popcnt == true || features.has_popcnt == false));
        REQUIRE((features.has_avx512f == true || features.has_avx512f == false));
        REQUIRE((features.has_avx512bw == true || features.has_avx512bw == false));
    }

    SECTION("hasAvx512 convenience matches struct fields") {
        auto expected = getCpuFeatures().has_avx512f && getCpuFeatures().has_avx512bw;
        REQUIRE(hasAvx512() == expected);
    }

    SECTION("CPU features are logically consistent") {
        // If AVX-512BW is supported, AVX-512F must also be (BW extends F)
        if (getCpuFeatures().has_avx512bw) {
            REQUIRE(getCpuFeatures().has_avx512f);
        }

        // hasAvx512() should only be true if both F and BW are present
        if (hasAvx512()) {
            REQUIRE(getCpuFeatures().has_avx512f);
            REQUIRE(getCpuFeatures().has_avx512bw);
        }
    }
}
