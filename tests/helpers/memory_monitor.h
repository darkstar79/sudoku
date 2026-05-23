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

#include <cstddef>

namespace sudoku::test {

/// RAII utility for monitoring memory usage during test execution
/// Tracks process memory growth to detect memory leaks and unbounded allocations
/// Platform-specific implementation (Linux: /proc/self/status, fallback: conservative estimate)
class MemoryMonitor {
public:
    MemoryMonitor() = default;

    /// Start monitoring (capture baseline memory usage)
    void start();

    /// Get current memory usage in bytes
    /// @return Current process memory usage (resident set size)
    [[nodiscard]] size_t getCurrentMemoryUsage() const;

    /// Get memory increase since start() was called
    /// @return Bytes of memory growth since baseline
    [[nodiscard]] size_t getMemoryIncrease() const;

    /// Check if memory growth exceeds threshold
    /// @param max_bytes Maximum allowed memory growth in bytes
    /// @return true if current memory increase exceeds threshold
    [[nodiscard]] bool exceededThreshold(size_t max_bytes) const;

private:
    size_t baseline_memory_ = 0;

    /// Platform-specific implementation for reading process memory
    /// Linux: Reads /proc/self/status (VmRSS field)
    /// Other platforms: Returns 0 (fallback - monitor won't work but tests won't fail)
    [[nodiscard]] size_t getProcessMemory() const;
};

}  // namespace sudoku::test
