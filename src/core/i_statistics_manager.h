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

#include "core/constants.h"
#include "i_puzzle_generator.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace sudoku::core {

/// Statistics for a single game session
///
/// A session covers one *play segment*. Quitting and resuming the same puzzle produces several
/// records, and each one carries the progress that preceded it (see seedSessionProgress) so the
/// per-game view — play time, hint budget, personal bests — stays truthful on its own. The
/// carried_* fields below record how much of that total was already banked by an earlier segment,
/// which is what lets the lifetime aggregate count each unit of play exactly once. They are 0 for
/// a normally-started game and for every session recorded before they existed, so a record with no
/// carried progress is wholly its own segment's play.
struct GameStats {
    Difficulty difficulty{Difficulty::Medium};
    double puzzle_rating{0.0};  // Sudoku Explainer rating (SE 1.0-12.0 scale)
    std::chrono::milliseconds time_played{0};
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    bool completed{false};
    int moves_made{0};
    int hints_used{0};
    int mistakes{0};
    uint32_t puzzle_seed{0};

    /// True when this session resumed a saved game rather than starting a new one. The puzzle was
    /// already counted as attempted by the segment that began it, so a continuation must not
    /// increment games_played / total_games (nor contribute a second copy of the same puzzle's
    /// rating to average_ratings, which divides by that count).
    bool continued_from_save{false};

    /// Progress inherited from earlier segments of the same puzzle. Subtracted from this record's
    /// totals when folding it into the lifetime aggregate.
    int carried_moves{0};
    int carried_hints{0};
    int carried_mistakes{0};
    std::chrono::milliseconds carried_time_played{0};
};

/// Aggregate statistics across multiple games
struct AggregateStats {
    // Per difficulty level. 64-bit so lifetime cumulative counts can't wrap (issue #27).
    std::array<int64_t, DIFFICULTY_COUNT> games_played{0, 0, 0, 0, 0};  // Easy, Medium, Hard, Expert, Master
    std::array<int64_t, DIFFICULTY_COUNT> games_completed{0, 0, 0, 0, 0};
    std::array<std::chrono::milliseconds, DIFFICULTY_COUNT> best_times{
        std::chrono::milliseconds::max(), std::chrono::milliseconds::max(), std::chrono::milliseconds::max(),
        std::chrono::milliseconds::max(), std::chrono::milliseconds::max()};
    std::array<std::chrono::milliseconds, DIFFICULTY_COUNT> average_times{
        std::chrono::milliseconds{0}, std::chrono::milliseconds{0}, std::chrono::milliseconds{0},
        std::chrono::milliseconds{0}, std::chrono::milliseconds{0}};

