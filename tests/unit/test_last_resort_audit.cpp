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

// Per-state findStep probing audit for the last-resort strategy tier (rating >= 7.0).
//
// Why this exists: the [.][strategy][correctness] harness only validates a deduction when
// solvePuzzle's first-applicable chain actually fires it. The last-resort tier is registered
// ~40th onward, so cheaper strategies preempt it on virtually every generated puzzle and it gets
// ZERO positive replay coverage. Issue #59 (a Kraken inverted elimination) hid in exactly this
// blind spot — reachable only by calling findStep independently, never by the solve loop, yet
// user-reachable through the "Find step by technique" feature. This harness generalizes the
// [.][kraken][mining] probe across the whole tier: it advances a persistent CandidateGrid via the
// full chain and, at EVERY intermediate state, asks each last-resort strategy what it would deduce,
// ground-truthing every elimination AND placement against the puzzle's known solution.

#include "../../src/core/backtracking_solver.h"
#include "../../src/core/candidate_grid.h"
#include "../../src/core/game_validator.h"
#include "../../src/core/i_solving_strategy.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/solve_step.h"
#include "../helpers/strategy_test_utils.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// A strategy is "last-resort" iff its difficulty rating clears this threshold. The next strategy
// below the tier is Grouped X-Cycles at 6.8, so 7.0 selects exactly the intended 13 (ALS / forcing-
// chain / Kraken / Exocet / Nice-Loop families). Filtering by rating — rather than a hardcoded list
// — auto-covers any future last-resort strategy; the name-set assertion below makes a drift loud.
constexpr double kLastResortRating = 7.0;

// Cap reproducer dumps per strategy so a common bug (e.g. #59 fired on 814 Hard puzzles) does not
// flood stderr; counts still accumulate in full.
constexpr int kMaxDumpsPerStrategy = 3;

// Optional per-difficulty corpus scaling for the Ask-First "widen corpus" path. Defaults keep the
// committed harness modest so the standing regression run stays fast; export e.g.
// SUDOKU_AUDIT_MASTER=400 to exercise the rare tier (Junior Exocet) far harder. Non-throwing.
[[nodiscard]] int envCount(const char* var, int fallback) {
    const char* raw = std::getenv(var);
    if (raw == nullptr) {
        return fallback;
    }
    const std::string_view sv{raw};
    int value = fallback;
    if (std::from_chars(sv.data(), sv.data() + sv.size(), value).ec == std::errc{} && value >= 0) {
        return value;
    }
    return fallback;
}

struct StrategyAuditResult {
    std::string name;
    long probed_steps{0};
    long inverted_elims{0};
    long wrong_placements{0};
    int dumps{0};
};

struct AuditTotals {
    int generated{0};
    int aborted{0};  // puzzles excluded because the advancing chain itself diverged from truth
    std::vector<StrategyAuditResult> per_strategy;
};

// True iff `step`, applied to a truth-consistent state, contradicts the known solution: a placement
// of a non-solution value, or an elimination of the solution's own value. Used both to flag a
// last-resort deduction and to detect (and abandon) a puzzle whose advancing chain has gone wrong.
[[nodiscard]] bool stepContradictsTruth(const SolveStep& step, const BoardData& truth) {
    if (step.type == SolveStepType::Placement) {
        return truth[step.position.row][step.position.col] != step.value;
    }
    return std::ranges::any_of(
        step.eliminations, [&truth](const Elimination& e) { return truth[e.position.row][e.position.col] == e.value; });
}

void dumpReproducer(StrategyAuditResult& res, const SolveStep& step, const BoardData& truth, int seed, int difficulty,
                    const BoardData& givens, const BoardData& working) {
    if (res.dumps >= kMaxDumpsPerStrategy) {
        return;
    }
    res.dumps++;
    std::cerr << "\n=== LAST-RESORT AUDIT REPRODUCER (" << res.name << ") ===\n"
              << "Seed " << seed << " (difficulty " << difficulty << "): ";
    if (step.type == SolveStepType::Placement) {
        std::cerr << "placed " << step.value << " at R" << (step.position.row + 1) << "C" << (step.position.col + 1)
                  << " but the solution there is " << truth[step.position.row][step.position.col] << ".\n";
    } else {
        std::cerr << "eliminated a candidate equal to the solution value.\n";
    }
    std::cerr << "Explanation: " << step.explanation << "\n"
              << "Original puzzle (flat): " << sudoku::testing::boardToFlatString(givens)
              << "\nWorking board at step (flat): " << sudoku::testing::boardToFlatString(working) << "\n";
}

