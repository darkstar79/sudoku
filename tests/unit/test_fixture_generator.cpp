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

#include "../../src/core/puzzle_generator.h"
#include "../helpers/fixture_serializer.h"
#include "../helpers/strategy_test_utils.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

using namespace sudoku::core;
using namespace sudoku::testing;

namespace {

constexpr int MAX_ITERATIONS_PER_PUZZLE = 300;
constexpr int FLUSH_INTERVAL = 50;

const std::string DATA_DIR = "tests/data";
const std::string FIXTURES_DIR = DATA_DIR + "/fixtures";
const std::string STATE_PATH = DATA_DIR + "/generator_state.yaml";
const std::string CONFIG_PATH = DATA_DIR + "/generator_config.yaml";

[[nodiscard]] Difficulty parseDifficulty(const std::string& name) {
    if (name == "Easy") {
        return Difficulty::Easy;
    }
    if (name == "Medium") {
        return Difficulty::Medium;
    }
    if (name == "Hard") {
        return Difficulty::Hard;
    }
    if (name == "Expert") {
        return Difficulty::Expert;
    }
    if (name == "Master") {
        return Difficulty::Master;
    }
    return Difficulty::Hard;
}

[[nodiscard]] FixtureSnapshot buildSnapshot(const Puzzle& puzzle, const std::string& difficulty,
                                            const BoardData& board_at_step,
                                            const std::vector<FixtureElim>& prior_eliminations, const SolveStep& step,
                                            int step_index) {
    FixtureSnapshot snapshot;
    snapshot.id = fmt::format("{}_s{}_i{}", difficulty, puzzle.seed, step_index);
    snapshot.seed = puzzle.seed;
    snapshot.difficulty = difficulty;
    snapshot.clue_count = puzzle.clue_count;
    snapshot.rating = puzzle.rating;
    snapshot.board_flat = boardToFlatString(puzzle.board);
    snapshot.solution_flat = boardToFlatString(puzzle.solution);
    snapshot.step_index = step_index;
    snapshot.board_at_step = boardToFlatString(board_at_step);
    snapshot.prior_eliminations = prior_eliminations;

    snapshot.technique = std::string(getTechniqueName(step.technique));
    snapshot.technique_id = static_cast<int>(step.technique);
    snapshot.step_type = (step.type == SolveStepType::Placement) ? "Placement" : "Elimination";
    snapshot.se_rating = getTechniqueRating(step.technique);
    snapshot.correct = true;

    if (step.type == SolveStepType::Placement) {
        snapshot.placement = FixtureElim{.row = step.position.row, .col = step.position.col, .value = step.value};
    }
    for (const auto& e : step.eliminations) {
        snapshot.eliminations.push_back(FixtureElim{.row = e.position.row, .col = e.position.col, .value = e.value});
    }

    // Training data
    snapshot.explanation = step.explanation;
    for (const auto& pos : step.explanation_data.positions) {
        snapshot.positions.emplace_back(pos.row, pos.col);
    }
    snapshot.values = step.explanation_data.values;
    for (const auto& role : step.explanation_data.position_roles) {
        snapshot.position_roles.emplace_back(getCellRoleName(role));
    }
    snapshot.region_type = std::string(getRegionTypeName(step.explanation_data.region_type));
    snapshot.region_index = step.explanation_data.region_index;
    snapshot.pattern_axis = std::string(getRegionTypeName(step.explanation_data.pattern_axis));
    snapshot.elimination_axis = std::string(getRegionTypeName(step.explanation_data.elimination_axis));
    snapshot.technique_subtype = step.explanation_data.technique_subtype;

    return snapshot;
}

[[nodiscard]] FalsePositive buildFalsePositive(const Puzzle& puzzle, const std::string& difficulty,
                                               const BoardData& board_at_step,
                                               const std::vector<FixtureElim>& prior_eliminations,
                                               const SolveStep& step, int step_index, const std::string& error_type,
                                               int correct_value, size_t error_row, size_t error_col) {
    FalsePositive fp;
    fp.id = fmt::format("{}_s{}_i{}", difficulty, puzzle.seed, step_index);
    fp.seed = puzzle.seed;
    fp.board_flat = boardToFlatString(puzzle.board);
    fp.solution_flat = boardToFlatString(puzzle.solution);
    fp.technique = std::string(getTechniqueName(step.technique));
    fp.step_index = step_index;
    fp.board_at_step = boardToFlatString(board_at_step);
    fp.error_type = error_type;
    fp.correct_value = correct_value;
    fp.error_row = error_row;
    fp.error_col = error_col;

    if (step.type == SolveStepType::Placement) {
        fp.actual_result.push_back(
            FixtureElim{.row = step.position.row, .col = step.position.col, .value = step.value});
    } else {
        for (const auto& e : step.eliminations) {
            fp.actual_result.push_back(FixtureElim{.row = e.position.row, .col = e.position.col, .value = e.value});
        }
    }
    fp.prior_eliminations = prior_eliminations;

    return fp;
}

[[nodiscard]] bool allCoverageMet(const std::map<std::string, CoverageEntry>& coverage) {
    for (const auto& [name, entry] : coverage) {
        if (entry.count < entry.target) {
            return false;
        }
    }
    return true;
}

void printCoverageSummary(const std::map<std::string, CoverageEntry>& coverage) {
    std::cerr << "\n=== FIXTURE COVERAGE SUMMARY ===\n";
    int met = 0;
    int total = 0;
    for (const auto& [name, entry] : coverage) {
        ++total;
        bool complete = entry.count >= entry.target;
        if (complete) {
            ++met;
        }
        std::cerr << fmt::format("  {:30s} {:4d}/{:4d} {}\n", name, entry.count, entry.target,
                                 complete ? "[OK]" : "[NEED]");
    }
    std::cerr << fmt::format("\n  Coverage: {}/{} techniques at target\n", met, total);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void generateForDifficulty(const std::string& difficulty_name, const GeneratorConfig& config, GeneratorState& state) {
    auto difficulty = parseDifficulty(difficulty_name);
    auto strategies = createStrategyChain();
    PuzzleGenerator generator;

    auto& diff_state = state.difficulties[difficulty_name];
    int start_seed = diff_state.last_seed + 1;
    int max_seed = start_seed + config.max_puzzles_per_difficulty;

    std::string fixture_dir = FIXTURES_DIR + "/" + difficulty_name;
    std::string fixture_path = fixture_dir + "/fixtures.yaml";
    std::string fp_path = fixture_dir + "/false_positives.yaml";

    // Load existing fixtures for this difficulty (coverage already counted globally)
    auto snapshots = readFixtureFile(fixture_path);

    std::vector<FalsePositive> false_positives;
    std::map<std::string, int> fp_by_technique;
    int total_steps_validated = 0;
    int puzzles_this_run = 0;

    for (int seed = start_seed; seed < max_seed; ++seed) {
        if (allCoverageMet(state.coverage)) {
            std::cerr << fmt::format("  All coverage targets met at seed {} for {}\n", seed, difficulty_name);
            break;
        }

        GenerationSettings settings{
            .difficulty = difficulty,
            .seed = static_cast<uint32_t>(seed),
            .ensure_unique = true,
            .max_attempts = 500,
        };

        auto puzzle_result = generator.generatePuzzle(settings);
        if (!puzzle_result.has_value()) {
            continue;
        }

        auto& puzzle = puzzle_result.value();
        const auto& truth = puzzle.solution;
        auto working = puzzle.board;
        CandidateGrid candidates(working);
        std::vector<FixtureElim> prior_elims;

        for (int step_idx = 0; step_idx < MAX_ITERATIONS_PER_PUZZLE; ++step_idx) {
            if (isBoardComplete(working)) {
                break;
            }

            // Find next step
            std::optional<SolveStep> found_step;
            for (const auto& strategy : strategies) {
                auto step = strategy->findStep(working, candidates);
                if (step.has_value()) {
                    found_step = step.value();
                    break;
                }
            }
            if (!found_step.has_value()) {
                break;  // Needs backtracking
            }

            ++total_steps_validated;
            auto technique_name = std::string(getTechniqueName(found_step->technique));

            // Validate against ground truth
            bool is_correct = true;
            std::string error_type;
            int correct_val = 0;
            size_t err_row = 0;
            size_t err_col = 0;

            if (found_step->type == SolveStepType::Placement) {
                correct_val = truth[found_step->position.row][found_step->position.col];
                if (found_step->value != correct_val) {
                    is_correct = false;
                    error_type = "wrong_placement";
                    err_row = found_step->position.row;
                    err_col = found_step->position.col;
                }
            } else {
                for (const auto& elim : found_step->eliminations) {
                    correct_val = truth[elim.position.row][elim.position.col];
                    if (elim.value == correct_val) {
                        is_correct = false;
                        error_type = "wrong_elimination";
                        err_row = elim.position.row;
                        err_col = elim.position.col;
                        break;
                    }
                }
            }

            if (!is_correct) {
                auto fp = buildFalsePositive(puzzle, difficulty_name, working, prior_elims, *found_step, step_idx,
                                             error_type, correct_val, err_row, err_col);
                false_positives.push_back(fp);
                fp_by_technique[technique_name]++;
                WARN(fmt::format("FALSE POSITIVE: {} at seed {} step {} ({})", technique_name, seed, step_idx,
                                 error_type));
                break;  // Candidate state corrupted
            }

            // Capture snapshot if technique/variant needs more coverage
            auto cov_key =
                coverageKey(technique_name, found_step->technique, found_step->explanation_data.technique_subtype);
            auto cov_it = state.coverage.find(cov_key);
            if (cov_it != state.coverage.end() && cov_it->second.count < cov_it->second.target) {
                auto snapshot = buildSnapshot(puzzle, difficulty_name, working, prior_elims, *found_step, step_idx);
                snapshots.push_back(snapshot);
                cov_it->second.count++;
                diff_state.snapshots_generated++;
            }

            // Apply step
            if (found_step->type == SolveStepType::Placement) {
                working[found_step->position.row][found_step->position.col] = found_step->value;
                candidates.placeValue(found_step->position.row, found_step->position.col, found_step->value);
            } else {
                for (const auto& elim : found_step->eliminations) {
                    candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
                    prior_elims.push_back(
                        FixtureElim{.row = elim.position.row, .col = elim.position.col, .value = elim.value});
                }
            }
        }

        diff_state.last_seed = seed;
        diff_state.puzzles_processed++;
        puzzles_this_run++;

        // Periodic flush
        if (puzzles_this_run % FLUSH_INTERVAL == 0) {
            writeFixtureFile(fixture_path, difficulty_name, snapshots);
            writeGeneratorState(STATE_PATH, state);
            std::cerr << fmt::format("  [{} seed {}] {} snapshots, {} false positives\n", difficulty_name, seed,
                                     snapshots.size(), false_positives.size());
        }
    }

    // Final flush
    writeFixtureFile(fixture_path, difficulty_name, snapshots);
    writeFalsePositives(fp_path, difficulty_name, false_positives, diff_state.puzzles_processed, total_steps_validated,
                        fp_by_technique);
    writeGeneratorState(STATE_PATH, state);

    std::cerr << fmt::format("\n  {} complete: {} puzzles, {} snapshots, {} false positives\n", difficulty_name,
                             puzzles_this_run, snapshots.size(), false_positives.size());
}

}  // namespace

TEST_CASE("Generate strategy fixture data", "[.][fixtures][generator]") {
    auto config = readGeneratorConfig(CONFIG_PATH);

    auto state = readGeneratorState(STATE_PATH);

    // Initialize coverage targets for all techniques and their variants
    auto strategies = createStrategyChain();
    for (const auto& strategy : strategies) {
        auto name = std::string(strategy->getName());
        auto technique = strategy->getTechnique();
        auto variants = getKnownVariants(technique);

        if (variants.empty()) {
            state.coverage[name] = CoverageEntry{.count = 0, .target = config.getTarget(name)};
        } else {
            for (const auto& variant : variants) {
                auto key = name + "/";
                key += variant;
                state.coverage[key] = CoverageEntry{.count = 0, .target = config.getTarget(key)};
            }
        }
    }

    // Recount coverage from ALL difficulty fixture files (cumulative across difficulties)
    for (const auto& diff : config.difficulties) {
        auto path = FIXTURES_DIR + "/";
        path += diff;
        path += "/fixtures.yaml";
        auto existing = readFixtureFile(path);
        for (const auto& s : existing) {
            auto technique = static_cast<SolvingTechnique>(s.technique_id);
            auto key = coverageKey(s.technique, technique, s.technique_subtype);
            if (state.coverage.contains(key)) {
                state.coverage[key].count++;
            }
        }
    }

    for (const auto& difficulty_name : config.difficulties) {
        SECTION(difficulty_name) {
            generateForDifficulty(difficulty_name, config, state);
        }
    }

    printCoverageSummary(state.coverage);
}

TEST_CASE("Fixture serializer round-trips fish axis fields", "[fixtures][serializer]") {
    // gh#39: pattern_axis (base orientation) and elimination_axis (where elims land)
    // must survive a write/read cycle alongside the legacy region_type/region_index.
    FixtureSnapshot in;
    in.id = "axis_roundtrip";
    in.seed = 42;
    in.clue_count = 30;
    in.rating = 5.2;
    in.board_flat = std::string(static_cast<size_t>(TOTAL_CELLS), '0');
    in.solution_flat = std::string(static_cast<size_t>(TOTAL_CELLS), '0');
    in.board_at_step = std::string(static_cast<size_t>(TOTAL_CELLS), '0');
    in.technique = "Finned X-Wing";
    in.technique_id = static_cast<int>(SolvingTechnique::FinnedXWing);
    in.step_type = "Elimination";
    in.se_rating = 5.2;
    in.eliminations = {FixtureElim{.row = 4, .col = 7, .value = 6}};
    in.pattern_axis = "Row";
    in.elimination_axis = "Box";

    const auto path = (std::filesystem::temp_directory_path() / "sudoku_axis_roundtrip.yaml").string();
    writeFixtureFile(path, "Hard", {in});
    auto out = readFixtureFile(path);
    std::filesystem::remove(path);

    REQUIRE(out.size() == 1);
    CHECK(out[0].pattern_axis == "Row");
    CHECK(out[0].elimination_axis == "Box");
}
