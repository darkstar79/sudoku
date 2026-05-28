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

#include "../../src/core/statistics_serializer.h"
#include "../helpers/test_utils.h"

#include <climits>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::core::statistics_serializer;

TEST_CASE("StatisticsSerializer - AggregateStats YAML round-trip", "[statistics_serializer]") {
    sudoku::test::TempTestDir tmp_dir;

    SECTION("Default stats round-trip preserves values") {
        AggregateStats original;
        original.games_played = {10, 20, 30, 5, 2};
        original.games_completed = {8, 15, 20, 3, 1};
        original.best_times = {std::chrono::milliseconds(60000), std::chrono::milliseconds(120000),
                               std::chrono::milliseconds(180000), std::chrono::milliseconds(300000),
                               std::chrono::milliseconds(600000)};
        original.average_times = {std::chrono::milliseconds(90000), std::chrono::milliseconds(150000),
                                  std::chrono::milliseconds(240000), std::chrono::milliseconds(360000),
                                  std::chrono::milliseconds(720000)};
        original.average_ratings = {100, 200, 400, 800, 1500};
        original.min_ratings = {50, 100, 250, 500, 1000};
        original.max_ratings = {150, 300, 600, 1200, 2000};
        original.total_games = 67;
        original.total_completed = 47;
        original.total_moves = 3500;
        original.total_hints = 25;
        original.total_mistakes = 42;
        original.total_time_played = std::chrono::milliseconds(5400000);
        original.current_win_streak = 3;
        original.best_win_streak = 7;

        auto file_path = tmp_dir.path() / "stats.yaml";
        auto timestamp = std::chrono::system_clock::now();

        auto write_result = serializeStatsToYaml(original, file_path, timestamp);
        REQUIRE(write_result.has_value());

        auto read_result = deserializeStatsFromYaml(file_path);
        REQUIRE(read_result.has_value());

        const auto& loaded = read_result.value();
        REQUIRE(loaded.games_played == original.games_played);
        REQUIRE(loaded.games_completed == original.games_completed);
        REQUIRE(loaded.best_times == original.best_times);
        REQUIRE(loaded.average_times == original.average_times);
        REQUIRE(loaded.average_ratings == original.average_ratings);
        REQUIRE(loaded.min_ratings == original.min_ratings);
        REQUIRE(loaded.max_ratings == original.max_ratings);
        REQUIRE(loaded.total_games == original.total_games);
        REQUIRE(loaded.total_completed == original.total_completed);
        REQUIRE(loaded.total_moves == original.total_moves);
        REQUIRE(loaded.total_hints == original.total_hints);
        REQUIRE(loaded.total_mistakes == original.total_mistakes);
        REQUIRE(loaded.total_time_played == original.total_time_played);
        REQUIRE(loaded.current_win_streak == original.current_win_streak);
        REQUIRE(loaded.best_win_streak == original.best_win_streak);
    }

    SECTION("Empty stats round-trip") {
        AggregateStats original;  // All defaults
        auto file_path = tmp_dir.path() / "empty_stats.yaml";

        auto write_result = serializeStatsToYaml(original, file_path, std::chrono::system_clock::now());
        REQUIRE(write_result.has_value());

        auto read_result = deserializeStatsFromYaml(file_path);
        REQUIRE(read_result.has_value());

        const auto& loaded = read_result.value();
        REQUIRE(loaded.total_games == 0);
        REQUIRE(loaded.total_completed == 0);
    }
}

TEST_CASE("StatisticsSerializer - AggregateStats YAML error handling", "[statistics_serializer]") {
    SECTION("Non-existent file returns error") {
        auto result = deserializeStatsFromYaml("/tmp/nonexistent_stats_file_12345.yaml");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == StatisticsError::SerializationError);
    }

    SECTION("Invalid path for writing returns error") {
        AggregateStats stats;
        auto result =
            serializeStatsToYaml(stats, "/nonexistent_dir/subdir/stats.yaml", std::chrono::system_clock::now());
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == StatisticsError::FileAccessError);
    }
}

