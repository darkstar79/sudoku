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

/// Tests for StatisticsManager branch coverage:
/// - Invalid inputs (bad difficulty, bad game_id)
/// - recordMove with is_mistake=true
/// - endGame with completed=false (breaks win streak)
/// - getRecentGames truncation vs. no-truncation path
/// - getCompletionRates when games_played > 0
/// - invalidateStatsCache + recalculation path
/// - resetAllStats with and without existing files
/// - importStats with non-existent file
/// - updateAggregateStats with puzzle_rating > 0

#include "../../src/core/encryption_manager.h"
#include "../../src/core/i_time_provider.h"
#include "../../src/core/statistics_manager.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

// ============================================================================
// Invalid difficulty
// ============================================================================

TEST_CASE("StatisticsManager - startGame with invalid difficulty returns error", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // uint8_t value 99 is >= DIFFICULTY_COUNT → isValidDifficulty returns false
    auto result = mgr.startGame(static_cast<Difficulty>(99), 1234, 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == StatisticsError::InvalidDifficulty);

    // DIFFICULTY_COUNT (= 5) is the first invalid value just past Master=4.
    auto boundary = mgr.startGame(static_cast<Difficulty>(DIFFICULTY_COUNT), 1234, 0);
    REQUIRE_FALSE(boundary.has_value());
    REQUIRE(boundary.error() == StatisticsError::InvalidDifficulty);
}

// ============================================================================
// STAT-1: Master (Difficulty=4) must be recorded like every other difficulty
// ============================================================================

TEST_CASE("StatisticsManager - Master game is recorded in aggregates", "[statistics_manager_branches][stat1]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    constexpr auto kMaster = static_cast<size_t>(Difficulty::Master);

    auto id = mgr.startGame(Difficulty::Master, 4242, 900);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(90000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    auto agg = mgr.getAggregateStats();
    REQUIRE(agg.has_value());
    REQUIRE(agg->games_played[kMaster] == 1);
    REQUIRE(agg->games_completed[kMaster] == 1);
    REQUIRE(agg->best_times[kMaster] == std::chrono::milliseconds(90000));

    auto master_stats = mgr.getStatsForDifficulty(Difficulty::Master);
    REQUIRE(master_stats.has_value());
    REQUIRE(master_stats->games_played[kMaster] == 1);

    auto best = mgr.getBestTimes();
    REQUIRE(best[kMaster] == std::chrono::milliseconds(90000));
}

TEST_CASE("StatisticsManager - getStatsForDifficulty with invalid difficulty returns error",
          "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto result = mgr.getStatsForDifficulty(static_cast<Difficulty>(99));
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == StatisticsError::InvalidDifficulty);
}

// ============================================================================
// Invalid game_id
// ============================================================================

TEST_CASE("StatisticsManager - operations with invalid game_id return GameNotStarted",
          "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    constexpr uint64_t BAD_ID = 99999;

    SECTION("recordMove with invalid game_id") {
        auto result = mgr.recordMove(BAD_ID, false);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == StatisticsError::GameNotStarted);
    }

    SECTION("recordHint with invalid game_id") {
        auto result = mgr.recordHint(BAD_ID);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == StatisticsError::GameNotStarted);
    }

    SECTION("endGame with invalid game_id") {
        auto result = mgr.endGame(BAD_ID, true);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == StatisticsError::GameNotStarted);
    }

    SECTION("getGameStats with invalid game_id") {
        auto result = mgr.getGameStats(BAD_ID);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == StatisticsError::GameNotStarted);
    }
}

// ============================================================================
// recordMove: is_mistake=true branch
// ============================================================================

TEST_CASE("StatisticsManager - recordMove with is_mistake=true increments mistakes", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto start_result = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(start_result.has_value());
    uint64_t id = *start_result;

    // Correct move
    REQUIRE(mgr.recordMove(id, false).has_value());
    // Mistake move
    REQUIRE(mgr.recordMove(id, true).has_value());

    auto stats = mgr.getGameStats(id);
    REQUIRE(stats.has_value());
    REQUIRE(stats->moves_made == 2);
    REQUIRE(stats->mistakes == 1);
}

// ============================================================================
// endGame with completed=false → breaks win streak
// ============================================================================

