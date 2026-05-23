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

#include "training_statistics_manager.h"

#include "infrastructure/app_directory_manager.h"
#include "training_statistics_serializer.h"

#include <filesystem>
#include <utility>

#include <spdlog/spdlog.h>

namespace sudoku::core {

TrainingStatisticsManager::TrainingStatisticsManager(std::filesystem::path stats_directory,
                                                     std::shared_ptr<ITimeProvider> time_provider)
    : time_provider_(std::move(time_provider)), stats_directory_(std::move(stats_directory)) {
    if (stats_directory_.empty()) {
        stats_directory_ =
            infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::TrainingStatistics);
    }
    stats_file_ = stats_directory_ / "training_stats.yaml";

    auto load_result = load();
    if (!load_result) {
        spdlog::warn("Could not load training stats: will start fresh");
    }
}

std::expected<void, TrainingStatsError> TrainingStatisticsManager::recordLesson(const TrainingLessonResult& result) {
    auto& ts = stats_[result.technique];

    ts.total_exercises_attempted += result.total_exercises;
    ts.total_correct += result.correct_count;
    ts.last_practiced = time_provider_->systemNow();

    // Update running average hints (incremental mean)
    int total_sessions =
        (ts.total_exercises_attempted > 0) ? (ts.total_exercises_attempted / result.total_exercises) : 1;
    ts.average_hints +=
        (static_cast<double>(result.hints_used) - ts.average_hints) / static_cast<double>(total_sessions);

    // Update best score
    if (result.correct_count > ts.best_score) {
        ts.best_score = result.correct_count;
        ts.best_session_hints = result.hints_used;
    } else if (result.correct_count == ts.best_score && ts.best_session_hints >= 0 &&
               result.hints_used < ts.best_session_hints) {
        ts.best_session_hints = result.hints_used;
    }

    // Track mastery progression
    if (result.correct_count == result.total_exercises && result.hints_used == 0) {
        ts.mastered_count++;
    }
    if (result.correct_count == result.total_exercises && result.hints_used <= 5) {
        ts.proficient_count++;
    }

    return save();
}

TechniqueStats TrainingStatisticsManager::getStats(SolvingTechnique technique) const {
    auto it = stats_.find(technique);
    if (it != stats_.end()) {
        return it->second;
    }
    return {};
}

std::unordered_map<SolvingTechnique, TechniqueStats> TrainingStatisticsManager::getAllStats() const {
    return stats_;
}

MasteryLevel TrainingStatisticsManager::getMastery(SolvingTechnique technique) const {
    return computeMastery(getStats(technique));
}

void TrainingStatisticsManager::resetAllStats() {
    stats_.clear();
    auto result = save();
    if (!result) {
        spdlog::error("Failed to save after resetting training stats");
    }
}

std::expected<void, TrainingStatsError> TrainingStatisticsManager::ensureDirectoryExists() const {
    try {
        if (!std::filesystem::exists(stats_directory_)) {
            std::filesystem::create_directories(stats_directory_);
        }
        return {};
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("Failed to create training stats directory: {}", e.what());
        return std::unexpected(TrainingStatsError::FileAccessError);
    }
}

std::expected<void, TrainingStatsError> TrainingStatisticsManager::load() {
    auto result = training_stats_serializer::deserializeFromYaml(stats_file_);
    if (!result) {
        return std::unexpected(result.error());
    }
    stats_ = std::move(*result);
    return {};
}

std::expected<void, TrainingStatsError> TrainingStatisticsManager::save() const {
    auto dir_result = ensureDirectoryExists();
    if (!dir_result) {
        return dir_result;
    }
    return training_stats_serializer::serializeToYaml(stats_, stats_file_);
}

}  // namespace sudoku::core
