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

/// Targeted branch-coverage tests for statistics_manager.cpp YAML serialization:
/// - deserializeStatsFromYaml: missing "difficulties" section (line 677 false)
/// - deserializeStatsFromYaml: missing "overall" section (line 713 false)
/// - deserializeStatsFromYaml: missing specific difficulty nodes (line 684 false)
/// - deserializeStatsFromYaml: missing individual difficulty fields (lines 687-707 false)
/// - deserializeStatsFromYaml: missing individual overall fields (lines 716-738 false)
/// - deserializeGameStatsFromYaml: empty sessions file (line 813 loop not entered)
/// - deserializeGameStatsFromYaml: session with missing fields (lines 816-846 false)
/// - serializeGameStatsToYaml: append=true with existing file (line 762 path)

#include "../../src/core/i_time_provider.h"
#include "../../src/core/statistics_manager.h"
#include "../helpers/test_utils.h"

#include <filesystem>
#include <fstream>
#include <random>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

namespace {

// Write content to a file
void writeFile(const fs::path& path, const std::string& content) {
    std::ofstream ofs(path);
    ofs << content;
}

}  // namespace

// ============================================================================
// deserializeStatsFromYaml via importStats
// ============================================================================

TEST_CASE("StatisticsManager YAML - missing difficulties section", "[statistics_yaml]") {
    // ARRANGE: YAML with only "overall" section, no "difficulties"
    // This covers line 677 false branch (root["difficulties"] is absent)
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    fs::path yaml_path = tmp.path() / "partial_overall_only.yaml";
    writeFile(yaml_path, R"(
overall:
  total_games: 5
  total_completed: 3
  total_moves: 100
  total_hints: 5
  total_mistakes: 2
  total_time_played: 180000
  current_win_streak: 1
  best_win_streak: 3
)");

    // ACT
    auto result = mgr.importStats(yaml_path.string());

    // ASSERT
    REQUIRE(result.has_value());
    auto stats = mgr.getAggregateStats();
    REQUIRE(stats.has_value());
    // Imported overall values were merged into cached_stats_
    REQUIRE(stats->total_games >= 5);
    REQUIRE(stats->total_completed >= 3);
    REQUIRE(stats->best_win_streak >= 3);
}

TEST_CASE("StatisticsManager YAML - missing overall section", "[statistics_yaml]") {
    // ARRANGE: YAML with only "difficulties" section, no "overall"
    // This covers line 713 false branch (root["overall"] is absent)
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    fs::path yaml_path = tmp.path() / "partial_difficulties_only.yaml";
    writeFile(yaml_path, R"(
difficulties:
  easy:
    games_played: 4
    games_completed: 3
    best_time: 45000
    average_time: 60000
    average_rating: 120
    min_rating: 80
    max_rating: 200
)");

    // ACT
    auto result = mgr.importStats(yaml_path.string());

    // ASSERT
    REQUIRE(result.has_value());
    auto stats = mgr.getAggregateStats();
    REQUIRE(stats.has_value());
    REQUIRE(stats->games_played[0] >= 4);  // easy
    REQUIRE(stats->games_completed[0] >= 3);
}

TEST_CASE("StatisticsManager YAML - missing specific difficulty nodes", "[statistics_yaml]") {
    // ARRANGE: YAML has "difficulties" but only "easy" — medium/hard/expert absent
    // Covers line 684 false branch for i=1,2,3 (medium, hard, expert not present)
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    fs::path yaml_path = tmp.path() / "partial_only_easy.yaml";
    writeFile(yaml_path, R"(
difficulties:
  easy:
    games_played: 2
    games_completed: 1
    best_time: 30000
    average_time: 40000
    average_rating: 100
    min_rating: 90
    max_rating: 110
overall:
  total_games: 2
  total_completed: 1
  total_moves: 50
  total_hints: 0
  total_mistakes: 0
  total_time_played: 40000
  current_win_streak: 1
  best_win_streak: 1
)");

    // ACT
    auto result = mgr.importStats(yaml_path.string());

    // ASSERT
    REQUIRE(result.has_value());
    auto stats = mgr.getAggregateStats();
    REQUIRE(stats.has_value());
    REQUIRE(stats->games_played[0] >= 2);  // easy present
    // medium/hard/expert default to 0 (no merge)
    REQUIRE(stats->games_played[1] == 0);
    REQUIRE(stats->games_played[2] == 0);
    REQUIRE(stats->games_played[3] == 0);
}