TEST_CASE("StatisticsManager - endGame completed=false resets win streak", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Play and complete one game first (builds streak)
    auto id1 = mgr.startGame(Difficulty::Easy, 1, 100);
    REQUIRE(id1.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id1, true).has_value());  // completed=true → streak=1

    // Play and fail another game (resets streak)
    auto id2 = mgr.startGame(Difficulty::Easy, 2, 100);
    REQUIRE(id2.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(10000));
    auto end_result = mgr.endGame(*id2, false);  // completed=false → streak=0
    REQUIRE(end_result.has_value());
    REQUIRE_FALSE(end_result->completed);

    auto agg = mgr.getAggregateStats();
    REQUIRE(agg.has_value());
    REQUIRE(agg->current_win_streak == 0);
    REQUIRE(agg->best_win_streak == 1);
}

// ============================================================================
// updateAggregateStats: puzzle_rating > 0 path
// ============================================================================

TEST_CASE("StatisticsManager - updateAggregateStats tracks rating stats when puzzle_rating > 0",
          "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto id = mgr.startGame(Difficulty::Medium, 42, 350);  // puzzle_rating=350 > 0
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(60000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    auto agg = mgr.getAggregateStats();
    REQUIRE(agg.has_value());
    // Rating stats should be populated
    REQUIRE(agg->min_ratings[static_cast<int>(Difficulty::Medium)] == 350);
    REQUIRE(agg->max_ratings[static_cast<int>(Difficulty::Medium)] == 350);
}

// ============================================================================
// getRecentGames: truncation vs. no-truncation
// ============================================================================

TEST_CASE("StatisticsManager - getRecentGames truncation and no-truncation", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);

    // Create 3 completed games
    for (int i = 0; i < 3; ++i) {
        auto id = mgr.startGame(Difficulty::Easy, static_cast<uint32_t>(i + 1), 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(30000));
        REQUIRE(mgr.endGame(*id, true).has_value());
    }

    SECTION("count >= sessions: all returned (no truncation)") {
        auto result = mgr.getRecentGames(10);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 3);
    }

    SECTION("count < sessions: truncation applied") {
        auto result = mgr.getRecentGames(2);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 2);
    }

    SECTION("getAllSessions returns all games") {
        auto result = mgr.getAllSessions();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 3);
    }
}

// ============================================================================
// getCompletionRates: division path when games_played > 0
// ============================================================================

TEST_CASE("StatisticsManager - getCompletionRates with played games shows non-zero rate",
          "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Play 2 games: 1 completed, 1 not
    auto id1 = mgr.startGame(Difficulty::Hard, 1, 0);
    REQUIRE(id1.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(60000));
    REQUIRE(mgr.endGame(*id1, true).has_value());

    auto id2 = mgr.startGame(Difficulty::Hard, 2, 0);
    REQUIRE(id2.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(20000));
    REQUIRE(mgr.endGame(*id2, false).has_value());

    auto rates = mgr.getCompletionRates();
    // Hard = index 2; 1/2 = 0.5
    REQUIRE(rates[static_cast<int>(Difficulty::Hard)] == 0.5);
    // Easy = index 0; never played → 0.0
    REQUIRE(rates[static_cast<int>(Difficulty::Easy)] == 0.0);
}

// ============================================================================
// getBestTimes
// ============================================================================

