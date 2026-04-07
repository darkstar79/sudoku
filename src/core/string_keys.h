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
// Application
// =========================================================================
inline constexpr std::string_view AppTitle = "app.title";

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
inline constexpr std::string_view MenuGetCoachingHint = "menu.get_coaching_hint";
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
inline constexpr std::string_view ButtonClearNotes = "button.clear_notes";
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
inline constexpr std::string_view TrainingErrorBacktracking = "training.error_backtracking";
inline constexpr std::string_view TrainingErrorNoStep = "training.error_no_step";
inline constexpr std::string_view TrainingCorrectContinue = "training.correct_continue";
inline constexpr std::string_view TrainingFeedbackCorrect = "training.feedback_correct";
inline constexpr std::string_view TrainingFeedbackPartial = "training.feedback_partial";
inline constexpr std::string_view TrainingFeedbackIncorrect = "training.feedback_incorrect";
inline constexpr std::string_view TrainingFeedbackUnknown = "training.feedback_unknown";
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
inline constexpr std::string_view DialogNewGameConfirm = "dialog.new_game_confirm";

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
inline constexpr std::string_view SavePreviewTitle = "save.preview_title";
inline constexpr std::string_view SavePreviewDifficulty = "save.preview_difficulty";
inline constexpr std::string_view SavePreviewTime = "save.preview_time";
inline constexpr std::string_view SavePreviewMoves = "save.preview_moves";
inline constexpr std::string_view SavePreviewMistakes = "save.preview_mistakes";
inline constexpr std::string_view SaveNamePlaceholder = "save.name_placeholder";
inline constexpr std::string_view SaveNameEmpty = "save.name_empty";
inline constexpr std::string_view SaveOverwriteConfirm = "save.overwrite_confirm";

// =========================================================================
// Dialogs — Load
// =========================================================================
inline constexpr std::string_view DialogLoadGame = "dialog.load_game";
inline constexpr std::string_view DialogRecentSaves = "dialog.recent_saves";
inline constexpr std::string_view LoadColName = "load.col_name";
inline constexpr std::string_view LoadColDifficulty = "load.col_difficulty";
inline constexpr std::string_view LoadColDate = "load.col_date";
inline constexpr std::string_view LoadColTime = "load.col_time";
inline constexpr std::string_view LoadColRating = "load.col_rating";

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
inline constexpr std::string_view StatsTimeNa = "stats.time_na";

// Statistics dialog — tabs
inline constexpr std::string_view StatsTabOverview = "stats.tab_overview";
inline constexpr std::string_view StatsTabPerDifficulty = "stats.tab_per_difficulty";
inline constexpr std::string_view StatsTabRecentGames = "stats.tab_recent_games";

// Statistics dialog — totals
inline constexpr std::string_view StatsTotalMoves = "stats.total_moves";
inline constexpr std::string_view StatsTotalHints = "stats.total_hints";
inline constexpr std::string_view StatsTotalMistakes = "stats.total_mistakes";
inline constexpr std::string_view StatsTotalTime = "stats.total_time";

// Statistics dialog — per-difficulty table rows
inline constexpr std::string_view StatsRowPlayed = "stats.row_played";
inline constexpr std::string_view StatsRowCompleted = "stats.row_completed";
inline constexpr std::string_view StatsRowBestTime = "stats.row_best_time";
inline constexpr std::string_view StatsRowAvgTime = "stats.row_avg_time";
inline constexpr std::string_view StatsRowAvgRating = "stats.row_avg_rating";

// Statistics dialog — recent games columns
inline constexpr std::string_view StatsColDate = "stats.col_date";
inline constexpr std::string_view StatsColDifficulty = "stats.col_difficulty";
inline constexpr std::string_view StatsColTime = "stats.col_time";
inline constexpr std::string_view StatsColRating = "stats.col_rating";
inline constexpr std::string_view StatsColMoves = "stats.col_moves";
inline constexpr std::string_view StatsColMistakes = "stats.col_mistakes";

