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
// importStats average-time correctness (issue #27): the weighted average must
// be computed from the PRE-merge completion counts. Importing identical data
// must leave the average unchanged; merging different data must weight properly.
// ============================================================================

TEST_CASE("StatisticsManager - importStats preserves average time on identical merge", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // One completed Easy game: 40s
    auto id = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(40000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    const int easy = static_cast<int>(Difficulty::Easy);
    auto avg_before = mgr.getAggregateStats()->average_times[easy];
    REQUIRE(avg_before == std::chrono::milliseconds(40000));

    fs::path exp = tmp.path() / "exported.yaml";
    REQUIRE(mgr.exportStats(exp.string()).has_value());

    // Merging an identical 40s dataset must keep the average at 40s.
    REQUIRE(mgr.importStats(exp.string()).has_value());
    REQUIRE(mgr.getAggregateStats()->average_times[easy] == std::chrono::milliseconds(40000));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with multiple REQUIRE/loop checks; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - importStats computes weighted average across differing data", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // Build an export with a single 60s Hard game in a separate manager/dir.
    fs::path exp;
    {
        TempTestDir tmp2;
        StatisticsManager src(tmp2.path().string(), time);
        auto id = src.startGame(Difficulty::Hard, 1, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(60000));
        REQUIRE(src.endGame(*id, true).has_value());
        // Export into the outer temp dir so the file survives tmp2's destruction.
        exp = tmp.path() / "src.yaml";
        REQUIRE(src.exportStats(exp.string()).has_value());
    }

    // Local manager has two 30s Hard games.
    StatisticsManager mgr(tmp.path().string(), time);
    for (int i = 0; i < 2; ++i) {
        auto id = mgr.startGame(Difficulty::Hard, static_cast<uint32_t>(i + 1), 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(30000));
        REQUIRE(mgr.endGame(*id, true).has_value());
    }

    const int hard = static_cast<int>(Difficulty::Hard);
    REQUIRE(mgr.getAggregateStats()->average_times[hard] == std::chrono::milliseconds(30000));

    // Weighted avg = (30000*2 + 60000*1) / 3 = 40000 ms.
    REQUIRE(mgr.importStats(exp.string()).has_value());
    REQUIRE(mgr.getAggregateStats()->average_times[hard] == std::chrono::milliseconds(40000));
}

// ============================================================================
// Regression (issue #27): recalculateAggregateStats() must count each session
// exactly once over the full disk + in-memory pending set — never double-count
// (the reported HIGH bug) and never drop unflushed pending sessions.
// recalc is private and only reachable via a friend test-peer when pending is
// non-empty, so we drive it directly through StatisticsManagerTestPeer.
// ============================================================================

namespace sudoku::core {
struct StatisticsManagerTestPeer {
    static std::expected<void, StatisticsError> recalc(const StatisticsManager& mgr) {
        return mgr.recalculateAggregateStats();
    }
};
}  // namespace sudoku::core

TEST_CASE("StatisticsManager - recalc counts disk and pending sessions exactly once", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);

    // Game 1: end then flush → lives on disk, pending cleared.
    auto id1 = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(id1.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id1, true).has_value());
    mgr.flushSessions();

    // Game 2: end but do NOT flush → stays in pending_sessions_ only.
    auto id2 = mgr.startGame(Difficulty::Easy, 2, 0);
    REQUIRE(id2.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id2, true).has_value());

    // Force a full rebuild while disk has S1 and pending has S2.
    REQUIRE(StatisticsManagerTestPeer::recalc(mgr).has_value());

    auto agg = mgr.getAggregateStats();
    REQUIRE(agg.has_value());
    // 2 ⇒ each session counted once. A double-count would give 4; dropping the
    // unflushed pending session would give 1.
    REQUIRE(agg->total_games == 2);
    REQUIRE(agg->total_completed == 2);
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
// getAllSessions with corrupt encrypted file: must FAIL CLOSED (issue #26).
// Returning empty here is the original data-loss bug — flushSessions would then
// overwrite the file. The read must error and the file must be left intact.
// ============================================================================

