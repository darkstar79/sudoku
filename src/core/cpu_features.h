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

#pragma once

#include <cstdint>

namespace sudoku::core {

/**
 * @brief CPU feature flags detected at runtime via CPUID.
 *
 * Used for runtime SIMD dispatch: a single binary auto-selects
 * Scalar/AVX2/AVX512 code paths based on the running CPU.
 */
struct CpuFeatures {
    bool has_avx2{false};      ///< AVX2 (256-bit SIMD)
    bool has_avx512f{false};   ///< AVX-512 Foundation
    bool has_avx512bw{false};  ///< AVX-512 Byte/Word
    bool has_popcnt{false};    ///< Hardware POPCNT instruction
};

/**
 * @brief Get cached CPU features (Meyer's singleton, first-call CPUID detection).
 *
 * Safe to call from any point in program execution (no static init order issues).
 * The detection runs exactly once; subsequent calls return a cached reference.
 *
 * @return Immutable reference to detected CPU features
 */
[[nodiscard]] const CpuFeatures& getCpuFeatures();

/**
 * @brief Convenience: is AVX2 available at runtime?
 * @return true if CPU supports AVX2 and OS enables AVX state saving
 */
[[nodiscard]] inline bool hasAvx2() {
    return getCpuFeatures().has_avx2;
}

/**
 * @brief Convenience: is AVX-512 available at runtime?
 * @return true if CPU supports AVX-512F + AVX-512BW and OS enables AVX-512 state saving
 */
[[nodiscard]] inline bool hasAvx512() {
    const auto& features = getCpuFeatures();
    return features.has_avx512f && features.has_avx512bw;
}

/// Solver SIMD path selection for benchmarking and forced dispatch.
enum class SolverPath : std::uint8_t {
    Auto = 0,    ///< Runtime detection (default production behavior)
    Scalar = 1,  ///< Force scalar ConstraintState path
    AVX2 = 2,    ///< Force AVX2 SIMDConstraintState path
    AVX512 = 3   ///< Force AVX-512 SIMDConstraintState path
};

/// Check if a solver path is available on this CPU.
[[nodiscard]] inline bool isSolverPathAvailable(SolverPath path) {
    switch (path) {
        case SolverPath::Auto:
        case SolverPath::Scalar:
            return true;
        case SolverPath::AVX2:
            return hasAvx2();
        case SolverPath::AVX512:
            return hasAvx512();
    }
    return false;
}

/// Human-readable name for a solver path.
[[nodiscard]] inline const char* solverPathName(SolverPath path) {
    switch (path) {
        case SolverPath::Auto:
            return "Auto";
        case SolverPath::Scalar:
            return "Scalar";
        case SolverPath::AVX2:
            return "AVX2";
        case SolverPath::AVX512:
            return "AVX-512";
    }
    return "Unknown";
}

}  // namespace sudoku::core