// Probe one last-resort strategy at the current state and ground-truth its deduction.
void recordProbe(ISolvingStrategy* strategy, StrategyAuditResult& res, const BoardData& working,
                 const CandidateGrid& candidates, const BoardData& truth, int seed, int difficulty,
                 const BoardData& givens) {
    auto step = strategy->findStep(working, candidates);
    if (!step.has_value()) {
        return;
    }
    res.probed_steps++;
    if (step->type == SolveStepType::Placement) {
        if (truth[step->position.row][step->position.col] != step->value) {
            res.wrong_placements++;
            dumpReproducer(res, *step, truth, seed, difficulty, givens, working);
        }
        return;
    }
    bool dumped = false;
    for (const auto& e : step->eliminations) {
        if (truth[e.position.row][e.position.col] == e.value) {
            res.inverted_elims++;  // count every inverted elimination...
            if (!dumped) {         // ...but dump the reproducer only once per step
                dumpReproducer(res, *step, truth, seed, difficulty, givens, working);
                dumped = true;
            }
        }
    }
}

// Walk a corpus: generate each puzzle, advance a persistent CandidateGrid via the full chain, and at
// every intermediate state probe each last-resort strategy. Accumulates into `totals` (whose
// per_strategy vector is index-aligned with `probes`).
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — diagnostic harness; nesting is inherent
void auditCorpus(Difficulty difficulty, int num_puzzles, int start_seed, const std::vector<ISolvingStrategy*>& probes,
                 const std::vector<std::unique_ptr<ISolvingStrategy>>& chain, AuditTotals& totals) {
    PuzzleGenerator generator;
    for (int seed = start_seed; seed < start_seed + num_puzzles; ++seed) {
        GenerationSettings settings{
            .difficulty = difficulty, .seed = static_cast<uint32_t>(seed), .ensure_unique = true, .max_attempts = 500};
        auto puzzle = generator.generatePuzzle(settings);
        if (!puzzle.has_value()) {
            continue;
        }
        totals.generated++;
        const auto& truth = puzzle->solution;
        auto working = puzzle->board;
        CandidateGrid candidates(working);

        constexpr int kMaxIterations = 300;
        for (int iter = 0; iter < kMaxIterations && !sudoku::testing::isBoardComplete(working); ++iter) {
            // The state is truth-consistent here (any divergence is caught on the advance below, then
            // we break before the next probe). Probe every last-resort strategy independently.
            for (size_t i = 0; i < probes.size(); ++i) {
                recordProbe(probes[i], totals.per_strategy[i], working, candidates, truth, seed,
                            static_cast<int>(difficulty), puzzle->board);
            }

            // Advance the replay using the chain's first-applicable step.
            std::optional<SolveStep> step;
            for (const auto& strategy : chain) {
                step = strategy->findStep(working, candidates);
                if (step.has_value()) {
                    break;
                }
            }
            if (!step.has_value()) {
                break;  // would fall back to backtracking
            }
            // Safety net: if the advancing chain itself ever contradicts the solution, abandon this
            // puzzle so an upstream cheap-strategy bug is never attributed to a last-resort strategy.
            if (stepContradictsTruth(*step, truth)) {
                totals.aborted++;
                break;
            }
            if (step->type == SolveStepType::Placement) {
                working[step->position.row][step->position.col] = step->value;
                candidates.placeValue(step->position.row, step->position.col, step->value);
            } else {
                for (const auto& e : step->eliminations) {
                    candidates.eliminateCandidate(e.position.row, e.position.col, e.value);
                }
            }
        }
    }
}

// Curated fixtures for strategies that generated corpora cannot reach. Junior Exocet (rating 9.4)
// is the canonical case: probing its findStep at the initial state of 2000 generated Expert/Master
// puzzles fired ZERO times, and its 9 generated intermediate-state hits do not reconstruct durably.
// So its only durable audit coverage is hand-curated boards where it provably fires at the initial
// candidate state. This board is the SudokuWiki/Exocet compendium worked example (already a positive
// fixture in test_junior_exocet_strategy.cpp); here it is additionally ground-truthed — verifying
// the published elimination is SOUND against the solution, not merely that it fires. Add more
// authoritative known-Exocet boards here as they surface.
constexpr std::array<std::string_view, 1> kCuratedFixtures{
    "007020004930000600600300000000000050200010008006900400003700900020050001000008000"};

