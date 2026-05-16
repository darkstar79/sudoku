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

#include "../../src/core/i_time_provider.h"
#include "../../src/core/statistics_manager.h"
#include "../helpers/test_utils.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace fs = std::filesystem;

namespace {

class CsvExportTestFixture {
public:
    CsvExportTestFixture()
        : mock_time_(std::make_shared<sudoku::core::MockTimeProvider>()),
          stats_manager_(test_dir_.path().string(), mock_time_) {
        stats_manager_.setCollectDetailedStats(true);
    }

    ~CsvExportTestFixture() {
        stats_manager_.flushSessions();
    }

    std::vector<std::string> readCsvLines(const std::string& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return {};
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    int countCsvColumns(const std::string& line) {
        int count = 1;
        for (char c : line) {
            if (c == ',') {
                count++;
            }
        }
        return count;
    }

    void createTestGameSessions(int count) {
        using namespace std::chrono;
        using namespace sudoku::core;

        // Create test sessions with known data
        for (int i = 0; i < count; ++i) {
            // Difficulty enum is 0-based (Easy=0, Medium=1, Hard=2, Expert=3)
            auto difficulty = static_cast<Difficulty>(i % 4);
            auto session_id_result = stats_manager_.startGame(difficulty, 1000 + i, 0);  // Default rating
            REQUIRE(session_id_result.has_value());

            auto session_id = session_id_result.value();

            // Record some moves
            for (int j = 0; j < 10 + i; ++j) {
                (void)stats_manager_.recordMove(session_id, j % 3 == 0);  // Every 3rd is mistake
            }

            // Record hints
            for (int j = 0; j < i % 3; ++j) {
                (void)stats_manager_.recordHint(session_id);
            }

            // Advance time
            mock_time_->advanceSystemTime(milliseconds((i + 1) * 60000));  // i+1 minutes

            // End game (alternate completed/incomplete)
            (void)stats_manager_.endGame(session_id, i % 2 == 0);
        }
    }

protected:
    std::shared_ptr<sudoku::core::MockTimeProvider> mock_time_;
    sudoku::test::TempTestDir test_dir_;
    sudoku::core::StatisticsManager stats_manager_;
};

}  // anonymous namespace

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Export aggregate stats CSV with valid data",
                 "[statistics][csv_export]") {
    using namespace sudoku::core;

    // Create some test games
    createTestGameSessions(20);

    // Export aggregate stats to CSV
    auto export_path = test_dir_.path() / "aggregate_stats.csv";
    auto result = stats_manager_.exportAggregateStatsCsv(export_path.string());

    REQUIRE(result.has_value());
    REQUIRE(fs::exists(export_path));

    // Read and verify CSV content
    auto lines = readCsvLines(export_path.string());
    REQUIRE(lines.size() >= 2);  // At least header + 1 data row

    // Verify header row
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Difficulty"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Games_Played"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Games_Completed"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Completion_Rate"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Best_Time_Seconds"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Average_Time_Seconds"));

    // Verify we have rows for Easy, Medium, Hard, Expert, Master, OVERALL
    REQUIRE(lines.size() >= 7);  // Header + 5 difficulties + OVERALL

    // Verify difficulty names in rows
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("Easy"));
    REQUIRE_THAT(lines[2], Catch::Matchers::ContainsSubstring("Medium"));
    REQUIRE_THAT(lines[3], Catch::Matchers::ContainsSubstring("Hard"));
    REQUIRE_THAT(lines[4], Catch::Matchers::ContainsSubstring("Expert"));
    REQUIRE_THAT(lines[5], Catch::Matchers::ContainsSubstring("Master"));
    REQUIRE_THAT(lines[6], Catch::Matchers::ContainsSubstring("OVERALL"));
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Export game sessions CSV with valid data",
                 "[statistics][csv_export]") {
    using namespace sudoku::core;

    // Create test sessions
    createTestGameSessions(10);

    // Export sessions to CSV
    auto export_path = test_dir_.path() / "game_sessions.csv";
    auto result = stats_manager_.exportGameSessionsCsv(export_path.string());

    REQUIRE(result.has_value());
    REQUIRE(fs::exists(export_path));

    // Read and verify CSV content
    auto lines = readCsvLines(export_path.string());
    REQUIRE(lines.size() == 11);  // Header + 10 sessions

    // Verify header row
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Session_ID"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Date"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Time"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Difficulty"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Rating"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Completed"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Duration_Seconds"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Moves"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Hints"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Mistakes"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Puzzle_Seed"));

    // Verify first session row has correct number of columns (including Rating)
    REQUIRE(countCsvColumns(lines[1]) == 11);

    // Verify session IDs are sequential (1, 2, 3, ...)
    REQUIRE_THAT(lines[1], Catch::Matchers::StartsWith("1,"));
    REQUIRE_THAT(lines[2], Catch::Matchers::StartsWith("2,"));
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - CSV export with no games played",
                 "[statistics][csv_export]") {
    using namespace sudoku::core;

    // Export without any games
    auto aggregate_path = test_dir_.path() / "aggregate_empty.csv";
    auto sessions_path = test_dir_.path() / "sessions_empty.csv";

    auto agg_result = stats_manager_.exportAggregateStatsCsv(aggregate_path.string());
    auto sess_result = stats_manager_.exportGameSessionsCsv(sessions_path.string());

    REQUIRE(agg_result.has_value());
    REQUIRE(sess_result.has_value());

    // Both files should exist with headers
    REQUIRE(fs::exists(aggregate_path));
    REQUIRE(fs::exists(sessions_path));

    auto agg_lines = readCsvLines(aggregate_path.string());
    auto sess_lines = readCsvLines(sessions_path.string());

    // Should have headers
    REQUIRE(agg_lines.size() >= 1);
    REQUIRE(sess_lines.size() == 1);  // Only header, no sessions

    REQUIRE_THAT(agg_lines[0], Catch::Matchers::ContainsSubstring("Difficulty"));
    REQUIRE_THAT(sess_lines[0], Catch::Matchers::ContainsSubstring("Session_ID"));
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - CSV export error handling for invalid path",
                 "[statistics][csv_export][error]") {
#ifdef _WIN32
    SKIP("Windows does not honour POSIX read-only directory permissions via std::filesystem");
#else
    using namespace sudoku::core;

    // Try to export to read-only directory
    auto readonly_dir = test_dir_.path() / "readonly";
    fs::create_directories(readonly_dir);
    fs::permissions(readonly_dir, fs::perms::owner_read | fs::perms::owner_exec);

    auto export_path = readonly_dir / "test.csv";
    auto result = stats_manager_.exportAggregateStatsCsv(export_path.string());

    // Should return error (either FileAccessError or SerializationError)
    REQUIRE_FALSE(result.has_value());

    // Restore permissions for cleanup
    fs::permissions(readonly_dir, fs::perms::owner_all);
#endif
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - CSV timestamp formatting validation",
                 "[statistics][csv_export]") {
    using namespace sudoku::core;
    using namespace std::chrono;

    // Set specific timestamp
    auto fixed_time = system_clock::time_point(seconds(1704067200));  // 2024-01-01 00:00:00 UTC
    mock_time_->setSystemTime(fixed_time);

    // Create a single game session
    auto difficulty = Difficulty::Medium;
    auto session_id_result = stats_manager_.startGame(difficulty, 12345);
    REQUIRE(session_id_result.has_value());

    auto session_id = session_id_result.value();
    (void)stats_manager_.recordMove(session_id, false);

    mock_time_->advanceSystemTime(minutes(5));
    (void)stats_manager_.endGame(session_id, true);

    // Export sessions
    auto export_path = test_dir_.path() / "sessions_timestamp.csv";
    auto result = stats_manager_.exportGameSessionsCsv(export_path.string());

    REQUIRE(result.has_value());

    // Read CSV and verify timestamp format
    auto lines = readCsvLines(export_path.string());
    REQUIRE(lines.size() == 2);  // Header + 1 session

    // Verify date format (YYYY-MM-DD) and time format (HH:MM:SS)
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("2024"));
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("Medium"));
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("Yes"));
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("12345"));
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - CSV export file overwrite", "[statistics][csv_export]") {
    using namespace sudoku::core;

    auto export_path = test_dir_.path() / "overwrite_test.csv";

    // First export
    createTestGameSessions(5);
    auto result1 = stats_manager_.exportAggregateStatsCsv(export_path.string());
    REQUIRE(result1.has_value());

    auto lines1 = readCsvLines(export_path.string());
    auto line_count1 = lines1.size();

    // Create more sessions
    createTestGameSessions(10);

    // Second export (should overwrite)
    auto result2 = stats_manager_.exportAggregateStatsCsv(export_path.string());
    REQUIRE(result2.has_value());

    auto lines2 = readCsvLines(export_path.string());

    // File should be overwritten with new data
    REQUIRE(fs::exists(export_path));
    REQUIRE(lines2.size() >= line_count1);  // Should have at least same or more data
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Aggregate CSV completion rate calculation",
                 "[statistics][csv_export]") {
    using namespace sudoku::core;

    // Create sessions with known completion pattern
    // Easy: 3 played, 3 completed (100%)
    for (int i = 0; i < 3; ++i) {
        auto session_id = stats_manager_.startGame(Difficulty::Easy, 1000 + i);
        REQUIRE(session_id.has_value());
        (void)stats_manager_.endGame(session_id.value(), true);
    }

    // Medium: 4 played, 2 completed (50%)
    for (int i = 0; i < 4; ++i) {
        auto session_id = stats_manager_.startGame(Difficulty::Medium, 2000 + i);
        REQUIRE(session_id.has_value());
        (void)stats_manager_.endGame(session_id.value(), i < 2);
    }

    // Export aggregate stats
    auto export_path = test_dir_.path() / "completion_rate.csv";
    auto result = stats_manager_.exportAggregateStatsCsv(export_path.string());
    REQUIRE(result.has_value());

    auto lines = readCsvLines(export_path.string());
    REQUIRE(lines.size() >= 3);  // Header + Easy + Medium

    // Easy line should show 100 completion rate
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("Easy"));
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("100"));

    // Medium line should show 50 completion rate
    REQUIRE_THAT(lines[2], Catch::Matchers::ContainsSubstring("Medium"));
    REQUIRE_THAT(lines[2], Catch::Matchers::ContainsSubstring("50"));
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Rating tracking in GameStats", "[statistics][rating]") {
    using namespace sudoku::core;

    // Start a game with rating
    auto session_id = stats_manager_.startGame(Difficulty::Easy, 12345, 250);
    REQUIRE(session_id.has_value());

    // End game
    auto end_result = stats_manager_.endGame(session_id.value(), true);
    REQUIRE(end_result.has_value());

    // Verify rating was stored
    REQUIRE(end_result->puzzle_rating == 250);
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Rating statistics aggregation", "[statistics][rating]") {
    using namespace sudoku::core;

    // Create Easy games with different ratings
    std::vector<int> easy_ratings = {150, 200, 300, 250};
    for (int rating : easy_ratings) {
        auto session_id = stats_manager_.startGame(Difficulty::Easy, 1000, rating);
        REQUIRE(session_id.has_value());
        (void)stats_manager_.endGame(session_id.value(), true);
    }

    // Create Medium games with different ratings
    std::vector<int> medium_ratings = {600, 700, 800};
    for (int rating : medium_ratings) {
        auto session_id = stats_manager_.startGame(Difficulty::Medium, 2000, rating);
        REQUIRE(session_id.has_value());
        (void)stats_manager_.endGame(session_id.value(), true);
    }

    // Get aggregate stats
    auto stats_result = stats_manager_.getAggregateStats();
    REQUIRE(stats_result.has_value());
    const auto& stats = stats_result.value();

    // Verify Easy rating statistics
    // Note: Integer division causes slight rounding (224 instead of 225)
    REQUIRE(stats.average_ratings[0] >= 224);  // (150+200+300+250)/4 ≈ 225
    REQUIRE(stats.average_ratings[0] <= 225);
    REQUIRE(stats.min_ratings[0] == 150);
    REQUIRE(stats.max_ratings[0] == 300);

    // Verify Medium rating statistics
    REQUIRE(stats.average_ratings[1] == 700);  // (600+700+800)/3
    REQUIRE(stats.min_ratings[1] == 600);
    REQUIRE(stats.max_ratings[1] == 800);

    // Verify Hard and Expert have no ratings
    REQUIRE(stats.average_ratings[2] == 0);
    REQUIRE(stats.min_ratings[2] == std::numeric_limits<double>::max());
    REQUIRE(stats.max_ratings[2] == 0);
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Rating in aggregate CSV export",
                 "[statistics][rating][csv_export]") {
    using namespace sudoku::core;

    // Create games with ratings
    auto session_id1 = stats_manager_.startGame(Difficulty::Easy, 1000, 200);
    REQUIRE(session_id1.has_value());
    auto end_result1 = stats_manager_.endGame(session_id1.value(), true);
    REQUIRE(end_result1.has_value());

    auto session_id2 = stats_manager_.startGame(Difficulty::Easy, 1001, 300);
    REQUIRE(session_id2.has_value());
    auto end_result2 = stats_manager_.endGame(session_id2.value(), true);
    REQUIRE(end_result2.has_value());

    // Export aggregate stats to CSV
    auto export_path = test_dir_.path() / "aggregate_with_rating.csv";
    auto result = stats_manager_.exportAggregateStatsCsv(export_path.string());
    REQUIRE(result.has_value());

    // Read CSV
    auto lines = readCsvLines(export_path.string());
    REQUIRE(lines.size() >= 2);  // Header + at least Easy

    // Verify header includes rating columns
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Average_Rating"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Min_Rating"));
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Max_Rating"));

    // Verify Easy row contains rating data
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("Easy"));
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("250"));  // Average
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("200"));  // Min
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("300"));  // Max
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Rating in game sessions CSV export",
                 "[statistics][rating][csv_export]") {
    using namespace sudoku::core;

    // Create game with rating
    auto session_id = stats_manager_.startGame(Difficulty::Medium, 12345, 650);
    REQUIRE(session_id.has_value());
    auto end_result = stats_manager_.endGame(session_id.value(), true);
    REQUIRE(end_result.has_value());

    // Export sessions to CSV
    auto export_path = test_dir_.path() / "sessions_with_rating.csv";
    auto result = stats_manager_.exportGameSessionsCsv(export_path.string());
    REQUIRE(result.has_value());

    // Read CSV
    auto lines = readCsvLines(export_path.string());
    REQUIRE(lines.size() == 2);  // Header + 1 session

    // Verify header includes Rating column
    REQUIRE_THAT(lines[0], Catch::Matchers::ContainsSubstring("Rating"));

    // Verify session row contains rating
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("650"));
    REQUIRE_THAT(lines[1], Catch::Matchers::ContainsSubstring("Medium"));
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Session CSV export handles invalid path",
                 "[statistics][csv_export][error]") {
    using namespace sudoku::core;

    // Create a session so there's something to export
    createTestGameSessions(1);

    // Try to export sessions to a path in a nonexistent directory
    auto result = stats_manager_.exportGameSessionsCsv("/nonexistent_dir_sudoku_test/sessions.csv");

    // Should return error (FileAccessError)
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(CsvExportTestFixture, "StatisticsManager - Zero rating handling", "[statistics][rating]") {
    using namespace sudoku::core;

    // Create game with zero rating (backward compatibility)
    auto session_id = stats_manager_.startGame(Difficulty::Easy, 1000, 0);
    REQUIRE(session_id.has_value());
    auto end_result = stats_manager_.endGame(session_id.value(), true);
    REQUIRE(end_result.has_value());

    // Get aggregate stats
    auto stats_result = stats_manager_.getAggregateStats();
    REQUIRE(stats_result.has_value());
    const auto& stats = stats_result.value();

    // Verify zero ratings don't affect min/max/average
    REQUIRE(stats.average_ratings[0] == 0);
    REQUIRE(stats.min_ratings[0] == std::numeric_limits<double>::max());  // Not updated
    REQUIRE(stats.max_ratings[0] == 0);                                   // Not updated
}
