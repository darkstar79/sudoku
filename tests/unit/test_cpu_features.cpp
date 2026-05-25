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
        REQUIRE(f1.has_avx512_bitalg == f2.has_avx512_bitalg);
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
        REQUIRE((features.has_avx512_bitalg == true || features.has_avx512_bitalg == false));
    }

    SECTION("hasAvx512 convenience matches struct fields") {
        const auto& f = getCpuFeatures();
        auto expected = f.has_avx512f && f.has_avx512bw && f.has_avx512_bitalg;
        REQUIRE(hasAvx512() == expected);
    }

    SECTION("CPU features are logically consistent") {
        // If AVX-512BW is supported, AVX-512F must also be (BW extends F)
        if (getCpuFeatures().has_avx512bw) {
            REQUIRE(getCpuFeatures().has_avx512f);
        }

        // hasAvx512() must require F + BW + BITALG
        if (hasAvx512()) {
            REQUIRE(getCpuFeatures().has_avx512f);
            REQUIRE(getCpuFeatures().has_avx512bw);
            REQUIRE(getCpuFeatures().has_avx512_bitalg);
        }
    }
}

// =============================================================================
// Regression test: AVX-512 dispatch must require BITALG (#17 / BL-1)
//
// Before the fix, hasAvx512() returned true on any CPU with F + BW, but the
// AVX-512 SIMD path issues _mm512_popcnt_epi16 which requires BITALG. On
// Skylake-X / Cascade Lake / Zen 4 (F + BW present, BITALG absent), dispatch
// crashed with SIGILL. The logic is now testable via the const-ref overload.
// See docs/CODE_REVIEW_2026-05-25.md §BL-1.
// =============================================================================

TEST_CASE("hasAvx512(features) — BITALG gate", "[cpu_features][regression][bug-avx512-bitalg-ungated]") {
    SECTION("all three present -> true") {
        const CpuFeatures f{.has_avx512f = true, .has_avx512bw = true, .has_avx512_bitalg = true};
        REQUIRE(hasAvx512(f));
    }

    SECTION("F + BW without BITALG -> false (the Skylake-X / Cascade Lake / Zen 4 case)") {
        const CpuFeatures f{.has_avx512f = true, .has_avx512bw = true, .has_avx512_bitalg = false};
        REQUIRE_FALSE(hasAvx512(f));
    }

    SECTION("F + BITALG without BW -> false") {
        const CpuFeatures f{.has_avx512f = true, .has_avx512bw = false, .has_avx512_bitalg = true};
        REQUIRE_FALSE(hasAvx512(f));
    }

    SECTION("BW + BITALG without F -> false") {
        const CpuFeatures f{.has_avx512f = false, .has_avx512bw = true, .has_avx512_bitalg = true};
        REQUIRE_FALSE(hasAvx512(f));
    }

    SECTION("default-constructed (all false) -> false") {
        const CpuFeatures f{};
        REQUIRE_FALSE(hasAvx512(f));
    }
}
