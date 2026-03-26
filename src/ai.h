#pragma once
#include "common.h"
#include "cards.h"

/*
 * AI hidden-info rule for ai*.{c,h}:
 * - Never read the opponent hand directly via g.own[opp].
 * - Never read unopened draw-pile contents via g.floor.
 * - Visible cards may be read directly: g.own[player], g.deck, g.invent.
 * - Estimation is allowed only by inferring unseen cards from visible cards.
 *
 * CardSet仕様:
 * - g.floor ... 山札: (0 ~ start -1: open済み ※相手手札のみ invisible, start ~ num: まだめくられていない山札 ※invisible)
 * - g.own[{0|1}] ... 手札 ※自分の手札 = visible, 相手の手札 = invisible
 * - g.deck ... 場札 (数: 不定<最大48>, 途中に null が含まれることがある) ※visible cards
 * - g.invent[{0|1}][{0|1|2|3}] ... 取り札 (KASU, TAN, TANE, GOKOU 毎) ※visible cards
 */

enum {
    AI_THINK_STRATEGY_SKIP_REACH = 1 << 0,
    AI_THINK_STRATEGY_SKIP_RISK = 1 << 1,
    AI_THINK_STRATEGY_SKIP_PRIORITY = 1 << 2,
    AI_THINK_STRATEGY_SKIP_BIAS = 1 << 3,
};

#ifndef PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
#define PHASE2A_PLAN_DOMAIN_SCALING_ENABLE 1
#endif
#ifndef PLAN_PRESS_MULT
#define PLAN_PRESS_MULT 120
#endif
#ifndef PLAN_SURVIVE_MULT
#define PLAN_SURVIVE_MULT 85
#endif
#ifndef PLAN_SURVIVE_DEFENCE_MULT
#define PLAN_SURVIVE_DEFENCE_MULT 110
#endif
#ifndef DOMAIN_LIGHT_MULT
#define DOMAIN_LIGHT_MULT 115
#endif
#ifndef DOMAIN_SAKE_MULT
#define DOMAIN_SAKE_MULT 115
#endif
#ifndef DOMAIN_AKATAN_MULT
#define DOMAIN_AKATAN_MULT 112
#endif
#ifndef DOMAIN_AOTAN_MULT
#define DOMAIN_AOTAN_MULT 112
#endif
#ifndef DOMAIN_ISC_MULT
#define DOMAIN_ISC_MULT 110
#endif
#ifndef DOMAIN_NONE_MULT
#define DOMAIN_NONE_MULT 100
#endif
#ifndef SURVIVE_OFFENCE_FLOOR_MULT
#define SURVIVE_OFFENCE_FLOOR_MULT 70
#endif
#ifndef DROP_DOMAIN_HOT_BONUS
#define DROP_DOMAIN_HOT_BONUS 12000
#endif
#ifndef WATCH_TRACE_ENABLE
#define WATCH_TRACE_ENABLE 1
#endif
#ifndef WATCH_TRACE_DROP_TOP_K
#define WATCH_TRACE_DROP_TOP_K 5
#endif
#ifndef WATCH_TRACE_SELECT_TOP_K
#define WATCH_TRACE_SELECT_TOP_K 3
#endif
#ifndef ENDGAME_ENABLE
#define ENDGAME_ENABLE 1
#endif
#ifndef ENDGAME_WEIGHT_100
#define ENDGAME_WEIGHT_100 100
#endif
#ifndef ENDGAME_CLINCH_BONUS
#define ENDGAME_CLINCH_BONUS 120
#endif
#ifndef ENDGAME_EXCESS_PENALTY_UNIT
#define ENDGAME_EXCESS_PENALTY_UNIT 12
#endif
#ifndef ENDGAME_SHORTFALL_PENALTY_UNIT
#define ENDGAME_SHORTFALL_PENALTY_UNIT 8
#endif

