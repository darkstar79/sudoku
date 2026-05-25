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

#ifdef _MSC_VER
#    include <intrin.h>
#else
#    include <cpuid.h>
#endif

#include <cstdint>

namespace sudoku::core {

namespace {

/// Read Extended Control Register (XCR0) via inline assembly.
/// This avoids requiring the -mxsave compiler flag.
/// XCR0 bits indicate which SIMD state the OS saves/restores on context switches.
[[nodiscard]] uint64_t readXcr0() {
#ifdef _MSC_VER
    return _xgetbv(0);
#else
    uint32_t eax = 0;
    uint32_t edx = 0;
    // XGETBV: ECX=0 selects XCR0
    __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return (static_cast<uint64_t>(edx) << 32) | eax;
#endif
}

/// Check if OS supports saving/restoring AVX state (XCR0 bits 1+2).
/// Without this, using AVX instructions would silently corrupt state.
[[nodiscard]] bool osSupportsAvx() {
    // First check if OSXSAVE is enabled (CPUID.1:ECX bit 27)
    unsigned int ecx = 0;
#ifdef _MSC_VER
    int regs[4];
    __cpuid(regs, 1);
    ecx = static_cast<unsigned int>(regs[2]);
#else
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int edx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
#endif

    // Bit 27: OSXSAVE — OS uses XSAVE/XRSTOR for extended state management
    if ((ecx & (1U << 27)) == 0) {
        return false;
    }

    // XCR0 bit 1 = SSE state, bit 2 = AVX state
    uint64_t xcr0 = readXcr0();
    return (xcr0 & 0x6) == 0x6;
}

/// Check if OS supports saving/restoring AVX-512 state (XCR0 bits 5+6+7).
[[nodiscard]] bool osSupportsAvx512() {
    if (!osSupportsAvx()) {
        return false;
    }

    // XCR0 bit 5 = opmask, bit 6 = ZMM_Hi256, bit 7 = Hi16_ZMM
    uint64_t xcr0 = readXcr0();
    return (xcr0 & 0xE0) == 0xE0;
}

/// Internal: detect CPU features via CPUID leaves 1 and 7.
[[nodiscard]] CpuFeatures detect() {
    CpuFeatures features;

    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;

    // --- CPUID leaf 1: Basic feature flags ---
#ifdef _MSC_VER
    int regs[4];
    __cpuid(regs, 1);
    ecx = static_cast<unsigned int>(regs[2]);
#else
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
#endif

    // ECX bit 23: POPCNT
    features.has_popcnt = (ecx & (1U << 23)) != 0;

    // --- CPUID leaf 7, sub-leaf 0: Extended feature flags ---
    eax = ebx = ecx = edx = 0;
#ifdef _MSC_VER
    __cpuidex(regs, 7, 0);
    ebx = static_cast<unsigned int>(regs[1]);
#else
    __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
#endif

    // EBX bit 5: AVX2 (requires OS AVX support)
    if (osSupportsAvx()) {
        features.has_avx2 = (ebx & (1U << 5)) != 0;
    }

    // EBX bit 16: AVX-512F, bit 30: AVX-512BW (requires OS AVX-512 support)
    // ECX bit 12: AVX-512 BITALG — required for `_mm512_popcnt_epi16` used in
    // findMRVCell. Skylake-X / Cascade Lake / Zen 4 have F+BW but no BITALG;
    // dispatch onto those CPUs without this gate would SIGILL (#17 / BL-1).
    if (osSupportsAvx512()) {
        features.has_avx512f = (ebx & (1U << 16)) != 0;
        features.has_avx512bw = (ebx & (1U << 30)) != 0;
        features.has_avx512_bitalg = (ecx & (1U << 12)) != 0;
    }

    return features;
}

}  // anonymous namespace

const CpuFeatures& getCpuFeatures() {
    static const CpuFeatures features = detect();
    return features;
}

}  // namespace sudoku::core
