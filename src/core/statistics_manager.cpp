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

#include "statistics_manager.h"

#include "core/constants.h"
#include "core/i_statistics_manager.h"
#include "core/i_time_provider.h"
#include "infrastructure/app_directory_manager.h"
#include "statistics_serializer.h"

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <exception>
#include <fstream>
#include <ratio>
#include <sstream>
#include <utility>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::core {

StatisticsManager::StatisticsManager(std::filesystem::path stats_directory,
                                     std::shared_ptr<ITimeProvider> time_provider)
    : time_provider_(std::move(time_provider)), encryption_manager_(std::make_unique<EncryptionManager>()) {
    if (stats_directory.empty()) {
        // Use platform-appropriate default directory
        stats_directory_ =
            infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Statistics);
    } else {
        stats_directory_ = std::move(stats_directory);
    }

    stats_file_ = stats_directory_ / "aggregate_stats.yaml";
    sessions_file_ = stats_directory_ / "game_sessions.yaml";

    auto result = ensureDirectoryExists();
    if (!result) {
        spdlog::warn("Failed to create stats directory: {}", stats_directory_.string());
    }

    // Load existing statistics
    auto load_result = loadStatistics();
    if (!load_result) {
        spdlog::warn("Failed to load existing statistics, starting fresh");
        // Initialize empty stats
        cached_stats_ = AggregateStats{};
        stats_cache_valid_ = true;
    }
}

StatisticsManager::~StatisticsManager() {
    // End any active sessions as incomplete
    for (auto& [session_id, stats] : active_sessions_) {
        stats.end_time = time_provider_->systemNow();
        stats.time_played = std::chrono::duration_cast<std::chrono::milliseconds>(stats.end_time - stats.start_time);
        stats.completed = false;

        if (collect_detailed_stats_) {
            pending_sessions_.push_back(stats);
        }
        updateAggregateStats(stats);
    }

    try {
        flushSessions();
    } catch (const std::exception& e) {
        spdlog::error("Failed to flush sessions in destructor: {}", e.what());
    }
    std::ignore = saveStatistics();
}

std::expected<uint64_t, StatisticsError> StatisticsManager::startGame(Difficulty difficulty, uint32_t puzzle_seed,
                                                                      double puzzle_rating) {
    if (!isValidDifficulty(difficulty)) {
        return std::unexpected(StatisticsError::InvalidDifficulty);
    }

    uint64_t session_id = generateSessionId();

    GameStats stats;
    stats.difficulty = difficulty;
    stats.puzzle_seed = puzzle_seed;
    stats.puzzle_rating = puzzle_rating;
    stats.start_time = time_provider_->systemNow();
    stats.completed = false;
    stats.moves_made = 0;
    stats.hints_used = 0;
    stats.mistakes = 0;

    active_sessions_[session_id] = stats;

    spdlog::debug("Started game session {} with difficulty {} (rating: {})", session_id, static_cast<int>(difficulty),
                  puzzle_rating);

    return session_id;
}

std::expected<void, StatisticsError> StatisticsManager::recordMove(uint64_t game_id, bool is_mistake) {
    if (!isValidGameSession(game_id)) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    auto& stats = active_sessions_[game_id];
    stats.moves_made++;
    if (is_mistake) {
        stats.mistakes++;
    }

    spdlog::debug("Recorded move for session {}: moves={}, mistakes={}", game_id, stats.moves_made, stats.mistakes);

    return {};
}

std::expected<void, StatisticsError> StatisticsManager::recordHint(uint64_t game_id) {
    if (!isValidGameSession(game_id)) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    auto& stats = active_sessions_[game_id];
    stats.hints_used++;

    spdlog::debug("Recorded hint for session {}: hints={}", game_id, stats.hints_used);

    return {};
}

