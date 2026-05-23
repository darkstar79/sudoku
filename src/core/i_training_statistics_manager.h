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

#include "solving_technique.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <string>
#include <unordered_map>

namespace sudoku::core {

/// Mastery level for a technique — progression through practice
enum class MasteryLevel : uint8_t {
    Beginner,      ///< < 3 correct in best session
    Intermediate,  ///< 3-4 correct in best session
    Proficient,    ///< 5/5 correct with ≤ 5 hints total
    Mastered       ///< 5/5 correct with 0 hints, achieved twice
};

/// Per-technique training statistics
struct TechniqueStats {
    int total_exercises_attempted{0};
    int total_correct{0};
    int best_score{0};           ///< Best score out of 5 in a single session
    int best_session_hints{-1};  ///< Hints used in best-score session (-1 = no session)
    int proficient_count{0};     ///< Number of times 5/5 with ≤ 5 hints achieved
    int mastered_count{0};       ///< Number of times 5/5 with 0 hints achieved
    double average_hints{0.0};   ///< Running average of hints per exercise
    std::chrono::system_clock::time_point last_practiced;

    bool operator==(const TechniqueStats& other) const = default;
};

/// Result of a completed training lesson (5 exercises on one technique)
struct TrainingLessonResult {
    SolvingTechnique technique{SolvingTechnique::NakedSingle};
    int correct_count{0};    ///< Out of total_exercises
    int total_exercises{5};  ///< Typically 5
    int hints_used{0};       ///< Total hints across all exercises
    std::chrono::system_clock::time_point timestamp;
};

/// Error types for training statistics operations
enum class TrainingStatsError : uint8_t {
    FileAccessError,
    SerializationError,
    InvalidData
};

/// Interface for persisting per-technique training statistics
class ITrainingStatisticsManager {
public:
    virtual ~ITrainingStatisticsManager() = default;

    /// Record a completed lesson result
    [[nodiscard]] virtual std::expected<void, TrainingStatsError> recordLesson(const TrainingLessonResult& result) = 0;

    /// Get statistics for a specific technique
    [[nodiscard]] virtual TechniqueStats getStats(SolvingTechnique technique) const = 0;

    /// Get statistics for all practiced techniques
    [[nodiscard]] virtual std::unordered_map<SolvingTechnique, TechniqueStats> getAllStats() const = 0;

    /// Compute mastery level from technique stats
    [[nodiscard]] static constexpr MasteryLevel computeMastery(const TechniqueStats& stats) {
        if (stats.mastered_count >= 2) {
            return MasteryLevel::Mastered;
        }
        if (stats.proficient_count >= 1) {
            return MasteryLevel::Proficient;
        }
        if (stats.best_score >= 3) {
            return MasteryLevel::Intermediate;
        }
        return MasteryLevel::Beginner;
    }

    /// Get mastery level for a technique
    [[nodiscard]] virtual MasteryLevel getMastery(SolvingTechnique technique) const = 0;

    /// Reset all training statistics
    virtual void resetAllStats() = 0;

protected:
    ITrainingStatisticsManager() = default;
    ITrainingStatisticsManager(const ITrainingStatisticsManager&) = default;
    ITrainingStatisticsManager& operator=(const ITrainingStatisticsManager&) = default;
    ITrainingStatisticsManager(ITrainingStatisticsManager&&) = default;
    ITrainingStatisticsManager& operator=(ITrainingStatisticsManager&&) = default;
};

}  // namespace sudoku::core
