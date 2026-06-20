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

#include "i_play_time_ledger.h"
#include "i_time_provider.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace sudoku::core {

/// Encrypted, date-keyed implementation of IPlayTimeLedger.
///
/// Persistence mirrors statistics_manager: a versioned YAML payload encrypted with
/// EncryptionManager::encryptInteractive (XSalsa20-Poly1305, system-keyed Argon2id) and written
/// atomically. Encryption is **unconditional** — there is no opt-out toggle, because a toggle
/// would itself be the bypass. The schema version lives inside the (plaintext-before-encrypt)
/// payload, so the 1.0.0 format freeze is unchanged and unknown future versions are tolerated.
///
/// Two distinct anti-bypass properties:
///   - **Restart-proof** comes from the file living *outside* the saves directory (a sibling of
///     settings.yaml): deleting a save cannot reset the day.
///   - **Edit-proof** comes from the AEAD MAC: any hand-edit fails to decrypt. On a present-but-
///     undecryptable file the ledger fails CLOSED (preserve the file, report exhausted) rather
///     than granting a fresh day — corrupt is NOT the same as missing.
///
/// Neither defends against *deleting* the file or against *system-clock rollback* — both remain
/// accepted bypasses by design (clock rollback is explicitly out of scope for this feature).
class PlayTimeLedger final : public IPlayTimeLedger {
public:
    /// Construct with an explicit file path (for testability).
    explicit PlayTimeLedger(std::filesystem::path ledger_path,
                            std::shared_ptr<ITimeProvider> time_provider = std::make_shared<SystemTimeProvider>());

    /// Construct with the default platform path (~/.local/share/sudoku/play_time.yaml — the
    /// sibling of settings.yaml, NOT inside the saves directory).
    PlayTimeLedger();

    ~PlayTimeLedger() override = default;
    PlayTimeLedger(const PlayTimeLedger&) = delete;
    PlayTimeLedger& operator=(const PlayTimeLedger&) = delete;
    PlayTimeLedger(PlayTimeLedger&&) = delete;
    PlayTimeLedger& operator=(PlayTimeLedger&&) = delete;

    void addActivePlay(std::chrono::milliseconds delta) override;
    void addActivePlay(std::chrono::milliseconds delta, std::int64_t attribution_day_key) override;
    [[nodiscard]] std::chrono::milliseconds accruedToday() const override;
    void recordSessionEnd() override;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> lastSessionEnd() const override;
    [[nodiscard]] std::chrono::milliseconds cooldownRemaining(int cooldown_minutes) const override;
    [[nodiscard]] bool isUnreadable() const override;

    /// Current UTC calendar-day key (days since the system_clock epoch).
    [[nodiscard]] std::int64_t currentDayKey() const override;

private:
    void load();
    void save() const;

    std::filesystem::path ledger_path_;
    std::shared_ptr<ITimeProvider> time_provider_;

    std::int64_t day_key_{0};                     // calendar day the accrual belongs to
    std::chrono::milliseconds accrued_today_{0};  // active play accrued on day_key_
    std::optional<std::chrono::system_clock::time_point> last_session_end_;
    bool unreadable_{false};  // present-but-undecryptable → fail closed, preserve the file
};

}  // namespace sudoku::core
