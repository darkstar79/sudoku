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

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sudoku::testing {

struct FixtureElim {
    size_t row{};
    size_t col{};
    int value{};
};

struct FixtureSnapshot {
    std::string id;

    // Puzzle metadata
    uint32_t seed{};
    std::string difficulty;
    int clue_count{};
    double rating{};
    std::string board_flat;     // 81-char original puzzle
    std::string solution_flat;  // 81-char solution

    // State at this step
    int step_index{};
    std::string board_at_step;  // 81-char board at this step
    std::vector<FixtureElim> prior_eliminations;

    // Strategy result
    std::string technique;
    int technique_id{};
    std::string step_type;  // "Placement" or "Elimination"
    double se_rating{};
    bool correct{true};
    std::optional<FixtureElim> placement;
    std::vector<FixtureElim> eliminations;

    // Training data
    std::string explanation;
    std::vector<std::pair<size_t, size_t>> positions;  // (row, col) pairs
    std::vector<int> values;
    std::vector<std::string> position_roles;
    std::string region_type;
    size_t region_index{};
    std::string pattern_axis;      // fish base-axis orientation (gh#39)
    std::string elimination_axis;  // axis/region where eliminations land (gh#39)
    int technique_subtype{-1};
};

struct FalsePositive {
    std::string id;

    // Puzzle
    uint32_t seed{};
    std::string board_flat;
    std::string solution_flat;

    // Error details
    std::string technique;
    int step_index{};
    std::string board_at_step;
    std::string error_type;  // "wrong_elimination" or "wrong_placement"
    std::vector<FixtureElim> actual_result;
    int correct_value{};
    size_t error_row{};
    size_t error_col{};

    // Context for reproduction
    std::vector<FixtureElim> prior_eliminations;
};

struct CoverageEntry {
    int count{};
    int target{};
};

struct DifficultyState {
    int last_seed{};
    int puzzles_processed{};
    int snapshots_generated{};
};

struct GeneratorState {
    std::map<std::string, DifficultyState> difficulties;
    std::map<std::string, CoverageEntry> coverage;
};

struct GeneratorConfig {
    int default_target{20};
    std::map<std::string, int> targets;
    std::vector<std::string> difficulties;
    int max_puzzles_per_difficulty{5000};

    [[nodiscard]] int getTarget(const std::string& technique) const {
        auto it = targets.find(technique);
        return it != targets.end() ? it->second : default_target;
    }
};

// Serialization functions
void writeFixtureFile(const std::string& path, const std::string& difficulty,
                      const std::vector<FixtureSnapshot>& snapshots);
[[nodiscard]] std::vector<FixtureSnapshot> readFixtureFile(const std::string& path);

void writeFalsePositives(const std::string& path, const std::string& difficulty,
                         const std::vector<FalsePositive>& false_positives, int total_puzzles, int total_steps,
                         const std::map<std::string, int>& by_technique);

void writeGeneratorState(const std::string& path, const GeneratorState& state);
[[nodiscard]] GeneratorState readGeneratorState(const std::string& path);

[[nodiscard]] GeneratorConfig readGeneratorConfig(const std::string& path);

}  // namespace sudoku::testing
