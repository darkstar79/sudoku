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

#include "cpu_features.h"
#include "i_puzzle_generator.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace sudoku::core {

/// Fixed-size open-addressing hash table for Zobrist memoization cache.
/// Zero heap allocation, power-of-2 sizing with Fibonacci hashing.
/// Replaces std::unordered_map to eliminate per-insert malloc overhead.
class FlatSolutionCache {
public:
    /// Table capacity (must be power of 2). 2^13 = 8192 entries × 16 bytes = 128 KB.
    /// Sized for L2 cache — 4x average utilization (~2K entries), 32x smaller than original 4 MB.
    static constexpr size_t CAPACITY = 8192;
    static constexpr size_t MASK = CAPACITY - 1;

    /// Fibonacci hashing constant for 64-bit keys (golden ratio × 2^64)
    static constexpr uint64_t FIBONACCI_HASH = 11400714819323198485ULL;

    /// Sentinel key indicating an empty slot
    static constexpr uint64_t EMPTY_KEY = 0;

    struct Entry {
        uint64_t key{EMPTY_KEY};  ///< Zobrist hash (0 = empty slot)
        int value{0};             ///< Cached solution count
    };

    FlatSolutionCache() = default;

    /// Look up a key. Returns pointer to value if found, nullptr if not.
    [[nodiscard]] const int* find(uint64_t key) const {
        if (key == EMPTY_KEY) {
            return nullptr;  // Sentinel key cannot be stored
        }
        size_t idx = fibHash(key);
        for (size_t probe = 0; probe < MAX_PROBE; ++probe) {
            const auto& entry = table_[(idx + probe) & MASK];
            if (entry.key == key) {
                return &entry.value;
            }
            if (entry.key == EMPTY_KEY) {
                return nullptr;  // Empty slot = key not in table
            }
        }
        return nullptr;  // Probe limit reached
    }

    /// Insert or update a key-value pair. Silently drops if table is too full.
    void insert(uint64_t key, int value) {
        if (key == EMPTY_KEY || size_ >= MAX_LOAD) {
            return;  // Can't store sentinel key or table too full
        }
        size_t idx = fibHash(key);
        for (size_t probe = 0; probe < MAX_PROBE; ++probe) {
            auto& entry = table_[(idx + probe) & MASK];
            if (entry.key == EMPTY_KEY) {
                entry.key = key;
                entry.value = value;
                ++size_;
                return;
            }
            if (entry.key == key) {
                entry.value = value;  // Update existing
                return;
            }
        }
        // Probe limit reached — silently drop
    }

    /// Clear all entries
    void clear() {
        table_.fill(Entry{});
        size_ = 0;
    }

    [[nodiscard]] size_t size() const {
        return size_;
    }

private:
    [[nodiscard]] static size_t fibHash(uint64_t key) {
        return static_cast<size_t>((key * FIBONACCI_HASH) >> (64 - 13));  // 13 = log2(CAPACITY)
    }

    /// Max probe distance before giving up (keeps worst-case bounded)
    static constexpr size_t MAX_PROBE = 64;

    /// Max entries before we stop inserting (75% load factor)
    static constexpr size_t MAX_LOAD = (CAPACITY * 3) / 4;  // 6144

    std::array<Entry, CAPACITY> table_{};
    size_t size_{0};
};

// Forward declarations
struct Board;
class ConstraintState;
struct SIMDConstraintState;

/// Counts solutions for Sudoku boards using backtracking with Zobrist memoization.
/// Supports scalar, AVX2, and AVX-512 code paths via runtime CPU dispatch.
class SolutionCounter {
public:
    SolutionCounter();

    /// Counts solutions up to max_solutions (default: 2 for uniqueness check)
    [[nodiscard]] int countSolutions(const std::vector<std::vector<int>>& board, int max_solutions = 2) const;

    /// Counts solutions with explicit timeout
    [[nodiscard]] int countSolutionsWithTimeout(const std::vector<std::vector<int>>& board, int max_solutions,
                                                std::chrono::milliseconds timeout) const;

    /// Checks if board has exactly one solution
    [[nodiscard]] bool hasUniqueSolution(const std::vector<std::vector<int>>& board) const;

    /// Checks if board has any contradictions (empty domains)
    [[nodiscard]] static bool hasContradiction(const std::vector<std::vector<int>>& board);

    /// Applies iterative constraint propagation until fixed point
    [[nodiscard]] static std::expected<std::vector<std::vector<int>>, GenerationError>
    propagateConstraints(const std::vector<std::vector<int>>& board);

    /// Scalar version of constraint propagation
    [[nodiscard]] static std::expected<std::vector<std::vector<int>>, GenerationError>
    propagateConstraintsScalar(const std::vector<std::vector<int>>& board);

    /// SIMD version of constraint propagation
    [[nodiscard]] static std::expected<std::vector<std::vector<int>>, GenerationError>
    propagateConstraintsSIMD(const std::vector<std::vector<int>>& board);

    /// Clears the memoization cache (called at start of each generation attempt)
    void clearCache() const;

    /// Set solver path override for benchmarking
    void setSolverPath(SolverPath path) {
        active_solver_path_ = path;
    }

    /// Get the currently active solver path
    [[nodiscard]] SolverPath solverPath() const {
        return active_solver_path_;
    }

private:
    mutable SolverPath active_solver_path_{SolverPath::Auto};

    /// Zobrist hash table for board state hashing (initialized once)
    mutable std::array<std::array<std::array<uint64_t, 10>, 9>, 9> zobrist_table_{};

    /// Memoization cache for solution counts (cleared per generation attempt)
    mutable FlatSolutionCache solution_count_cache_;

    /// Initializes Zobrist hash table with random values
    void initializeZobristTable();

    /// Computes Zobrist hash for a board state
    [[nodiscard]] uint64_t computeBoardHash(const Board& board) const;

    /// Resolve Auto solver path to a concrete path based on CPU features
    [[nodiscard]] SolverPath resolveEffectivePath() const;

    /// Scalar backtracking helper for counting solutions
    void countSolutionsHelper(Board& board, ConstraintState& state, int& count, int max_solutions,
                              const std::chrono::steady_clock::time_point& start_time,
                              const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                              uint32_t& recursion_count) const;

    /// SIMD-optimized backtracking helper for counting solutions
    void countSolutionsHelperSIMD(Board& board, SIMDConstraintState& state, int& count, int max_solutions,
                                  const std::chrono::steady_clock::time_point& start_time,
                                  const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                                  uint32_t& recursion_count, uint32_t dirty_regions) const;

    /// Apply iterative propagation to fill all forced moves (scalar version)
    [[nodiscard]] static bool applyIterativePropagationScalar(std::vector<std::vector<int>>& board,
                                                              ConstraintState& state);

    /// Apply iterative propagation to fill all forced moves (SIMD version)
    [[nodiscard]] static bool applyIterativePropagationSIMD(std::vector<std::vector<int>>& board,
                                                            SIMDConstraintState& state);
};

}  // namespace sudoku::core
