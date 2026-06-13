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

#include "puzzle_technique_dialog.h"

#include "../core/i18n_helpers.h"
#include "../core/solving_technique.h"

#include <array>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace sudoku::view {

namespace {

QString qstr(std::string_view sv) {
    return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
}

// All logical techniques, in registration order (matches SudokuSolver). Backtracking is
// intentionally omitted — the L-D feature is for *logical* techniques only.
constexpr std::array kSelectableTechniques = {
    core::SolvingTechnique::FullHouse,
    core::SolvingTechnique::NakedSingle,
    core::SolvingTechnique::HiddenSingle,
    core::SolvingTechnique::NakedPair,
    core::SolvingTechnique::NakedTriple,
    core::SolvingTechnique::HiddenPair,
    core::SolvingTechnique::HiddenTriple,
    core::SolvingTechnique::PointingPair,
    core::SolvingTechnique::BoxLineReduction,
    core::SolvingTechnique::NakedQuad,
    core::SolvingTechnique::HiddenQuad,
    core::SolvingTechnique::XWing,
    core::SolvingTechnique::XYWing,
    core::SolvingTechnique::Swordfish,
    core::SolvingTechnique::Skyscraper,
    core::SolvingTechnique::TwoStringKite,
    core::SolvingTechnique::XYZWing,
    core::SolvingTechnique::WWing,
    core::SolvingTechnique::UniqueRectangle,
    core::SolvingTechnique::UniqueLoop,
    core::SolvingTechnique::SimpleColoring,
    core::SolvingTechnique::FinnedXWing,
    core::SolvingTechnique::SashimiXWing,
    core::SolvingTechnique::RemotePairs,
    core::SolvingTechnique::BUG,
    core::SolvingTechnique::HiddenUniqueRectangle,
    core::SolvingTechnique::AvoidableRectangle,
    core::SolvingTechnique::XCycles,
    core::SolvingTechnique::Jellyfish,
    core::SolvingTechnique::FinnedSwordfish,
    core::SolvingTechnique::SashimiSwordfish,
    core::SolvingTechnique::EmptyRectangle,
    core::SolvingTechnique::WXYZWing,
    core::SolvingTechnique::MultiColoring,
    core::SolvingTechnique::ThreeDMedusa,
    core::SolvingTechnique::FinnedJellyfish,
    core::SolvingTechnique::SashimiJellyfish,
    core::SolvingTechnique::XYChain,
    core::SolvingTechnique::VWXYZWing,
    core::SolvingTechnique::FrankenFish,
    core::SolvingTechnique::MutantFish,
    core::SolvingTechnique::GroupedXCycles,
    core::SolvingTechnique::ALSxZ,
    core::SolvingTechnique::SueDeCoq,
    core::SolvingTechnique::ALSXYWing,
    core::SolvingTechnique::DeathBlossom,
    core::SolvingTechnique::ALSChain,
    core::SolvingTechnique::ForcingChain,
    core::SolvingTechnique::UnitForcingChain,
    core::SolvingTechnique::RegionForcingChain,
    core::SolvingTechnique::KrakenFish,
    core::SolvingTechnique::JuniorExocet,
    core::SolvingTechnique::NiceLoop,
    core::SolvingTechnique::ContinuousNiceLoop,
    core::SolvingTechnique::GroupedNiceLoop,
};

}  // namespace

PuzzleTechniqueDialog::PuzzleTechniqueDialog(QWidget* parent)
    : QDialog(parent), technique_combo_(new QComboBox), find_btn_(new QPushButton(qstr(core::loc("Sudoku", "Find")))),
      cancel_btn_(new QPushButton(qstr(core::loc("Sudoku", "Cancel")))) {
    setWindowTitle(qstr(core::loc("Sudoku", "Find Step by Technique")));
    setModal(true);
    resize(360, 140);

    auto* layout = new QVBoxLayout(this);

    auto* prompt = new QLabel(qstr(core::loc("Sudoku", "Pick a solving technique:")));
    prompt->setTextFormat(Qt::PlainText);
    layout->addWidget(prompt);

    for (auto t : kSelectableTechniques) {
        technique_combo_->addItem(qstr(core::getLocalizedTechniqueName(t)), static_cast<int>(t));
    }
    layout->addWidget(technique_combo_);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    find_btn_->setDefault(true);
    buttons->addWidget(cancel_btn_);
    buttons->addWidget(find_btn_);
    layout->addLayout(buttons);

    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(find_btn_, &QPushButton::clicked, this,
            [this]() { emit findRequested(technique_combo_->currentData().toInt()); });
}

}  // namespace sudoku::view
