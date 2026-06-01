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

#include "fixture_serializer.h"

#include <filesystem>
#include <fstream>

#include <yaml-cpp/yaml.h>

namespace sudoku::testing {

namespace {

YAML::Node serializeElim(const FixtureElim& e) {
    YAML::Node node;
    node["r"] = e.row;
    node["c"] = e.col;
    node["v"] = e.value;
    return node;
}

FixtureElim deserializeElim(const YAML::Node& node) {
    return {.row = node["r"].as<size_t>(), .col = node["c"].as<size_t>(), .value = node["v"].as<int>()};
}

YAML::Node serializeSnapshot(const FixtureSnapshot& s) {
    YAML::Node node;
    node["id"] = s.id;

    YAML::Node puzzle;
    puzzle["seed"] = s.seed;
    puzzle["clue_count"] = s.clue_count;
    puzzle["rating"] = s.rating;
    puzzle["board"] = s.board_flat;
    puzzle["solution"] = s.solution_flat;
    node["puzzle"] = puzzle;

    YAML::Node state;
    state["step_index"] = s.step_index;
    state["board"] = s.board_at_step;
    for (const auto& e : s.prior_eliminations) {
        state["prior_eliminations"].push_back(serializeElim(e));
    }
    node["state"] = state;

    YAML::Node result;
    result["technique"] = s.technique;
    result["technique_id"] = s.technique_id;
    result["step_type"] = s.step_type;
    result["se_rating"] = s.se_rating;
    result["correct"] = s.correct;
    if (s.placement.has_value()) {
        result["placement"] = serializeElim(*s.placement);
    }
    for (const auto& e : s.eliminations) {
        result["eliminations"].push_back(serializeElim(e));
    }
    node["result"] = result;

    YAML::Node training;
    training["explanation"] = s.explanation;
    for (const auto& [r, c] : s.positions) {
        YAML::Node pos;
        pos["r"] = r;
        pos["c"] = c;
        training["positions"].push_back(pos);
    }
    for (int v : s.values) {
        training["values"].push_back(v);
    }
    for (const auto& role : s.position_roles) {
        training["position_roles"].push_back(role);
    }
    training["region_type"] = s.region_type;
    training["region_index"] = s.region_index;
    training["pattern_axis"] = s.pattern_axis;
    training["elimination_axis"] = s.elimination_axis;
    if (s.technique_subtype >= 0) {
        training["technique_subtype"] = s.technique_subtype;
    }
    node["training"] = training;

    return node;
}

FixtureSnapshot deserializeSnapshot(const YAML::Node& node) {
    FixtureSnapshot s;
    s.id = node["id"].as<std::string>();

    const auto& puzzle = node["puzzle"];
    s.seed = puzzle["seed"].as<uint32_t>();
    s.clue_count = puzzle["clue_count"].as<int>();
    s.rating = puzzle["rating"].as<double>();
    s.board_flat = puzzle["board"].as<std::string>();
    s.solution_flat = puzzle["solution"].as<std::string>();

    const auto& state = node["state"];
    s.step_index = state["step_index"].as<int>();
    s.board_at_step = state["board"].as<std::string>();
    if (state["prior_eliminations"]) {
        for (const auto& e : state["prior_eliminations"]) {
            s.prior_eliminations.push_back(deserializeElim(e));
        }
    }

    const auto& result = node["result"];
    s.technique = result["technique"].as<std::string>();
    s.technique_id = result["technique_id"].as<int>();
    s.step_type = result["step_type"].as<std::string>();
    s.se_rating = result["se_rating"].as<double>();
    s.correct = result["correct"].as<bool>();
    if (result["placement"]) {
        s.placement = deserializeElim(result["placement"]);
    }
    if (result["eliminations"]) {
        for (const auto& e : result["eliminations"]) {
            s.eliminations.push_back(deserializeElim(e));
        }
    }

    const auto& training = node["training"];
    s.explanation = training["explanation"].as<std::string>();
    if (training["positions"]) {
        for (const auto& pos : training["positions"]) {
            s.positions.emplace_back(pos["r"].as<size_t>(), pos["c"].as<size_t>());
        }
    }
    if (training["values"]) {
        for (const auto& v : training["values"]) {
            s.values.push_back(v.as<int>());
        }
    }
    if (training["position_roles"]) {
        for (const auto& r : training["position_roles"]) {
            s.position_roles.push_back(r.as<std::string>());
        }
    }
    s.region_type = training["region_type"].as<std::string>("");
    s.region_index = training["region_index"].as<size_t>(0);
    s.pattern_axis = training["pattern_axis"].as<std::string>("");
    s.elimination_axis = training["elimination_axis"].as<std::string>("");
    s.technique_subtype = training["technique_subtype"].as<int>(-1);

    return s;
}

}  // namespace

void writeFixtureFile(const std::string& path, const std::string& difficulty,
                      const std::vector<FixtureSnapshot>& snapshots) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    YAML::Node root;
    root["version"] = "1.0";
    root["difficulty"] = difficulty;
    root["snapshot_count"] = static_cast<int>(snapshots.size());