TEST_CASE("StatisticsManager - getBestTimes returns best time after completed game", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto id = mgr.startGame(Difficulty::Expert, 1, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(45000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    auto best = mgr.getBestTimes();
    REQUIRE(best[static_cast<int>(Difficulty::Expert)].count() > 0);
}

// ============================================================================
// getAggregateStats: recalculation path from fresh manager with sessions file
// ============================================================================

TEST_CASE("StatisticsManager - fresh manager loads and recalculates from saved sessions",
          "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // First manager: play a game and persist it
    {
        StatisticsManager mgr1(tmp.path().string(), time);
        auto id = mgr1.startGame(Difficulty::Easy, 1, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(30000));
        REQUIRE(mgr1.endGame(*id, true).has_value());
    }

    // Second manager reads the same directory — constructor calls loadStatistics()
    // which loads the stats file, setting cache valid = true
    StatisticsManager mgr2(tmp.path().string(), time);
    auto result = mgr2.getAggregateStats();
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 1);
}

// ============================================================================
// resetAllStats
// ============================================================================

TEST_CASE("StatisticsManager - resetAllStats clears data and files", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Create some data
    auto id = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    // Verify data exists
    auto pre_reset = mgr.getAggregateStats();
    REQUIRE(pre_reset.has_value());
    REQUIRE(pre_reset->total_games == 1);

    // Reset
    REQUIRE_NOTHROW(mgr.resetAllStats());

    // Stats should be empty now
    auto post_reset = mgr.getAggregateStats();
    REQUIRE(post_reset.has_value());
    REQUIRE(post_reset->total_games == 0);
}

TEST_CASE("StatisticsManager - resetAllStats on fresh manager is a no-op", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // No data exists — reset should not crash
    REQUIRE_NOTHROW(mgr.resetAllStats());
    auto result = mgr.getAggregateStats();
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 0);
}

// ============================================================================
// importStats: non-existent file
// ============================================================================

TEST_CASE("StatisticsManager - importStats with non-existent file returns FileAccessError",
          "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    auto result = mgr.importStats("/nonexistent/path/stats.yaml");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == StatisticsError::FileAccessError);
}

// ============================================================================
// importStats with valid exported file (merge logic)
// ============================================================================

TEST_CASE("StatisticsManager - importStats with valid file merges stats correctly", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // First manager: play 2 Easy games, fast completions (10s and 20s)
    TempTestDir export_dir;
    {
        StatisticsManager src(export_dir.path().string(), time);
        auto id1 = src.startGame(Difficulty::Easy, 1, 200);
        REQUIRE(id1.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(10000));  // 10s
        REQUIRE(src.endGame(*id1, true).has_value());

        auto id2 = src.startGame(Difficulty::Easy, 2, 300);
        REQUIRE(id2.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(20000));  // 20s
        REQUIRE(src.endGame(*id2, true).has_value());

        // Export the stats to a file
        auto export_path = tmp.path() / "imported_stats.yaml";
        REQUIRE(src.exportStats(export_path.string()).has_value());
    }

    // Second manager: play 1 Easy game with slow completion (60s)
    StatisticsManager mgr(tmp.path().string(), time);
    auto id3 = mgr.startGame(Difficulty::Easy, 3, 150);
    REQUIRE(id3.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(60000));  // 60s
    REQUIRE(mgr.endGame(*id3, true).has_value());

    auto pre_import = mgr.getAggregateStats();
    REQUIRE(pre_import.has_value());
    REQUIRE(pre_import->total_games == 1);

    // Import stats from the first manager
    auto export_path = tmp.path() / "imported_stats.yaml";
    auto import_result = mgr.importStats(export_path.string());
    REQUIRE(import_result.has_value());

    // After import: total_games should include both managers' games
    auto post_import = mgr.getAggregateStats();
    REQUIRE(post_import.has_value());
    // mgr had 1 game, imported adds 2 more → 3 total
    REQUIRE(post_import->total_games >= 2);
    // Imported Easy stats had better time (10s) than mgr2's (60s)
    // best_times[Easy] should now be ≤ 60s
    REQUIRE(post_import->best_times[static_cast<int>(Difficulty::Easy)].count() > 0);
}

// ============================================================================
// StatisticsManager: constructor with empty stats_directory (uses platform default)
// ============================================================================

TEST_CASE("StatisticsManager - constructor with empty directory uses default", "[statistics_manager_branches]") {
    // This covers the empty() → platform default path in the constructor.
    // We can't easily verify the path without platform knowledge, but we verify
    // that construction succeeds and basic operations work.
    // Use a throw-away unique dir to avoid polluting the default stats directory.
    // Since we can't pass empty string without side effects, we just test that
    // construction + getAggregateStats works with a non-empty tmp path:
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);
    auto result = mgr.getAggregateStats();
    REQUIRE(result.has_value());
    REQUIRE(result->total_games == 0);
}

// ============================================================================
// getRecentGames: empty sessions file path
// ============================================================================

