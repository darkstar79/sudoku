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

#include "i_statistics_manager.h"

#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace sudoku::core::statistics_serializer {

/// Serializes aggregate statistics to a YAML file
/// @param stats Aggregate statistics to serialize
/// @param file_path Output file path
/// @param timestamp Current time for the "last_updated" field
[[nodiscard]] std::expected<void, StatisticsError>
serializeStatsToYaml(const AggregateStats& stats, const std::filesystem::path& file_path,
                     std::chrono::system_clock::time_point timestamp);

/// Deserializes aggregate statistics from a YAML file
[[nodiscard]] std::expected<AggregateStats, StatisticsError>
deserializeStatsFromYaml(const std::filesystem::path& file_path);

/// Serializes a single game session to a YAML file (appends to existing sessions)
[[nodiscard]] std::expected<void, StatisticsError>
serializeGameStatsToYaml(const GameStats& stats, const std::filesystem::path& file_path, bool append = true);

/// Deserializes all game sessions from a YAML file
[[nodiscard]] std::expected<std::vector<GameStats>, StatisticsError>
deserializeGameStatsFromYaml(const std::filesystem::path& file_path);

/// Deserializes game sessions from an already-parsed YAML node (for encrypted data)
[[nodiscard]] std::expected<std::vector<GameStats>, StatisticsError>
deserializeGameStatsFromNode(const YAML::Node& sessions_node);

/// Writes aggregate statistics as CSV to a file
[[nodiscard]] std::expected<void, StatisticsError> exportAggregateStatsCsv(const AggregateStats& stats,
                                                                           const std::string& file_path);

/// Writes game sessions as CSV to a file
[[nodiscard]] std::expected<void, StatisticsError> exportGameSessionsCsv(const std::vector<GameStats>& sessions,
                                                                         const std::string& file_path);

}  // namespace sudoku::core::statistics_serializer