// =========================================================================
// Dialogs — About
// =========================================================================
inline constexpr std::string_view DialogAbout = "dialog.about";
inline constexpr std::string_view AboutSudokuGame = "about.sudoku_game";
inline constexpr std::string_view AboutBuiltWith = "about.built_with";
inline constexpr std::string_view AboutDescription = "about.description";
inline constexpr std::string_view RatingFormat = "rating.format";

// =========================================================================
// Dialogs — Settings
// =========================================================================
inline constexpr std::string_view DialogSettings = "dialog.settings";
inline constexpr std::string_view DialogThirdPartyLicenses = "dialog.third_party_licenses";
inline constexpr std::string_view SettingsTabGameplay = "settings.tab_gameplay";
inline constexpr std::string_view SettingsTabDisplay = "settings.tab_display";
inline constexpr std::string_view SettingsMaxHints = "settings.max_hints";
inline constexpr std::string_view SettingsAutoSaveInterval = "settings.auto_save_interval";
inline constexpr std::string_view SettingsDefaultDifficulty = "settings.default_difficulty";
inline constexpr std::string_view SettingsSecondsSuffix = "settings.seconds_suffix";
inline constexpr std::string_view SettingsHighlightConflicts = "settings.highlight_conflicts";
inline constexpr std::string_view SettingsShowHints = "settings.show_hints";
inline constexpr std::string_view SettingsCollectDetailedStats = "settings.collect_detailed_stats";
inline constexpr std::string_view SettingsEncryptDetailedStats = "settings.encrypt_detailed_stats";
inline constexpr std::string_view SettingsEncryptDetailedStatsTooltip = "settings.encrypt_detailed_stats_tooltip";
inline constexpr std::string_view SettingsTabStatistics = "settings.tab_statistics";
inline constexpr std::string_view StatsDeletePrompt = "stats.delete_prompt";
inline constexpr std::string_view StatsSessionsDeleted = "stats.sessions_deleted";

// =========================================================================
// Tooltips — Rating scale
// =========================================================================
inline constexpr std::string_view TooltipRatingScale = "tooltip.rating_scale";
inline constexpr std::string_view TooltipTechniquesRequired = "tooltip.techniques_required";
inline constexpr std::string_view TooltipInputMode = "tooltip.input_mode";
inline constexpr std::string_view TooltipPlaceDigit = "tooltip.place_digit";
inline constexpr std::string_view TooltipEliminateDigit = "tooltip.eliminate_digit";

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
inline constexpr std::string_view ToastNoStrategies = "toast.no_strategies";

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
// ViewModel — Coaching hint messages
// =========================================================================
inline constexpr std::string_view CoachingNoRemaining = "coaching.no_remaining";
inline constexpr std::string_view CoachingNoTechnique = "coaching.no_technique";
inline constexpr std::string_view CoachingMaxLevel = "coaching.max_level";
inline constexpr std::string_view CoachingLevelHeader = "coaching.level_header";
inline constexpr std::string_view CoachingTryIt = "coaching.try_it";
inline constexpr std::string_view CoachingCheckCorrect = "coaching.check_correct";
inline constexpr std::string_view CoachingCheckPartial = "coaching.check_partial";
inline constexpr std::string_view CoachingCheckWrong = "coaching.check_wrong";
inline constexpr std::string_view CoachingApplied = "coaching.applied";
inline constexpr std::string_view CoachingButtonCheck = "coaching.button_check";
inline constexpr std::string_view CoachingButtonApply = "coaching.button_apply";
inline constexpr std::string_view CoachingTryItLabel = "coaching.try_it_label";
inline constexpr std::string_view CoachingWhatToLookFor = "coaching.what_to_look_for";
inline constexpr std::string_view CoachingCheckZero = "coaching.check_zero";