    // Rating statistics per difficulty (SE scale)
    std::array<double, DIFFICULTY_COUNT> average_ratings{0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, DIFFICULTY_COUNT> min_ratings{
        std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    std::array<double, DIFFICULTY_COUNT> max_ratings{0.0, 0.0, 0.0, 0.0, 0.0};

    // Overall stats. 64-bit to avoid 2^31 overflow for long-lived players (issue #27).
    int64_t total_games{0};
    int64_t total_completed{0};
    int64_t total_moves{0};
    int64_t total_hints{0};
    int64_t total_mistakes{0};
    std::chrono::milliseconds total_time_played{0};

    // Streaks
    int64_t current_win_streak{0};
    int64_t best_win_streak{0};

    // First played
    std::chrono::system_clock::time_point first_game_date;
    std::chrono::system_clock::time_point last_game_date;
};

/// Upper bound for a progress counter carried in from a save file. Far above anything real play
/// produces (an 81-cell board with heavy erasing stays in the thousands), yet low enough that a
/// hostile value cannot overflow the int64 aggregates it later feeds, nor render as an absurd
/// figure in the game-info dialog. Shared by IStatisticsManager::seedSessionProgress and the
/// ViewModel's restore path, which are the only two sinks these unvalidated fields reach.
inline constexpr int MAX_PROGRESS_COUNTER = 1'000'000;

/// Error types for statistics operations
enum class StatisticsError : std::uint8_t {
    InvalidGameData,
    FileAccessError,
    SerializationError,
    InvalidDifficulty,
    GameNotStarted,
    GameAlreadyEnded
};

/// Interface for managing game statistics and progress tracking
class IStatisticsManager {
public:
    virtual ~IStatisticsManager() = default;

    /// Starts tracking a new game session
    /// @param difficulty Difficulty level of the game
    /// @param puzzle_seed Seed used to generate the puzzle
    /// @param puzzle_rating Sudoku Explainer rating (SE 1.0-12.0 scale)
    /// @return Game session ID or error
    [[nodiscard]] virtual std::expected<uint64_t, StatisticsError>
    startGame(Difficulty difficulty, uint32_t puzzle_seed, double puzzle_rating = 0.0) = 0;

    /// Records a move made during the game
    /// @param game_id Active game session ID
    /// @param is_mistake True if the move was incorrect
    [[nodiscard]] virtual std::expected<void, StatisticsError> recordMove(uint64_t game_id,
                                                                          bool is_mistake = false) = 0;

    /// Records a hint used during the game
    /// @param game_id Active game session ID
    [[nodiscard]] virtual std::expected<void, StatisticsError> recordHint(uint64_t game_id) = 0;

    /// Seeds a freshly started session with progress carried over from a saved game.
    ///
    /// Restore-only contract: a resumed game is a continuation, not a new game, so its session must
    /// start from the counters and play time the player already accumulated. Without this, a resumed
    /// game's hint budget resets on every relaunch and its completion under-reports play time
    /// (findings SAVE-2 / SAVE-3). Call once, immediately after startGame().
    ///
    /// All counters are clamped to a sane non-negative range — the values originate in a save file
    /// and are not otherwise validated, so a hostile file must not be able to drive a hint budget
    /// negative. @p prior_play_time is stored as the session's starting play time; endGame() adds
    /// the active span on top of it.
    ///
    /// Also marks the session as a continuation and records the clamped values as GameStats
    /// carried progress, so the lifetime aggregate can bank this segment's own play only.
    ///
    /// @param game_id Active game session ID
    /// @param moves_made Moves already made before the save
    /// @param hints_used Hints already consumed before the save
    /// @param mistakes Mistakes already made before the save
    /// @param prior_play_time Play time accumulated before the save
    /// @return Success, or GameNotStarted if @p game_id is not an active session
    [[nodiscard]] virtual std::expected<void, StatisticsError>
    seedSessionProgress(uint64_t game_id, int moves_made, int hints_used, int mistakes,
                        std::chrono::milliseconds prior_play_time) = 0;

    /// Ends a game session
    /// @param game_id Active game session ID
    /// @param completed True if game was completed successfully
    /// @return Final game statistics
    [[nodiscard]] virtual std::expected<GameStats, StatisticsError> endGame(uint64_t game_id,
                                                                            bool completed = false) = 0;

    /// Gets statistics for a specific active game session
    /// @param game_id Active game session ID
    /// @return Game statistics for that session
    [[nodiscard]] virtual std::expected<GameStats, StatisticsError> getGameStats(uint64_t game_id) const = 0;

    /// Gets aggregate statistics across all games
    /// @return Aggregate statistics or error
    [[nodiscard]] virtual std::expected<AggregateStats, StatisticsError> getAggregateStats() const = 0;

    /// Gets statistics for a specific difficulty level
    /// @param difficulty Difficulty level to query
    /// @return Statistics for that difficulty
    [[nodiscard]] virtual std::expected<AggregateStats, StatisticsError>
    getStatsForDifficulty(Difficulty difficulty) const = 0;

    /// Gets all stored game sessions (disk + pending), sorted newest-first.
    /// @return All session records or error
    [[nodiscard]] virtual std::expected<std::vector<GameStats>, StatisticsError> getAllSessions() const = 0;

    /// Gets the N most recent game sessions, sorted newest-first.
    /// @param count Number of recent games to return (must be > 0)
    /// @return Up to @p count session records or error
    [[nodiscard]] virtual std::expected<std::vector<GameStats>, StatisticsError> getRecentGames(int count) const = 0;

    /// Gets personal best times for each difficulty
    /// @return Array of best times [Easy, Medium, Hard, Expert, Master]
    [[nodiscard]] virtual std::array<std::chrono::milliseconds, DIFFICULTY_COUNT> getBestTimes() const = 0;

    /// Calculates completion percentage for each difficulty
    /// @return Array of completion percentages [Easy, Medium, Hard, Expert, Master]
    [[nodiscard]] virtual std::array<double, DIFFICULTY_COUNT> getCompletionRates() const = 0;

    /// Exports statistics to a file
    /// @param file_path Path to export file
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, StatisticsError> exportStats(const std::string& file_path) const = 0;

    /// Exports aggregate statistics to CSV (per-difficulty summary)
    /// @param file_path Path to CSV export file
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, StatisticsError>
    exportAggregateStatsCsv(const std::string& file_path) const = 0;

    /// Exports all game sessions to CSV (historical data)
    /// @param file_path Path to CSV export file
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, StatisticsError>
    exportGameSessionsCsv(const std::string& file_path) const = 0;

    /// Imports statistics from a file
    /// @param file_path Path to import file
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, StatisticsError> importStats(const std::string& file_path) = 0;

    /// Resets all statistics (for testing or user preference)
    virtual void resetAllStats() = 0;

    /// Enable/disable detailed per-session data collection.
    /// When disabled, endGame() still updates aggregate stats but does not persist individual sessions.
    virtual void setCollectDetailedStats(bool enabled) = 0;

    /// Enable/disable encryption of the session history file.
    virtual void setEncryptSessions(bool enabled) = 0;

    /// Delete all stored per-session history data.
    [[nodiscard]] virtual std::expected<void, StatisticsError> deleteSessionHistory() = 0;

    /// Flush any buffered sessions to disk (called on app exit).
    virtual void flushSessions() = 0;

    /// Whether the on-disk session history could not be read on the last attempt
    /// (e.g. an encrypted file that no longer decrypts after a hostname/machine-id
    /// change, or a partial read). While true, the manager refuses to overwrite the
    /// file so the original bytes are never destroyed. The flag is updated on every
    /// session read and cleared by a successful read or by archiveUnreadableSessions().
    [[nodiscard]] virtual bool hasUnreadableSessionHistory() const = 0;

    /// Move an unreadable session-history file aside to "<name>.corrupt-<unix_ts>" so a
    /// fresh history can be started without destroying the original (recoverable) bytes.
    /// Never deletes. No-op (returns the live path) when the history is readable or absent.
    /// @return Path the file was archived to, or an error if the rename failed.
    [[nodiscard]] virtual std::expected<std::filesystem::path, StatisticsError> archiveUnreadableSessions() = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    IStatisticsManager() = default;
    IStatisticsManager(const IStatisticsManager&) = default;
    IStatisticsManager& operator=(const IStatisticsManager&) = default;
    IStatisticsManager(IStatisticsManager&&) = default;
    IStatisticsManager& operator=(IStatisticsManager&&) = default;
};

}  // namespace sudoku::core