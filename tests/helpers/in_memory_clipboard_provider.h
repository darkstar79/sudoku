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

#include "../../src/core/i_clipboard_provider.h"

#include <string>
#include <string_view>

namespace sudoku::test {

/// Deterministic, headless-safe IClipboardProvider for tests.
class InMemoryClipboardProvider : public core::IClipboardProvider {
public:
    void setText(std::string_view text) override {
        contents_ = std::string(text);
    }

    [[nodiscard]] std::string getText() const override {
        return contents_;
    }

private:
    std::string contents_;
};

}  // namespace sudoku::test
