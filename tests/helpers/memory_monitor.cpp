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

#include "memory_monitor.h"

#include <fstream>
#include <sstream>
#include <string>

namespace sudoku::test {

void MemoryMonitor::start() {
    baseline_memory_ = getProcessMemory();
}

size_t MemoryMonitor::getCurrentMemoryUsage() const {
    return getProcessMemory();
}

size_t MemoryMonitor::getMemoryIncrease() const {
    size_t current = getProcessMemory();
    if (current < baseline_memory_) {
        return 0;  // Memory decreased or monitoring not started
    }
    return current - baseline_memory_;
}

bool MemoryMonitor::exceededThreshold(size_t max_bytes) const {
    return getMemoryIncrease() > max_bytes;
}

size_t MemoryMonitor::getProcessMemory() const {
#ifdef __linux__
    // Read /proc/self/status to get VmRSS (Resident Set Size)
    std::ifstream status_file("/proc/self/status");
    if (!status_file.is_open()) {
        return 0;  // Can't read proc file - return 0 (monitoring disabled)
    }

    std::string line;
    while (std::getline(status_file, line)) {
        // Look for VmRSS line (format: "VmRSS:    12345 kB")
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line.substr(6));  // Skip "VmRSS:"
            size_t memory_kb = 0;
            iss >> memory_kb;
            return memory_kb * 1024;  // Convert KB to bytes
        }
    }

    return 0;  // VmRSS not found
#else
    // Non-Linux platforms: return 0 (monitoring not supported)
    // Tests will still run but memory assertions will be skipped
    return 0;
#endif
}

}  // namespace sudoku::test