// =========================================================================
// Training hints — progressive hint text per category/level
// =========================================================================
inline constexpr std::string_view HintSinglesL1 = "hint.singles.l1";
inline constexpr std::string_view HintSinglesL2Region = "hint.singles.l2_region";
inline constexpr std::string_view HintSinglesL2NoRegion = "hint.singles.l2_no_region";
inline constexpr std::string_view HintSinglesL3 = "hint.singles.l3";
inline constexpr std::string_view HintSubsetsL1Region = "hint.subsets.l1_region";
inline constexpr std::string_view HintSubsetsL1NoRegion = "hint.subsets.l1_no_region";
inline constexpr std::string_view HintSubsetsL2Values = "hint.subsets.l2_values";
inline constexpr std::string_view HintSubsetsL2NoValues = "hint.subsets.l2_no_values";
inline constexpr std::string_view HintSubsetsL3 = "hint.subsets.l3";
inline constexpr std::string_view HintIntersectionsL1Value = "hint.intersections.l1_value";
inline constexpr std::string_view HintIntersectionsL1NoValue = "hint.intersections.l1_no_value";
inline constexpr std::string_view HintIntersectionsL2 = "hint.intersections.l2";
inline constexpr std::string_view HintIntersectionsL3 = "hint.intersections.l3";
inline constexpr std::string_view HintFishL1Value = "hint.fish.l1_value";
inline constexpr std::string_view HintFishL1NoValue = "hint.fish.l1_no_value";
inline constexpr std::string_view HintFishL2 = "hint.fish.l2";
inline constexpr std::string_view HintFishL3 = "hint.fish.l3";
inline constexpr std::string_view HintWingsL1 = "hint.wings.l1";
inline constexpr std::string_view HintWingsL2 = "hint.wings.l2";
inline constexpr std::string_view HintWingsL3 = "hint.wings.l3";
inline constexpr std::string_view HintSingleDigitL1Value = "hint.single_digit.l1_value";
inline constexpr std::string_view HintSingleDigitL1NoValue = "hint.single_digit.l1_no_value";
inline constexpr std::string_view HintSingleDigitL2 = "hint.single_digit.l2";
inline constexpr std::string_view HintSingleDigitL3 = "hint.single_digit.l3";
inline constexpr std::string_view HintColoringL1Value = "hint.coloring.l1_value";
inline constexpr std::string_view HintColoringL1NoValue = "hint.coloring.l1_no_value";
inline constexpr std::string_view HintColoringL2 = "hint.coloring.l2";
inline constexpr std::string_view HintColoringL3 = "hint.coloring.l3";
inline constexpr std::string_view HintUniqueRectL1 = "hint.unique_rect.l1";
inline constexpr std::string_view HintUniqueRectL2 = "hint.unique_rect.l2";
inline constexpr std::string_view HintUniqueRectL3 = "hint.unique_rect.l3";
inline constexpr std::string_view HintChainsL1Pos = "hint.chains.l1_pos";
inline constexpr std::string_view HintChainsL1NoPos = "hint.chains.l1_no_pos";
inline constexpr std::string_view HintChainsL2 = "hint.chains.l2";
inline constexpr std::string_view HintChainsL3Placement = "hint.chains.l3_placement";
inline constexpr std::string_view HintChainsL3Elimination = "hint.chains.l3_elimination";
inline constexpr std::string_view HintSetLogicL1 = "hint.set_logic.l1";
inline constexpr std::string_view HintSetLogicL2 = "hint.set_logic.l2";
inline constexpr std::string_view HintSetLogicL3 = "hint.set_logic.l3";
inline constexpr std::string_view HintSpecialL1 = "hint.special.l1";
inline constexpr std::string_view HintSpecialL2 = "hint.special.l2";