typedef struct {
    int action_mc_trigger_count[2];
    int action_mc_penalty_applied_count[2];
    int action_mc_changed_choice_count[2];
    int action_mc_applied_best_count[2];
    int action_mc_total_candidates[2];
    int action_mc_qdiff_total[2];
    int action_mc_qdiff_count[2];
    int action_mc_qdiff_min[2];
    int action_mc_qdiff_max[2];
    int action_mc_qdiff_lt0_count[2];
    int action_mc_qdiff_le_500_count[2];
    int action_mc_qdiff_le_1000_count[2];
    int action_mc_qdiff_le_2000_count[2];
    int action_mc_qdiff_le_4000_count[2];
    int action_mc_best_gap_count[2];
    int action_mc_best_min_gap_before[2];
    int action_mc_best_min_gap_after[2];
    int action_mc_trigger_by_material[2][7];
    int action_mc_changed_by_material[2][7];
    int month_lock_trigger_count[2];
    int month_lock_changed_choice_count[2];
    int month_lock_hidden_key_trigger_count[2];
    int month_lock_hidden_key_changed_choice_count[2];
    int month_lock_hidden_deny_trigger_count[2];
    int month_lock_hidden_deny_changed_choice_count[2];
    int overpay_delay_penalty_applied_count[2];
    int overpay_delay_changed_choice_count[2];
    int certain_seven_plus_trigger_count[2];
    int certain_seven_plus_changed_choice_count[2];
    int sevenline_trigger_count[2];
    int sevenline_changed_count[2];
    int sevenline_margin_total[2];
    int sevenline_margin_count[2];
    int sevenline_margin_min[2];
    int sevenline_margin_max[2];
    int sevenline_margin_hist[2][3];
    int sevenline_margin1_reach_hist[2][3];
    int sevenline_delay_hist[2][4];
    int sevenline_changed_delay_hist[2][4];
    int sevenline_changed_margin_hist[2][3];
    int sevenline_bonus_total[2];
    int sevenline_bonus_count[2];
    int sevenline_bonus_min[2];
    int sevenline_bonus_max[2];
    int sevenline_low_reach_count[2];
    int sevenline_press_applied_count[2];
    int sevenline_d1_loss_reason_count[2][4];
    int sevenline_d1_override_trigger_count[2];
    int sevenline_d1_override_applied_count[2];
    int sevenline_d1_override_changed_count[2];
    int sevenline_d1_override_blocked_count[2][2];
    int sevenline_d1_gap_total_before[2];
    int sevenline_d1_gap_min_before[2];
    int sevenline_d1_gap_max_before[2];
    int sevenline_d1_gap_total_after[2];
    int sevenline_d1_gap_min_after[2];
    int sevenline_d1_gap_max_after[2];
    int sevenline_d1_gap_count[2];
    int mode_greedy_count[2];
    int mode_balanced_count[2];
    int mode_defensive_count[2];
    int combo7_trigger_count[2];
    int combo7_changed_choice_count[2];
    int combo7_greedy_trigger_count[2];
    int combo7_greedy_changed_choice_count[2];
    int combo7_balanced_trigger_count[2];
    int combo7_balanced_changed_choice_count[2];
    int combo7_defensive_trigger_count[2];
    int combo7_defensive_changed_choice_count[2];
    int combo7_margin_total[2];
    int combo7_margin_count[2];
    int combo7_margin_flip_count[2];
    int combo7_margin_lt_5k_count[2];
    int combo7_margin_lt_10k_count[2];
    int combo7_margin_lt_20k_count[2];
    int best_plus1_used_rank_count[2][2];
    int koikoi_base6_push_trigger_count[2];
    int koikoi_base6_push_applied_count[2];
    int koikoi_base6_push_flipped_count[2];
    int koikoi_locked_threshold_trigger_count[2];
    int koikoi_locked_threshold_applied_count[2];
    int keytarget_expose_trigger_count[2];
    int keytarget_expose_changed_choice_count[2];
    int env_mc_trigger_count[2];
    int env_mc_changed_choice_count[2];
    int env_mc_e_total[2];
    int env_mc_e_count[2];
    int env_mc_e_min[2];
    int env_mc_e_max[2];
    int env_mc_mode_trigger_count[2][3];
    int env_mc_mode_changed_choice_count[2][3];
    int plan_count[2][AI_PLAN_MAX];
    int plan_reason_count[2][5];
    int initial_sake_round_count[2];
    int endgame_trigger_count[2];
    int endgame_changed_choice_count[2];
    int endgame_k_hist[2][4];
    int endgame_changed_needed_hist[2][4];
    int phase2a_trigger_count[2];
    int phase2a_changed_choice_count[2];
    int phase2a_plan_count[2][AI_PLAN_MAX];
    int phase2a_domain_count[2][AI_ENV_DOMAIN_MAX];
    int phase2a_reason_trigger_count[2][5];
    int phase2a_reason_changed_count[2][5];
    int phase2a_reason_trigger_by_plan[2][AI_PLAN_MAX][5];
    int phase2a_reason_changed_by_plan[2][AI_PLAN_MAX][5];
} AiDebugMetrics;