TEST_CASE("StatisticsSerializer - GameStats YAML round-trip", "[statistics_serializer]") {
    sudoku::test::TempTestDir tmp_dir;

    SECTION("Single game session round-trip") {
        GameStats original = sudoku::test::createTestGameStats(Difficulty::Hard, true);
        original.puzzle_rating = 750;

        auto file_path = tmp_dir.path() / "sessions.yaml";

        auto write_result = serializeGameStatsToYaml(original, file_path, false);
        REQUIRE(write_result.has_value());

        auto read_result = deserializeGameStatsFromYaml(file_path);
        REQUIRE(read_result.has_value());
        REQUIRE(read_result.value().size() == 1);

        const auto& loaded = read_result.value()[0];
        REQUIRE(loaded.difficulty == original.difficulty);
        REQUIRE(loaded.puzzle_rating == original.puzzle_rating);
        REQUIRE(loaded.completed == original.completed);
        REQUIRE(loaded.moves_made == original.moves_made);
        REQUIRE(loaded.hints_used == original.hints_used);
        REQUIRE(loaded.mistakes == original.mistakes);
        REQUIRE(loaded.puzzle_seed == original.puzzle_seed);
        REQUIRE(loaded.time_played == original.time_played);
    }

    SECTION("Appending multiple sessions") {
        auto file_path = tmp_dir.path() / "multi_sessions.yaml";

        auto stats1 = sudoku::test::createTestGameStats(Difficulty::Easy, true);
        auto stats2 = sudoku::test::createTestGameStats(Difficulty::Hard, false);

        auto r1 = serializeGameStatsToYaml(stats1, file_path, false);
        REQUIRE(r1.has_value());

        auto r2 = serializeGameStatsToYaml(stats2, file_path, true);
        REQUIRE(r2.has_value());

        auto read_result = deserializeGameStatsFromYaml(file_path);
        REQUIRE(read_result.has_value());
        REQUIRE(read_result.value().size() == 2);
        REQUIRE(read_result.value()[0].difficulty == Difficulty::Easy);
        REQUIRE(read_result.value()[1].difficulty == Difficulty::Hard);
    }

    SECTION("All difficulties round-trip correctly") {
        auto file_path = tmp_dir.path() / "all_diff.yaml";

        constexpr std::array<Difficulty, 5> diffs = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard,
                                                     Difficulty::Expert, Difficulty::Master};

        for (size_t i = 0; i < diffs.size(); ++i) {
            auto stats = sudoku::test::createTestGameStats(diffs[i], true);
            auto r = serializeGameStatsToYaml(stats, file_path, i > 0);
            REQUIRE(r.has_value());
        }

        auto read_result = deserializeGameStatsFromYaml(file_path);
        REQUIRE(read_result.has_value());
        REQUIRE(read_result.value().size() == 5);

        for (size_t i = 0; i < diffs.size(); ++i) {
            REQUIRE(read_result.value()[i].difficulty == diffs[i]);
        }
    }
}

TEST_CASE("StatisticsSerializer - GameStats YAML error handling", "[statistics_serializer]") {
    SECTION("Non-existent file returns error") {
        auto result = deserializeGameStatsFromYaml("/tmp/nonexistent_sessions_12345.yaml");
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("StatisticsSerializer - CSV export", "[statistics_serializer]") {
    sudoku::test::TempTestDir tmp_dir;

    SECTION("Aggregate stats CSV has correct header") {
        AggregateStats stats;
        stats.games_played = {5, 10, 3, 1, 0};
        stats.games_completed = {4, 8, 2, 0, 0};
        stats.total_games = 19;
        stats.total_completed = 14;

        auto file_path = (tmp_dir.path() / "aggregate.csv").string();
        auto result = exportAggregateStatsCsv(stats, file_path);
        REQUIRE(result.has_value());

        std::ifstream file(file_path);
        REQUIRE(file.is_open());

        std::string header;
        std::getline(file, header);
        REQUIRE(header.find("Difficulty") != std::string::npos);
        REQUIRE(header.find("Games_Played") != std::string::npos);
        REQUIRE(header.find("Completion_Rate") != std::string::npos);

        // Verify 5 difficulty rows + 1 overall row exist
        int row_count = 0;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                ++row_count;
            }
        }
        // 5 difficulty rows + OVERALL + aggregate header + aggregate data = 8
        REQUIRE(row_count >= 7);
    }

    SECTION("Game sessions CSV has correct header") {
        std::vector<GameStats> sessions;
        sessions.push_back(sudoku::test::createTestGameStats(Difficulty::Medium, true));
        sessions.push_back(sudoku::test::createTestGameStats(Difficulty::Hard, false));

        auto file_path = (tmp_dir.path() / "sessions.csv").string();
        auto result = exportGameSessionsCsv(sessions, file_path);
        REQUIRE(result.has_value());

        std::ifstream file(file_path);
        REQUIRE(file.is_open());

        std::string header;
        std::getline(file, header);
        REQUIRE(header.find("Session_ID") != std::string::npos);
        REQUIRE(header.find("Difficulty") != std::string::npos);
        REQUIRE(header.find("Completed") != std::string::npos);

        // Verify correct number of data rows
        int row_count = 0;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                ++row_count;
            }
        }
        REQUIRE(row_count == 2);
    }

    SECTION("CSV export to invalid path returns error") {
        AggregateStats stats;
        auto result = exportAggregateStatsCsv(stats, "/nonexistent_dir/aggregate.csv");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == StatisticsError::FileAccessError);
    }

    SECTION("Empty sessions CSV has only header") {
        std::vector<GameStats> sessions;
        auto file_path = (tmp_dir.path() / "empty_sessions.csv").string();
        auto result = exportGameSessionsCsv(sessions, file_path);
        REQUIRE(result.has_value());

        std::ifstream file(file_path);
        std::string header;
        std::getline(file, header);
        REQUIRE(header.find("Session_ID") != std::string::npos);

        std::string line;
        REQUIRE_FALSE(std::getline(file, line));  // No data rows
    }
}