TEST_CASE("StatisticsManager YAML - partial difficulty fields", "[statistics_yaml]") {
    // ARRANGE: difficulties.easy has only some fields (missing games_completed,
    // average_time, average_rating, min_rating, max_rating)
    // Covers false branches on lines 690, 696, 699, 702, 705
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    fs::path yaml_path = tmp.path() / "partial_diff_fields.yaml";
    writeFile(yaml_path, R"(
difficulties:
  easy:
    games_played: 3
    best_time: 25000
)");

    // ACT
    auto result = mgr.importStats(yaml_path.string());

    // ASSERT
    REQUIRE(result.has_value());
    auto stats = mgr.getAggregateStats();
    REQUIRE(stats.has_value());
    REQUIRE(stats->games_played[0] >= 3);
    REQUIRE(stats->games_completed[0] == 0);  // missing field → default
    REQUIRE(stats->average_ratings[0] == 0);  // missing field → default
}

TEST_CASE("StatisticsManager YAML - partial overall fields", "[statistics_yaml]") {
    // ARRANGE: overall section has only total_games and best_win_streak
    // Covers false branches on lines 719, 722, 725, 728, 731, 734
    // (total_completed, total_moves, total_hints, total_mistakes, total_time_played,
    //  current_win_streak all absent)
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    fs::path yaml_path = tmp.path() / "partial_overall_fields.yaml";
    writeFile(yaml_path, R"(
overall:
  total_games: 7
  best_win_streak: 4
)");

    // ACT
    auto result = mgr.importStats(yaml_path.string());

    // ASSERT
    REQUIRE(result.has_value());
    auto stats = mgr.getAggregateStats();
    REQUIRE(stats.has_value());
    REQUIRE(stats->total_games >= 7);
    REQUIRE(stats->best_win_streak >= 4);
    REQUIRE(stats->total_moves == 0);  // missing field → default
    REQUIRE(stats->total_hints == 0);  // missing field → default
}

// ============================================================================
// loadStatistics: corrupted aggregate_stats.yaml → warn + recalculate (lines 549-551)
// ============================================================================

TEST_CASE("StatisticsManager YAML - corrupted aggregate stats file triggers recalculation", "[statistics_yaml]") {
    // ARRANGE: Write invalid YAML to aggregate_stats.yaml before constructing manager
    // Constructor calls loadStatistics() which tries deserializeStatsFromYaml().
    // On failure it warns and calls recalculateAggregateStats() (covers lines 549-551).
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // Write malformed YAML so deserialization fails
    writeFile(tmp.path() / "aggregate_stats.yaml", "invalid: yaml: {{{broken\n");

    // ACT: constructor triggers loadStatistics → deserialization fails → recalculate
    StatisticsManager mgr(tmp.path().string(), time);

    // ASSERT: recalculate from empty sessions → 0 games
    auto result = mgr.getAggregateStats();
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 0);
}

// ============================================================================
// deserializeGameStatsFromYaml via getRecentGames
// ============================================================================

TEST_CASE("StatisticsManager YAML - empty sessions file", "[statistics_yaml]") {
    // ARRANGE: sessions file is an empty YAML sequence
    // Covers line 813: loop body never entered (empty range)
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Write empty sessions file
    writeFile(tmp.path() / "game_sessions.yaml", "[]\n");

    // ACT
    auto result = mgr.getAllSessions();

    // ASSERT
    REQUIRE(result.has_value());
    REQUIRE(result->empty());
}