enum {
    AI_ACTION_MC_MATERIAL_LIGHT = 0,
    AI_ACTION_MC_MATERIAL_SAKE = 1,
    AI_ACTION_MC_MATERIAL_AKATAN = 2,
    AI_ACTION_MC_MATERIAL_AOTAN = 3,
    AI_ACTION_MC_MATERIAL_ISC = 4,
    AI_ACTION_MC_MATERIAL_AME = 5,
    AI_ACTION_MC_MATERIAL_OTHER_HIGH = 6,
    AI_ACTION_MC_MATERIAL_MAX = 7,
};

enum {
    AI_SEVENLINE_D1_LOSS_DEFENCE = 0,
    AI_SEVENLINE_D1_LOSS_COMBO7 = 1,
    AI_SEVENLINE_D1_LOSS_DOMAIN_HOT = 2,
    AI_SEVENLINE_D1_LOSS_OTHER = 3,
    AI_SEVENLINE_D1_LOSS_MAX = 4,
};

enum {
    AI_SEVENLINE_D1_OVERRIDE_BLOCK_KOIKOI_OPP = 0,
    AI_SEVENLINE_D1_OVERRIDE_BLOCK_INVALID = 1,
    AI_SEVENLINE_D1_OVERRIDE_BLOCK_MAX = 2,
};

enum {
    AI_PLAN_REASON_DIFF = 0,
    AI_PLAN_REASON_COMBO7 = 1,
    AI_PLAN_REASON_SEVENLINE = 2,
    AI_PLAN_REASON_BLOCK_THREAT = 3,
    AI_PLAN_REASON_SURVIVE_DIFF = 4,
    AI_PLAN_REASON_MAX = 5,
};

enum {
    AI_PHASE2A_REASON_NONE = 0,
    AI_PHASE2A_REASON_7PLUS = 1,
    AI_PHASE2A_REASON_COMBO7 = 2,
    AI_PHASE2A_REASON_HOT = 3,
    AI_PHASE2A_REASON_DEFENCE = 4,
};

typedef struct {
    int value;
    int combo_count;
    int combo_score_sum;
    int combo_delay;
    int combo_reach;
    int plus1_rank;
    int wids[3];
} AiCombo7Eval;

typedef struct {
    int card_no;
    int taken_card_no;
    int cluster;
} AiActionMcDropCandidate;