// =========================================================================
// Technique descriptions — what_it_is and what_to_look_for per technique
// =========================================================================
inline constexpr std::string_view TechDescNakedSingleWhatItIs = "tech.desc.naked_single.what_it_is";
inline constexpr std::string_view TechDescNakedSingleWhatToLookFor = "tech.desc.naked_single.what_to_look_for";
inline constexpr std::string_view TechDescHiddenSingleWhatItIs = "tech.desc.hidden_single.what_it_is";
inline constexpr std::string_view TechDescHiddenSingleWhatToLookFor = "tech.desc.hidden_single.what_to_look_for";
inline constexpr std::string_view TechDescNakedPairWhatItIs = "tech.desc.naked_pair.what_it_is";
inline constexpr std::string_view TechDescNakedPairWhatToLookFor = "tech.desc.naked_pair.what_to_look_for";
inline constexpr std::string_view TechDescNakedTripleWhatItIs = "tech.desc.naked_triple.what_it_is";
inline constexpr std::string_view TechDescNakedTripleWhatToLookFor = "tech.desc.naked_triple.what_to_look_for";
inline constexpr std::string_view TechDescHiddenPairWhatItIs = "tech.desc.hidden_pair.what_it_is";
inline constexpr std::string_view TechDescHiddenPairWhatToLookFor = "tech.desc.hidden_pair.what_to_look_for";
inline constexpr std::string_view TechDescHiddenTripleWhatItIs = "tech.desc.hidden_triple.what_it_is";
inline constexpr std::string_view TechDescHiddenTripleWhatToLookFor = "tech.desc.hidden_triple.what_to_look_for";
inline constexpr std::string_view TechDescPointingPairWhatItIs = "tech.desc.pointing_pair.what_it_is";
inline constexpr std::string_view TechDescPointingPairWhatToLookFor = "tech.desc.pointing_pair.what_to_look_for";
inline constexpr std::string_view TechDescBoxLineReductionWhatItIs = "tech.desc.box_line_reduction.what_it_is";
inline constexpr std::string_view TechDescBoxLineReductionWhatToLookFor =
    "tech.desc.box_line_reduction.what_to_look_for";
