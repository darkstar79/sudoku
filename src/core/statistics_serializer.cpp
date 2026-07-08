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

#include "statistics_serializer.h"

#include "core/constants.h"
#include "core/file_utils.h"
#include "core/i_puzzle_generator.h"
#include "core/i_statistics_manager.h"

#include <array>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::core::statistics_serializer {

std::expected<void, StatisticsError> serializeStatsToYaml(const AggregateStats& stats,
                                                          const std::filesystem::path& file_path,
                                                          std::chrono::system_clock::time_point timestamp) {
    try {
        YAML::Node root;

        root["version"] = "1.0";
        root["last_updated"] = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();

        // Per-difficulty stats
        YAML::Node difficulties;
        constexpr std::array<const char*, DIFFICULTY_COUNT> difficulty_names = {"easy", "medium", "hard", "expert",
                                                                                "master"};
        static_assert(difficulty_names.size() == DIFFICULTY_COUNT, "difficulty_names size must match DIFFICULTY_COUNT");

        for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
            YAML::Node diff_node;
            diff_node["games_played"] = stats.games_played[i];
            diff_node["games_completed"] = stats.games_completed[i];
            diff_node["best_time"] = stats.best_times[i].count();
            diff_node["average_time"] = stats.average_times[i].count();
            diff_node["average_rating"] = stats.average_ratings[i];
            diff_node["min_rating"] = stats.min_ratings[i];
            diff_node["max_rating"] = stats.max_ratings[i];
            difficulties[difficulty_names[i]] = diff_node;
        }
        root["difficulties"] = difficulties;

        // Overall stats
        YAML::Node overall;
        overall["total_games"] = stats.total_games;
        overall["total_completed"] = stats.total_completed;
        overall["total_moves"] = stats.total_moves;
        overall["total_hints"] = stats.total_hints;
        overall["total_mistakes"] = stats.total_mistakes;
        overall["total_time_played"] = stats.total_time_played.count();
        overall["current_win_streak"] = stats.current_win_streak;
        overall["best_win_streak"] = stats.best_win_streak;
        root["overall"] = overall;

        // Write the whole file atomically (tmp + fsync + rename, #25): an in-place
        // rewrite that crashes mid-write would truncate the aggregate stats. Emit
        // to a string first, then hand the bytes to the shared crash-safe writer.
        std::stringstream out;
        out << root;
        const std::string yaml_str = out.str();
        const std::vector<uint8_t> bytes(yaml_str.begin(), yaml_str.end());

        if (auto write_result = file_utils::atomicWriteFile(file_path, bytes); !write_result) {
            return std::unexpected(StatisticsError::FileAccessError);
        }

        return {};

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<AggregateStats, StatisticsError>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — parses all stats sections from YAML; each section is necessary; cannot split without adding indirection
deserializeStatsFromYaml(const std::filesystem::path& file_path) {
    try {
        YAML::Node root = YAML::LoadFile(file_path.string());

        AggregateStats stats;

        // Per-difficulty stats
        if (root["difficulties"]) {
            const auto& difficulties = root["difficulties"];
            constexpr std::array<const char*, DIFFICULTY_COUNT> difficulty_names = {"easy", "medium", "hard", "expert",
                                                                                    "master"};
            static_assert(difficulty_names.size() == DIFFICULTY_COUNT,
                          "difficulty_names size must match DIFFICULTY_COUNT");

            for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
                if (difficulties[difficulty_names[i]]) {
                    const auto& diff_node = difficulties[difficulty_names[i]];

                    if (diff_node["games_played"]) {
                        stats.games_played[i] = diff_node["games_played"].as<int64_t>();
                    }
                    if (diff_node["games_completed"]) {
                        stats.games_completed[i] = diff_node["games_completed"].as<int64_t>();
                    }
                    if (diff_node["best_time"]) {
                        stats.best_times[i] = std::chrono::milliseconds(diff_node["best_time"].as<long long>());
                    }
                    if (diff_node["average_time"]) {
                        stats.average_times[i] = std::chrono::milliseconds(diff_node["average_time"].as<long long>());
                    }
                    if (diff_node["average_rating"]) {
                        stats.average_ratings[i] = diff_node["average_rating"].as<double>();
                    }
                    if (diff_node["min_rating"]) {
                        stats.min_ratings[i] = diff_node["min_rating"].as<double>();
                    }
                    if (diff_node["max_rating"]) {
                        stats.max_ratings[i] = diff_node["max_rating"].as<double>();
                    }
                }
            }
        }

        // Overall stats
        if (root["overall"]) {
            const auto& overall = root["overall"];

            if (overall["total_games"]) {
                stats.total_games = overall["total_games"].as<int64_t>();
            }
            if (overall["total_completed"]) {
                stats.total_completed = overall["total_completed"].as<int64_t>();
            }
            if (overall["total_moves"]) {
                stats.total_moves = overall["total_moves"].as<int64_t>();
            }
            if (overall["total_hints"]) {
                stats.total_hints = overall["total_hints"].as<int64_t>();
            }
            if (overall["total_mistakes"]) {
                stats.total_mistakes = overall["total_mistakes"].as<int64_t>();
            }
            if (overall["total_time_played"]) {
                stats.total_time_played = std::chrono::milliseconds(overall["total_time_played"].as<long long>());
            }
            if (overall["current_win_streak"]) {
                stats.current_win_streak = overall["current_win_streak"].as<int64_t>();
            }
            if (overall["best_win_streak"]) {
                stats.best_win_streak = overall["best_win_streak"].as<int64_t>();
            }
        }

        return stats;

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> serializeGameStatsToYaml(const GameStats& stats,
                                                              const std::filesystem::path& file_path, bool append) {
    try {
        YAML::Node sessions;

        // Load existing sessions if appending
        if (append && std::filesystem::exists(file_path)) {
            try {
                sessions = YAML::LoadFile(file_path.string());
            } catch (const YAML::Exception&) {
                // If loading fails, start with empty node
                sessions = YAML::Node(YAML::NodeType::Sequence);
            }
        } else {
            sessions = YAML::Node(YAML::NodeType::Sequence);
        }

        // Create new session entry
        YAML::Node session;
        session["difficulty"] = static_cast<int>(stats.difficulty);
        session["puzzle_rating"] = stats.puzzle_rating;
        session["start_time"] =
            std::chrono::duration_cast<std::chrono::seconds>(stats.start_time.time_since_epoch()).count();
        session["end_time"] =
            std::chrono::duration_cast<std::chrono::seconds>(stats.end_time.time_since_epoch()).count();
        session["time_played"] = stats.time_played.count();
        session["completed"] = stats.completed;
        session["moves_made"] = stats.moves_made;
        session["hints_used"] = stats.hints_used;
        session["mistakes"] = stats.mistakes;
        session["puzzle_seed"] = stats.puzzle_seed;

        sessions.push_back(session);

        // Rewrite the whole sessions file atomically (tmp + fsync + rename, #25):
        // an in-place rewrite that crashes mid-write would truncate accumulated
        // history. Emit to a string first, then hand the bytes to the writer.
        std::stringstream out;
        out << sessions;
        const std::string yaml_str = out.str();
        const std::vector<uint8_t> bytes(yaml_str.begin(), yaml_str.end());

        if (auto write_result = file_utils::atomicWriteFile(file_path, bytes); !write_result) {
            return std::unexpected(StatisticsError::FileAccessError);
        }

        return {};

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML game stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Game stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<std::vector<GameStats>, StatisticsError> deserializeGameStatsFromNode(const YAML::Node& sessions_node) {
    try {
        std::vector<GameStats> result;

        for (const auto& session_node : sessions_node) {
            GameStats stats;

            if (session_node["difficulty"]) {
                // Range-check the raw int BEFORE the cast (STAT-2): Difficulty's underlying
                // type is uint8_t, so static_cast wraps modulo-256 (e.g. 260 → 4/Master),
                // which would silently launder hostile values. Reject the whole node parse on
                // any out-of-range value so it routes through getAllSessions()'s fail-closed
                // latch into the issue-#26 archive flow (never silently skip a session — a
                // skipped session would be destroyed on the next flush rewrite).
                const int diff_val = session_node["difficulty"].as<int>();
                if (diff_val < 0 || diff_val >= static_cast<int>(DIFFICULTY_COUNT)) {
                    spdlog::error("Session difficulty out of range: {} (valid 0..{})", diff_val,
                                  static_cast<int>(DIFFICULTY_COUNT) - 1);
                    return std::unexpected(StatisticsError::SerializationError);
                }
                stats.difficulty = static_cast<Difficulty>(diff_val);
            }
            if (session_node["puzzle_rating"]) {
                stats.puzzle_rating = session_node["puzzle_rating"].as<double>();
            }
            if (session_node["start_time"]) {
                auto seconds = session_node["start_time"].as<long long>();
                stats.start_time = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
            }
            if (session_node["end_time"]) {
                auto seconds = session_node["end_time"].as<long long>();
                stats.end_time = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
            }
            if (session_node["time_played"]) {
                stats.time_played = std::chrono::milliseconds(session_node["time_played"].as<long long>());
            }
            if (session_node["completed"]) {
                stats.completed = session_node["completed"].as<bool>();
            }
            if (session_node["moves_made"]) {
                stats.moves_made = session_node["moves_made"].as<int>();
            }
            if (session_node["hints_used"]) {
                stats.hints_used = session_node["hints_used"].as<int>();
            }
            if (session_node["mistakes"]) {
                stats.mistakes = session_node["mistakes"].as<int>();
            }
            if (session_node["puzzle_seed"]) {
                stats.puzzle_seed = session_node["puzzle_seed"].as<uint32_t>();
            }

            result.push_back(stats);
        }

        return result;

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML game stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Game stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<std::vector<GameStats>, StatisticsError>
deserializeGameStatsFromYaml(const std::filesystem::path& file_path) {
    try {
        YAML::Node sessions = YAML::LoadFile(file_path.string());
        return deserializeGameStatsFromNode(sessions);
    } catch (const YAML::Exception& e) {
        spdlog::error("YAML game stats file load error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> exportAggregateStatsCsv(const AggregateStats& stats,
                                                             const std::string& file_path) {
    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open CSV file for writing: {}", file_path);
            return std::unexpected(StatisticsError::FileAccessError);
        }

        // Write header row
        file << "Difficulty,Games_Played,Games_Completed,Completion_Rate,Best_Time_Seconds,Average_Time_Seconds,"
                "Average_Rating,Min_Rating,Max_Rating\n";

        // Difficulty names for CSV output
        constexpr std::array<std::string_view, DIFFICULTY_COUNT> csv_difficulty_names = {"Easy", "Medium", "Hard",
                                                                                         "Expert", "Master"};
        static_assert(csv_difficulty_names.size() == DIFFICULTY_COUNT,
                      "csv_difficulty_names size must match DIFFICULTY_COUNT");

        // Write per-difficulty rows
        for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
            file << csv_difficulty_names[i] << ",";
            file << stats.games_played[i] << ",";
            file << stats.games_completed[i] << ",";

            // Calculate completion rate
            double completion_rate = 0.0;
            if (stats.games_played[i] > 0) {
                completion_rate =
                    (static_cast<double>(stats.games_completed[i]) / static_cast<double>(stats.games_played[i])) *
                    100.0;
            }
            file << completion_rate << ",";

            // Convert times from milliseconds to seconds
            double best_time_seconds = static_cast<double>(stats.best_times[i].count()) / 1000.0;
            double avg_time_seconds = static_cast<double>(stats.average_times[i].count()) / 1000.0;
            file << best_time_seconds << ",";
            file << avg_time_seconds << ",";

            // Rating statistics
            file << stats.average_ratings[i] << ",";
            file << (stats.min_ratings[i] >= std::numeric_limits<double>::max() ? 0.0 : stats.min_ratings[i]) << ",";
            file << stats.max_ratings[i] << "\n";
        }

        // Write OVERALL row
        file << "OVERALL,";
        file << stats.total_games << ",";
        file << stats.total_completed << ",";
        double overall_completion_rate = 0.0;
        if (stats.total_games > 0) {
            overall_completion_rate =
                (static_cast<double>(stats.total_completed) / static_cast<double>(stats.total_games)) * 100.0;
        }
        file << overall_completion_rate << ",";
        file << "N/A,N/A,N/A,N/A,N/A\n";  // N/A for time and rating columns

        // Write empty line separator
        file << "\n";

        // Write additional aggregate stats header
        file << "Total_Moves,Total_Hints,Total_Mistakes,Total_Time_Hours,Current_Streak,Best_Streak\n";

        // Calculate total time in hours
        double total_time_hours = static_cast<double>(stats.total_time_played.count()) / (1000.0 * 60.0 * 60.0);

        // Write aggregate stats row
        file << stats.total_moves << ",";
        file << stats.total_hints << ",";
        file << stats.total_mistakes << ",";
        file << total_time_hours << ",";
        file << stats.current_win_streak << ",";
        file << stats.best_win_streak << "\n";

        file.close();
        spdlog::info("Aggregate statistics exported to CSV: {}", file_path);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during CSV export: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> exportGameSessionsCsv(const std::vector<GameStats>& sessions,
                                                           const std::string& file_path) {
    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open CSV file for writing: {}", file_path);
            return std::unexpected(StatisticsError::FileAccessError);
        }

        // Write header row
        file << "Session_ID,Date,Time,Difficulty,Rating,Completed,Duration_Seconds,Moves,Hints,Mistakes,Puzzle_Seed\n";

        // Difficulty names for CSV output
        constexpr std::array<std::string_view, DIFFICULTY_COUNT> csv_difficulty_names = {"Easy", "Medium", "Hard",
                                                                                         "Expert", "Master"};
        static_assert(csv_difficulty_names.size() == DIFFICULTY_COUNT,
                      "csv_difficulty_names size must match DIFFICULTY_COUNT");

        // Write session data rows
        for (size_t i = 0; i < sessions.size(); ++i) {
            const auto& session = sessions[i];

            // Session ID (1-indexed)
            file << (i + 1) << ",";

            // Format timestamp as date and time. Reentrant conversion (#24 M3) — mirrors the
            // idiom in save_manager.cpp:93-101; the plain C library call returns a shared buffer.
            // A zero-initialized tm_val is the benign fallback on the (practically unreachable)
            // conversion failure, keeping the two CSV columns aligned without a branch.
            auto time_t_val = std::chrono::system_clock::to_time_t(session.start_time);
            std::tm tm_val{};
#ifdef _MSC_VER
            (void)localtime_s(&tm_val, &time_t_val);
#else
            (void)localtime_r(&time_t_val, &tm_val);
#endif

            // Date (YYYY-MM-DD)
            file << std::put_time(&tm_val, "%Y-%m-%d") << ",";

            // Time (HH:MM:SS)
            file << std::put_time(&tm_val, "%H:%M:%S") << ",";

            // Difficulty (enum is 0-based, matches array indexing)
            file << csv_difficulty_names[static_cast<size_t>(session.difficulty)] << ",";

            // Rating
            file << session.puzzle_rating << ",";

            // Completed status
            file << (session.completed ? "Yes" : "No") << ",";

            // Duration in seconds
            double duration_seconds = static_cast<double>(session.time_played.count()) / 1000.0;
            file << duration_seconds << ",";

            // Moves, hints, mistakes
            file << session.moves_made << ",";
            file << session.hints_used << ",";
            file << session.mistakes << ",";

            // Puzzle seed
            file << session.puzzle_seed << "\n";
        }

        file.close();
        spdlog::info("Game sessions exported to CSV: {} ({} sessions)", file_path, sessions.size());
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during CSV export: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

}  // namespace sudoku::core::statistics_serializer
