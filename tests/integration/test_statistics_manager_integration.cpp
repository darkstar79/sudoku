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

#include <chrono>
#include <filesystem>
#include <memory>
#include <tuple>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
namespace fs = std::filesystem;

// Test fixture for StatisticsManager integration tests
class StatisticsManagerTestFixture {
public:
    StatisticsManagerTestFixture()
        : mock_time_(std::make_shared<MockTimeProvider>()),
          test_dir_("./test_stats_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())),
          stats_manager_(test_dir_, mock_time_) {
        // Create test directory
        fs::create_directories(test_dir_);
        stats_manager_.setCollectDetailedStats(true);
    }

    ~StatisticsManagerTestFixture() {
        // Flush before removing the directory (destructor body runs before member destruction)
        stats_manager_.flushSessions();
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    std::shared_ptr<MockTimeProvider> mock_time_;
    std::string test_dir_;
    StatisticsManager stats_manager_;
};

TEST_CASE("StatisticsManager - Game session tracking", "[statistics_manager][integration]") {
    StatisticsManagerTestFixture fixture;

    SECTION("Start and end game session") {
        auto start_result = fixture.stats_manager_.startGame(Difficulty::Medium, 12345);
        REQUIRE(start_result.has_value());

        uint64_t game_id = *start_result;
        REQUIRE(game_id > 0);

        // Record some moves
        auto move_result1 = fixture.stats_manager_.recordMove(game_id, false);
        REQUIRE(move_result1.has_value());

        auto move_result2 = fixture.stats_manager_.recordMove(game_id, false);
        REQUIRE(move_result2.has_value());

        auto move_result3 = fixture.stats_manager_.recordMove(game_id, true);  // mistake
        REQUIRE(move_result3.has_value());

        // Advance time to simulate game duration
        fixture.mock_time_->advanceSystemTime(std::chrono::milliseconds(100));

        // End game
        auto end_result = fixture.stats_manager_.endGame(game_id, true);
        REQUIRE(end_result.has_value());

        auto& game_stats = *end_result;
        REQUIRE(game_stats.difficulty == Difficulty::Medium);
        REQUIRE(game_stats.moves_made == 3);
        REQUIRE(game_stats.mistakes == 1);
        REQUIRE(game_stats.completed);
        REQUIRE(game_stats.time_played.count() == 100);  // Exact assertion
    }

    SECTION("Multiple game sessions") {
        // Start three games
        auto game1 = fixture.stats_manager_.startGame(Difficulty::Easy, 1111);
        auto game2 = fixture.stats_manager_.startGame(Difficulty::Medium, 2222);
        auto game3 = fixture.stats_manager_.startGame(Difficulty::Hard, 3333);

        REQUIRE(game1.has_value());
        REQUIRE(game2.has_value());
        REQUIRE(game3.has_value());

        // Each should have unique ID
        REQUIRE(*game1 != *game2);
        REQUIRE(*game2 != *game3);
        REQUIRE(*game1 != *game3);

        // Record moves for each
        std::ignore = fixture.stats_manager_.recordMove(*game1, false);
        std::ignore = fixture.stats_manager_.recordMove(*game2, false);
        std::ignore = fixture.stats_manager_.recordMove(*game3, true);

        // End all games
        auto end1 = fixture.stats_manager_.endGame(*game1, true);
        auto end2 = fixture.stats_manager_.endGame(*game2, false);
        auto end3 = fixture.stats_manager_.endGame(*game3, true);

        REQUIRE(end1.has_value());
        REQUIRE(end2.has_value());
        REQUIRE(end3.has_value());

        REQUIRE(end1->completed);
        REQUIRE_FALSE(end2->completed);
        REQUIRE(end3->completed);
    }

    SECTION("Record hints") {
        auto game_id = fixture.stats_manager_.startGame(Difficulty::Medium, 5555);
        REQUIRE(game_id.has_value());

        // Record multiple hints
        auto hint1 = fixture.stats_manager_.recordHint(*game_id);
        REQUIRE(hint1.has_value());

        auto hint2 = fixture.stats_manager_.recordHint(*game_id);
        REQUIRE(hint2.has_value());

        auto hint3 = fixture.stats_manager_.recordHint(*game_id);
        REQUIRE(hint3.has_value());

        // End game and verify hint count
        auto end_result = fixture.stats_manager_.endGame(*game_id, true);
        REQUIRE(end_result.has_value());
        REQUIRE(end_result->hints_used == 3);
    }

    SECTION("Get aggregate statistics") {
        // Complete multiple games
        for (int i = 0; i < 5; ++i) {
            auto game_id = fixture.stats_manager_.startGame(Difficulty::Easy, 1000 + i);
            REQUIRE(game_id.has_value());

            for (int j = 0; j < (i + 1) * 10; ++j) {
                std::ignore = fixture.stats_manager_.recordMove(*game_id, false);
            }

            // Advance time to simulate game duration
            fixture.mock_time_->advanceSystemTime(std::chrono::milliseconds(10));
            std::ignore = fixture.stats_manager_.endGame(*game_id, true);
        }

        // Get aggregate stats
        auto stats_result = fixture.stats_manager_.getAggregateStats();
        REQUIRE(stats_result.has_value());

        auto& stats = *stats_result;
        REQUIRE(stats.total_games >= 5);
        REQUIRE(stats.total_completed >= 5);
        // Difficulty enum is 0-based (Easy=0), matches array indexing
        REQUIRE(stats.games_completed[static_cast<int>(Difficulty::Easy)] >= 5);
    }

    SECTION("Get statistics for specific difficulty") {
        // Complete games at different difficulties
        auto easy_game = fixture.stats_manager_.startGame(Difficulty::Easy, 1001);
        std::ignore = fixture.stats_manager_.endGame(*easy_game, true);

        auto medium_game = fixture.stats_manager_.startGame(Difficulty::Medium, 1002);
        std::ignore = fixture.stats_manager_.endGame(*medium_game, true);

        auto hard_game = fixture.stats_manager_.startGame(Difficulty::Hard, 1003);
        std::ignore = fixture.stats_manager_.endGame(*hard_game, false);

        // Get difficulty-specific stats
        auto easy_stats = fixture.stats_manager_.getStatsForDifficulty(Difficulty::Easy);
        REQUIRE(easy_stats.has_value());
        // Difficulty enum is 0-based (Easy=0), matches array indexing
        REQUIRE(easy_stats->games_completed[static_cast<int>(Difficulty::Easy)] >= 1);

        auto medium_stats = fixture.stats_manager_.getStatsForDifficulty(Difficulty::Medium);
        REQUIRE(medium_stats.has_value());
        // Difficulty enum is 0-based (Medium=1), matches array indexing
        REQUIRE(medium_stats->games_completed[static_cast<int>(Difficulty::Medium)] >= 1);

        auto hard_stats = fixture.stats_manager_.getStatsForDifficulty(Difficulty::Hard);
        REQUIRE(hard_stats.has_value());
        // Difficulty enum is 0-based (Hard=2), matches array indexing
        REQUIRE(hard_stats->games_played[static_cast<int>(Difficulty::Hard)] >= 1);
    }

    SECTION("Get recent games") {
        // Create multiple games
        for (int i = 0; i < 10; ++i) {
            auto game_id = fixture.stats_manager_.startGame(Difficulty::Medium, 2000 + i);
            REQUIRE(game_id.has_value());
            std::ignore = fixture.stats_manager_.endGame(*game_id, i % 2 == 0);
            // Advance time to ensure distinct timestamps
            fixture.mock_time_->advanceSystemTime(std::chrono::milliseconds(10));
        }

        // Get recent games (limited)
        auto recent_result = fixture.stats_manager_.getRecentGames(5);
        REQUIRE(recent_result.has_value());
        REQUIRE(recent_result->size() <= 5);

        // Get all sessions (unlimited)
        auto all_result = fixture.stats_manager_.getAllSessions();
        REQUIRE(all_result.has_value());
        REQUIRE(all_result->size() == 10);

        // Verify they're sorted by most recent first
        if (all_result->size() > 1) {
            for (size_t i = 1; i < all_result->size(); ++i) {
                REQUIRE((*all_result)[i - 1].end_time >= (*all_result)[i].end_time);
            }
        }
    }

    SECTION("Get best times") {
        // Complete games with different times
        for (int i = 0; i < 3; ++i) {
            auto game_id = fixture.stats_manager_.startGame(Difficulty::Easy, 3000 + i);
            REQUIRE(game_id.has_value());

            // Advance time to simulate different game durations
            fixture.mock_time_->advanceSystemTime(std::chrono::milliseconds(50 * (i + 1)));
            std::ignore = fixture.stats_manager_.endGame(*game_id, true);
        }

        auto best_times = fixture.stats_manager_.getBestTimes();

        // Verify we have times recorded
        // Difficulty enum is 0-based (Easy=0), matches array indexing
        REQUIRE(best_times[static_cast<int>(Difficulty::Easy)].count() > 0);
    }

    SECTION("Get completion rates") {
        // Complete some games, abandon others
        for (int i = 0; i < 6; ++i) {
            auto game_id = fixture.stats_manager_.startGame(Difficulty::Medium, 4000 + i);
            REQUIRE(game_id.has_value());

            // Complete 2/3 of games
            bool complete = (i % 3 != 0);
            std::ignore = fixture.stats_manager_.endGame(*game_id, complete);
        }

        auto completion_rates = fixture.stats_manager_.getCompletionRates();

        // Medium difficulty should have completion rate of ~66%
        // Difficulty enum is 0-based (Medium=1), matches array indexing
        REQUIRE(completion_rates[static_cast<int>(Difficulty::Medium)] > 0.0);
        REQUIRE(completion_rates[static_cast<int>(Difficulty::Medium)] <= 1.0);
    }

    SECTION("Export and import statistics") {
        // Create some game data
        for (int i = 0; i < 3; ++i) {
            auto game_id = fixture.stats_manager_.startGame(Difficulty::Easy, 5000 + i);
            REQUIRE(game_id.has_value());
            std::ignore = fixture.stats_manager_.recordMove(*game_id, false);
            std::ignore = fixture.stats_manager_.endGame(*game_id, true);
        }

        std::string export_path = fixture.test_dir_ + "/stats_export.yaml";

        // Export statistics
        auto export_result = fixture.stats_manager_.exportStats(export_path);
        REQUIRE(export_result.has_value());
        REQUIRE(fs::exists(export_path));

        // Get stats before import
        auto stats_before = fixture.stats_manager_.getAggregateStats();
        REQUIRE(stats_before.has_value());

        // Import statistics (should merge with existing)
        auto import_result = fixture.stats_manager_.importStats(export_path);
        REQUIRE(import_result.has_value());
    }

    SECTION("Reset all statistics") {
        // Create some games
        for (int i = 0; i < 5; ++i) {
            auto game_id = fixture.stats_manager_.startGame(Difficulty::Easy, 6000 + i);
            REQUIRE(game_id.has_value());
            std::ignore = fixture.stats_manager_.endGame(*game_id, true);
        }

        // Verify stats exist
        auto stats_before = fixture.stats_manager_.getAggregateStats();
        REQUIRE(stats_before.has_value());
        REQUIRE(stats_before->total_games > 0);

        // Reset statistics
        fixture.stats_manager_.resetAllStats();

        // Verify stats are cleared
        auto stats_after = fixture.stats_manager_.getAggregateStats();
        REQUIRE(stats_after.has_value());
        REQUIRE(stats_after->total_games == 0);
        REQUIRE(stats_after->total_completed == 0);
    }
}

TEST_CASE("StatisticsManager - Error handling", "[statistics_manager][integration]") {
    StatisticsManagerTestFixture fixture;

    SECTION("Record move for invalid game ID") {
        auto result = fixture.stats_manager_.recordMove(99999, false);
        REQUIRE(!result.has_value());
        REQUIRE(result.error() == StatisticsError::GameNotStarted);
    }

    SECTION("End game that was never started") {
        auto result = fixture.stats_manager_.endGame(99999, true);
        REQUIRE(!result.has_value());
        REQUIRE(result.error() == StatisticsError::GameNotStarted);
    }

    SECTION("Record hint for invalid game ID") {
        auto result = fixture.stats_manager_.recordHint(99999);
        REQUIRE(!result.has_value());
        REQUIRE(result.error() == StatisticsError::GameNotStarted);
    }

    SECTION("End game twice") {
        auto game_id = fixture.stats_manager_.startGame(Difficulty::Easy, 7777);
        REQUIRE(game_id.has_value());

        // End game first time - should succeed
        auto end1 = fixture.stats_manager_.endGame(*game_id, true);
        REQUIRE(end1.has_value());

        // End game second time - should fail
        auto end2 = fixture.stats_manager_.endGame(*game_id, true);
        REQUIRE(!end2.has_value());
        REQUIRE(end2.error() == StatisticsError::GameNotStarted);
    }

    SECTION("Import non-existent file") {
        auto result = fixture.stats_manager_.importStats("/nonexistent/path.yaml");
        REQUIRE(!result.has_value());
        REQUIRE(result.error() == StatisticsError::FileAccessError);
    }

    SECTION("Export to invalid path") {
        auto result = fixture.stats_manager_.exportStats("/invalid/path/that/does/not/exist/stats.yaml");
        REQUIRE(!result.has_value());
        REQUIRE(result.error() == StatisticsError::FileAccessError);
    }
}

TEST_CASE("StatisticsManager - Win streak tracking", "[statistics_manager][integration]") {
    StatisticsManagerTestFixture fixture;

    SECTION("Build and break win streak") {
        // Win 3 games in a row
        for (int i = 0; i < 3; ++i) {
            auto game_id = fixture.stats_manager_.startGame(Difficulty::Medium, 8000 + i);
            REQUIRE(game_id.has_value());
            std::ignore = fixture.stats_manager_.endGame(*game_id, true);
        }

        auto stats1 = fixture.stats_manager_.getAggregateStats();
        REQUIRE(stats1.has_value());
        REQUIRE(stats1->current_win_streak >= 3);

        // Lose a game (break streak)
        auto lose_game = fixture.stats_manager_.startGame(Difficulty::Medium, 8888);
        REQUIRE(lose_game.has_value());
        std::ignore = fixture.stats_manager_.endGame(*lose_game, false);

        auto stats2 = fixture.stats_manager_.getAggregateStats();
        REQUIRE(stats2.has_value());
        REQUIRE(stats2->current_win_streak == 0);

        // Best streak should still be at least 3
        REQUIRE(stats2->best_win_streak >= 3);
    }
}