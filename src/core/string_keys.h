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

#include <string_view>

namespace sudoku::core::StringKeys {

// =========================================================================
// Menu items
// =========================================================================
inline constexpr std::string_view MenuGame = "menu.game";
inline constexpr std::string_view MenuNewGame = "menu.new_game";
inline constexpr std::string_view MenuResetPuzzle = "menu.reset_puzzle";
inline constexpr std::string_view MenuSave = "menu.save";
inline constexpr std::string_view MenuLoad = "menu.load";
inline constexpr std::string_view MenuStatistics = "menu.statistics";
inline constexpr std::string_view MenuExportAggregate = "menu.export_aggregate";
inline constexpr std::string_view MenuExportSessions = "menu.export_sessions";
inline constexpr std::string_view MenuExit = "menu.exit";
inline constexpr std::string_view MenuEdit = "menu.edit";
inline constexpr std::string_view MenuUndo = "menu.undo";
inline constexpr std::string_view MenuRedo = "menu.redo";
inline constexpr std::string_view MenuClearCell = "menu.clear_cell";
inline constexpr std::string_view MenuHelp = "menu.help";
inline constexpr std::string_view MenuGetHint = "menu.get_hint";
inline constexpr std::string_view MenuAbout = "menu.about";
inline constexpr std::string_view MenuTrainingMode = "menu.training_mode";
inline constexpr std::string_view MenuAnalyzePosition = "menu.analyze_position";
inline constexpr std::string_view MenuResumeGame = "menu.resume_game";
inline constexpr std::string_view MenuSettings = "menu.settings";
inline constexpr std::string_view MenuThirdPartyLicenses = "menu.third_party_licenses";

// =========================================================================
// Toolbar
// =========================================================================
inline constexpr std::string_view ToolbarNewGame = "toolbar.new_game";
inline constexpr std::string_view ToolbarDifficulty = "toolbar.difficulty";
inline constexpr std::string_view ToolbarHints = "toolbar.hints";
inline constexpr std::string_view ToolbarRating = "toolbar.rating";
inline constexpr std::string_view ToolbarHelpIcon = "toolbar.help_icon";

// =========================================================================
// Difficulty names (shared across toolbar combo, dialogs, sidebar)
// =========================================================================
inline constexpr std::string_view DifficultyEasy = "difficulty.easy";
inline constexpr std::string_view DifficultyMedium = "difficulty.medium";
inline constexpr std::string_view DifficultyHard = "difficulty.hard";
inline constexpr std::string_view DifficultyExpert = "difficulty.expert";
inline constexpr std::string_view DifficultyMaster = "difficulty.master";
inline constexpr std::string_view DifficultyUnknown = "difficulty.unknown";

// =========================================================================
// Action buttons
// =========================================================================
inline constexpr std::string_view ButtonCheckSolution = "button.check_solution";
inline constexpr std::string_view ButtonResetPuzzle = "button.reset_puzzle";
inline constexpr std::string_view ButtonUndo = "button.undo";
inline constexpr std::string_view ButtonRedo = "button.redo";
inline constexpr std::string_view ButtonUndoToValid = "button.undo_to_valid";
inline constexpr std::string_view ButtonAutoNotesOn = "button.auto_notes_on";
inline constexpr std::string_view ButtonAutoNotesOff = "button.auto_notes_off";
inline constexpr std::string_view ButtonFillNotes = "button.fill_notes";
inline constexpr std::string_view ButtonUndoUntilValid = "button.undo_until_valid";

// =========================================================================
// Input modes
// =========================================================================
inline constexpr std::string_view ModeNormal = "mode.normal";
inline constexpr std::string_view ModeNotes = "mode.notes";
inline constexpr std::string_view ModeColor = "mode.color";

// =========================================================================
// Training mode
// =========================================================================
inline constexpr std::string_view TrainingTitle = "training.title";
inline constexpr std::string_view TrainingAnalyzeTitle = "training.analyze_title";
inline constexpr std::string_view TrainingSelectTechnique = "training.select_technique";
inline constexpr std::string_view TrainingBackToGame = "training.back_to_game";
inline constexpr std::string_view TrainingGroupFoundations = "training.group.foundations";
inline constexpr std::string_view TrainingGroupSubsetBasics = "training.group.subset_basics";
inline constexpr std::string_view TrainingGroupIntersections = "training.group.intersections";
inline constexpr std::string_view TrainingGroupBasicFish = "training.group.basic_fish";
inline constexpr std::string_view TrainingGroupLinks = "training.group.links";
inline constexpr std::string_view TrainingGroupAdvancedFish = "training.group.advanced_fish";
inline constexpr std::string_view TrainingGroupFinnedFish = "training.group.finned_fish";
inline constexpr std::string_view TrainingGroupChains = "training.group.chains";
inline constexpr std::string_view TrainingGroupInference = "training.group.inference";
inline constexpr std::string_view TrainingWhatItIs = "training.what_it_is";
inline constexpr std::string_view TrainingWhatToLookFor = "training.what_to_look_for";
inline constexpr std::string_view TrainingStartExercises = "training.start_exercises";
inline constexpr std::string_view TrainingBack = "training.back";
inline constexpr std::string_view TrainingDifficultyPoints = "training.difficulty_points";
inline constexpr std::string_view TrainingPrerequisites = "training.prerequisites";
inline constexpr std::string_view TrainingExerciseHeader = "training.exercise_header";
inline constexpr std::string_view TrainingColor = "training.color";
inline constexpr std::string_view TrainingSubmit = "training.submit";
inline constexpr std::string_view TrainingHint = "training.hint";
inline constexpr std::string_view TrainingSkip = "training.skip";
inline constexpr std::string_view TrainingQuitLesson = "training.quit_lesson";
inline constexpr std::string_view TrainingNextExercise = "training.next_exercise";
inline constexpr std::string_view TrainingRetry = "training.retry";
inline constexpr std::string_view TrainingShowSolution = "training.show_solution";
inline constexpr std::string_view TrainingScore = "training.score";
inline constexpr std::string_view TrainingCorrect = "training.correct";
inline constexpr std::string_view TrainingPartiallyCorrect = "training.partially_correct";
inline constexpr std::string_view TrainingIncorrect = "training.incorrect";
inline constexpr std::string_view TrainingLessonComplete = "training.lesson_complete";
inline constexpr std::string_view TrainingTryAgain = "training.try_again";
inline constexpr std::string_view TrainingPickTechnique = "training.pick_technique";
inline constexpr std::string_view TrainingReturnToGame = "training.return_to_game";
inline constexpr std::string_view TrainingTechnique = "training.technique";
inline constexpr std::string_view TrainingHintsUsed = "training.hints_used";
inline constexpr std::string_view TrainingMastery = "training.mastery";
inline constexpr std::string_view TrainingPointsFmt = "training.points_fmt";
inline constexpr std::string_view TrainingPrereqNotMet = "training.prereq_not_met";
inline constexpr std::string_view TrainingRecommended = "training.recommended";
inline constexpr std::string_view TrainingApplicable = "training.applicable";
inline constexpr std::string_view TrainingExcellent = "training.excellent";
inline constexpr std::string_view TrainingGoodProgress = "training.good_progress";
inline constexpr std::string_view TrainingKeepPracticing = "training.keep_practicing";
inline constexpr std::string_view MasteryBeginner = "mastery.beginner";
inline constexpr std::string_view MasteryIntermediate = "mastery.intermediate";
inline constexpr std::string_view MasteryProficient = "mastery.proficient";
inline constexpr std::string_view MasteryMastered = "mastery.mastered";

// =========================================================================
// Status bar
// =========================================================================
inline constexpr std::string_view StatusCompleted = "status.completed";
inline constexpr std::string_view StatusPlaying = "status.playing";
inline constexpr std::string_view StatusReady = "status.ready";
inline constexpr std::string_view StatusPressF1 = "status.press_f1";
inline constexpr std::string_view StatusInProgress = "status.in_progress";

// =========================================================================
// Game board
// =========================================================================
inline constexpr std::string_view BoardNoGameLoaded = "board.no_game_loaded";

// =========================================================================
// Dialogs — New Game
// =========================================================================
inline constexpr std::string_view DialogNewGame = "dialog.new_game";
inline constexpr std::string_view DialogSelectDifficulty = "dialog.select_difficulty";
inline constexpr std::string_view DialogStartGame = "dialog.start_game";
inline constexpr std::string_view DialogCancel = "dialog.cancel";

// =========================================================================
// Dialogs — Reset
// =========================================================================
inline constexpr std::string_view DialogResetPuzzle = "dialog.reset_puzzle";
inline constexpr std::string_view DialogResetWarning = "dialog.reset_warning";
inline constexpr std::string_view DialogReset = "dialog.reset";

// =========================================================================
// Dialogs — Save
// =========================================================================
inline constexpr std::string_view DialogSaveGame = "dialog.save_game";
inline constexpr std::string_view DialogEnterSaveName = "dialog.enter_save_name";
inline constexpr std::string_view DialogSave = "dialog.save";

// =========================================================================
// Dialogs — Load
// =========================================================================
inline constexpr std::string_view DialogLoadGame = "dialog.load_game";
inline constexpr std::string_view DialogRecentSaves = "dialog.recent_saves";

// =========================================================================
// Dialogs — Statistics (parameterized with fmt-style {0})
// =========================================================================
inline constexpr std::string_view DialogStatistics = "dialog.statistics";
inline constexpr std::string_view DialogClose = "dialog.close";
inline constexpr std::string_view StatsGamesPlayed = "stats.games_played";
inline constexpr std::string_view StatsGamesCompleted = "stats.games_completed";
inline constexpr std::string_view StatsCompletionRate = "stats.completion_rate";
inline constexpr std::string_view StatsBestTime = "stats.best_time";
inline constexpr std::string_view StatsAverageTime = "stats.average_time";
inline constexpr std::string_view StatsCurrentStreak = "stats.current_streak";
inline constexpr std::string_view StatsBestStreak = "stats.best_streak";

// =========================================================================
// Dialogs — About
// =========================================================================
inline constexpr std::string_view DialogAbout = "dialog.about";
inline constexpr std::string_view AboutSudokuGame = "about.sudoku_game";
inline constexpr std::string_view AboutPhaseInfo = "about.phase_info";
inline constexpr std::string_view AboutBuiltWith = "about.built_with";

// =========================================================================
// Tooltips — Rating scale
// =========================================================================
inline constexpr std::string_view TooltipRatingScale = "tooltip.rating_scale";
inline constexpr std::string_view TooltipTechniquesRequired = "tooltip.techniques_required";

// =========================================================================
// Dialogs — Puzzle Difficulty
// =========================================================================
inline constexpr std::string_view DialogPuzzleDifficulty = "dialog.puzzle_difficulty";
inline constexpr std::string_view DialogPuzzleRating = "dialog.puzzle_rating";
inline constexpr std::string_view DialogTechniquesRequired = "dialog.techniques_required";
inline constexpr std::string_view DialogNoTechniqueDetails = "dialog.no_technique_details";

// =========================================================================
// Toolbar — Rating button format
// =========================================================================
inline constexpr std::string_view ToolbarRatingWithTechniques = "toolbar.rating_with_techniques";
inline constexpr std::string_view ToolbarRatingSimple = "toolbar.rating_simple";

// =========================================================================
// Toast notifications
// =========================================================================
inline constexpr std::string_view ToastGameSaved = "toast.game_saved";
inline constexpr std::string_view ToastAggregateExported = "toast.aggregate_exported";
inline constexpr std::string_view ToastSessionsExported = "toast.sessions_exported";
inline constexpr std::string_view ToastExportFailed = "toast.export_failed";

// =========================================================================
// Sidebar
// =========================================================================
inline constexpr std::string_view SidebarDifficulty = "sidebar.difficulty";
inline constexpr std::string_view SidebarRating = "sidebar.rating";
inline constexpr std::string_view SidebarLanguage = "sidebar.language";

// =========================================================================
// ViewModel — Error messages
// =========================================================================
inline constexpr std::string_view ErrorGeneratePuzzle = "error.generate_puzzle";
inline constexpr std::string_view ErrorLoadGame = "error.load_game";
inline constexpr std::string_view ErrorNoActiveGame = "error.no_active_game";
inline constexpr std::string_view ErrorSaveGame = "error.save_game";
inline constexpr std::string_view ErrorExportStats = "error.export_stats";
inline constexpr std::string_view ErrorExportAggregate = "error.export_aggregate";
inline constexpr std::string_view ErrorExportSessions = "error.export_sessions";
inline constexpr std::string_view ErrorFileAccess = "error.file_access";
inline constexpr std::string_view ErrorSerialization = "error.serialization";
inline constexpr std::string_view ErrorUnknown = "error.unknown";

// =========================================================================
// ViewModel — Status messages
// =========================================================================
inline constexpr std::string_view StatusNoValidState = "status.no_valid_state";
inline constexpr std::string_view StatusBoardValid = "status.board_valid";
inline constexpr std::string_view StatusUndoneToValid = "status.undone_to_valid";
inline constexpr std::string_view StatusPuzzleCompleted = "status.puzzle_completed";
inline constexpr std::string_view StatusSolutionErrors = "status.solution_errors";

// =========================================================================
// ViewModel — Hint messages
// =========================================================================
inline constexpr std::string_view HintNoRemaining = "hint.no_remaining";
inline constexpr std::string_view HintSelectCell = "hint.select_cell";
inline constexpr std::string_view HintCannotRevealGiven = "hint.cannot_reveal_given";
inline constexpr std::string_view HintCellHasValue = "hint.cell_has_value";
inline constexpr std::string_view HintNoTechnique = "hint.no_technique";
inline constexpr std::string_view HintSuggestionPlace = "hint.suggestion_place";

// =========================================================================
// ViewModel — Technique formatting
// =========================================================================
inline constexpr std::string_view TechniquePointsFmt = "technique.points_fmt";
inline constexpr std::string_view TechniqueBacktracking = "technique.backtracking";

// =========================================================================
// ViewModel — Statistics error strings
// =========================================================================
inline constexpr std::string_view StatsErrInvalidData = "stats_err.invalid_data";
inline constexpr std::string_view StatsErrFileAccess = "stats_err.file_access";
inline constexpr std::string_view StatsErrSerialization = "stats_err.serialization";
inline constexpr std::string_view StatsErrInvalidDifficulty = "stats_err.invalid_difficulty";
inline constexpr std::string_view StatsErrGameNotStarted = "stats_err.game_not_started";
inline constexpr std::string_view StatsErrGameAlreadyEnded = "stats_err.game_already_ended";
inline constexpr std::string_view StatsErrUnknown = "stats_err.unknown";

// =========================================================================
// Technique names
// =========================================================================
inline constexpr std::string_view TechNakedSingle = "tech.naked_single";
inline constexpr std::string_view TechHiddenSingle = "tech.hidden_single";
inline constexpr std::string_view TechNakedPair = "tech.naked_pair";
inline constexpr std::string_view TechNakedTriple = "tech.naked_triple";
inline constexpr std::string_view TechHiddenPair = "tech.hidden_pair";
inline constexpr std::string_view TechHiddenTriple = "tech.hidden_triple";
inline constexpr std::string_view TechPointingPair = "tech.pointing_pair";
inline constexpr std::string_view TechBoxLineReduction = "tech.box_line_reduction";
inline constexpr std::string_view TechNakedQuad = "tech.naked_quad";
inline constexpr std::string_view TechHiddenQuad = "tech.hidden_quad";
inline constexpr std::string_view TechXWing = "tech.x_wing";
inline constexpr std::string_view TechXYWing = "tech.xy_wing";
inline constexpr std::string_view TechSwordfish = "tech.swordfish";
inline constexpr std::string_view TechSkyscraper = "tech.skyscraper";
inline constexpr std::string_view TechTwoStringKite = "tech.two_string_kite";
inline constexpr std::string_view TechXYZWing = "tech.xyz_wing";
inline constexpr std::string_view TechUniqueRectangle = "tech.unique_rectangle";
inline constexpr std::string_view TechWWing = "tech.w_wing";
inline constexpr std::string_view TechSimpleColoring = "tech.simple_coloring";
inline constexpr std::string_view TechFinnedXWing = "tech.finned_x_wing";
inline constexpr std::string_view TechRemotePairs = "tech.remote_pairs";
inline constexpr std::string_view TechBUG = "tech.bug";
inline constexpr std::string_view TechJellyfish = "tech.jellyfish";
inline constexpr std::string_view TechFinnedSwordfish = "tech.finned_swordfish";
inline constexpr std::string_view TechEmptyRectangle = "tech.empty_rectangle";
inline constexpr std::string_view TechWXYZWing = "tech.wxyz_wing";
inline constexpr std::string_view TechFinnedJellyfish = "tech.finned_jellyfish";
inline constexpr std::string_view TechXYChain = "tech.xy_chain";
inline constexpr std::string_view TechMultiColoring = "tech.multi_coloring";
inline constexpr std::string_view TechALSxZ = "tech.als_xz";
inline constexpr std::string_view TechSueDeCoq = "tech.sue_de_coq";
inline constexpr std::string_view TechForcingChain = "tech.forcing_chain";
inline constexpr std::string_view TechNiceLoop = "tech.nice_loop";
inline constexpr std::string_view TechXCycles = "tech.x_cycles";
inline constexpr std::string_view TechThreeDMedusa = "tech.three_d_medusa";
inline constexpr std::string_view TechHiddenUniqueRectangle = "tech.hidden_unique_rectangle";
inline constexpr std::string_view TechAvoidableRectangle = "tech.avoidable_rectangle";
inline constexpr std::string_view TechALSXYWing = "tech.als_xy_wing";
inline constexpr std::string_view TechDeathBlossom = "tech.death_blossom";
inline constexpr std::string_view TechVWXYZWing = "tech.vwxyz_wing";
inline constexpr std::string_view TechFrankenFish = "tech.franken_fish";
inline constexpr std::string_view TechGroupedXCycles = "tech.grouped_x_cycles";
inline constexpr std::string_view TechBacktrackingName = "tech.backtracking_name";
inline constexpr std::string_view TechUnknown = "tech.unknown";

// =========================================================================
// Region names
// =========================================================================
inline constexpr std::string_view RegionRow = "region.row";
inline constexpr std::string_view RegionColumn = "region.column";
inline constexpr std::string_view RegionBox = "region.box";
inline constexpr std::string_view RegionUnknown = "region.unknown";

// =========================================================================
// Position format
// =========================================================================
inline constexpr std::string_view PositionFmt = "position.fmt";

// =========================================================================
// Explanation templates (one per technique variant)
// =========================================================================
inline constexpr std::string_view ExplainNakedSingle = "explain.naked_single";
inline constexpr std::string_view ExplainHiddenSingle = "explain.hidden_single";
inline constexpr std::string_view ExplainNakedPair = "explain.naked_pair";
inline constexpr std::string_view ExplainNakedTriple = "explain.naked_triple";
inline constexpr std::string_view ExplainHiddenPair = "explain.hidden_pair";
inline constexpr std::string_view ExplainHiddenTriple = "explain.hidden_triple";
inline constexpr std::string_view ExplainPointingPair = "explain.pointing_pair";
inline constexpr std::string_view ExplainBoxLineReduction = "explain.box_line_reduction";
inline constexpr std::string_view ExplainNakedQuad = "explain.naked_quad";
inline constexpr std::string_view ExplainHiddenQuad = "explain.hidden_quad";
inline constexpr std::string_view ExplainXWingRow = "explain.x_wing_row";
inline constexpr std::string_view ExplainXWingCol = "explain.x_wing_col";
inline constexpr std::string_view ExplainXYWing = "explain.xy_wing";
inline constexpr std::string_view ExplainSwordfishRow = "explain.swordfish_row";
inline constexpr std::string_view ExplainSwordfishCol = "explain.swordfish_col";
inline constexpr std::string_view ExplainSkyscraper = "explain.skyscraper";
inline constexpr std::string_view ExplainTwoStringKite = "explain.two_string_kite";
inline constexpr std::string_view ExplainXYZWing = "explain.xyz_wing";
inline constexpr std::string_view ExplainUniqueRectangle = "explain.unique_rectangle";
inline constexpr std::string_view ExplainWWing = "explain.w_wing";
inline constexpr std::string_view ExplainSimpleColoringContradiction = "explain.simple_coloring_contradiction";
inline constexpr std::string_view ExplainSimpleColoringExclusion = "explain.simple_coloring_exclusion";
inline constexpr std::string_view ExplainUniqueRectangleType2 = "explain.unique_rectangle_type2";
inline constexpr std::string_view ExplainUniqueRectangleType3 = "explain.unique_rectangle_type3";
inline constexpr std::string_view ExplainUniqueRectangleType4 = "explain.unique_rectangle_type4";
inline constexpr std::string_view ExplainFinnedXWingRow = "explain.finned_x_wing_row";
inline constexpr std::string_view ExplainFinnedXWingCol = "explain.finned_x_wing_col";
inline constexpr std::string_view ExplainRemotePairs = "explain.remote_pairs";
inline constexpr std::string_view ExplainBUG = "explain.bug";
inline constexpr std::string_view ExplainJellyfishRow = "explain.jellyfish_row";
inline constexpr std::string_view ExplainJellyfishCol = "explain.jellyfish_col";
inline constexpr std::string_view ExplainFinnedSwordfishRow = "explain.finned_swordfish_row";
inline constexpr std::string_view ExplainFinnedSwordfishCol = "explain.finned_swordfish_col";
inline constexpr std::string_view ExplainEmptyRectangle = "explain.empty_rectangle";
inline constexpr std::string_view ExplainWXYZWing = "explain.wxyz_wing";
inline constexpr std::string_view ExplainFinnedJellyfishRow = "explain.finned_jellyfish_row";
inline constexpr std::string_view ExplainFinnedJellyfishCol = "explain.finned_jellyfish_col";
inline constexpr std::string_view ExplainXYChain = "explain.xy_chain";
inline constexpr std::string_view ExplainMultiColoringWrap = "explain.multi_coloring_wrap";
inline constexpr std::string_view ExplainMultiColoringTrap = "explain.multi_coloring_trap";
inline constexpr std::string_view ExplainALSxZ = "explain.als_xz";
inline constexpr std::string_view ExplainSueDeCoq = "explain.sue_de_coq";
inline constexpr std::string_view ExplainForcingChain = "explain.forcing_chain";
inline constexpr std::string_view ExplainNiceLoop = "explain.nice_loop";
inline constexpr std::string_view ExplainXCyclesType1 = "explain.x_cycles_type1";
inline constexpr std::string_view ExplainXCyclesType2 = "explain.x_cycles_type2";
inline constexpr std::string_view ExplainXCyclesType3 = "explain.x_cycles_type3";
inline constexpr std::string_view ExplainThreeDMedusa = "explain.three_d_medusa";
inline constexpr std::string_view ExplainHiddenUniqueRectangle = "explain.hidden_unique_rectangle";
inline constexpr std::string_view ExplainAvoidableRectangle = "explain.avoidable_rectangle";
inline constexpr std::string_view ExplainALSXYWing = "explain.als_xy_wing";
inline constexpr std::string_view ExplainDeathBlossom = "explain.death_blossom";
inline constexpr std::string_view ExplainVWXYZWing = "explain.vwxyz_wing";
inline constexpr std::string_view ExplainFrankenFish = "explain.franken_fish";
inline constexpr std::string_view ExplainGroupedXCycles = "explain.grouped_x_cycles";

}  // namespace sudoku::core::StringKeys
