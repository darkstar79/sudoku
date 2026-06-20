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

#include "play_time_ledger.h"

#include "encryption_manager.h"
#include "file_utils.h"
#include "infrastructure/app_directory_manager.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::core {

namespace {

// Schema version of the play_time.yaml payload (inside the encrypted blob). Bump only on a
// breaking layout change; readers tolerate unknown/future versions by reading the fields they
// recognize, exactly like the save/statistics blobs.
constexpr int kLedgerSchemaVersion = 1;

// When the file is present but unreadable the ledger fails closed: a year of "accrued" time and
// "cooldown remaining" so any *enabled* limit keeps blocking. With limits disabled the launch
// policy ignores these values, so a corrupt ledger never blocks an unlimited launch.
constexpr std::chrono::milliseconds kFailClosedSentinel = std::chrono::hours{24 * 365};

}  // namespace

namespace {
[[nodiscard]] std::filesystem::path defaultLedgerPath() {
    // Sibling of settings.yaml in the app-data dir — deliberately NOT inside the saves dir, so
    // deleting a save cannot reset the day.
    return infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Saves)
               .parent_path() /
           "play_time.yaml";
}
}  // namespace

PlayTimeLedger::PlayTimeLedger(std::filesystem::path ledger_path, std::shared_ptr<ITimeProvider> time_provider)
    : ledger_path_(ledger_path.empty() ? defaultLedgerPath() : std::move(ledger_path)),
      time_provider_(std::move(time_provider)) {
    load();
}

PlayTimeLedger::PlayTimeLedger() : PlayTimeLedger(std::filesystem::path{}) {
}

std::int64_t PlayTimeLedger::currentDayKey() const {
    return std::chrono::floor<std::chrono::days>(time_provider_->systemNow()).time_since_epoch().count();
}

void PlayTimeLedger::addActivePlay(std::chrono::milliseconds delta) {
    addActivePlay(delta, currentDayKey());
}

void PlayTimeLedger::addActivePlay(std::chrono::milliseconds delta, std::int64_t attribution_day_key) {
    if (unreadable_ || delta <= std::chrono::milliseconds::zero()) {
        return;  // preserve a corrupt file; ignore non-positive deltas
    }
    if (day_key_ != attribution_day_key) {
        day_key_ = attribution_day_key;
        accrued_today_ = std::chrono::milliseconds::zero();
    }
    accrued_today_ += delta;
    save();
}

std::chrono::milliseconds PlayTimeLedger::accruedToday() const {
    if (unreadable_) {
        return kFailClosedSentinel;
    }
    return (day_key_ == currentDayKey()) ? accrued_today_ : std::chrono::milliseconds::zero();
}

void PlayTimeLedger::recordSessionEnd() {
    if (unreadable_) {
        return;
    }
    last_session_end_ = time_provider_->systemNow();
    save();
}

std::optional<std::chrono::system_clock::time_point> PlayTimeLedger::lastSessionEnd() const {
    return last_session_end_;
}

std::chrono::milliseconds PlayTimeLedger::cooldownRemaining(int cooldown_minutes) const {
    if (unreadable_) {
        return kFailClosedSentinel;
    }
    if (!last_session_end_ || cooldown_minutes <= 0) {
        return std::chrono::milliseconds::zero();
    }
    // Clamp elapsed at 0: if last_session_end_ is in the future (the clock was set forward when the
    // session ended, then corrected back), a negative elapsed would inflate the lockout past the
    // configured cooldown. Only clock-*backward* is an accepted bypass; never over-lock the user.
    const auto elapsed = std::max(
        std::chrono::milliseconds::zero(),
        std::chrono::duration_cast<std::chrono::milliseconds>(time_provider_->systemNow() - *last_session_end_));
    const auto total = std::chrono::milliseconds{static_cast<std::int64_t>(cooldown_minutes) * 60'000};
    const auto remaining = total - elapsed;
    return remaining > std::chrono::milliseconds::zero() ? remaining : std::chrono::milliseconds::zero();
}

bool PlayTimeLedger::isUnreadable() const {
    return unreadable_;
}

void PlayTimeLedger::load() {
    if (!std::filesystem::exists(ledger_path_)) {
        return;  // missing == a clean fresh day (delete-to-reset is an accepted bypass)
    }

    std::ifstream file(ledger_path_, std::ios::binary);
    std::vector<std::uint8_t> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // The ledger is ALWAYS written encrypted. A present-but-not-encrypted file means it was
    // tampered with (or hand-written) — fail closed, do not trust it.
    if (!EncryptionManager::isEncrypted(file_data)) {
        spdlog::warn("Play-time ledger {} is not encrypted; preserving and failing closed", ledger_path_.string());
        unreadable_ = true;
        return;
    }

    auto decrypted = EncryptionManager::decrypt(file_data);
    if (!decrypted) {
        // A byte-flip / hand-edit makes Poly1305 verification fail. Preserve the file (never
        // overwrite) and fail closed so "edit-to-corrupt" cannot buy a fresh day.
        spdlog::warn("Play-time ledger {} failed to decrypt; preserving and failing closed", ledger_path_.string());
        unreadable_ = true;
        return;
    }

    try {
        const std::string yaml_str(decrypted->begin(), decrypted->end());
        const YAML::Node root = YAML::Load(yaml_str);
        if (auto v = root["day_key"]) {
            day_key_ = v.as<std::int64_t>();
        }
        if (auto v = root["accrued_today_ms"]) {
            accrued_today_ = std::chrono::milliseconds{v.as<std::int64_t>()};
        }
        if (auto v = root["last_session_end_ms_epoch"]) {
            last_session_end_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{v.as<std::int64_t>()}};
        }
    } catch (const std::exception& e) {
        spdlog::warn("Play-time ledger {} parse failed ({}); preserving and failing closed", ledger_path_.string(),
                     e.what());
        unreadable_ = true;
    }
}

void PlayTimeLedger::save() const {
    if (unreadable_) {
        return;  // never overwrite a preserved corrupt file
    }

    YAML::Node root;
    root["version"] = kLedgerSchemaVersion;
    root["day_key"] = day_key_;
    root["accrued_today_ms"] = static_cast<std::int64_t>(accrued_today_.count());
    if (last_session_end_) {
        root["last_session_end_ms_epoch"] = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(last_session_end_->time_since_epoch()).count());
    }

    std::stringstream ss;
    ss << root;
    const std::string yaml_str = ss.str();
    const std::vector<std::uint8_t> plaintext(yaml_str.begin(), yaml_str.end());

    // INTERACTIVE KDF: the ledger is flushed on the auto-save cadence (frequent writes), the same
    // reason statistics uses it. Encryption is unconditional — never fall back to plaintext.
    auto encrypted = EncryptionManager::encryptInteractive(plaintext);
    if (!encrypted) {
        spdlog::warn("Play-time ledger encryption unavailable; not writing {} (ledger degrades to unavailable)",
                     ledger_path_.string());
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(ledger_path_.parent_path(), ec);
    if (auto result = file_utils::atomicWriteFile(ledger_path_, *encrypted); !result) {
        spdlog::warn("Failed to write play-time ledger {}", ledger_path_.string());
    }
}

}  // namespace sudoku::core