    for (const auto& s : snapshots) {
        root["snapshots"].push_back(serializeSnapshot(s));
    }

    std::ofstream out(path);
    out << root;
}

std::vector<FixtureSnapshot> readFixtureFile(const std::string& path) {
    std::vector<FixtureSnapshot> result;
    if (!std::filesystem::exists(path)) {
        return result;
    }

    YAML::Node root = YAML::LoadFile(path);
    if (!root["snapshots"]) {
        return result;
    }

    for (const auto& node : root["snapshots"]) {
        result.push_back(deserializeSnapshot(node));
    }
    return result;
}

void writeFalsePositives(const std::string& path, const std::string& difficulty,
                         const std::vector<FalsePositive>& false_positives, int total_puzzles, int total_steps,
                         const std::map<std::string, int>& by_technique) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    YAML::Node root;
    root["version"] = "1.0";
    root["difficulty"] = difficulty;

    for (const auto& fp : false_positives) {
        YAML::Node node;
        node["id"] = fp.id;

        YAML::Node puzzle;
        puzzle["seed"] = fp.seed;
        puzzle["board"] = fp.board_flat;
        puzzle["solution"] = fp.solution_flat;
        node["puzzle"] = puzzle;

        YAML::Node error;
        error["technique"] = fp.technique;
        error["step_index"] = fp.step_index;
        error["board_at_step"] = fp.board_at_step;
        error["error_type"] = fp.error_type;
        for (const auto& e : fp.actual_result) {
            error["actual_result"].push_back(serializeElim(e));
        }
        error["correct_value"] = fp.correct_value;
        error["error_row"] = fp.error_row;
        error["error_col"] = fp.error_col;
        node["error"] = error;

        for (const auto& e : fp.prior_eliminations) {
            node["prior_eliminations"].push_back(serializeElim(e));
        }

        root["false_positives"].push_back(node);
    }

    YAML::Node summary;
    summary["total_puzzles"] = total_puzzles;
    summary["total_steps"] = total_steps;
    summary["false_positive_count"] = static_cast<int>(false_positives.size());
    for (const auto& [technique, count] : by_technique) {
        summary["by_technique"][technique] = count;
    }
    root["summary"] = summary;

    std::ofstream out(path);
    out << root;
}

void writeGeneratorState(const std::string& path, const GeneratorState& state) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    YAML::Node root;
    for (const auto& [diff, ds] : state.difficulties) {
        YAML::Node d;
        d["last_seed"] = ds.last_seed;
        d["puzzles"] = ds.puzzles_processed;
        d["snapshots"] = ds.snapshots_generated;
        root["difficulties"][diff] = d;
    }
    for (const auto& [technique, ce] : state.coverage) {
        YAML::Node c;
        c["count"] = ce.count;
        c["target"] = ce.target;
        root["coverage"][technique] = c;
    }

    std::ofstream out(path);
    out << root;
}

GeneratorState readGeneratorState(const std::string& path) {
    GeneratorState state;
    if (!std::filesystem::exists(path)) {
        return state;
    }

    YAML::Node root = YAML::LoadFile(path);

    if (root["difficulties"]) {
        for (const auto& it : root["difficulties"]) {
            auto name = it.first.as<std::string>();
            const auto& d = it.second;
            state.difficulties[name] = {.last_seed = d["last_seed"].as<int>(0),
                                        .puzzles_processed = d["puzzles"].as<int>(0),
                                        .snapshots_generated = d["snapshots"].as<int>(0)};
        }
    }

    if (root["coverage"]) {
        for (const auto& it : root["coverage"]) {
            auto name = it.first.as<std::string>();
            const auto& c = it.second;
            state.coverage[name] = {.count = c["count"].as<int>(0), .target = c["target"].as<int>(0)};
        }
    }

    return state;
}

GeneratorConfig readGeneratorConfig(const std::string& path) {
    GeneratorConfig config;
    if (!std::filesystem::exists(path)) {
        return config;
    }

    YAML::Node root = YAML::LoadFile(path);

    config.default_target = root["default_target"].as<int>(20);

    if (root["targets"]) {
        for (const auto& it : root["targets"]) {
            config.targets[it.first.as<std::string>()] = it.second.as<int>();
        }
    }

    if (root["generation"]) {
        const auto& gen = root["generation"];
        if (gen["difficulties"]) {
            config.difficulties.clear();
            for (const auto& d : gen["difficulties"]) {
                config.difficulties.push_back(d.as<std::string>());
            }
        }
        config.max_puzzles_per_difficulty = gen["max_puzzles_per_difficulty"].as<int>(5000);
    }

    if (config.difficulties.empty()) {
        config.difficulties = {"Hard", "Expert", "Master"};
    }

    return config;
}

}  // namespace sudoku::testing
