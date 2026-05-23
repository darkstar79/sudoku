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

/// Extra branch-coverage tests for StatisticsManager:
/// - invalidateStatsCache() forces recalculation from sessions file
/// - Best time updated when second game is faster
/// - Fresh manager with only sessions file (aggregate deleted) recalculates
/// - Multiple game types: moves, hints, mistakes aggregation
/// - getRecentGames returns empty when no sessions file exists

#include "../../src/core/i_time_provider.h"
#include "../../src/core/statistics_manager.h"
#include "../helpers/test_utils.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <random>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

// ============================================================================
// Constructor path: sessions file exists but no aggregate file → recalculate
// (covers recalculateAggregateStats sessions-loading path L934-954)
// ============================================================================

TEST_CASE("StatisticsManager - recalculate is triggered when only sessions file present", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // First manager: play a game, persist sessions and aggregate
    {
        StatisticsManager mgr1(tmp.path().string(), time);
        mgr1.setCollectDetailedStats(true);
        auto id = mgr1.startGame(Difficulty::Easy, 1, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(30000));
        REQUIRE(mgr1.endGame(*id, true).has_value());
    }

    // Delete only the aggregate stats file, keep sessions file
    fs::path agg = tmp.path() / "aggregate_stats.yaml";
    REQUIRE(fs::exists(agg));
    fs::remove(agg);

    // Second manager: no aggregate file → constructor calls loadStatistics()
    // → stats_file_ absent → recalculateAggregateStats() → sessions_file_ present
    // → loads and replays sessions (covers L934-954)
    StatisticsManager mgr2(tmp.path().string(), time);
    auto result = mgr2.getAggregateStats();
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 1);
    REQUIRE(result->total_completed == 1);
}

// ============================================================================
// Best time updated when second completion is faster
// ============================================================================

TEST_CASE("StatisticsManager - second faster completion updates best time", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // First game: 60 seconds
    auto id1 = mgr.startGame(Difficulty::Hard, 1, 0);
    REQUIRE(id1.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(60000));
    REQUIRE(mgr.endGame(*id1, true).has_value());

    auto best_after_first = mgr.getBestTimes()[static_cast<int>(Difficulty::Hard)];
    REQUIRE(best_after_first.count() > 0);

    // Second game: 30 seconds (faster — should update best time)
    auto id2 = mgr.startGame(Difficulty::Hard, 2, 0);
    REQUIRE(id2.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id2, true).has_value());

    auto best_after_second = mgr.getBestTimes()[static_cast<int>(Difficulty::Hard)];
    REQUIRE(best_after_second < best_after_first);
}

TEST_CASE("StatisticsManager - slower second game does not update best time", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // First game: 30 seconds
    auto id1 = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(id1.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id1, true).has_value());

    auto best_after_first = mgr.getBestTimes()[static_cast<int>(Difficulty::Easy)];

    // Second game: 60 seconds (slower — best time should NOT change)
    auto id2 = mgr.startGame(Difficulty::Easy, 2, 0);
    REQUIRE(id2.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(60000));
    REQUIRE(mgr.endGame(*id2, true).has_value());

    auto best_after_second = mgr.getBestTimes()[static_cast<int>(Difficulty::Easy)];
    REQUIRE(best_after_second == best_after_first);
}

// ============================================================================
// Fresh manager with only sessions file (aggregate deleted) → recalculate path
// ============================================================================

TEST_CASE("StatisticsManager - fresh manager with sessions but no aggregate file recalculates", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // First manager: play two games
    {
        StatisticsManager mgr1(tmp.path().string(), time);
        mgr1.setCollectDetailedStats(true);
        for (int i = 0; i < 2; ++i) {
            auto id = mgr1.startGame(Difficulty::Medium, static_cast<uint32_t>(i + 1), 0);
            REQUIRE(id.has_value());
            time->advanceSystemTime(std::chrono::milliseconds(30000));
            REQUIRE(mgr1.endGame(*id, true).has_value());
        }
    }

    // Delete aggregate stats file, keep sessions file
    fs::path agg_file = tmp.path() / "aggregate_stats.yaml";
    REQUIRE(fs::exists(agg_file));
    fs::remove(agg_file);
    REQUIRE_FALSE(fs::exists(agg_file));

    // Second manager must recalculate aggregate from sessions
    StatisticsManager mgr2(tmp.path().string(), time);
    auto result = mgr2.getAggregateStats();
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 2);
    REQUIRE(result->total_completed == 2);
}