TEST_CASE("StatisticsManager - getAllSessions fails closed on corrupt encrypted file", "[statistics_extra]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    const fs::path sessions_file = tmp.path() / "game_sessions.yaml";
    // Write data that looks encrypted (starts with magic bytes) but is corrupt
    {
        std::ofstream f(sessions_file, std::ios::binary);
        // EncryptionManager magic: "SDKENC" + version byte + flags byte + garbage
        std::array<char, 15> data = {'S', 'D', 'K', 'E', 'N', 'C', 0x01, 0x00, 'g', 'a', 'r', 'b', 'a', 'g', 'e'};
        f.write(data.data(), data.size());
    }

    // getAllSessions must report the failure instead of silently dropping history,
    // and flag the history as unreadable so the file is preserved.
    auto sessions = mgr.getAllSessions();
    REQUIRE_FALSE(sessions.has_value());
    REQUIRE(sessions.error() == StatisticsError::FileAccessError);
    REQUIRE(mgr.hasUnreadableSessionHistory());
    REQUIRE(fs::exists(sessions_file));
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

// ============================================================================
// Story 8.1 (SAVE-2/SAVE-3): seedSessionProgress — the restore-only API that
// re-seats a resumed game's counters and prior play time onto a fresh session.
// Without it, a resumed game restarted at 0 hints used / 0 moves / 0 mistakes
// and reported only the post-resume span as its play time.
// ============================================================================

TEST_CASE("StatisticsManager - seedSessionProgress seeds counters and prior play time", "[statistics_extra][restore]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto id = mgr.startGame(Difficulty::Hard, 0, 5.5);
    REQUIRE(id.has_value());

    REQUIRE(mgr.seedSessionProgress(*id, 12, 3, 2, std::chrono::seconds(90)).has_value());

    auto stats = mgr.getGameStats(*id);
    REQUIRE(stats.has_value());
    REQUIRE(stats->moves_made == 12);
    REQUIRE(stats->hints_used == 3);
    REQUIRE(stats->mistakes == 2);
    REQUIRE(stats->time_played == std::chrono::seconds(90));
}

TEST_CASE("StatisticsManager - seedSessionProgress rejects an unknown session", "[statistics_extra][restore]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto result = mgr.seedSessionProgress(9999, 1, 1, 1, std::chrono::seconds(1));

    REQUIRE(!result.has_value());
    REQUIRE(result.error() == StatisticsError::GameNotStarted);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("StatisticsManager - seedSessionProgress clamps hostile counter values", "[statistics_extra][restore]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    SECTION("Negative counters and negative prior time clamp to zero") {
        auto id = mgr.startGame(Difficulty::Easy, 0, 0.0);
        REQUIRE(id.has_value());

        REQUIRE(mgr.seedSessionProgress(*id, -5, -7, -9, std::chrono::seconds(-42)).has_value());

        auto stats = mgr.getGameStats(*id);
        REQUIRE(stats.has_value());
        REQUIRE(stats->moves_made == 0);
        REQUIRE(stats->hints_used == 0);
        REQUIRE(stats->mistakes == 0);
        REQUIRE(stats->time_played.count() == 0);
    }

    SECTION("Absurdly large counters clamp to the sane maximum") {
        auto id = mgr.startGame(Difficulty::Easy, 0, 0.0);
        REQUIRE(id.has_value());

        constexpr int kAbsurd = 2'000'000'000;
        REQUIRE(mgr.seedSessionProgress(*id, kAbsurd, kAbsurd, kAbsurd, std::chrono::seconds(0)).has_value());

        auto stats = mgr.getGameStats(*id);
        REQUIRE(stats.has_value());
        REQUIRE(stats->moves_made < kAbsurd);
        REQUIRE(stats->hints_used < kAbsurd);
        REQUIRE(stats->mistakes < kAbsurd);
        REQUIRE(stats->moves_made >= 0);
    }
}

TEST_CASE("StatisticsManager - endGame adds the active span on top of seeded prior time",
          "[statistics_extra][restore]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto id = mgr.startGame(Difficulty::Medium, 0, 0.0);
    REQUIRE(id.has_value());
    REQUIRE(mgr.seedSessionProgress(*id, 4, 1, 0, std::chrono::seconds(90)).has_value());

    time->advanceSystemTime(std::chrono::seconds(30));  // post-resume play
    auto ended = mgr.endGame(*id, true);

    REQUIRE(ended.has_value());
    REQUIRE(ended->time_played == std::chrono::seconds(120));  // 90 seeded + 30 played
    REQUIRE(ended->moves_made == 4);
    REQUIRE(ended->hints_used == 1);
    REQUIRE(ended->completed);
}

TEST_CASE("StatisticsManager - endGame on a non-seeded session reports the active span unchanged",
          "[statistics_extra][restore]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto id = mgr.startGame(Difficulty::Medium, 0, 0.0);
    REQUIRE(id.has_value());

    time->advanceSystemTime(std::chrono::seconds(30));
    auto ended = mgr.endGame(*id, true);

    REQUIRE(ended.has_value());
    REQUIRE(ended->time_played == std::chrono::seconds(30));  // AC9e: numerically unchanged
}

TEST_CASE("StatisticsManager - shutdown abandons a seeded session with the same total as endGame",
          "[statistics_extra][restore]") {
    // The destructor is the cross-process exit path (quit while a resumed game is in progress).
    // It must agree with endGame(): both ADD the active span to the seeded prior time. Before this
    // was aligned, the same session reported 30 s via shutdown and 120 s via endGame — the kind of
    // "same input, two stored numbers" divergence that later reads as a flake.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setCollectDetailedStats(true);
        auto id = mgr.startGame(Difficulty::Medium, 0, 0.0);
        REQUIRE(id.has_value());
        REQUIRE(mgr.seedSessionProgress(*id, 4, 1, 0, std::chrono::seconds(90)).has_value());
        time->advanceSystemTime(std::chrono::seconds(30));
    }  // destructor ends the session as abandoned and flushes

    StatisticsManager reopened(tmp.path().string(), time);
    auto sessions = reopened.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->size() == 1);
    REQUIRE(sessions->front().time_played == std::chrono::seconds(120));  // 90 seeded + 30 played
    REQUIRE(!sessions->front().completed);
    REQUIRE(sessions->front().moves_made == 4);
}