std::expected<GameStats, StatisticsError> StatisticsManager::endGame(uint64_t game_id, bool completed) {
    if (!isValidGameSession(game_id)) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    auto& stats = active_sessions_[game_id];
    stats.end_time = time_provider_->systemNow();
    stats.time_played = std::chrono::duration_cast<std::chrono::milliseconds>(stats.end_time - stats.start_time);
    stats.completed = completed;

    // Buffer the session if collection is enabled (flushed on app exit)
    if (collect_detailed_stats_) {
        pending_sessions_.push_back(stats);
    }

    // Update aggregate statistics
    updateAggregateStats(stats);

    // Save updated aggregate stats
    auto save_agg_result = saveStatistics();
    if (!save_agg_result) {
        spdlog::warn("Failed to save aggregate statistics");
    }

    GameStats result = stats;
    active_sessions_.erase(game_id);

    spdlog::info("Ended game session {}: completed={}, time={}ms, moves={}", game_id, completed,
                 result.time_played.count(), result.moves_made);

    return result;
}

std::expected<GameStats, StatisticsError> StatisticsManager::getGameStats(uint64_t game_id) const {
    auto it = active_sessions_.find(game_id);
    if (it == active_sessions_.end()) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    return it->second;
}

std::expected<AggregateStats, StatisticsError> StatisticsManager::getAggregateStats() const {
    if (!stats_cache_valid_) {
        auto recalc_result = recalculateAggregateStats();
        if (!recalc_result) {
            return std::unexpected(recalc_result.error());
        }
    }

    return cached_stats_;
}

std::expected<AggregateStats, StatisticsError> StatisticsManager::getStatsForDifficulty(Difficulty difficulty) const {
    if (!isValidDifficulty(difficulty)) {
        return std::unexpected(StatisticsError::InvalidDifficulty);
    }

    auto all_stats_result = getAggregateStats();
    if (!all_stats_result) {
        return std::unexpected(all_stats_result.error());
    }

    const auto& all_stats = *all_stats_result;
    // Enum is 0-based (Easy=0, Medium=1, Hard=2, Expert=3), matches array indexing
    int diff_index = static_cast<int>(difficulty);

    AggregateStats difficulty_stats;
    difficulty_stats.games_played[diff_index] = all_stats.games_played[diff_index];
    difficulty_stats.games_completed[diff_index] = all_stats.games_completed[diff_index];
    difficulty_stats.best_times[diff_index] = all_stats.best_times[diff_index];
    difficulty_stats.average_times[diff_index] = all_stats.average_times[diff_index];

    // Calculate totals for this difficulty only
    difficulty_stats.total_games = all_stats.games_played[diff_index];
    difficulty_stats.total_completed = all_stats.games_completed[diff_index];

    return difficulty_stats;
}

std::expected<std::vector<GameStats>, StatisticsError> StatisticsManager::getAllSessions() const {
    try {
        std::vector<GameStats> sessions;

        if (std::filesystem::exists(sessions_file_)) {
            // Read file as binary to detect encryption
            std::ifstream file(sessions_file_, std::ios::binary);
            std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (EncryptionManager::isEncrypted(file_data)) {
                // Decrypt then parse YAML
                auto dec_result = EncryptionManager::decrypt(file_data);
                if (dec_result) {
                    std::string yaml_str(dec_result->begin(), dec_result->end());
                    YAML::Node root = YAML::Load(yaml_str);
                    auto parsed = statistics_serializer::deserializeGameStatsFromNode(root);
                    if (parsed) {
                        sessions = std::move(*parsed);
                    }
                } else {
                    spdlog::warn("Failed to decrypt sessions file, treating as empty");
                }
            } else {
                // Plain YAML
                auto result = statistics_serializer::deserializeGameStatsFromYaml(sessions_file_);
                if (!result) {
                    return std::unexpected(result.error());
                }
                sessions = std::move(*result);
            }
        }

        // Merge pending (in-memory) sessions
        sessions.insert(sessions.end(), pending_sessions_.begin(), pending_sessions_.end());

        // Sort by start time (newest first)
        std::ranges::sort(sessions,
                          [](const GameStats& lhs, const GameStats& rhs) { return lhs.start_time > rhs.start_time; });

        return sessions;

    } catch (const std::exception& e) {
        spdlog::error("Exception loading all sessions: {}", e.what());
        return std::unexpected(StatisticsError::FileAccessError);
    }
}