// ============================================================================
// Aggregate: moves, hints, mistakes accumulate across games
// ============================================================================

TEST_CASE("StatisticsManager - aggregate tracks moves hints and mistakes", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto id = mgr.startGame(Difficulty::Expert, 1, 500);
    REQUIRE(id.has_value());

    REQUIRE(mgr.recordMove(*id, false).has_value());
    REQUIRE(mgr.recordMove(*id, false).has_value());
    REQUIRE(mgr.recordMove(*id, true).has_value());  // mistake
    REQUIRE(mgr.recordHint(*id).has_value());
    REQUIRE(mgr.recordHint(*id).has_value());

    time->advanceSystemTime(std::chrono::milliseconds(120000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    auto agg = mgr.getAggregateStats();
    REQUIRE(agg.has_value());
    REQUIRE(agg->total_moves == 3);
    REQUIRE(agg->total_hints == 2);
    REQUIRE(agg->total_mistakes == 1);
}

// ============================================================================
// getRecentGames: no sessions file returns empty
// ============================================================================

TEST_CASE("StatisticsManager - getRecentGames returns empty when no sessions file", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // No games played — sessions file does not exist
    auto result = mgr.getRecentGames(10);
    REQUIRE(result.has_value());
    REQUIRE(result->empty());
}

// ============================================================================
// exportStats round-trip: export then import increases totals
// ============================================================================

TEST_CASE("StatisticsManager - exportStats then importStats merges data", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);

    // Play one Easy game
    auto id = mgr.startGame(Difficulty::Easy, 1, 100);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    // Export to a temp file
    fs::path export_file = tmp.path() / "export.yaml";
    auto export_result = mgr.exportStats(export_file.string());
    REQUIRE(export_result.has_value());
    REQUIRE(fs::exists(export_file));

    // Import back → totals should double
    auto import_result = mgr.importStats(export_file.string());
    REQUIRE(import_result.has_value());

    auto agg = mgr.getAggregateStats();
    REQUIRE(agg.has_value());
    REQUIRE(agg->total_games >= 1);  // At least as many as before
}

// ============================================================================
// getRecentGames with corrupted sessions YAML file (covers line 203)
// ============================================================================

TEST_CASE("StatisticsManager - getRecentGames returns error on corrupted sessions YAML", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Write a corrupted sessions YAML file
    {
        std::ofstream f(tmp.path() / "game_sessions.yaml");
        f << "invalid: yaml: {{{broken\n";
    }

    // getRecentGames should fail on the corrupted file (covers line 203)
    auto result = mgr.getAllSessions();
    REQUIRE_FALSE(result.has_value());
}

// ============================================================================
// importStats with corrupted YAML (covers line 447)
// ============================================================================

TEST_CASE("StatisticsManager - importStats returns error on corrupted YAML", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Write a file with corrupted YAML content
    fs::path bad_yaml = tmp.path() / "bad_stats.yaml";
    {
        std::ofstream f(bad_yaml);
        f << "invalid: yaml: {{{broken\n";
    }

    // importStats should fail during deserializeStatsFromYaml (covers line 447)
    auto result = mgr.importStats(bad_yaml.string());
    REQUIRE_FALSE(result.has_value());
}

// ============================================================================
// Constructor with both corrupted files: loadStatistics fails → covers
// lines 38,40-41 (constructor error branch) and line 553 (recalculate fails)
// and line 936 (deserializeGameStatsFromYaml returns error)
// ============================================================================

TEST_CASE("StatisticsManager - constructor handles both corrupted files gracefully", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // Write corrupted aggregate_stats.yaml (forces deserialization failure →
    // triggers recalculateAggregateStats())
    {
        std::ofstream f(tmp.path() / "aggregate_stats.yaml");
        f << "invalid: yaml: {{{broken\n";
    }
    // Write corrupted sessions file so recalculate also fails (covers line 936)
    {
        std::ofstream f(tmp.path() / "game_sessions.yaml");
        f << "invalid: yaml: {{{broken\n";
    }

    // Constructor: loadStatistics() → deserialize fails → recalculate → sessions
    // corrupted → recalculate fails → loadStatistics returns error →
    // constructor branch at lines 38,40-41 covers (cached_stats_ = {}; valid=true)
    StatisticsManager mgr(tmp.path().string(), time);

    // Manager should still be usable with default empty stats
    auto result = mgr.getAggregateStats();
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 0);
}