TEST_CASE("StatisticsManager - shutdown of a non-seeded session reports the active span unchanged",
          "[statistics_extra][restore]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setCollectDetailedStats(true);
        auto id = mgr.startGame(Difficulty::Medium, 0, 0.0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::seconds(30));
    }

    StatisticsManager reopened(tmp.path().string(), time);
    auto sessions = reopened.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->size() == 1);
    REQUIRE(sessions->front().time_played == std::chrono::seconds(30));  // unchanged for normal games
}

// ============================================================================
// Story 8.18: session-per-segment accounting. Story 8.1 made a resumed session
// carry the progress accumulated before the save, which is right for the session
// record but was then folded into the lifetime aggregate in full — so every
// quit→resume cycle re-banked the whole history to date. These tests assert on
// getAggregateStats(), the consumer 8.1 never looked at.
// ============================================================================

namespace {

constexpr uint32_t kResumedSeed = 4242;
constexpr double kResumedRating = 5.5;

// The progress a save file carries for the "restore, play nothing, quit" cycles.
constexpr int kSavedMoves = 20;
constexpr int kSavedHints = 2;
constexpr int kSavedMistakes = 3;
constexpr auto kSavedElapsed = std::chrono::seconds(90);

/// One resume segment in which the player touches nothing: restore the save, quit again.
void replayWithoutPlaying(StatisticsManager& mgr) {
    auto id = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(id.has_value());
    REQUIRE(mgr.seedSessionProgress(*id, kSavedMoves, kSavedHints, kSavedMistakes, kSavedElapsed).has_value());
    REQUIRE(mgr.endGame(*id, false).has_value());
}

/// Records @p moves moves — the first @p mistakes of them wrong — and @p hints hints, reporting
/// whether every call succeeded. The result is collapsed into one value on purpose: a REQUIRE per
/// iteration expands to a try/catch/do-while each time and blows the cognitive-complexity budget.
[[nodiscard]] bool recordProgress(StatisticsManager& mgr, uint64_t id, int moves, int mistakes, int hints) {
    bool ok = true;
    for (int i = 0; i < moves; ++i) {
        ok = ok && mgr.recordMove(id, i < mistakes).has_value();
    }
    for (int i = 0; i < hints; ++i) {
        ok = ok && mgr.recordHint(id).has_value();
    }
    return ok;
}

/// Play the opening segment of the resumed puzzle, then quit: kSavedMoves moves,
/// kSavedHints hints, kSavedMistakes mistakes over kSavedElapsed. Leaves exactly the
/// progress that replayWithoutPlaying() then carries back in.
void playOpeningSegment(StatisticsManager& mgr, MockTimeProvider& time) {
    auto id = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(id.has_value());
    REQUIRE(recordProgress(mgr, *id, kSavedMoves, kSavedMistakes, kSavedHints));

    time.advanceSystemTime(kSavedElapsed);
    REQUIRE(mgr.endGame(*id, false).has_value());
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - a resumed game banks each unit of play exactly once",
          "[statistics_extra][restore][regression][bug-resumed-stats-inflation]") {
    // Implementer probe from story 8.18: play 60 s / 2 moves, quit, resume, play 30 s / 1 move,
    // finish. Both session records are individually correct — the second carries the first's totals
    // per story 8.1 AC8 — but the aggregate used to add both in full and reported 150 s / 5 moves
    // for 90 s / 3 moves of real play.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    const int medium = static_cast<int>(Difficulty::Medium);

    auto first = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(first.has_value());
    REQUIRE(mgr.recordMove(*first, false).has_value());
    REQUIRE(mgr.recordMove(*first, false).has_value());
    time->advanceSystemTime(std::chrono::seconds(60));
    REQUIRE(mgr.endGame(*first, false).has_value());

    auto second = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(second.has_value());
    REQUIRE(mgr.seedSessionProgress(*second, 2, 0, 0, std::chrono::seconds(60)).has_value());
    REQUIRE(mgr.recordMove(*second, false).has_value());
    time->advanceSystemTime(std::chrono::seconds(30));
    REQUIRE(mgr.endGame(*second, true).has_value());

    auto agg = mgr.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->total_time_played == std::chrono::seconds(90));  // was 150 s
    REQUIRE(agg->total_moves == 3);                               // was 5
    // AC6 option (b): two segments of one puzzle are one game attempted.
    REQUIRE(agg->total_games == 1);
    REQUIRE(agg->games_played[medium] == 1);
    REQUIRE(agg->total_completed == 1);
    REQUIRE(agg->games_completed[medium] == 1);
    // AC3: the completed record keeps its full seeded time, so the personal best is the whole
    // puzzle (90 s) and not just the closing segment (30 s).
    REQUIRE(agg->best_times[medium] == std::chrono::seconds(90));
    REQUIRE(agg->average_times[medium] == std::chrono::seconds(90));
    // AC6 knock-on: average_ratings divides by games_played, so a continuation segment must not
    // contribute a second copy of the same puzzle's rating either.
    REQUIRE(agg->average_ratings[medium] == kResumedRating);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - restoring a save without playing does not move the aggregate",
          "[statistics_extra][restore][regression][bug-resumed-stats-inflation]") {
    // Adversarial-review probe from story 8.18: relaunch into the same save three times and touch
    // nothing. Each restore used to re-bank the save's whole carried history, so lifetime totals
    // grew purely from reopening the app.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    const int medium = static_cast<int>(Difficulty::Medium);

    playOpeningSegment(mgr, *time);
    auto after_opening = mgr.getAggregateStats();
    REQUIRE(after_opening.has_value());

    replayWithoutPlaying(mgr);
    replayWithoutPlaying(mgr);
    replayWithoutPlaying(mgr);

    auto agg = mgr.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->total_time_played == after_opening->total_time_played);
    REQUIRE(agg->total_moves == after_opening->total_moves);
    REQUIRE(agg->total_hints == after_opening->total_hints);
    REQUIRE(agg->total_mistakes == after_opening->total_mistakes);
    REQUIRE(agg->total_games == after_opening->total_games);
    REQUIRE(agg->games_played[medium] == after_opening->games_played[medium]);
    // Absolute values, so a fix that zeroed both sides would not pass vacuously.
    REQUIRE(agg->total_time_played == kSavedElapsed);
    REQUIRE(agg->total_moves == kSavedMoves);
    REQUIRE(agg->total_hints == kSavedHints);
    REQUIRE(agg->total_mistakes == kSavedMistakes);
    REQUIRE(agg->total_games == 1);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - segment accounting survives a rebuild from the session log",
          "[statistics_extra][restore][regression][bug-resumed-stats-inflation]") {
    // AC4: recalculateAggregateStats() rebuilds the aggregate from the persisted records, so an
    // in-memory-only fix is undone by the next rebuild. The records themselves must carry enough
    // information to tell carried-in progress from progress made in that segment.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const int medium = static_cast<int>(Difficulty::Medium);

    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setCollectDetailedStats(true);
        playOpeningSegment(mgr, *time);
        replayWithoutPlaying(mgr);
        replayWithoutPlaying(mgr);
        mgr.flushSessions();
    }

    // Drop the aggregate file so the reopened manager has to rebuild from game_sessions.yaml.
    fs::path agg_file = tmp.path() / "aggregate_stats.yaml";
    REQUIRE(fs::exists(agg_file));
    fs::remove(agg_file);

    StatisticsManager reopened(tmp.path().string(), time);
    auto agg = reopened.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->total_time_played == kSavedElapsed);
    REQUIRE(agg->total_moves == kSavedMoves);
    REQUIRE(agg->total_hints == kSavedHints);
    REQUIRE(agg->total_mistakes == kSavedMistakes);
    REQUIRE(agg->total_games == 1);
    REQUIRE(agg->games_played[medium] == 1);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - shutdown banks a resumed segment the same way endGame does",
          "[statistics_extra][restore][regression][bug-resumed-stats-inflation]") {
    // AC7: quitting the app with a resumed game in progress exits through ~StatisticsManager, not
    // endGame(). The two paths must contribute identically, or the same play produces different
    // lifetime totals depending on how the player left.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const int medium = static_cast<int>(Difficulty::Medium);

    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setCollectDetailedStats(true);

        auto first = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
        REQUIRE(first.has_value());
        REQUIRE(mgr.recordMove(*first, false).has_value());
        REQUIRE(mgr.recordMove(*first, false).has_value());
        time->advanceSystemTime(std::chrono::seconds(60));
        REQUIRE(mgr.endGame(*first, false).has_value());

        auto second = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
        REQUIRE(second.has_value());
        REQUIRE(mgr.seedSessionProgress(*second, 2, 0, 0, std::chrono::seconds(60)).has_value());
        REQUIRE(mgr.recordMove(*second, false).has_value());
        time->advanceSystemTime(std::chrono::seconds(30));
    }  // destructor abandons the resumed session and folds it into the aggregate

    StatisticsManager reopened(tmp.path().string(), time);
    auto agg = reopened.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->total_time_played == std::chrono::seconds(90));
    REQUIRE(agg->total_moves == 3);
    REQUIRE(agg->total_games == 1);
    REQUIRE(agg->games_played[medium] == 1);
    REQUIRE(agg->total_completed == 0);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - a legacy session log rebuilds with its recorded totals intact",
          "[statistics_extra][restore]") {
    // AC5 end-to-end: a game_sessions.yaml written before story 8.18 has no carried_* keys at all.
    // Rebuilding from it must produce the same aggregate it produces today — the new accounting
    // must not retroactively reinterpret history it has no information about.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const int medium = static_cast<int>(Difficulty::Medium);

    fs::path sessions_file = tmp.path() / "game_sessions.yaml";
    std::ofstream(sessions_file) << R"(- difficulty: 1
  puzzle_rating: 5.5
  start_time: 1700000000
  end_time: 1700000090
  time_played: 90000
  completed: true
  moves_made: 20
  hints_used: 2
  mistakes: 3
  puzzle_seed: 4242
)";
    REQUIRE(fs::exists(sessions_file));
    REQUIRE(!fs::exists(tmp.path() / "aggregate_stats.yaml"));

    StatisticsManager mgr(tmp.path().string(), time);
    auto agg = mgr.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(!mgr.hasUnreadableSessionHistory());  // no quarantine on the unknown-field-free file
    REQUIRE(agg->total_time_played == std::chrono::milliseconds(90000));
    REQUIRE(agg->total_moves == 20);
    REQUIRE(agg->total_hints == 2);
    REQUIRE(agg->total_mistakes == 3);
    REQUIRE(agg->total_games == 1);
    REQUIRE(agg->games_played[medium] == 1);
    REQUIRE(agg->total_completed == 1);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - a completion always counts as an attempt",
          "[statistics_extra][restore][regression][bug-resumed-stats-inflation]") {
    // The opening segment reaches the aggregate only via endGame() or ~StatisticsManager, neither of
    // which runs when the process is killed — while the auto-save the app resumes from is written by
    // a timer. So a resumed completion can be a puzzle's only witness. Suppressing its attempt count
    // as "already counted" would push games_completed above games_played, and getCompletionRates()
    // divides one by the other: the review measured 0% and 200%.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    const int medium = static_cast<int>(Difficulty::Medium);

    auto honest = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(honest.has_value());
    time->advanceSystemTime(std::chrono::seconds(60));
    REQUIRE(mgr.endGame(*honest, true).has_value());

    // The orphaned puzzle: no opening segment was ever folded in, only the resumed completion.
    auto orphaned = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(orphaned.has_value());
    REQUIRE(mgr.seedSessionProgress(*orphaned, kSavedMoves, kSavedHints, kSavedMistakes, kSavedElapsed).has_value());
    time->advanceSystemTime(std::chrono::seconds(30));
    REQUIRE(mgr.endGame(*orphaned, true).has_value());

    auto agg = mgr.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->games_completed[medium] <= agg->games_played[medium]);
    REQUIRE(agg->total_completed <= agg->total_games);
    REQUIRE(agg->games_played[medium] == 2);
    REQUIRE(agg->games_completed[medium] == 2);
    REQUIRE(mgr.getCompletionRates()[medium] <= 1.0);
    // The orphaned puzzle counts as an attempt, so its rating must be averaged in with it — the
    // denominator of average_ratings is games_played.
    REQUIRE(agg->average_ratings[medium] == kResumedRating);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - reopening a save without playing does not break a win streak",
          "[statistics_extra][restore][regression][bug-resumed-stats-inflation]") {
    // A resumed segment that ends un-played is not a second abandonment of the same puzzle, so it
    // must not reset the streak. Before this, merely launching the app onto its auto-save and
    // closing it again wiped the streak shown in the statistics dialog.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto first = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(first.has_value());
    time->advanceSystemTime(std::chrono::seconds(60));
    REQUIRE(mgr.endGame(*first, true).has_value());
    auto after_win = mgr.getAggregateStats();
    REQUIRE(after_win.has_value());
    REQUIRE(after_win->current_win_streak == 1);

    replayWithoutPlaying(mgr);
    replayWithoutPlaying(mgr);

    auto agg = mgr.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->current_win_streak == 1);
    REQUIRE(agg->best_win_streak == 1);
    // Abandoning a puzzle for the first time still breaks it — unchanged behaviour.
    auto fresh = mgr.startGame(Difficulty::Medium, kResumedSeed, kResumedRating);
    REQUIRE(fresh.has_value());
    REQUIRE(mgr.endGame(*fresh, false).has_value());
    REQUIRE(mgr.getAggregateStats()->current_win_streak == 0);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with many REQUIREs; complexity is inherent to test coverage
