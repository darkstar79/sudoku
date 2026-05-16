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

#include "../../src/core/puzzle_analyzer.h"

#include <chrono>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sudoku::test {

/// Wraps a real IPuzzleAnalyzer and forces scoreDifficulty to return ScoringError::Timeout.
/// Drives the auto-analysis timeout-swallow path deterministically without depending on
/// wall-clock against an adversarial board. parse/serialize/validate/uniqueness all delegate
/// to the inner analyzer so the calling pipeline reaches scoreDifficulty normally.
class TimeoutScoreAnalyzer : public core::IPuzzleAnalyzer {
public:
    explicit TimeoutScoreAnalyzer(std::shared_ptr<core::IPuzzleAnalyzer> inner) : inner_(std::move(inner)) {
    }

    [[nodiscard]] std::expected<core::BoardData, core::ParseErrorDetail>
    parseString(std::string_view input) const override {
        return inner_->parseString(input);
    }
    [[nodiscard]] std::string serializeToString(const core::BoardData& board) const override {
        return inner_->serializeToString(board);
    }
    [[nodiscard]] std::expected<void, core::BoardValidationError>
    validate(const core::BoardData& board) const override {
        return inner_->validate(board);
    }
    [[nodiscard]] core::UniquenessResult checkUniqueness(const core::BoardData& board) const override {
        return inner_->checkUniqueness(board);
    }
    [[nodiscard]] std::expected<core::DifficultyScore, core::ScoringError>
    scoreDifficulty(const core::BoardData& /*board*/, std::chrono::milliseconds /*budget*/) const override {
        return std::unexpected(core::ScoringError::Timeout);
    }

private:
    std::shared_ptr<core::IPuzzleAnalyzer> inner_;
};

}  // namespace sudoku::test
