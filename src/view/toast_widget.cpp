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

#include "toast_widget.h"

#include "ui_colors.h"

#include <QTimer>
#include <QtCore/qobjectdefs.h>
#include <qnamespace.h>

namespace sudoku::view {

ToastWidget::ToastWidget(QWidget* parent) : QLabel(parent), hide_timer_(new QTimer(this)) {
    setStyleSheet(QString("background-color: %1;"
                          "color: %2;"
                          "padding: 10px 20px;"
                          "border-radius: 6px;"
                          "font-size: 14px;")
                      .arg(UIColors::TOAST_BG, UIColors::TOAST_TEXT));
    setAlignment(Qt::AlignCenter);
    hide();
    hide_timer_->setSingleShot(true);
    connect(hide_timer_, &QTimer::timeout, this, &QWidget::hide);
}

void ToastWidget::show(const QString& message, int duration_ms) {
    setText(message);
    adjustSize();

    // Position at bottom-center of parent
    if (parentWidget() != nullptr) {
        auto* pw = parentWidget();
        int x_pos = (pw->width() - width()) / 2;
        int y_pos = pw->height() - height() - 60;
        move(x_pos, y_pos);
    }

    QLabel::show();
    raise();
    hide_timer_->start(duration_ms);
}

}  // namespace sudoku::view
