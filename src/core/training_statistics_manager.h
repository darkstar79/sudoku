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

#include "i_time_provider.h"
#include "i_training_statistics_manager.h"

#include <expected>
#include <filesystem>
#include <memory>
#include <unordered_map>

namespace sudoku::core {

/// YAML-based implementation of ITrainingStatisticsManager
class TrainingStatisticsManager : public ITrainingStatisticsManager {
public:
    explicit TrainingStatisticsManager(
        std::filesystem::path stats_directory = {},
        std::shared_ptr<ITimeProvider> time_provider = std::make_shared<SystemTimeProvider>());

    [[nodiscard]] std::expected<void, TrainingStatsError> recordLesson(const TrainingLessonResult& result) override;

    [[nodiscard]] TechniqueStats getStats(SolvingTechnique technique) const override;

    [[nodiscard]] std::unordered_map<SolvingTechnique, TechniqueStats> getAllStats() const override;

    [[nodiscard]] MasteryLevel getMastery(SolvingTechnique technique) const override;

    void resetAllStats() override;

private:
    std::shared_ptr<ITimeProvider> time_provider_;
    std::filesystem::path stats_directory_;
    std::filesystem::path stats_file_;

    std::unordered_map<SolvingTechnique, TechniqueStats> stats_;

    [[nodiscard]] std::expected<void, TrainingStatsError> ensureDirectoryExists() const;
    [[nodiscard]] std::expected<void, TrainingStatsError> load();
    [[nodiscard]] std::expected<void, TrainingStatsError> save() const;
};

}  // namespace sudoku::core