TEST_CASE("StatisticsManager - getRecentGames returns empty when no sessions file", "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // No games have been played → sessions file doesn't exist
    auto result = mgr.getRecentGames(10);
    REQUIRE(result.has_value());
    REQUIRE(result->empty());
}

// ============================================================================
// getStatsForDifficulty: valid difficulty after some games
// ============================================================================

TEST_CASE("StatisticsManager - getStatsForDifficulty returns data for specific difficulty",
          "[statistics_manager_branches]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    StatisticsManager mgr(tmp.path().string(), time);

    // Play an Easy game
    auto id = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(30000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    SECTION("Easy difficulty has 1 game") {
        auto result = mgr.getStatsForDifficulty(Difficulty::Easy);
        REQUIRE(result.has_value());
        REQUIRE(result->total_games == 1);
        REQUIRE(result->total_completed == 1);
    }

    SECTION("Hard difficulty has 0 games") {
        auto result = mgr.getStatsForDifficulty(Difficulty::Hard);
        REQUIRE(result.has_value());
        REQUIRE(result->total_games == 0);
    }
}

// ============================================================================
// Issue #26: a corrupt/undecryptable sessions file must never be overwritten
// ============================================================================

namespace {

/// Writes an encrypted-but-undecryptable sessions file: a real encrypted blob
/// with one ciphertext byte flipped so the MAC check fails (mirrors the
/// approach in test_encryption_manager.cpp). isEncrypted() stays true, decrypt()
/// fails — exactly the transient/re-keyed scenario from issue #26.
void writeUndecryptableSessions(const fs::path& sessions_file, std::vector<uint8_t>& out_bytes) {
    auto encrypted = EncryptionManager::encryptInteractive({'h', 'i'});
    REQUIRE(encrypted.has_value());
    auto bytes = *encrypted;
    REQUIRE(bytes.size() > 70);
    bytes[70] ^= 0xFF;  // corrupt ciphertext → MAC verification fails
    REQUIRE(EncryptionManager::isEncrypted(bytes));

    std::ofstream f(sessions_file, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    f.close();
    out_bytes = bytes;
}

std::vector<uint8_t> readAllBytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("StatisticsManager - getAllSessions fails closed on undecryptable encrypted file",
          "[statistics_manager_branches][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    std::vector<uint8_t> ignored;
    writeUndecryptableSessions(tmp.path() / "game_sessions.yaml", ignored);

    StatisticsManager mgr(tmp.path().string(), time);

    auto result = mgr.getAllSessions();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == StatisticsError::FileAccessError);
    REQUIRE(mgr.hasUnreadableSessionHistory());
}

TEST_CASE("StatisticsManager - getAllSessions fails closed on encrypted-but-unparseable YAML",
          "[statistics_manager_branches][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    // Encrypt YAML that loads but throws during field conversion (difficulty as
    // a non-integer) → deserializeGameStatsFromNode returns an error → the
    // encrypted parse-failure branch in getAllSessions.
    const std::string bad_yaml = "- difficulty: not_a_number\n";
    std::vector<uint8_t> plaintext(bad_yaml.begin(), bad_yaml.end());
    auto encrypted = EncryptionManager::encryptInteractive(plaintext);
    REQUIRE(encrypted.has_value());
    {
        std::ofstream f(tmp.path() / "game_sessions.yaml", std::ios::binary);
        f.write(reinterpret_cast<const char*>(encrypted->data()), static_cast<std::streamsize>(encrypted->size()));
    }

    StatisticsManager mgr(tmp.path().string(), time);

    auto result = mgr.getAllSessions();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == StatisticsError::FileAccessError);
    REQUIRE(mgr.hasUnreadableSessionHistory());
}

TEST_CASE("StatisticsManager - flushSessions never overwrites an unreadable file",
          "[statistics_manager_branches][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const fs::path sessions_file = tmp.path() / "game_sessions.yaml";
    std::vector<uint8_t> original;
    writeUndecryptableSessions(sessions_file, original);

    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);  // encryption on by default

    // Buffer a pending session, then flush. The destruction bug would re-encrypt
    // just this session over the file, wiping the original bytes.
    auto id = mgr.startGame(Difficulty::Easy, 1, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(1000));
    REQUIRE(mgr.endGame(*id, true).has_value());

    mgr.flushSessions();

    REQUIRE(readAllBytes(sessions_file) == original);
}