TEST_CASE("StatisticsManager - a hostile session record cannot inflate or overflow the totals",
          "[statistics_extra][restore][regression][bug-resumed-stats-inflation]") {
    // carried_* and the running totals are read from a user-editable file and are NOT range-checked
    // at the parse boundary (unlike difficulty, per story 8.2). segmentDelta floors both inputs
    // before subtracting: a negative carried must not widen the delta, and a negative total against
    // a positive carried must not overflow — signed overflow is fatal under the sanitizer build.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    fs::path sessions_file = tmp.path() / "game_sessions.yaml";
    std::ofstream(sessions_file) << R"(- difficulty: 1
  time_played: -9223372036854775808
  completed: false
  moves_made: -2147483648
  hints_used: 5
  mistakes: 5
  continued_from_save: true
  carried_moves: 2147483647
  carried_hints: -2147483648
  carried_mistakes: -1
  carried_time_played: 9223372036854775807
)";

    StatisticsManager mgr(tmp.path().string(), time);
    auto agg = mgr.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->total_moves == 0);
    REQUIRE(agg->total_time_played.count() == 0);
    // A negative carried is floored, so it cannot widen the delta past the record's own total.
    REQUIRE(agg->total_hints == 5);
    REQUIRE(agg->total_mistakes == 5);
}

TEST_CASE("StatisticsManager - carried progress is ignored on a record that is not a continuation",
          "[statistics_extra][restore]") {
    // continued_from_save is the single source of truth. A record with carried_* set but the flag
    // clear must contribute its play in full, so the two halves of the model cannot disagree.
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    fs::path sessions_file = tmp.path() / "game_sessions.yaml";
    std::ofstream(sessions_file) << R"(- difficulty: 1
  time_played: 90000
  completed: false
  moves_made: 20
  hints_used: 2
  mistakes: 3
  continued_from_save: false
  carried_moves: 20
  carried_hints: 2
  carried_mistakes: 3
  carried_time_played: 90000
)";

    StatisticsManager mgr(tmp.path().string(), time);
    auto agg = mgr.getAggregateStats();

    REQUIRE(agg.has_value());
    REQUIRE(agg->total_moves == 20);
    REQUIRE(agg->total_time_played == std::chrono::milliseconds(90000));
    REQUIRE(agg->total_games == 1);
}