TEST_CASE("StatisticsManager YAML - session with missing fields", "[statistics_yaml]") {
    // ARRANGE: sessions YAML has one entry with only 'difficulty' and 'completed'
    // Covers false branches on lines 819, 822, 826, 830, 836, 839, 842, 845
    // (puzzle_rating, start_time, end_time, time_played, moves_made,
    //  hints_used, mistakes, puzzle_seed all absent)
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    writeFile(tmp.path() / "game_sessions.yaml", R"(
- difficulty: 0
  completed: true
)");

    // ACT
    auto result = mgr.getAllSessions();

    // ASSERT
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    const auto& session = (*result)[0];
    REQUIRE(session.difficulty == Difficulty::Easy);
    REQUIRE(session.completed);
    REQUIRE(session.puzzle_rating == 0);  // missing → default
    REQUIRE(session.moves_made == 0);     // missing → default
    REQUIRE(session.hints_used == 0);     // missing → default
    REQUIRE(session.mistakes == 0);       // missing → default
    REQUIRE(session.puzzle_seed == 0U);   // missing → default
}

// ============================================================================
// serializeGameStatsToYaml: append=true with existing file (line 762)
// ============================================================================

TEST_CASE("StatisticsManager YAML - append mode saves two sessions", "[statistics_yaml]") {
    // ARRANGE
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);
    mgr.setEncryptSessions(false);  // Plain YAML to test append path

    // ACT: play first game, flush to create sessions file
    auto id1 = mgr.startGame(Difficulty::Easy, 1, 100);
    REQUIRE(id1.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(10000));
    auto end1 = mgr.endGame(*id1, true);
    REQUIRE(end1.has_value());

    // Force flush so the file exists on disk for the append path
    mgr.flushSessions();

    // Verify sessions file was created
    REQUIRE(fs::exists(tmp.path() / "game_sessions.yaml"));

    // ACT: play second game — this triggers the append path
    auto id2 = mgr.startGame(Difficulty::Medium, 2, 200);
    REQUIRE(id2.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(20000));
    auto end2 = mgr.endGame(*id2, false);
    REQUIRE(end2.has_value());

    // ASSERT: both sessions should be present
    auto recent = mgr.getAllSessions();
    REQUIRE(recent.has_value());
    REQUIRE(recent->size() == 2);
}

TEST_CASE("StatisticsManager YAML - corrupted sessions file on append is handled gracefully", "[statistics_yaml]") {
    // ARRANGE: write invalid YAML to sessions file, then play a game (append=true)
    // This covers the catch (const YAML::Exception&) at lines 763-765 in
    // serializeGameStatsToYaml: the LoadFile fails, so we start with a fresh sequence
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);
    mgr.setEncryptSessions(false);  // Plain YAML to test append path

    // Write corrupted sessions file so the append-mode YAML load fails
    writeFile(tmp.path() / "game_sessions.yaml", "invalid: yaml: {{{broken\n");

    // ACT: play a game — endGame calls serializeGameStatsToYaml with append=true
    auto id = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(10000));
    auto end_result = mgr.endGame(*id, true);

    // ASSERT: game saved successfully (append recovered from corrupted file)
    REQUIRE(end_result.has_value());
}

TEST_CASE("StatisticsManager YAML - getRecentGames with many sessions exercises sort", "[statistics_yaml]") {
    // ARRANGE: play 5 games so getRecentGames sort comparator is called multiple times
    // This exercises the sort lambda at line 210 with more iterations
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);

    for (int i = 0; i < 5; ++i) {
        auto id = mgr.startGame(static_cast<Difficulty>(i % 4), static_cast<uint32_t>(i + 1), 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(10000 * (i + 1)));
        REQUIRE(mgr.endGame(*id, i % 2 == 0).has_value());
    }

    // ACT
    auto result = mgr.getAllSessions();

    // ASSERT: all 5 sessions returned and sorted newest-first
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 5);
    for (size_t i = 0; i + 1 < result->size(); ++i) {
        REQUIRE((*result)[i].start_time >= (*result)[i + 1].start_time);
    }
}

TEST_CASE("StatisticsManager YAML - getRecentGames with count limit trims results", "[statistics_yaml]") {
    // Covers the count limit branch (line 213: count >= 0 && count < sessions.size())
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);

    // Play 4 games
    for (int i = 0; i < 4; ++i) {
        auto id = mgr.startGame(Difficulty::Easy, static_cast<uint32_t>(i + 1), 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(10000));
        REQUIRE(mgr.endGame(*id, true).has_value());
    }

    // Request only 2 recent games
    auto result = mgr.getRecentGames(2);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
}
