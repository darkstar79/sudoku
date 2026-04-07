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

#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"
#include "core/technique_descriptions.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

TEST_CASE("GameViewModel - Coaching Hints", "[game_view_model][coaching]") {
    sudoku::test::GameViewModelFixture fixture;

    SECTION("Coaching is inactive by default") {
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
        REQUIRE_FALSE(fixture.view_model->coachingState.get().active);
        REQUIRE(fixture.view_model->coachingState.get().level == 0);
    }

    SECTION("Coaching requires active game") {
        fixture.view_model->requestCoachingHint();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Level 1 shows technique name and consumes one credit") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        int initial_hints = fixture.view_model->getHintCount();
        REQUIRE(initial_hints == 10);

        fixture.view_model->requestCoachingHint();

        const auto& state = fixture.view_model->coachingState.get();
        REQUIRE(state.active);
        REQUIRE(state.level == 1);
        REQUIRE_FALSE(state.message.empty());
        REQUIRE(fixture.view_model->getHintCount() == initial_hints - 1);
    }

    SECTION("Level 2 shows region/cell hint without consuming additional credit") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        int initial_hints = fixture.view_model->getHintCount();

        // Level 1
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().level == 1);

        // Level 2 — no additional credit consumed
        fixture.view_model->requestCoachingHint();

        const auto& state = fixture.view_model->coachingState.get();
        REQUIRE(state.active);
        REQUIRE(state.level == 2);
        REQUIRE_FALSE(state.message.empty());
        REQUIRE_FALSE(state.highlights.empty());
        REQUIRE(fixture.view_model->getHintCount() == initial_hints - 1);
    }

    SECTION("Level 3 shows full explanation with all highlights, no auto-placement, 1 credit total") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        const auto& game_state = fixture.view_model->gameState.get();
        int moves_before = game_state.getMoveCount();
        int initial_hints = fixture.view_model->getHintCount();

        // Escalate to level 3
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        const auto& state = fixture.view_model->coachingState.get();
        REQUIRE(state.active);
        REQUIRE(state.level == 3);
        REQUIRE_FALSE(state.message.empty());
        REQUIRE_FALSE(state.highlights.empty());
        REQUIRE_FALSE(state.highlights.empty());
        // Only 1 credit consumed for entire sequence
        REQUIRE(fixture.view_model->getHintCount() == initial_hints - 1);

        // No auto-placement — board unchanged
        REQUIRE(fixture.view_model->gameState.get().getMoveCount() == moves_before);
    }

    SECTION("Pressing coaching hint at level 3 is a no-op") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        int hints_after_3 = fixture.view_model->getHintCount();
        auto msg_after_3 = fixture.view_model->coachingState.get().message;

        // Level 4 attempt — should not change anything
        fixture.view_model->requestCoachingHint();

        REQUIRE(fixture.view_model->coachingState.get().level == 3);
        REQUIRE(fixture.view_model->getHintCount() == hints_after_3);
    }

    SECTION("Cached step is reused across levels (technique stays the same)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        auto technique_1 = fixture.view_model->coachingState.get().technique;

        fixture.view_model->requestCoachingHint();
        auto technique_2 = fixture.view_model->coachingState.get().technique;

        fixture.view_model->requestCoachingHint();
        auto technique_3 = fixture.view_model->coachingState.get().technique;

        REQUIRE(technique_1 == technique_2);
        REQUIRE(technique_2 == technique_3);
    }

    SECTION("Dismiss clears coaching state") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        fixture.view_model->dismissCoaching();

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
        REQUIRE(fixture.view_model->coachingState.get().level == 0);
        REQUIRE(fixture.view_model->coachingState.get().message.empty());
    }

    SECTION("Coaching resets when entering a number") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        // Enter a number in an empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching resets on undo") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make a move first so undo is possible
        const auto& state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);

        // Start coaching
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        fixture.view_model->undo();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching resets on redo") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make a move and undo to enable redo
        const auto& state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);
        fixture.view_model->undo();

        // Start coaching
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        fixture.view_model->redo();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching resets on clearCell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Place a number first
        const auto& state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);

        // Start coaching
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        fixture.view_model->clearCell(empty_opt.value());
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching resets on enterNote") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        const auto& state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNote(empty_opt.value(), 1);

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching resets when getHint is called") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        const auto& state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->getHint(empty_opt.value());

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching resets on resetGame") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        fixture.view_model->resetGame();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching resets on startNewGame") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->isCoachingActive());

        fixture.view_model->startNewGame(Difficulty::Medium);
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("Coaching after dismiss starts fresh at level 1") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().level == 2);

        fixture.view_model->dismissCoaching();
        fixture.view_model->requestCoachingHint();

        REQUIRE(fixture.view_model->coachingState.get().level == 1);
    }

    SECTION("CoachingState tracks max_level reached") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().max_level == 1);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().max_level == 2);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().max_level == 3);
    }

    SECTION("navigateCoachingLevel backward from level 2") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach level 2
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().level == 2);

        // Navigate back to level 1 (no hint credit consumed)
        int hints_before = fixture.view_model->getHintCount();
        fixture.view_model->navigateCoachingLevel(-1);

        REQUIRE(fixture.view_model->coachingState.get().level == 1);
        REQUIRE(fixture.view_model->coachingState.get().max_level == 2);
        REQUIRE(fixture.view_model->getHintCount() == hints_before);
    }

    SECTION("navigateCoachingLevel forward within reached levels") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach level 3
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        // Navigate back to level 1
        fixture.view_model->navigateCoachingLevel(-1);
        fixture.view_model->navigateCoachingLevel(-1);
        REQUIRE(fixture.view_model->coachingState.get().level == 1);

        // Navigate forward (no credit consumed)
        int hints_before = fixture.view_model->getHintCount();
        fixture.view_model->navigateCoachingLevel(1);
        REQUIRE(fixture.view_model->coachingState.get().level == 2);
        REQUIRE(fixture.view_model->getHintCount() == hints_before);

        fixture.view_model->navigateCoachingLevel(1);
        REQUIRE(fixture.view_model->coachingState.get().level == 3);
    }

    SECTION("navigateCoachingLevel does not go below 1 or above max_level") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach level 2
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        // Try to go below 1
        fixture.view_model->navigateCoachingLevel(-1);
        REQUIRE(fixture.view_model->coachingState.get().level == 1);
        fixture.view_model->navigateCoachingLevel(-1);
        REQUIRE(fixture.view_model->coachingState.get().level == 1);  // clamped

        // Try to go above max_level (2)
        fixture.view_model->navigateCoachingLevel(1);
        REQUIRE(fixture.view_model->coachingState.get().level == 2);
        fixture.view_model->navigateCoachingLevel(1);
        REQUIRE(fixture.view_model->coachingState.get().level == 2);  // clamped
    }

    SECTION("navigateCoachingLevel is no-op when coaching inactive") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->navigateCoachingLevel(1);
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("navigateCoachingLevel preserves technique and updates highlights") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach level 3
        fixture.view_model->requestCoachingHint();
        auto technique = fixture.view_model->coachingState.get().technique;
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        // Navigate back to 1
        fixture.view_model->navigateCoachingLevel(-1);
        fixture.view_model->navigateCoachingLevel(-1);

        // Technique should be the same
        REQUIRE(fixture.view_model->coachingState.get().technique == technique);
        // Message should be non-empty (level 1 text)
        REQUIRE_FALSE(fixture.view_model->coachingState.get().message.empty());
    }

    SECTION("Coaching shows error when no hint credits remain") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Use all 10 hint credits via coaching (1 per sequence + dismiss)
        for (int i = 0; i < 10; ++i) {
            fixture.view_model->requestCoachingHint();
            fixture.view_model->dismissCoaching();
        }
        REQUIRE(fixture.view_model->getHintCount() == 0);

        fixture.view_model->requestCoachingHint();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    }

    SECTION("Coaching blocked when no hint credits remain") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Use all 10 hint credits via regular hints
        for (int i = 0; i < 10; ++i) {
            const auto& current = fixture.view_model->gameState.get();
            auto empty_opt = sudoku::test::findEmptyCell(current);
            REQUIRE(empty_opt.has_value());
            fixture.view_model->getHint(empty_opt.value());
        }
        REQUIRE(fixture.view_model->getHintCount() == 0);

        fixture.view_model->requestCoachingHint();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("GetCoachingHint command dispatches to requestCoachingHint") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->executeCommand(GameCommand::GetCoachingHint);

        REQUIRE(fixture.view_model->isCoachingActive());
        REQUIRE(fixture.view_model->coachingState.get().level == 1);
    }

    SECTION("canExecuteCommand for GetCoachingHint matches GetHint") {
        // No game active
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::GetCoachingHint));

        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::GetCoachingHint));
    }

    // =========================================================================
    // Phase A: Credit fix — coaching sequence costs 1 credit, not 3
    // =========================================================================

    SECTION("Coaching sequence L1-L2-L3 consumes only 1 hint credit") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        int initial_hints = fixture.view_model->getHintCount();
        REQUIRE(initial_hints == 10);

        // Full coaching sequence
        fixture.view_model->requestCoachingHint();  // L1
        fixture.view_model->requestCoachingHint();  // L2
        fixture.view_model->requestCoachingHint();  // L3

        // Should only consume 1 credit for the entire sequence
        REQUIRE(fixture.view_model->getHintCount() == initial_hints - 1);
    }

    SECTION("Multiple coaching sequences each cost 1 credit") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        int initial_hints = fixture.view_model->getHintCount();

        // First sequence
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->dismissCoaching();

        // Second sequence
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->dismissCoaching();

        // 2 sequences = 2 credits
        REQUIRE(fixture.view_model->getHintCount() == initial_hints - 2);
    }

    // =========================================================================
    // Phase A: Level 1 includes what_to_look_for text
    // =========================================================================

    SECTION("Level 1 message contains what_to_look_for text") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->requestCoachingHint();

        const auto& state = fixture.view_model->coachingState.get();
        // The technique description's what_to_look_for should appear in level 1
        auto desc = core::getTechniqueDescription(*fixture.loc_manager, state.technique);
        REQUIRE(state.message.find(desc.what_to_look_for) != std::string::npos);
    }

    SECTION("Level 1 message still contains what_it_is text") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->requestCoachingHint();

        const auto& state = fixture.view_model->coachingState.get();
        auto desc = core::getTechniqueDescription(*fixture.loc_manager, state.technique);
        REQUIRE(state.message.find(desc.what_it_is) != std::string::npos);
    }

    SECTION("Navigating back to level 1 also shows what_to_look_for") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        // Navigate back to level 1
        fixture.view_model->navigateCoachingLevel(-1);

        const auto& state = fixture.view_model->coachingState.get();
        REQUIRE(state.level == 1);
        auto desc = core::getTechniqueDescription(*fixture.loc_manager, state.technique);
        REQUIRE(state.message.find(desc.what_to_look_for) != std::string::npos);
    }

    // =========================================================================
    // Phase C: TryIt phase — Level 3 enters TryIt, board edits don't dismiss
    // =========================================================================

    SECTION("Level 3 enters TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        const auto& state = fixture.view_model->coachingState.get();
        REQUIRE(state.active);
        REQUIRE(state.level == 3);
        REQUIRE(state.phase == CoachingPhase::TryIt);
    }

    SECTION("Board edits do NOT reset coaching in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        // Enter a note — should NOT dismiss coaching
        const auto& game_state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(game_state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNote(empty_opt.value(), 1);

        REQUIRE(fixture.view_model->isCoachingActive());
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);
    }

    SECTION("enterNumber does NOT reset coaching in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        const auto& game_state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(game_state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);

        REQUIRE(fixture.view_model->isCoachingActive());
    }

    SECTION("clearCell does NOT reset coaching in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Place a number first
        const auto& game_state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(game_state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        fixture.view_model->clearCell(empty_opt.value());

        REQUIRE(fixture.view_model->isCoachingActive());
    }

    SECTION("undo does NOT reset coaching in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make a move so undo is possible
        const auto& game_state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(game_state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNumber(empty_opt.value(), 1);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        fixture.view_model->undo();

        REQUIRE(fixture.view_model->isCoachingActive());
    }

    SECTION("Board edits still reset coaching in Hinting phase (regression guard)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Only reach level 1 (Hinting phase)
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::Hinting);

        const auto& game_state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(game_state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->enterNote(empty_opt.value(), 1);

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("resetGame always resets coaching even in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        fixture.view_model->resetGame();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("startNewGame always resets coaching even in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        fixture.view_model->startNewGame(Difficulty::Medium);
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("getHint always resets coaching even in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        const auto& game_state = fixture.view_model->gameState.get();
        auto empty_opt = sudoku::test::findEmptyCell(game_state);
        REQUIRE(empty_opt.has_value());
        fixture.view_model->getHint(empty_opt.value());

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    // =========================================================================
    // Phase C: Check coaching answer
    // =========================================================================

    SECTION("checkCoachingAnswer is no-op when not in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Only at level 1 (Hinting)
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::Hinting);

        fixture.view_model->checkCoachingAnswer();

        // Should still be at level 1, no change
        REQUIRE(fixture.view_model->coachingState.get().level == 1);
        REQUIRE_FALSE(fixture.view_model->coachingState.get().check_submitted);
    }

    SECTION("checkCoachingAnswer is no-op when coaching inactive") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->checkCoachingAnswer();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("checkCoachingAnswer sets check_submitted") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        fixture.view_model->checkCoachingAnswer();

        REQUIRE(fixture.view_model->coachingState.get().check_submitted);
        REQUIRE(fixture.view_model->isCoachingActive());
    }

    SECTION("checkCoachingAnswer never targets a given cell (solver invariant)") {
        // Documents the solver invariant that backs the assert in
        // evaluatePlacementCheck: a Placement step is never suggested on a
        // given cell. We verify by inspecting the highlights produced after
        // Check — none of them may land on a given.
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        fixture.view_model->checkCoachingAnswer();

        const auto& state = fixture.view_model->gameState.get();
        const auto& coaching = fixture.view_model->coachingState.get();
        REQUIRE(coaching.check_submitted);
        for (const auto& hl : coaching.highlights) {
            INFO("Highlight at (" << hl.position.row << "," << hl.position.col << ")");
            REQUIRE_FALSE(state.isGiven(hl.position));
        }
    }

    SECTION("checkCoachingAnswer with no user action shows MissedAnswer highlights") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        // Don't make any moves — just check
        fixture.view_model->checkCoachingAnswer();

        const auto& state = fixture.view_model->coachingState.get();
        REQUIRE(state.check_submitted);
        // Should have at least one highlight (the missed answer)
        REQUIRE_FALSE(state.highlights.empty());
        // At least one MissedAnswer role should be present
        bool has_missed = false;
        for (const auto& hl : state.highlights) {
            if (hl.role == CellRole::MissedAnswer) {
                has_missed = true;
                break;
            }
        }
        REQUIRE(has_missed);
    }

    // =========================================================================
    // Phase C: Apply coaching step
    // =========================================================================

    SECTION("applyCoachingStep is no-op when not in TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::Hinting);

        int moves_before = fixture.view_model->gameState.get().getMoveCount();
        fixture.view_model->applyCoachingStep();

        // No change
        REQUIRE(fixture.view_model->gameState.get().getMoveCount() == moves_before);
        REQUIRE(fixture.view_model->isCoachingActive());  // Still active at L1
    }

    SECTION("applyCoachingStep is no-op when coaching inactive") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->applyCoachingStep();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("applyCoachingStep dismisses coaching") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->coachingState.get().phase == CoachingPhase::TryIt);

        fixture.view_model->applyCoachingStep();

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("applyCoachingStep applies the cached step to the board") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Capture board before coaching
        auto board_before = fixture.view_model->gameState.get().extractNumbers();

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        const auto& coaching = fixture.view_model->coachingState.get();
        REQUIRE(coaching.phase == CoachingPhase::TryIt);

        fixture.view_model->applyCoachingStep();

        // Board should have changed (step applied)
        auto board_after = fixture.view_model->gameState.get().extractNumbers();
        REQUIRE(board_before != board_after);
    }

    SECTION("applyCoachingStep moves are undoable") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Reach TryIt phase
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        // Capture board state before apply
        auto board_before = fixture.view_model->gameState.get().extractNumbers();

        fixture.view_model->applyCoachingStep();
        REQUIRE_FALSE(fixture.view_model->isCoachingActive());

        // Undo should be possible
        REQUIRE(fixture.view_model->canUndo());
        fixture.view_model->undo();

        // Board should be back to before
        auto board_undone = fixture.view_model->gameState.get().extractNumbers();
        REQUIRE(board_undone == board_before);
    }

    // =========================================================================
    // Phase C: Commands
    // =========================================================================

    SECTION("CheckCoachingAnswer command dispatches correctly") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        fixture.view_model->executeCommand(GameCommand::CheckCoachingAnswer);

        REQUIRE(fixture.view_model->coachingState.get().check_submitted);
    }

    SECTION("ApplyCoachingStep command dispatches correctly") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();

        fixture.view_model->executeCommand(GameCommand::ApplyCoachingStep);

        REQUIRE_FALSE(fixture.view_model->isCoachingActive());
    }

    SECTION("canExecuteCommand for Check/Apply requires TryIt phase") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // No coaching active
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::CheckCoachingAnswer));
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ApplyCoachingStep));

        // Hinting phase (level 1)
        fixture.view_model->requestCoachingHint();
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::CheckCoachingAnswer));
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ApplyCoachingStep));

        // TryIt phase (level 3)
        fixture.view_model->requestCoachingHint();
        fixture.view_model->requestCoachingHint();
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::CheckCoachingAnswer));
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ApplyCoachingStep));
    }
}