inline constexpr std::string_view TechDescNakedQuadWhatItIs = "tech.desc.naked_quad.what_it_is";
inline constexpr std::string_view TechDescNakedQuadWhatToLookFor = "tech.desc.naked_quad.what_to_look_for";
inline constexpr std::string_view TechDescHiddenQuadWhatItIs = "tech.desc.hidden_quad.what_it_is";
inline constexpr std::string_view TechDescHiddenQuadWhatToLookFor = "tech.desc.hidden_quad.what_to_look_for";
inline constexpr std::string_view TechDescXWingWhatItIs = "tech.desc.x_wing.what_it_is";
inline constexpr std::string_view TechDescXWingWhatToLookFor = "tech.desc.x_wing.what_to_look_for";
inline constexpr std::string_view TechDescXYWingWhatItIs = "tech.desc.xy_wing.what_it_is";
inline constexpr std::string_view TechDescXYWingWhatToLookFor = "tech.desc.xy_wing.what_to_look_for";
inline constexpr std::string_view TechDescSwordfishWhatItIs = "tech.desc.swordfish.what_it_is";
inline constexpr std::string_view TechDescSwordfishWhatToLookFor = "tech.desc.swordfish.what_to_look_for";
inline constexpr std::string_view TechDescSkyscraperWhatItIs = "tech.desc.skyscraper.what_it_is";
inline constexpr std::string_view TechDescSkyscraperWhatToLookFor = "tech.desc.skyscraper.what_to_look_for";
inline constexpr std::string_view TechDescTwoStringKiteWhatItIs = "tech.desc.two_string_kite.what_it_is";
inline constexpr std::string_view TechDescTwoStringKiteWhatToLookFor = "tech.desc.two_string_kite.what_to_look_for";
inline constexpr std::string_view TechDescXYZWingWhatItIs = "tech.desc.xyz_wing.what_it_is";
inline constexpr std::string_view TechDescXYZWingWhatToLookFor = "tech.desc.xyz_wing.what_to_look_for";
inline constexpr std::string_view TechDescUniqueRectangleWhatItIs = "tech.desc.unique_rectangle.what_it_is";
inline constexpr std::string_view TechDescUniqueRectangleWhatToLookFor = "tech.desc.unique_rectangle.what_to_look_for";
inline constexpr std::string_view TechDescWWingWhatItIs = "tech.desc.w_wing.what_it_is";
inline constexpr std::string_view TechDescWWingWhatToLookFor = "tech.desc.w_wing.what_to_look_for";
inline constexpr std::string_view TechDescSimpleColoringWhatItIs = "tech.desc.simple_coloring.what_it_is";
inline constexpr std::string_view TechDescSimpleColoringWhatToLookFor = "tech.desc.simple_coloring.what_to_look_for";
inline constexpr std::string_view TechDescFinnedXWingWhatItIs = "tech.desc.finned_x_wing.what_it_is";
inline constexpr std::string_view TechDescFinnedXWingWhatToLookFor = "tech.desc.finned_x_wing.what_to_look_for";
inline constexpr std::string_view TechDescRemotePairsWhatItIs = "tech.desc.remote_pairs.what_it_is";
inline constexpr std::string_view TechDescRemotePairsWhatToLookFor = "tech.desc.remote_pairs.what_to_look_for";
inline constexpr std::string_view TechDescBUGWhatItIs = "tech.desc.bug.what_it_is";
inline constexpr std::string_view TechDescBUGWhatToLookFor = "tech.desc.bug.what_to_look_for";
inline constexpr std::string_view TechDescJellyfishWhatItIs = "tech.desc.jellyfish.what_it_is";
inline constexpr std::string_view TechDescJellyfishWhatToLookFor = "tech.desc.jellyfish.what_to_look_for";
inline constexpr std::string_view TechDescFinnedSwordfishWhatItIs = "tech.desc.finned_swordfish.what_it_is";
inline constexpr std::string_view TechDescFinnedSwordfishWhatToLookFor = "tech.desc.finned_swordfish.what_to_look_for";
inline constexpr std::string_view TechDescEmptyRectangleWhatItIs = "tech.desc.empty_rectangle.what_it_is";
inline constexpr std::string_view TechDescEmptyRectangleWhatToLookFor = "tech.desc.empty_rectangle.what_to_look_for";
inline constexpr std::string_view TechDescWXYZWingWhatItIs = "tech.desc.wxyz_wing.what_it_is";
inline constexpr std::string_view TechDescWXYZWingWhatToLookFor = "tech.desc.wxyz_wing.what_to_look_for";
inline constexpr std::string_view TechDescFinnedJellyfishWhatItIs = "tech.desc.finned_jellyfish.what_it_is";
inline constexpr std::string_view TechDescFinnedJellyfishWhatToLookFor = "tech.desc.finned_jellyfish.what_to_look_for";
inline constexpr std::string_view TechDescXYChainWhatItIs = "tech.desc.xy_chain.what_it_is";
inline constexpr std::string_view TechDescXYChainWhatToLookFor = "tech.desc.xy_chain.what_to_look_for";
inline constexpr std::string_view TechDescMultiColoringWhatItIs = "tech.desc.multi_coloring.what_it_is";
inline constexpr std::string_view TechDescMultiColoringWhatToLookFor = "tech.desc.multi_coloring.what_to_look_for";
inline constexpr std::string_view TechDescALSxZWhatItIs = "tech.desc.als_xz.what_it_is";
inline constexpr std::string_view TechDescALSxZWhatToLookFor = "tech.desc.als_xz.what_to_look_for";
inline constexpr std::string_view TechDescSueDeCoqWhatItIs = "tech.desc.sue_de_coq.what_it_is";
inline constexpr std::string_view TechDescSueDeCoqWhatToLookFor = "tech.desc.sue_de_coq.what_to_look_for";
inline constexpr std::string_view TechDescForcingChainWhatItIs = "tech.desc.forcing_chain.what_it_is";
inline constexpr std::string_view TechDescForcingChainWhatToLookFor = "tech.desc.forcing_chain.what_to_look_for";
inline constexpr std::string_view TechDescNiceLoopWhatItIs = "tech.desc.nice_loop.what_it_is";
inline constexpr std::string_view TechDescNiceLoopWhatToLookFor = "tech.desc.nice_loop.what_to_look_for";
inline constexpr std::string_view TechDescXCyclesWhatItIs = "tech.desc.x_cycles.what_it_is";
inline constexpr std::string_view TechDescXCyclesWhatToLookFor = "tech.desc.x_cycles.what_to_look_for";
inline constexpr std::string_view TechDescThreeDMedusaWhatItIs = "tech.desc.three_d_medusa.what_it_is";
inline constexpr std::string_view TechDescThreeDMedusaWhatToLookFor = "tech.desc.three_d_medusa.what_to_look_for";
inline constexpr std::string_view TechDescHiddenUniqueRectangleWhatItIs =
    "tech.desc.hidden_unique_rectangle.what_it_is";
