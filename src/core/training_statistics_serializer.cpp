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

#include "training_statistics_serializer.h"

#include "core/solving_technique.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::core::training_stats_serializer {

std::expected<void, TrainingStatsError>
serializeToYaml(const std::unordered_map<SolvingTechnique, TechniqueStats>& stats,
                const std::filesystem::path& file_path) {
    try {
        YAML::Node root;
        root["version"] = "1.0";

        YAML::Node techniques;
        for (const auto& [technique, ts] : stats) {
            YAML::Node node;
            node["total_exercises_attempted"] = ts.total_exercises_attempted;
            node["total_correct"] = ts.total_correct;
            node["best_score"] = ts.best_score;
            node["best_session_hints"] = ts.best_session_hints;
            node["proficient_count"] = ts.proficient_count;
            node["mastered_count"] = ts.mastered_count;
            node["average_hints"] = ts.average_hints;
            node["last_practiced"] =
                std::chrono::duration_cast<std::chrono::seconds>(ts.last_practiced.time_since_epoch()).count();

            techniques[std::string(getTechniqueName(technique))] = node;
        }
        root["techniques"] = techniques;

        std::ofstream file(file_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open training stats file for writing: {}", file_path.string());
            return std::unexpected(TrainingStatsError::FileAccessError);
        }
        file << root;
        return {};
    } catch (const std::exception& e) {
        spdlog::error("Failed to serialize training stats: {}", e.what());
        return std::unexpected(TrainingStatsError::SerializationError);
    }
}

namespace {

/// Map technique name string back to SolvingTechnique enum
[[nodiscard]] std::optional<SolvingTechnique> techniqueFromName(const std::string& name) {
    // Check all techniques by name
    for (int i = 0; i <= static_cast<int>(SolvingTechnique::GroupedXCycles); ++i) {
        auto tech = static_cast<SolvingTechnique>(i);
        if (getTechniqueName(tech) == name) {
            return tech;
        }
    }
    if (name == "Backtracking") {
        return SolvingTechnique::Backtracking;
    }
    return std::nullopt;
}

}  // namespace

std::expected<std::unordered_map<SolvingTechnique, TechniqueStats>, TrainingStatsError>
deserializeFromYaml(const std::filesystem::path& file_path) {
    try {
        if (!std::filesystem::exists(file_path)) {
            return std::unordered_map<SolvingTechnique, TechniqueStats>{};
        }

        YAML::Node root = YAML::LoadFile(file_path.string());
        std::unordered_map<SolvingTechnique, TechniqueStats> result;

        if (!root["techniques"]) {
            return result;
        }

        for (const auto& entry : root["techniques"]) {
            auto name = entry.first.as<std::string>();
            auto tech = techniqueFromName(name);
            if (!tech) {
                spdlog::warn("Unknown technique in training stats: {}", name);
                continue;
            }

            const auto& node = entry.second;
            TechniqueStats ts;
            ts.total_exercises_attempted = node["total_exercises_attempted"].as<int>(0);
            ts.total_correct = node["total_correct"].as<int>(0);
            ts.best_score = node["best_score"].as<int>(0);
            ts.best_session_hints = node["best_session_hints"].as<int>(-1);
            ts.proficient_count = node["proficient_count"].as<int>(0);
            ts.mastered_count = node["mastered_count"].as<int>(0);
            ts.average_hints = node["average_hints"].as<double>(0.0);

            auto last_practiced_seconds = node["last_practiced"].as<int64_t>(0);
            ts.last_practiced = std::chrono::system_clock::time_point(std::chrono::seconds(last_practiced_seconds));

            result[*tech] = ts;
        }

        return result;
    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize training stats: {}", e.what());
        return std::unexpected(TrainingStatsError::SerializationError);
    }
}

}  // namespace sudoku::core::training_stats_serializer