// ============================================================================
// Atomic write guarantee (issue #25): the sessions file is rewritten via a
// .tmp + rename so a crash cannot truncate accumulated history.
// ============================================================================

TEST_CASE("StatisticsSerializer - serializeGameStatsToYaml is atomic", "[statistics_serializer]") {
    sudoku::test::TempTestDir tmp_dir;
    auto file_path = tmp_dir.path() / "sessions.yaml";

    SECTION("Successful write leaves no .tmp scratch file") {
        auto stats = sudoku::test::createTestGameStats(Difficulty::Medium, true);
        auto result = serializeGameStatsToYaml(stats, file_path, false);
        REQUIRE(result.has_value());

        REQUIRE(std::filesystem::exists(file_path));
        REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(file_path) += ".tmp"));
    }

    SECTION("Stray truncated .tmp does not corrupt accumulated history") {
        auto stats1 = sudoku::test::createTestGameStats(Difficulty::Easy, true);
        auto stats2 = sudoku::test::createTestGameStats(Difficulty::Hard, false);
        REQUIRE(serializeGameStatsToYaml(stats1, file_path, false).has_value());
        REQUIRE(serializeGameStatsToYaml(stats2, file_path, true).has_value());

        // Simulate a crash during the next append: a partial .tmp is left behind
        // but never renamed into place, so the real file keeps both sessions.
        {
            std::ofstream partial(std::filesystem::path(file_path) += ".tmp", std::ios::binary | std::ios::trunc);
            partial << "- difficulty: 0\n  cor";  // truncated mid-record
        }

        auto read_result = deserializeGameStatsFromYaml(file_path);
        REQUIRE(read_result.has_value());
        REQUIRE(read_result->size() == 2);
    }
}

TEST_CASE("StatisticsSerializer - serializeStatsToYaml is atomic", "[statistics_serializer]") {
    sudoku::test::TempTestDir tmp_dir;
    auto file_path = tmp_dir.path() / "aggregate_stats.yaml";
    const auto now = std::chrono::system_clock::now();

    SECTION("Successful write leaves no .tmp scratch file") {
        AggregateStats stats;
        REQUIRE(serializeStatsToYaml(stats, file_path, now).has_value());

        REQUIRE(std::filesystem::exists(file_path));
        REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(file_path) += ".tmp"));
    }

    SECTION("A stray .tmp from a crashed prior write is cleared, target round-trips") {
        // Simulate a crash during a previous save that left a partial .tmp behind.
        {
            std::ofstream partial(std::filesystem::path(file_path) += ".tmp", std::ios::binary | std::ios::trunc);
            partial << "garbage from a crash";
        }

        AggregateStats stats;
        stats.total_games = 42;
        REQUIRE(serializeStatsToYaml(stats, file_path, now).has_value());

        // The atomic writer reuses <file>.tmp as its scratch file and renames it
        // into place, so no stray .tmp may survive a successful write.
        REQUIRE(std::filesystem::exists(file_path));
        REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(file_path) += ".tmp"));

        auto loaded = deserializeStatsFromYaml(file_path);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->total_games == 42);
    }
}