inline constexpr std::string_view TechDescHiddenUniqueRectangleWhatToLookFor =
    "tech.desc.hidden_unique_rectangle.what_to_look_for";
inline constexpr std::string_view TechDescAvoidableRectangleWhatItIs = "tech.desc.avoidable_rectangle.what_it_is";
inline constexpr std::string_view TechDescAvoidableRectangleWhatToLookFor =
    "tech.desc.avoidable_rectangle.what_to_look_for";
inline constexpr std::string_view TechDescALSXYWingWhatItIs = "tech.desc.als_xy_wing.what_it_is";
inline constexpr std::string_view TechDescALSXYWingWhatToLookFor = "tech.desc.als_xy_wing.what_to_look_for";
inline constexpr std::string_view TechDescDeathBlossomWhatItIs = "tech.desc.death_blossom.what_it_is";
inline constexpr std::string_view TechDescDeathBlossomWhatToLookFor = "tech.desc.death_blossom.what_to_look_for";
inline constexpr std::string_view TechDescVWXYZWingWhatItIs = "tech.desc.vwxyz_wing.what_it_is";
inline constexpr std::string_view TechDescVWXYZWingWhatToLookFor = "tech.desc.vwxyz_wing.what_to_look_for";
inline constexpr std::string_view TechDescFrankenFishWhatItIs = "tech.desc.franken_fish.what_it_is";
inline constexpr std::string_view TechDescFrankenFishWhatToLookFor = "tech.desc.franken_fish.what_to_look_for";
inline constexpr std::string_view TechDescGroupedXCyclesWhatItIs = "tech.desc.grouped_x_cycles.what_it_is";
inline constexpr std::string_view TechDescGroupedXCyclesWhatToLookFor = "tech.desc.grouped_x_cycles.what_to_look_for";
inline constexpr std::string_view TechDescSashimiXWingWhatItIs = "tech.desc.sashimi_x_wing.what_it_is";
inline constexpr std::string_view TechDescSashimiXWingWhatToLookFor = "tech.desc.sashimi_x_wing.what_to_look_for";
inline constexpr std::string_view TechDescSashimiSwordfishWhatItIs = "tech.desc.sashimi_swordfish.what_it_is";
inline constexpr std::string_view TechDescSashimiSwordfishWhatToLookFor =
    "tech.desc.sashimi_swordfish.what_to_look_for";
inline constexpr std::string_view TechDescSashimiJellyfishWhatItIs = "tech.desc.sashimi_jellyfish.what_it_is";
inline constexpr std::string_view TechDescSashimiJellyfishWhatToLookFor =
    "tech.desc.sashimi_jellyfish.what_to_look_for";
inline constexpr std::string_view TechDescUnitForcingChainWhatItIs = "tech.desc.unit_forcing_chain.what_it_is";
inline constexpr std::string_view TechDescUnitForcingChainWhatToLookFor =
    "tech.desc.unit_forcing_chain.what_to_look_for";
inline constexpr std::string_view TechDescRegionForcingChainWhatItIs = "tech.desc.region_forcing_chain.what_it_is";
inline constexpr std::string_view TechDescRegionForcingChainWhatToLookFor =
    "tech.desc.region_forcing_chain.what_to_look_for";
inline constexpr std::string_view TechDescMutantFishWhatItIs = "tech.desc.mutant_fish.what_it_is";
inline constexpr std::string_view TechDescMutantFishWhatToLookFor = "tech.desc.mutant_fish.what_to_look_for";
inline constexpr std::string_view TechDescKrakenFishWhatItIs = "tech.desc.kraken_fish.what_it_is";
inline constexpr std::string_view TechDescKrakenFishWhatToLookFor = "tech.desc.kraken_fish.what_to_look_for";
inline constexpr std::string_view TechDescALSChainWhatItIs = "tech.desc.als_chain.what_it_is";
inline constexpr std::string_view TechDescALSChainWhatToLookFor = "tech.desc.als_chain.what_to_look_for";
inline constexpr std::string_view TechDescUniqueLoopWhatItIs = "tech.desc.unique_loop.what_it_is";
inline constexpr std::string_view TechDescUniqueLoopWhatToLookFor = "tech.desc.unique_loop.what_to_look_for";
inline constexpr std::string_view TechDescJuniorExocetWhatItIs = "tech.desc.junior_exocet.what_it_is";
inline constexpr std::string_view TechDescJuniorExocetWhatToLookFor = "tech.desc.junior_exocet.what_to_look_for";
inline constexpr std::string_view TechDescContinuousNiceLoopWhatItIs = "tech.desc.continuous_nice_loop.what_it_is";
inline constexpr std::string_view TechDescContinuousNiceLoopWhatToLookFor =
    "tech.desc.continuous_nice_loop.what_to_look_for";