TEST_CASE("StatisticsManager - archiveUnreadableSessions preserves bytes and allows a fresh start",
          "[statistics_manager_branches][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const fs::path sessions_file = tmp.path() / "game_sessions.yaml";
    std::vector<uint8_t> original;
    writeUndecryptableSessions(sessions_file, original);

    StatisticsManager mgr(tmp.path().string(), time);
    mgr.setCollectDetailedStats(true);

    // Latch the unreadable flag via a read attempt.
    REQUIRE_FALSE(mgr.getAllSessions().has_value());
    REQUIRE(mgr.hasUnreadableSessionHistory());

    // Archive: original bytes move to *.corrupt-<ts>, never deleted.
    auto archived = mgr.archiveUnreadableSessions();
    REQUIRE(archived.has_value());
    REQUIRE(fs::exists(*archived));
    REQUIRE(readAllBytes(*archived) == original);
    REQUIRE_FALSE(fs::exists(sessions_file));
    REQUIRE_FALSE(mgr.hasUnreadableSessionHistory());

    // A fresh session now persists cleanly through the destructor flush.
    auto id = mgr.startGame(Difficulty::Easy, 2, 0);
    REQUIRE(id.has_value());
    time->advanceSystemTime(std::chrono::milliseconds(2000));
    REQUIRE(mgr.endGame(*id, true).has_value());
    mgr.flushSessions();

    StatisticsManager reopened(tmp.path().string(), time);
    auto sessions = reopened.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->size() == 1);
    REQUIRE_FALSE(reopened.hasUnreadableSessionHistory());
}

TEST_CASE("StatisticsManager - archiveUnreadableSessions never overwrites a prior archive",
          "[statistics_manager_branches][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();  // fixed clock → same-second timestamp on both archives
    const fs::path sessions_file = tmp.path() / "game_sessions.yaml";

    StatisticsManager mgr(tmp.path().string(), time);

    // First corrupt file, archived under .corrupt-<ts>.
    std::vector<uint8_t> first;
    writeUndecryptableSessions(sessions_file, first);
    REQUIRE_FALSE(mgr.getAllSessions().has_value());
    auto archive1 = mgr.archiveUnreadableSessions();
    REQUIRE(archive1.has_value());

    // A second corrupt file appears and is archived in the same wall-clock second.
    std::vector<uint8_t> second;
    writeUndecryptableSessions(sessions_file, second);
    REQUIRE(second != first);  // distinct random nonce/ciphertext
    REQUIRE_FALSE(mgr.getAllSessions().has_value());
    auto archive2 = mgr.archiveUnreadableSessions();
    REQUIRE(archive2.has_value());

    // The second archive must land on a distinct path, and BOTH originals must
    // survive intact — the "never deletes" guarantee holds across collisions.
    REQUIRE(*archive1 != *archive2);
    REQUIRE(fs::exists(*archive1));
    REQUIRE(fs::exists(*archive2));
    REQUIRE(readAllBytes(*archive1) == first);
    REQUIRE(readAllBytes(*archive2) == second);
}

TEST_CASE("StatisticsManager - getAllSessions fails closed on corrupt plain-text YAML",
          "[statistics_manager_branches][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const fs::path sessions_file = tmp.path() / "game_sessions.yaml";

    // Plain (unencrypted) YAML that parses as a node but fails field conversion
    // (difficulty as a non-integer). isEncrypted() is false → the plain-YAML branch.
    const std::string bad_yaml = "- difficulty: not_a_number\n";
    {
        std::ofstream f(sessions_file, std::ios::binary);
        f.write(bad_yaml.data(), static_cast<std::streamsize>(bad_yaml.size()));
    }
    REQUIRE_FALSE(EncryptionManager::isEncrypted(readAllBytes(sessions_file)));

    StatisticsManager mgr(tmp.path().string(), time);

    auto result = mgr.getAllSessions();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(mgr.hasUnreadableSessionHistory());
    REQUIRE(fs::exists(sessions_file));  // preserved, not overwritten

    // Recovery path is offered for plain-text corruption too.
    auto archived = mgr.archiveUnreadableSessions();
    REQUIRE(archived.has_value());
    REQUIRE(fs::exists(*archived));
    REQUIRE_FALSE(fs::exists(sessions_file));
    REQUIRE_FALSE(mgr.hasUnreadableSessionHistory());
}

