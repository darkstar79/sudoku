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

#include <QLabel>
#include <qstring.h>
#include <qtmetamacros.h>
#include <qwidget.h>
#include <qwindowdefs.h>

#ifdef SUDOKU_UI_TESTING
class TestViewModelBinding;
#endif

namespace sudoku::view {

class ToastWidget : public QLabel {
    Q_OBJECT

public:
    explicit ToastWidget(QWidget* parent = nullptr);

    void show(const QString& message, int duration_ms = 3000);

private:
    QTimer* hide_timer_{nullptr};

#ifdef SUDOKU_UI_TESTING
    friend class ::TestViewModelBinding;
#endif
};

}  // namespace sudoku::view