std::expected<std::vector<GameStats>, StatisticsError> StatisticsManager::getRecentGames(int count) const {
    auto all = getAllSessions();
    if (!all) {
        return all;
    }

    if (count > 0 && std::cmp_less(count, all->size())) {
        all->resize(count);
    }

    return all;
}

std::array<std::chrono::milliseconds, DIFFICULTY_COUNT> StatisticsManager::getBestTimes() const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return {};  // Return empty array on error
    }

    return stats_result->best_times;
}

std::array<double, DIFFICULTY_COUNT> StatisticsManager::getCompletionRates() const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return {};  // Return empty array on error
    }

    const auto& stats = *stats_result;
    std::array<double, DIFFICULTY_COUNT> rates{};

    for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
        if (stats.games_played[i] > 0) {
            rates[i] = static_cast<double>(stats.games_completed[i]) / stats.games_played[i];
        } else {
            rates[i] = 0.0;
        }
    }

    return rates;
}

std::expected<void, StatisticsError> StatisticsManager::exportStats(const std::string& file_path) const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return std::unexpected(stats_result.error());
    }

    try {
        auto serialize_result =
            statistics_serializer::serializeStatsToYaml(*stats_result, file_path, time_provider_->systemNow());
        if (!serialize_result) {
            return std::unexpected(serialize_result.error());
        }

        spdlog::info("Statistics exported to: {}", file_path);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats export: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::exportAggregateStatsCsv(const std::string& file_path) const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return std::unexpected(stats_result.error());
    }

    return statistics_serializer::exportAggregateStatsCsv(*stats_result, file_path);
}

