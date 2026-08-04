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

#include "../../src/core/game_validator.h"
#include "../../src/core/i_statistics_manager.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/settings_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/test_utils.h"

#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;

namespace {

/// Spy IStatisticsManager that records every setCollectDetailedStats/setEncryptSessions call.
/// Everything else is a minimal stub — these tests only assert on the settings-sync consumer.
class SpyStatisticsManager : public core::IStatisticsManager {
public:
    std::vector<bool> collect_detailed_stats_calls;
    std::vector<bool> encrypt_sessions_calls;

    [[nodiscard]] std::expected<uint64_t, core::StatisticsError> startGame(core::Difficulty, uint32_t,
                                                                           double) override {
        return uint64_t{1};
    }
    [[nodiscard]] std::expected<void, core::StatisticsError> recordMove(uint64_t, bool) override {
        return {};
    }
    [[nodiscard]] std::expected<void, core::StatisticsError> recordHint(uint64_t) override {
        return {};
    }
    [[nodiscard]] std::expected<void, core::StatisticsError> seedSessionProgress(uint64_t, int, int, int,
                                                                                 std::chrono::milliseconds) override {
        return {};
    }
    [[nodiscard]] std::expected<core::GameStats, core::StatisticsError> endGame(uint64_t, bool) override {
        return core::GameStats{};
    }
    [[nodiscard]] std::expected<core::GameStats, core::StatisticsError> getGameStats(uint64_t) const override {
        return core::GameStats{};
    }
    [[nodiscard]] std::expected<core::AggregateStats, core::StatisticsError> getAggregateStats() const override {
        return core::AggregateStats{};
    }
    [[nodiscard]] std::expected<core::AggregateStats, core::StatisticsError>
    getStatsForDifficulty(core::Difficulty) const override {
        return core::AggregateStats{};
    }
    [[nodiscard]] std::expected<std::vector<core::GameStats>, core::StatisticsError> getAllSessions() const override {
        return std::vector<core::GameStats>{};
    }
    [[nodiscard]] std::expected<std::vector<core::GameStats>, core::StatisticsError>
    getRecentGames(int) const override {
        return std::vector<core::GameStats>{};
    }
    [[nodiscard]] std::array<std::chrono::milliseconds, core::DIFFICULTY_COUNT> getBestTimes() const override {
        return {};
    }
    [[nodiscard]] std::array<double, core::DIFFICULTY_COUNT> getCompletionRates() const override {
        return {};
    }
    [[nodiscard]] std::expected<void, core::StatisticsError> exportStats(const std::string&) const override {
        return {};
    }
    [[nodiscard]] std::expected<void, core::StatisticsError>
    exportAggregateStatsCsv(const std::string&) const override {
        return {};
    }
    [[nodiscard]] std::expected<void, core::StatisticsError> exportGameSessionsCsv(const std::string&) const override {
        return {};
    }
    [[nodiscard]] std::expected<void, core::StatisticsError> importStats(const std::string&) override {
        return {};
    }
    void resetAllStats() override {
    }
    void setCollectDetailedStats(bool enabled) override {
        collect_detailed_stats_calls.push_back(enabled);
    }
    void setEncryptSessions(bool enabled) override {
        encrypt_sessions_calls.push_back(enabled);
    }
    [[nodiscard]] std::expected<void, core::StatisticsError> deleteSessionHistory() override {
        return {};
    }
    void flushSessions() override {
    }
    [[nodiscard]] bool hasUnreadableSessionHistory() const override {
        return false;
    }
    [[nodiscard]] std::expected<std::filesystem::path, core::StatisticsError> archiveUnreadableSessions() override {
        return std::filesystem::path{};
    }
};

struct SettingsSyncFixture {
    test::TempTestDir temp_dir;
    std::shared_ptr<core::IGameValidator> validator;
    std::shared_ptr<core::IPuzzleGenerator> generator;
    std::shared_ptr<core::ISudokuSolver> solver;
    std::shared_ptr<core::ISaveManager> save_manager;
    std::shared_ptr<SpyStatisticsManager> spy_stats;
    std::shared_ptr<core::SettingsManager> settings_manager;

    SettingsSyncFixture() {
        validator = std::make_shared<core::GameValidator>();
        generator = std::make_shared<core::PuzzleGenerator>();
        solver = std::make_shared<core::SudokuSolver>(validator);
        save_manager = std::make_shared<core::SaveManager>(temp_dir.path());
        spy_stats = std::make_shared<SpyStatisticsManager>();
        settings_manager = std::make_shared<core::SettingsManager>(temp_dir.path() / "settings.yaml");
    }
};

}  // namespace

// D2 regression (story 8-19): GameViewModel's constructor is the ONLY place that pushed
// collect_detailed_stats/encrypt_detailed_stats into IStatisticsManager — nothing subscribed to
// settingsObservable(), so toggling detailed stats mid-session never reached the consumer until
// the app restarted. These assert at the CONSUMER (IStatisticsManager), not just the settings
// manager, per 8-1's lesson: a record with no asserted consumer is not a test.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("GameViewModel settings sync propagates detailed-stats changes", "[regression][bug-settings-startup]") {
    SettingsSyncFixture fixture;

    SECTION("constructor seeds detailed-stats settings exactly once (AC5)") {
        REQUIRE(fixture.spy_stats->collect_detailed_stats_calls.empty());
        REQUIRE(fixture.spy_stats->encrypt_sessions_calls.empty());

        viewmodel::GameViewModel view_model(fixture.validator, fixture.generator, fixture.solver, fixture.spy_stats,
                                            fixture.save_manager, fixture.settings_manager);

        REQUIRE(fixture.spy_stats->collect_detailed_stats_calls.size() == 1);
        REQUIRE(fixture.spy_stats->collect_detailed_stats_calls.front() ==
                fixture.settings_manager->getSettings().collect_detailed_stats);
        REQUIRE(fixture.spy_stats->encrypt_sessions_calls.size() == 1);
        REQUIRE(fixture.spy_stats->encrypt_sessions_calls.front() ==
                fixture.settings_manager->getSettings().encrypt_detailed_stats);
    }

    SECTION("changing collect_detailed_stats after construction reaches the stats manager without a restart (AC4)") {
        viewmodel::GameViewModel view_model(fixture.validator, fixture.generator, fixture.solver, fixture.spy_stats,
                                            fixture.save_manager, fixture.settings_manager);
        fixture.spy_stats->collect_detailed_stats_calls.clear();
        fixture.spy_stats->encrypt_sessions_calls.clear();

        fixture.settings_manager->setCollectDetailedStats(true);

        REQUIRE(fixture.spy_stats->collect_detailed_stats_calls.size() == 1);
        REQUIRE(fixture.spy_stats->collect_detailed_stats_calls.front());
    }

    SECTION("changing encrypt_detailed_stats after construction reaches the stats manager without a restart (AC4)") {
        viewmodel::GameViewModel view_model(fixture.validator, fixture.generator, fixture.solver, fixture.spy_stats,
                                            fixture.save_manager, fixture.settings_manager);
        fixture.spy_stats->collect_detailed_stats_calls.clear();
        fixture.spy_stats->encrypt_sessions_calls.clear();

        fixture.settings_manager->setEncryptDetailedStats(false);

        REQUIRE(fixture.spy_stats->encrypt_sessions_calls.size() == 1);
        REQUIRE_FALSE(fixture.spy_stats->encrypt_sessions_calls.front());
    }
}