inline constexpr std::string_view TechDescGroupedNiceLoopWhatItIs = "tech.desc.grouped_nice_loop.what_it_is";
inline constexpr std::string_view TechDescGroupedNiceLoopWhatToLookFor = "tech.desc.grouped_nice_loop.what_to_look_for";
inline constexpr std::string_view TechDescBacktrackingWhatItIs = "tech.desc.backtracking.what_it_is";
inline constexpr std::string_view TechDescBacktrackingWhatToLookFor = "tech.desc.backtracking.what_to_look_for";
inline constexpr std::string_view TechDescUnknownWhatItIs = "tech.desc.unknown.what_it_is";
inline constexpr std::string_view TechDescUnknownWhatToLookFor = "tech.desc.unknown.what_to_look_for";

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
inline constexpr std::string_view TechSashimiXWing = "tech.sashimi_x_wing";
inline constexpr std::string_view TechSashimiSwordfish = "tech.sashimi_swordfish";
inline constexpr std::string_view TechSashimiJellyfish = "tech.sashimi_jellyfish";
inline constexpr std::string_view TechUnitForcingChain = "tech.unit_forcing_chain";
inline constexpr std::string_view TechRegionForcingChain = "tech.region_forcing_chain";
inline constexpr std::string_view TechMutantFish = "tech.mutant_fish";
inline constexpr std::string_view TechKrakenFish = "tech.kraken_fish";
inline constexpr std::string_view TechALSChain = "tech.als_chain";
inline constexpr std::string_view TechJuniorExocet = "tech.junior_exocet";
inline constexpr std::string_view TechUniqueLoop = "tech.unique_loop";
inline constexpr std::string_view TechContinuousNiceLoop = "tech.continuous_nice_loop";
inline constexpr std::string_view TechGroupedNiceLoop = "tech.grouped_nice_loop";
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
inline constexpr std::string_view ExplainUniqueRectangleType6 = "explain.unique_rectangle_type6";
inline constexpr std::string_view ExplainFinnedXWingRow = "explain.finned_x_wing_row";
inline constexpr std::string_view ExplainFinnedXWingCol = "explain.finned_x_wing_col";
inline constexpr std::string_view ExplainSashimiXWingRow = "explain.sashimi_x_wing_row";
inline constexpr std::string_view ExplainSashimiXWingCol = "explain.sashimi_x_wing_col";
inline constexpr std::string_view ExplainSashimiSwordfishRow = "explain.sashimi_swordfish_row";
inline constexpr std::string_view ExplainSashimiSwordfishCol = "explain.sashimi_swordfish_col";
inline constexpr std::string_view ExplainSashimiJellyfishRow = "explain.sashimi_jellyfish_row";
inline constexpr std::string_view ExplainSashimiJellyfishCol = "explain.sashimi_jellyfish_col";
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
inline constexpr std::string_view ExplainMutantFish = "explain.mutant_fish";
inline constexpr std::string_view ExplainGroupedXCycles = "explain.grouped_x_cycles";
inline constexpr std::string_view ExplainKrakenFish = "explain.kraken_fish";
inline constexpr std::string_view ExplainALSChain = "explain.als_chain";
inline constexpr std::string_view ExplainJuniorExocet = "explain.junior_exocet";
inline constexpr std::string_view ExplainUniqueLoop = "explain.unique_loop";
inline constexpr std::string_view ExplainContinuousNiceLoop = "explain.continuous_nice_loop";
inline constexpr std::string_view ExplainGroupedNiceLoop = "explain.grouped_nice_loop";

}  // namespace sudoku::core::StringKeys