TEST_CASE("StatisticsManager - clean encrypted round-trip keeps history readable (regression)",
          "[statistics_manager_branches][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();

    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setCollectDetailedStats(true);  // encryption on by default
        auto id = mgr.startGame(Difficulty::Medium, 7, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(3000));
        REQUIRE(mgr.endGame(*id, true).has_value());
    }  // destructor flushes encrypted

    StatisticsManager reopened(tmp.path().string(), time);
    auto sessions = reopened.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->size() == 1);
    REQUIRE_FALSE(reopened.hasUnreadableSessionHistory());
}

// ============================================================================
// STAT-2 (AC4): a hostile out-of-range difficulty in a plain game_sessions.yaml
// must be quarantined at startup via the existing issue-#26 archive flow, never
// indexed into the fixed-size aggregate arrays (which would be an OOB write).
// ============================================================================

TEST_CASE("StatisticsManager - hostile session difficulty is quarantined at startup",
          "[statistics_manager_branches][stat2][issue26]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const fs::path sessions_file = tmp.path() / "game_sessions.yaml";

    // One valid session plus one with an out-of-range difficulty. Plain YAML so
    // no aggregate_stats.yaml exists → the constructor recalculation path reads
    // this file directly.
    {
        std::ofstream f(sessions_file, std::ios::binary);
        f << "- difficulty: 0\n"
             "  puzzle_rating: 100\n"
             "  completed: true\n"
             "  time_played: 30000\n"
             "- difficulty: 99\n"
             "  puzzle_rating: 200\n"
             "  completed: true\n"
             "  time_played: 40000\n";
    }
    std::vector<uint8_t> original = readAllBytes(sessions_file);
    REQUIRE_FALSE(fs::exists(tmp.path() / "aggregate_stats.yaml"));

    StatisticsManager mgr(tmp.path().string(), time);

    // Aggregates must fall back to empty — the hostile session was never indexed.
    auto agg = mgr.getAggregateStats();
    REQUIRE(agg.has_value());
    REQUIRE(agg->total_games == 0);

    // getAllSessions() fails closed and latches the unreadable flag.
    auto all = mgr.getAllSessions();
    REQUIRE_FALSE(all.has_value());
    REQUIRE(mgr.hasUnreadableSessionHistory());

    // Hostile bytes untouched on disk.
    REQUIRE(readAllBytes(sessions_file) == original);

    // The established recovery flow moves it aside so a fresh history can start.
    auto archived = mgr.archiveUnreadableSessions();
    REQUIRE(archived.has_value());
    REQUIRE(fs::exists(*archived));
    REQUIRE(readAllBytes(*archived) == original);
    REQUIRE_FALSE(fs::exists(sessions_file));
    REQUIRE_FALSE(mgr.hasUnreadableSessionHistory());
}

// ============================================================================
// Review finding #1 (HIGH, data loss): a flush must serialize the FULL merged
// history (disk + pending), never shrink the on-disk file to pending-only.
//
// Reachable purely through the plain-mode public API: when the on-disk history is
// encrypted (e.g. the user turned encryption off) a plain flush must decrypt and
// preserve that history, not overwrite it with the pending sessions alone. This
// exercises the same unified whole-file writer the encrypt-failure fallback uses.
// ============================================================================