std::expected<void, StatisticsError> StatisticsManager::exportGameSessionsCsv(const std::string& file_path) const {
    try {
        // Load all game sessions (handles both encrypted and plain YAML + pending)
        auto sessions_result = getAllSessions();
        if (!sessions_result) {
            return std::unexpected(sessions_result.error());
        }

        return statistics_serializer::exportGameSessionsCsv(*sessions_result, file_path);

    } catch (const std::exception& e) {
        spdlog::error("Exception during CSV export: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::importStats(const std::string& file_path) {
    try {
        if (!std::filesystem::exists(file_path)) {
            return std::unexpected(StatisticsError::FileAccessError);
        }

        auto import_result = statistics_serializer::deserializeStatsFromYaml(file_path);
        if (!import_result) {
            return std::unexpected(import_result.error());
        }

        // Merge imported stats with current stats
        const auto& imported_stats = *import_result;

        for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
            cached_stats_.games_played[i] += imported_stats.games_played[i];
            cached_stats_.games_completed[i] += imported_stats.games_completed[i];

            // Update best times if imported time is better
            if (imported_stats.best_times[i] < cached_stats_.best_times[i]) {
                cached_stats_.best_times[i] = imported_stats.best_times[i];
            }

            // Recalculate average times (simplified approach)
            if (cached_stats_.games_completed[i] > 0) {
                auto total_time = cached_stats_.average_times[i] * cached_stats_.games_completed[i] +
                                  imported_stats.average_times[i] * imported_stats.games_completed[i];
                cached_stats_.average_times[i] = total_time / cached_stats_.games_completed[i];
            }
        }

        // Update totals
        cached_stats_.total_games += imported_stats.total_games;
        cached_stats_.total_completed += imported_stats.total_completed;
        cached_stats_.total_moves += imported_stats.total_moves;
        cached_stats_.total_hints += imported_stats.total_hints;
        cached_stats_.total_mistakes += imported_stats.total_mistakes;
        cached_stats_.total_time_played += imported_stats.total_time_played;

        // Update streaks (take the better values)
        cached_stats_.current_win_streak =
            std::max(cached_stats_.current_win_streak, imported_stats.current_win_streak);
        cached_stats_.best_win_streak = std::max(cached_stats_.best_win_streak, imported_stats.best_win_streak);

        stats_cache_valid_ = true;

        auto save_result = saveStatistics();
        if (!save_result) {
            spdlog::warn("Failed to save merged statistics");
        }

        spdlog::info("Statistics imported from: {}", file_path);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats import: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

void StatisticsManager::resetAllStats() {
    // Clear active sessions
    active_sessions_.clear();

    // Reset aggregate stats
    cached_stats_ = AggregateStats{};
    stats_cache_valid_ = true;

    try {
        // Delete stats files
        if (std::filesystem::exists(stats_file_)) {
            std::filesystem::remove(stats_file_);
        }
        if (std::filesystem::exists(sessions_file_)) {
            std::filesystem::remove(sessions_file_);
        }

        spdlog::info("All statistics reset");

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats reset: {}", e.what());
    }
}

// Private helper methods

uint64_t StatisticsManager::generateSessionId() {
    return next_session_id_++;
}

std::expected<void, StatisticsError> StatisticsManager::ensureDirectoryExists() const {
    try {
        if (!std::filesystem::exists(stats_directory_)) {
            std::filesystem::create_directories(stats_directory_);
        }
        return {};
    } catch (const std::exception& e) {
        spdlog::error("Failed to create directory {}: {}", stats_directory_.string(), e.what());
        return std::unexpected(StatisticsError::FileAccessError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::loadStatistics() {
    try {
        if (std::filesystem::exists(stats_file_)) {
            auto stats_result = statistics_serializer::deserializeStatsFromYaml(stats_file_);
            if (stats_result) {
                cached_stats_ = *stats_result;
                stats_cache_valid_ = true;
                spdlog::debug("Loaded aggregate statistics from file");
            } else {
                spdlog::warn("Failed to load aggregate stats, will recalculate");
                auto recalc_result = recalculateAggregateStats();
                if (!recalc_result) {
                    return std::unexpected(recalc_result.error());
                }
            }
        } else {
            // No existing stats file, try to recalculate from sessions
            auto recalc_result = recalculateAggregateStats();
            if (!recalc_result) {
                // If recalculation fails, start with empty stats
                cached_stats_ = AggregateStats{};
                stats_cache_valid_ = true;
            }
        }

        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception loading statistics: {}", e.what());
        return std::unexpected(StatisticsError::FileAccessError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::saveStatistics() const {
    try {
        if (!stats_cache_valid_) {
            return std::unexpected(StatisticsError::InvalidGameData);
        }

        // Write to temp file, then atomically rename
        auto tmp_path = stats_file_;
        tmp_path += ".tmp";

        auto serialize_result =
            statistics_serializer::serializeStatsToYaml(cached_stats_, tmp_path, time_provider_->systemNow());
        if (!serialize_result) {
            std::error_code ec;
            std::filesystem::remove(tmp_path, ec);
            return std::unexpected(serialize_result.error());
        }

        std::error_code ec;
        std::filesystem::rename(tmp_path, stats_file_, ec);
        if (ec) {
            spdlog::error("Failed to rename aggregate stats: {}", ec.message());
            std::filesystem::remove(tmp_path, ec);
            return std::unexpected(StatisticsError::FileAccessError);
        }

        spdlog::debug("Saved aggregate statistics to file");
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception saving statistics: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::saveGameSession(const GameStats& stats) const {
    try {
        auto serialize_result = statistics_serializer::serializeGameStatsToYaml(stats, sessions_file_, true);
        if (!serialize_result) {
            return std::unexpected(serialize_result.error());
        }

        spdlog::debug("Saved game session to file");
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception saving game session: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

void StatisticsManager::updateAggregateStats(const GameStats& completed_game) const {
    // Enum is 0-based (Easy=0, Medium=1, Hard=2, Expert=3), matches array indexing
    int diff_index = static_cast<int>(completed_game.difficulty);

    // Update rating statistics BEFORE incrementing games_played (track for all games, not just completed)
    if (completed_game.puzzle_rating > 0.0) {
        // Get count BEFORE incrementing
        auto games_count_before = cached_stats_.games_played[diff_index];

        // Update min rating
        cached_stats_.min_ratings[diff_index] =
            std::min(cached_stats_.min_ratings[diff_index], completed_game.puzzle_rating);

        // Update max rating
        cached_stats_.max_ratings[diff_index] =
            std::max(cached_stats_.max_ratings[diff_index], completed_game.puzzle_rating);

        // Update average rating
        auto current_rating_average = cached_stats_.average_ratings[diff_index];
        cached_stats_.average_ratings[diff_index] =
            (current_rating_average * games_count_before + completed_game.puzzle_rating) / (games_count_before + 1);
    }

    // Update per-difficulty stats
    cached_stats_.games_played[diff_index]++;
    if (completed_game.completed) {
        cached_stats_.games_completed[diff_index]++;

        // Update best time
        if (completed_game.time_played < cached_stats_.best_times[diff_index]) {
            cached_stats_.best_times[diff_index] = completed_game.time_played;
        }

        // Update average time
        auto current_average = cached_stats_.average_times[diff_index];
        auto completed_count = cached_stats_.games_completed[diff_index];
        cached_stats_.average_times[diff_index] =
            (current_average * (completed_count - 1) + completed_game.time_played) / completed_count;
    }

    // Update overall stats
    cached_stats_.total_games++;
    if (completed_game.completed) {
        cached_stats_.total_completed++;
        cached_stats_.current_win_streak++;
        cached_stats_.best_win_streak = std::max(cached_stats_.best_win_streak, cached_stats_.current_win_streak);
    } else {
        cached_stats_.current_win_streak = 0;
    }

    cached_stats_.total_moves += completed_game.moves_made;
    cached_stats_.total_hints += completed_game.hints_used;
    cached_stats_.total_mistakes += completed_game.mistakes;
    cached_stats_.total_time_played += completed_game.time_played;

    stats_cache_valid_ = true;
}

void StatisticsManager::invalidateStatsCache() {
    stats_cache_valid_ = false;
}

std::expected<void, StatisticsError> StatisticsManager::recalculateAggregateStats() const {
    try {
        if (!std::filesystem::exists(sessions_file_)) {
            // No sessions file, start with empty stats
            cached_stats_ = AggregateStats{};
            stats_cache_valid_ = true;
            return {};
        }

        // Read sessions (handles both encrypted and plain YAML)
        auto sessions_result = getAllSessions();
        if (!sessions_result) {
            return std::unexpected(sessions_result.error());
        }

        const auto& sessions = *sessions_result;

        // Reset stats
        cached_stats_ = AggregateStats{};

        // Recalculate from all sessions
        for (const auto& session : sessions) {
            updateAggregateStats(session);
        }

        stats_cache_valid_ = true;
        spdlog::debug("Recalculated aggregate statistics from {} sessions", sessions.size());

        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats recalculation: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

bool StatisticsManager::isValidGameSession(uint64_t game_id) const {
    return active_sessions_.contains(game_id);
}

bool StatisticsManager::isValidDifficulty(Difficulty difficulty) {
    int diff_val = static_cast<int>(difficulty);
    // Difficulty enum is 0-based: Easy=0, Medium=1, Hard=2, Expert=3
    return diff_val >= 0 && diff_val <= 3;
}

void StatisticsManager::setCollectDetailedStats(bool enabled) {
    collect_detailed_stats_ = enabled;
    spdlog::info("Detailed stats collection: {}", enabled ? "enabled" : "disabled");
}

void StatisticsManager::setEncryptSessions(bool enabled) {
    encrypt_sessions_ = enabled;
    spdlog::info("Session encryption: {}", enabled ? "enabled" : "disabled");
}

std::expected<void, StatisticsError> StatisticsManager::deleteSessionHistory() {
    pending_sessions_.clear();

    if (std::filesystem::exists(sessions_file_)) {
        try {
            std::filesystem::remove(sessions_file_);
            spdlog::info("Deleted session history file: {}", sessions_file_.string());
        } catch (const std::exception& e) {
            spdlog::error("Failed to delete session history: {}", e.what());
            return std::unexpected(StatisticsError::FileAccessError);
        }
    }

    return {};
}

void StatisticsManager::flushSessions() {
    if (pending_sessions_.empty()) {
        return;
    }

    if (encrypt_sessions_) {
        // Load existing sessions, merge with pending, write as encrypted blob.
        // getAllSessions() already merges pending_sessions_, so no extra append needed.
        auto all_result = getAllSessions();
        std::vector<GameStats> all_sessions;
        if (all_result) {
            all_sessions = std::move(*all_result);
        }

        // Serialize to YAML in memory
        YAML::Node sessions_node;
        for (const auto& s : all_sessions) {
            YAML::Node node;
            node["difficulty"] = static_cast<int>(s.difficulty);
            node["puzzle_rating"] = s.puzzle_rating;
            node["start_time"] = std::chrono::system_clock::to_time_t(s.start_time);
            node["end_time"] = std::chrono::system_clock::to_time_t(s.end_time);
            node["time_played"] = s.time_played.count();
            node["completed"] = s.completed;
            node["moves_made"] = s.moves_made;
            node["hints_used"] = s.hints_used;
            node["mistakes"] = s.mistakes;
            node["puzzle_seed"] = s.puzzle_seed;
            sessions_node.push_back(node);
        }
        std::stringstream ss;
        ss << sessions_node;
        auto yaml_str = ss.str();
        std::vector<uint8_t> data(yaml_str.begin(), yaml_str.end());

        // Encrypt with interactive KDF and write atomically
        auto enc_result = EncryptionManager::encryptInteractive(data);
        if (enc_result) {
            auto write_result = atomicWriteFile(sessions_file_, *enc_result);
            if (!write_result) {
                spdlog::warn("Atomic write failed for sessions file");
            }
        } else {
            spdlog::warn("Failed to encrypt sessions, falling back to plain YAML");
            for (const auto& session : pending_sessions_) {
                std::ignore = saveGameSession(session);
            }
        }
    } else {
        // Write as plain YAML (append mode)
        for (const auto& session : pending_sessions_) {
            std::ignore = saveGameSession(session);
        }
    }

    spdlog::info("Flushed {} sessions to disk (encrypted={})", pending_sessions_.size(), encrypt_sessions_);
    pending_sessions_.clear();
}

std::expected<void, StatisticsError> StatisticsManager::atomicWriteFile(const std::filesystem::path& target,
                                                                        const std::vector<uint8_t>& data) {
    auto tmp_path = target;
    tmp_path += ".tmp";

    {
        std::ofstream file(tmp_path, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open temp file for writing: {}", tmp_path.string());
            return std::unexpected(StatisticsError::FileAccessError);
        }
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file.good()) {
            spdlog::error("Failed to write temp file: {}", tmp_path.string());
            return std::unexpected(StatisticsError::FileAccessError);
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, target, ec);
    if (ec) {
        spdlog::error("Failed to rename {} -> {}: {}", tmp_path.string(), target.string(), ec.message());
        std::filesystem::remove(tmp_path, ec);
        return std::unexpected(StatisticsError::FileAccessError);
    }

    return {};
}

}  // namespace sudoku::core