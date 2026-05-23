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

#include "i_training_statistics_manager.h"

#include <expected>
#include <filesystem>
#include <unordered_map>

namespace sudoku::core::training_stats_serializer {

/// Serialize per-technique training stats to YAML
[[nodiscard]] std::expected<void, TrainingStatsError>
serializeToYaml(const std::unordered_map<SolvingTechnique, TechniqueStats>& stats,
                const std::filesystem::path& file_path);

/// Deserialize per-technique training stats from YAML
[[nodiscard]] std::expected<std::unordered_map<SolvingTechnique, TechniqueStats>, TrainingStatsError>
deserializeFromYaml(const std::filesystem::path& file_path);

}  // namespace sudoku::core::training_stats_serializer