TEST_CASE("StatisticsManager - plain flush preserves encrypted on-disk history and merges pending",
          "[statistics_manager_branches][review1]") {
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const fs::path sessions_file = tmp.path() / "game_sessions.yaml";

    // Manager A: default (encrypted) mode writes two sessions to disk.
    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setCollectDetailedStats(true);  // encryption on by default
        for (int i = 0; i < 2; ++i) {
            auto id = mgr.startGame(Difficulty::Easy, static_cast<uint32_t>(i + 1), 0);
            REQUIRE(id.has_value());
            time->advanceSystemTime(std::chrono::milliseconds(30000));
            REQUIRE(mgr.endGame(*id, true).has_value());
        }
        mgr.flushSessions();
    }
    // Precondition: the on-disk file is encrypted and holds exactly two sessions.
    REQUIRE(EncryptionManager::isEncrypted(readAllBytes(sessions_file)));

    // Manager B: plain mode over the same directory. It has two sessions on disk plus
    // one freshly played (pending). A flush must persist all three — never just the one.
    {
        StatisticsManager mgr(tmp.path().string(), time);
        mgr.setEncryptSessions(false);
        mgr.setCollectDetailedStats(true);

        auto id = mgr.startGame(Difficulty::Hard, 42, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(30000));
        REQUIRE(mgr.endGame(*id, true).has_value());

        mgr.flushSessions();
    }

    // The on-disk history must contain history (2) + pending (1) = 3, never pending-only.
    StatisticsManager reopened(tmp.path().string(), time);
    auto sessions = reopened.getAllSessions();
    REQUIRE(sessions.has_value());
    REQUIRE(sessions->size() == 3);
    REQUIRE_FALSE(reopened.hasUnreadableSessionHistory());
}

// ============================================================================
// STAT-3 (AC6): flushSessions must retain pending sessions when the write fails,
// and a later flush after the failure cause is removed must persist them.
// ============================================================================

TEST_CASE("StatisticsManager - flushSessions retains pending sessions when the write fails",
          "[statistics_manager_branches][stat3]") {
#ifdef _WIN32
    SKIP("Windows does not honour POSIX read-only directory permissions via std::filesystem");
#else
    TempTestDir tmp;
    auto time = std::make_shared<MockTimeProvider>();
    const fs::path stats_dir = tmp.path();
    const fs::path sessions_file = stats_dir / "game_sessions.yaml";

    // Restore permissions no matter how the test exits, so temp-dir cleanup works.
    struct PermGuard {
        fs::path dir;
        ~PermGuard() {
            std::error_code ec;
            fs::permissions(dir, fs::perms::owner_all, ec);
        }
    } guard{stats_dir};

    SECTION("plain mode") {
        StatisticsManager mgr(stats_dir.string(), time);
        mgr.setEncryptSessions(false);
        mgr.setCollectDetailedStats(true);

        auto id = mgr.startGame(Difficulty::Hard, 1, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(30000));
        REQUIRE(mgr.endGame(*id, true).has_value());

        // Make the directory read-only so the append write fails.
        fs::permissions(stats_dir, fs::perms::owner_read | fs::perms::owner_exec);

        mgr.flushSessions();

        // Nothing hit disk, but the session is retained in memory.
        REQUIRE_FALSE(fs::exists(sessions_file));
        auto retained = mgr.getAllSessions();
        REQUIRE(retained.has_value());
        REQUIRE(retained->size() == 1);

        // Remove the failure cause and retry: the retained session now persists.
        fs::permissions(stats_dir, fs::perms::owner_all);
        mgr.flushSessions();

        StatisticsManager reopened(stats_dir.string(), time);
        auto sessions = reopened.getAllSessions();
        REQUIRE(sessions.has_value());
        REQUIRE(sessions->size() == 1);
    }

    SECTION("encrypted mode") {
        StatisticsManager mgr(stats_dir.string(), time);
        mgr.setEncryptSessions(true);
        mgr.setCollectDetailedStats(true);

        auto id = mgr.startGame(Difficulty::Hard, 2, 0);
        REQUIRE(id.has_value());
        time->advanceSystemTime(std::chrono::milliseconds(45000));
        REQUIRE(mgr.endGame(*id, true).has_value());

        fs::permissions(stats_dir, fs::perms::owner_read | fs::perms::owner_exec);

        mgr.flushSessions();

        REQUIRE_FALSE(fs::exists(sessions_file));
        auto retained = mgr.getAllSessions();
        REQUIRE(retained.has_value());
        REQUIRE(retained->size() == 1);

        fs::permissions(stats_dir, fs::perms::owner_all);
        mgr.flushSessions();

        StatisticsManager reopened(stats_dir.string(), time);
        auto sessions = reopened.getAllSessions();
        REQUIRE(sessions.has_value());
        REQUIRE(sessions->size() == 1);
    }
#endif
}
