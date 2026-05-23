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

#include "core/i_clipboard_provider.h"

#include <string>
#include <string_view>

#include <QApplication>
#include <QClipboard>
#include <QString>

namespace sudoku::infrastructure {

/// Production IClipboardProvider wrapping QApplication::clipboard().
/// Requires a QApplication instance to be alive on the calling thread.
class QtClipboardProvider final : public core::IClipboardProvider {
public:
    void setText(std::string_view text) override {
        if (auto* clipboard = QApplication::clipboard()) {
            clipboard->setText(QString::fromUtf8(text.data(), static_cast<int>(text.size())));
        }
    }

    [[nodiscard]] std::string getText() const override {
        if (auto* clipboard = QApplication::clipboard()) {
            return clipboard->text().toStdString();
        }
        return {};
    }
};

}  // namespace sudoku::infrastructure