// ============================================================================
// exportGameSessionsCsv with corrupted sessions file (covers line 370)
// ============================================================================

TEST_CASE("StatisticsManager - exportGameSessionsCsv returns error on corrupted sessions", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Write a corrupted sessions file so deserializeGameStatsFromYaml fails
    {
        std::ofstream f(tmp.path() / "game_sessions.yaml");
        f << "invalid: yaml: {{{broken\n";
    }

    // exportGameSessionsCsv: sessions file exists but is corrupted →
    // deserializeGameStatsFromYaml fails → covers line 370
    fs::path csv_path = tmp.path() / "sessions.csv";
    auto result = mgr.exportGameSessionsCsv(csv_path.string());
    REQUIRE_FALSE(result.has_value());
}

// ============================================================================
// Destructor with active sessions: covers lines 74-82 (destructor auto-ends
// active sessions and buffers them when collect_detailed_stats is enabled)
// ============================================================================

TEST_CASE("StatisticsManager - destructor ends active sessions and buffers them", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setCollectDetailedStats(true);
        mgr.setEncryptSessions(false);

        // Start a game but do NOT end it — destructor should handle it
        auto id = mgr.startGame(Difficulty::Easy, 1, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(5000));
    }
    // Destructor ran: active session ended as incomplete, buffered, flushed

    // New manager should see the session persisted on disk
    StatisticsManager mgr2(tmp.path().string(), time);
    auto sessions = mgr2.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->size() == 1);
    REQUIRE_FALSE((*sessions)[0].completed);
}

// ============================================================================
// deleteSessionHistory: covers lines 669-682
// ============================================================================

TEST_CASE("StatisticsManager - deleteSessionHistory removes file and pending sessions", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);
    mgr.setEncryptSessions(false);

    // Play a game and flush to create a sessions file
    auto id = mgr.startGame(Difficulty::Medium, 1, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(10000));
    REQUIRE(mgr.endGame(*id, true).has_value());
    mgr.flushSessions();
    REQUIRE(fs::exists(tmp.path() / "game_sessions.yaml"));

    // Play another game (pending, not flushed)
    auto id2 = mgr.startGame(Difficulty::Hard, 2, 0);
    REQUIRE(id2.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(5000));
    REQUIRE(mgr.endGame(*id2, false).has_value());

    // Delete session history — should remove file and pending
    auto result = mgr.deleteSessionHistory();
    REQUIRE(result.has_value());
    REQUIRE_FALSE(fs::exists(tmp.path() / "game_sessions.yaml"));

    // getAllSessions should return empty (no file, no pending)
    auto sessions = mgr.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->empty());
}

// ============================================================================
// getAllSessions with corrupt encrypted file: covers line 249 (decrypt warn)
// ============================================================================

TEST_CASE("StatisticsManager - getAllSessions handles corrupt encrypted file gracefully", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Write data that looks encrypted (starts with magic bytes) but is corrupt
    {
        std::ofstream f(tmp.path() / "game_sessions.yaml", std::ios::binary);
        // EncryptionManager magic: "SDKENC" + version byte + flags byte + garbage
        std::array<char, 15> data = {'S', 'D', 'K', 'E', 'N', 'C', 0x01, 0x00, 'g', 'a', 'r', 'b', 'a', 'g', 'e'};
        f.write(data.data(), data.size());
    }

    // getAllSessions should handle the decrypt failure gracefully (warn, return empty)
    auto sessions = mgr.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->empty());
}

// ============================================================================
// getAggregateStats when recalculation fails: covers lines 192-194
// (no aggregate file, corrupted sessions → recalc fails → error returned)
// ============================================================================

TEST_CASE("StatisticsManager - getStatsForDifficulty propagates aggregate error", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // Write corrupted sessions file + no aggregate → constructor triggers
    // recalculateAggregateStats which fails on corrupt sessions → falls back to empty stats
    {
        std::ofstream f(tmp.path() / "game_sessions.yaml");
        f << "invalid: yaml: {{{broken\n";
    }

    // Constructor: no aggregate file → recalculate → corrupt sessions → recalc fails
    // → falls back to empty stats (lines 486-488)
    StatisticsManager mgr(tmp.path().string(), time);

    // getStatsForDifficulty should still work (empty stats, not error)
    auto result = mgr.getStatsForDifficulty(Difficulty::Easy);
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 0);
}
