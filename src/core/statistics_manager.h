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

#pragma once

#include "core/i_puzzle_generator.h"
#include "encryption_manager.h"
#include "i_statistics_manager.h"
#include "i_time_provider.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sudoku::core {

/// YAML-based implementation of IStatisticsManager
/// Handles game statistics tracking and persistence
class StatisticsManager : public IStatisticsManager {
public:
    explicit StatisticsManager(std::filesystem::path stats_directory = {},
                               std::shared_ptr<ITimeProvider> time_provider = std::make_shared<SystemTimeProvider>());
    ~StatisticsManager() override;
    StatisticsManager(const StatisticsManager&) = delete;
    StatisticsManager& operator=(const StatisticsManager&) = delete;
    StatisticsManager(StatisticsManager&&) = delete;
    StatisticsManager& operator=(StatisticsManager&&) = delete;

    // Game session management
    std::expected<uint64_t, StatisticsError> startGame(Difficulty difficulty, uint32_t puzzle_seed,
                                                       double puzzle_rating = 0.0) override;

    std::expected<void, StatisticsError> recordMove(uint64_t game_id, bool is_mistake) override;

    std::expected<void, StatisticsError> recordHint(uint64_t game_id) override;

    std::expected<GameStats, StatisticsError> endGame(uint64_t game_id, bool completed) override;

    // Statistics retrieval
    std::expected<GameStats, StatisticsError> getGameStats(uint64_t game_id) const override;

    std::expected<AggregateStats, StatisticsError> getAggregateStats() const override;

    std::expected<AggregateStats, StatisticsError> getStatsForDifficulty(Difficulty difficulty) const override;

    [[nodiscard]] std::expected<std::vector<GameStats>, StatisticsError> getAllSessions() const override;
    [[nodiscard]] std::expected<std::vector<GameStats>, StatisticsError> getRecentGames(int count) const override;

    std::array<std::chrono::milliseconds, 5> getBestTimes() const override;
    std::array<double, 5> getCompletionRates() const override;

    // Import/Export
    std::expected<void, StatisticsError> exportStats(const std::string& file_path) const override;

    [[nodiscard]] std::expected<void, StatisticsError>
    exportAggregateStatsCsv(const std::string& file_path) const override;

    [[nodiscard]] std::expected<void, StatisticsError>
    exportGameSessionsCsv(const std::string& file_path) const override;

    std::expected<void, StatisticsError> importStats(const std::string& file_path) override;

    void resetAllStats() override;

    void setCollectDetailedStats(bool enabled) override;
    void setEncryptSessions(bool enabled) override;
    [[nodiscard]] std::expected<void, StatisticsError> deleteSessionHistory() override;
    void flushSessions() override;

private:
    // Dependencies
    std::shared_ptr<ITimeProvider> time_provider_;
    std::unique_ptr<EncryptionManager> encryption_manager_;

    std::filesystem::path stats_directory_;
    std::filesystem::path stats_file_;
    std::filesystem::path sessions_file_;

    // Active game sessions
    std::unordered_map<uint64_t, GameStats> active_sessions_;
    uint64_t next_session_id_{1};

    // Detailed stats settings
    bool collect_detailed_stats_{false};
    bool encrypt_sessions_{true};
    std::vector<GameStats> pending_sessions_;

    // Cached aggregate statistics
    mutable AggregateStats cached_stats_;
    mutable bool stats_cache_valid_{false};

    // Helper methods
    uint64_t generateSessionId();
    std::expected<void, StatisticsError> ensureDirectoryExists() const;

    // Persistence
    std::expected<void, StatisticsError> loadStatistics();
    std::expected<void, StatisticsError> saveStatistics() const;
    std::expected<void, StatisticsError> saveGameSession(const GameStats& stats) const;

    // YAML serialization and CSV export are in statistics_serializer.h

    // Statistics calculation
    void updateAggregateStats(const GameStats& completed_game) const;
    void invalidateStatsCache();
    std::expected<void, StatisticsError> recalculateAggregateStats() const;

    // Atomic file write (write to .tmp, then rename)
    static std::expected<void, StatisticsError> atomicWriteFile(const std::filesystem::path& target,
                                                                const std::vector<uint8_t>& data);

    // Validation
    bool isValidGameSession(uint64_t game_id) const;
    static bool isValidDifficulty(Difficulty difficulty);
};

}  // namespace sudoku::core