void ai_think_start(void);
void ai_think_end(void);
void ai_vsync(void);
void ai_putlog(const char* fmt, ...);
void ai_putlog_env(const char* fmt, ...);
void ai_putlog_raw(const char* s);
void ai_watch_log_begin(void);
void ai_watch_log_end(void);
void ai_watch_min_set_action(int player, const char* fmt, ...);
void ai_watch_min_import_action(int player, const char* s);
const char* ai_watch_min_get_action(int player);
void ai_watch_min_clear_action(int player);
void ai_watch_min_emit_action(int player);
void ai_watch_min_set_koikoi(int player, const char* fmt, ...);
void ai_watch_min_emit_koikoi(int player);
int ai_env_category_from_card(int card_index);
int ai_env_effective_category_for_player(int player, int card_index);
int ai_env_score(int player);
int is_koikoi_enabled(int player);
void ai_think_strategy(int player, StrategyData* data);
void ai_think_strategy_mode(int player, StrategyData* data, int mode, const StrategyData* cache);
int ai_koikoi(int player, int round_score);
int ai_drop(int player);
int ai_select(int player, Card* card);
int ai_normal_koikoi(int player, int round_score);
int ai_normal_drop(int player);
int ai_normal_select(int player, Card* card);
int ai_hard_koikoi(int player, int round_score);
int ai_hard_drop(int player);
int ai_hard_select(int player, Card* card);
int ai_hard_rapacious_fallback_koikoi(int player, int round_score);
int ai_hard_rapacious_fallback_drop(int player);
int ai_hard_rapacious_fallback_select(int player, Card* card);
int ai_stenv_koikoi(int player, int round_score);
int ai_stenv_drop(int player);
int ai_stenv_select(int player, Card* card);
int ai_mcenv_koikoi(int player, int round_score);
int ai_mcenv_drop(int player);
int ai_mcenv_select(int player, Card* card);
void ai_debug_metrics_reset(void);
void ai_debug_reset_round_state(void);
const AiDebugMetrics* ai_debug_metrics_get(void);
void ai_debug_note_month_lock(int player, int changed_choice);
void ai_debug_note_month_lock_hidden_key(int player, int changed_choice);
void ai_debug_note_month_lock_hidden_deny(int player, int changed_choice);
void ai_debug_note_overpay_delay(int player, int changed_choice);
void ai_debug_note_certain_seven_plus(int player, int changed_choice);
void ai_debug_note_sevenline_trigger(int player, int margin, int reach, int delay, int bonus, int low_reach, int press_applied);
void ai_debug_note_sevenline_changed(int player, int margin, int delay);
void ai_debug_note_sevenline_d1_loss_reason(int player, int reason);
void ai_debug_note_sevenline_d1_override(int player, int applied, int changed);
void ai_debug_note_sevenline_d1_override_blocked(int player, int reason);
void ai_debug_note_sevenline_d1_gap(int player, int gap_before, int gap_after);
void ai_debug_note_strategy_mode(int player, int mode);
int ai_debug_get_round_last_strategy_mode(int player, int* has_mode);
void ai_debug_note_combo7(int player, int mode, int changed_choice);
void ai_debug_note_combo7_margin(int player, int margin, int flipped);
void ai_debug_note_best_plus1_used_rank(int player, int rank);
void ai_debug_note_koikoi_base6_push(int player, int applied, int flipped);
void ai_debug_note_koikoi_locked_threshold(int player, int applied);
void ai_debug_note_keytarget_expose(int player, int changed_choice);
void ai_debug_note_env_mc_trigger(int player, int mode);
void ai_debug_note_env_mc_changed_choice(int player, int mode);
void ai_debug_note_env_mc_E(int player, int env_e);
void ai_debug_note_plan_decision(int player, AiPlan plan, int reason);
void ai_debug_set_initial_sake(int player, int has_sake);
int ai_debug_has_initial_sake(int player);
void ai_debug_note_endgame(int player, int k_end, int changed_choice, int needed);
void ai_debug_note_phase2a_trigger(int player, AiPlan plan, AiEnvDomain domain, int reason);
void ai_debug_note_phase2a_changed_choice(int player, AiPlan plan, int reason);
void ai_debug_set_run_context(int seed, int game_index);
void ai_debug_note_action_mc_trigger(int player, int candidate_count);
void ai_debug_note_action_mc_material_trigger(int player, int material);
void ai_debug_note_action_mc_qdiff(int player, int qdiff);
void ai_debug_note_action_mc_penalty_applied(int player, int material);
void ai_debug_note_action_mc_changed_choice(int player, int material);
void ai_debug_note_action_mc_applied_best(int player);
void ai_debug_note_action_mc_best_gap(int player, int gap_before, int gap_after);
int ai_debug_current_seed(void);
int ai_debug_current_game_index(void);
void ai_watch_prepare_turn(int player);
int ai_watch_current_turn(int player);
void ai_set_last_think_context(int player, const StrategyData* s, int needed, int k_end, int best_immediate_gain, int second_immediate_gain,
                               int best_clinch_score, int second_clinch_score);
void ai_set_last_think_extra(int player, const char* fmt, ...);
const char* ai_get_last_think_extra(int player);
void ai_watch_begin_after_opp_window(int player);
void ai_watch_note_after_opp_none(int opponent);
void ai_watch_note_after_opp_koikoi(int player, int round_score);
void ai_watch_note_after_opp_win(int player, int round_score);
void ai_eval_combo7(int player, int round_score, const StrategyData* data, AiCombo7Eval* out_eval);
int ai_strategy_primary_cluster(const StrategyData* data);
void ai_eval_drop_action_mc(int player, const AiActionMcDropCandidate* candidates, int count, int sample_count, int* out_score_diff);
int ai_eval_drop_env_mc(int player, const AiActionMcDropCandidate* candidates, int count, int sample_count, int* out_E_avg);
int ai_is_card_critical_for_wid(int card_no, int wid);
int ai_is_card_related_for_wid(int card_no, int wid);
int ai_is_fixed_wid(int wid);
int ai_is_high_value_wid(int wid);
int ai_is_overpay_target_wid(int wid);
int ai_is_keycard_for_role(int card_no, int role);
int ai_role_progress_level(int player, int role);
int ai_has_obvious_alt_weak_card_in_hand(int player, int except_card_no);
int ai_keycard_drop_gives_opp_advantage(int player, int keycard_no);
int ai_wid_missing_count(int player, int wid);
int ai_would_complete_wid_by_taking_card(int player, int wid, int taken_card_no);
int ai_overpay_possible_now(int player, int wid);
int ai_count_unrevealed_same_month(int player, int month);