// Probe the last-resort tier on each curated fixture and ground-truth against a backtracking
// solution. Every last-resort technique is non-uniqueness logic, so any one valid solution is a
// sound oracle (a sound deduction can never strike the solution's own value).
void auditFixtures(const std::vector<ISolvingStrategy*>& probes, std::vector<StrategyAuditResult>& per_strategy,
                   int& fixtures_probed) {
    const BacktrackingSolver solver{std::make_shared<GameValidator>()};
    const PuzzleGenerator generator;
    for (const auto& flat : kCuratedFixtures) {
        BoardData board = sudoku::testing::flatStringToBoard(std::string(flat));
        // A multi-solution fixture would make the backtracking oracle arbitrary, so a genuinely
        // unsound deduction (valid only under a different solution) could read as sound. Require
        // uniqueness so the single solution is THE oracle.
        REQUIRE(generator.hasUniqueSolution(board));
        BoardData truth = board;
        REQUIRE(solver.solve(truth));  // fixture must be solvable for a valid oracle
        fixtures_probed++;
        CandidateGrid candidates(board);
        for (size_t i = 0; i < probes.size(); ++i) {
            recordProbe(probes[i], per_strategy[i], board, candidates, truth, /*seed=*/-1,
                        /*difficulty=*/-1, board);
        }
    }
}

}  // namespace

// Hidden audit harness ([.] = excluded from the default run). Probes every last-resort strategy's
// findStep independently at every intermediate candidate state across a generated corpus and ground-
// truths each elimination and placement. Run explicitly: unit_tests "[lastresort][audit]".
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — diagnostic harness with summary loop
TEST_CASE("Last-resort strategies emit no unsound deductions across a probed corpus", "[.][lastresort][audit]") {
    // The chain advances the replay; the same instances (filtered by rating) are probed directly.
    auto chain = sudoku::testing::createStrategyChain();
    std::vector<ISolvingStrategy*> probes;
    for (const auto& s : chain) {
        if (s->getDifficultyRating() >= kLastResortRating) {
            probes.push_back(s.get());
        }
    }

    // Guard against tier drift: a rating retune must not silently add/drop a probed strategy.
    const std::set<std::string_view> expected_tier{
        "ALS-XZ",        "Sue de Coq",           "ALS-XY-Wing",          "Death Blossom", "ALS Chain",
        "Forcing Chain", "Unit Forcing Chain",   "Region Forcing Chain", "Kraken Fish",   "Junior Exocet",
        "Nice Loop",     "Continuous Nice Loop", "Grouped Nice Loop"};
    std::set<std::string_view> actual_tier;
    for (auto* p : probes) {
        actual_tier.insert(p->getName());
    }
    REQUIRE(actual_tier == expected_tier);

    AuditTotals totals;
    totals.per_strategy.reserve(probes.size());
    for (auto* p : probes) {
        totals.per_strategy.push_back(StrategyAuditResult{.name = std::string(p->getName())});
    }

    // Hard-weighted, modest corpus: 13 heavy findStep calls per state is far costlier than the
    // single-strategy Kraken probe. Widen here for the Ask-First "widen corpus" path.
    const std::vector<std::pair<Difficulty, int>> corpus{{Difficulty::Hard, envCount("SUDOKU_AUDIT_HARD", 200)},
                                                         {Difficulty::Expert, envCount("SUDOKU_AUDIT_EXPERT", 100)},
                                                         {Difficulty::Master, envCount("SUDOKU_AUDIT_MASTER", 50)}};
    for (const auto& [difficulty, count] : corpus) {
        auditCorpus(difficulty, count, 1, probes, chain, totals);
    }

    // Curated fixtures cover strategies generated corpora cannot reach (Junior Exocet).
    int fixtures_probed = 0;
    auditFixtures(probes, totals.per_strategy, fixtures_probed);

    long total_inverted = 0;
    long total_wrong_placements = 0;
    long total_probed = 0;
    std::cerr << "\n[last-resort audit] generated=" << totals.generated << " aborted=" << totals.aborted
              << " curated_fixtures=" << fixtures_probed << "\n";
    for (const auto& res : totals.per_strategy) {
        std::cerr << "  " << res.name << ": probed=" << res.probed_steps << " inverted_elims=" << res.inverted_elims
                  << " wrong_placements=" << res.wrong_placements << "\n";
        total_inverted += res.inverted_elims;
        total_wrong_placements += res.wrong_placements;
        total_probed += res.probed_steps;
    }
    std::cerr << "[last-resort audit] total probed=" << total_probed << " inverted_elims=" << total_inverted
              << " wrong_placements=" << total_wrong_placements << "\n";

    // Every strategy must be probed at least once (else its green result is vacuous — the #59
    // false-negative trap, per-strategy). The default corpus fires all 12 generation-reachable
    // strategies; the curated fixture floors Junior Exocet at >= 1. A run scaled below the default
    // can legitimately trip this — it is a coverage signal, not a soundness failure.
    for (const auto& res : totals.per_strategy) {
        INFO("strategy probed zero times: " << res.name);
        CHECK(res.probed_steps > 0);
    }
    CHECK(total_probed > 0);
    CHECK(total_inverted == 0);
    CHECK(total_wrong_placements == 0);
}
