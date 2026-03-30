#include "ai.h"
// drop 思考ログを出して評価理由を追えるようにする。
#include <limits.h>

#define HARD_RAPACIOUS_FALLBACK_ENABLE 1
#define HARD_RAPACIOUS_FALLBACK_BEHIND_THRESHOLD 24

// 無効な delay 値として扱う番兵値。
#define RISK_DELAY_INVALID 99
// keep 評価がこの値以下なら致命的な抱え落ち候補とみなす。
#define KEEP_FATAL_THRESHOLD -350
// keep ペナルティの下限で、軽い事故でも過剰減点しないようにする。
#define KEEP_CLAMP_MIN -400
// keep ペナルティの絶対下限で、重大事故候補の減点をここで打ち止めにする。
#define KEEP_CLAMP_FATAL_MIN -800
// safety 評価がこの値以下なら危険札放出として除外寄りに扱う。
#define SAFETY_FATAL_THRESHOLD -250
// 安全に回収できる手へ与える基礎加点。
#define CAPTURE_DEFAULT_BONUS 900
// capture 局所評価を全体 value に混ぜる重み。
#define CAPTURE_TACTICAL_WEIGHT 3
// greedy fallback で Normal 評価にかける上位候補数。
#define GREEDY_FALLBACK_TOP_K 7
// greedy fallback が Hard 本体の大差優位を覆せる最大 value 差。
#define GREEDY_FALLBACK_MAX_VALUE_GAP 6000
// DEFENSIVE 時に deny 系価値を追加で重くする量。
#define DROP_MODE_DEFENCE_BONUS 20
// DEFENSIVE 時に安全性評価をさらに優先する量。
#define DROP_MODE_DEFENCE_SAFETY_BONUS 30
// GREEDY 時に攻撃評価を少し押し上げる量。
#define DROP_MODE_GREEDY_OFFENCE_BONUS 10
// 相手 KOIKOI 中に勝ち筋を取れる drop を強く優遇する加点。
#define DROP_OPP_KOIKOI_WIN_BONUS 100000
// 相手 KOIKOI 中の勝ち筋を見逃す drop への減点。
#define DROP_OPP_KOIKOI_MISS_PENALTY 40000
// 盃所持時に可視の月/桜光を早取りして酒リーチを作る独立加点。
#define DROP_VISIBLE_SAKE_LIGHT_SETUP_BONUS 72000
// その光札が相手の光系危険役にも絡む場合の追加加点。
#define DROP_VISIBLE_SAKE_LIGHT_DENY_BONUS 18000
// 相手 KOIKOI 中に即死筋の受け皿を新規開通する drop を強く抑止する減点。
// 回帰防止: 条件を広げたら tools/ai の fixed-seed run1k を回し、Hard 勝率 40.6% 未満なら条件か値を絞ること。
#define DROP_OPP_KOIKOI_KEYTARGET_EXPOSE_PENALTY 180000
// 相手 KOIKOI 中に次手即負けを招く候補を実質除外する大減点。
#define DROP_2PLY_KOIKOI_OPP_PRUNE_PENALTY 3000000
// 最終ラウンドで逆転不能な即時あがりを避ける大減点。
#define DROP_FINAL_NON_CLINCH_IMMEDIATE_PENALTY 3000000
// 相手の次手 7+ を許す候補を tie-break レベルで落とす減点。
#define DROP_2PLY_SEVEN_PLUS_TIEBREAK_PENALTY 117500
// 相手の次手 7+ が確定級の候補を強く弾く減点。
#define DROP_2PLY_CERTAIN_SEVEN_PLUS_PENALTY 700000
// 次手 7+ の 2-ply 読みを起動する脅威閾値。
#define DROP_2PLY_SEVEN_PLUS_GATE_THREAT 60
// 次手 7+ の 2-ply 読みを起動する推定距離閾値。
#define DROP_2PLY_SEVEN_PLUS_GATE_DISTANCE 2
// 2〜3手先の 7+ 育成を直接潰す評価の、脅威差分 1 点あたりの重み。
#define DROP_NEAR_SEVEN_PLUS_THREAT_UNIT 1200
// 2〜3手先の 7+ 育成を直接潰す評価の、距離差分 1 手あたりの重み。
#define DROP_NEAR_SEVEN_PLUS_DISTANCE_UNIT 28000
// 相手 7+ 距離が 1 手まで縮んだままの候補への追加減点。
#define DROP_NEAR_SEVEN_PLUS_CRITICAL_PENALTY 70000
// 相手 7+ 距離が 2 手以内のままの候補への追加減点。
#define DROP_NEAR_SEVEN_PLUS_NEAR_PENALTY 26000
// 相手 7+ 脅威が高いままの候補への追加減点。
#define DROP_NEAR_SEVEN_PLUS_HIGH_THREAT_PENALTY 32000
// 相手 7+ 脅威が中程度のままの候補への追加減点。
#define DROP_NEAR_SEVEN_PLUS_MID_THREAT_PENALTY 14000
// 相手 KOIKOI 中に 7+ 距離を詰めたままの候補への追加減点。
#define DROP_NEAR_SEVEN_PLUS_KOIKOI_PENALTY 40000
// 相手が 6 点以上で KOIKOI 中のとき、+1 ルート（タネ/タン/カス）を減らす価値。
#define DROP_EMERGENCY_ONE_POINT_ROUTE_BONUS 900
#define DROP_EMERGENCY_ONE_POINT_SHUTOUT_BONUS 1400
#define DROP_EMERGENCY_ONE_POINT_REACH_UNIT 22
#define DROP_EMERGENCY_ONE_POINT_DELAY_UNIT 380
#define DROP_EMERGENCY_ONE_POINT_STILL_CRITICAL_PENALTY 500
#define DROP_EMERGENCY_ONE_POINT_STILL_NEAR_PENALTY 180
// 中盤で、見えている相手手札の高打点役札へ月札を放り込む手を強く抑止する。
#define DROP_VISIBLE_ROLE_FEED_BASE_PENALTY 180
#define DROP_VISIBLE_ROLE_FEED_OPEN_BONUS 220
#define DROP_VISIBLE_ROLE_FEED_CRITICAL_BONUS 120
#define DROP_VISIBLE_ROLE_FEED_RELATED_BONUS 40
#define DROP_VISIBLE_ROLE_FEED_PROGRESS_UNIT 70
#define DROP_VISIBLE_ROLE_FEED_REACH_UNIT 4
#define DROP_VISIBLE_ROLE_FEED_SCORE_UNIT 30
#define DROP_VISIBLE_ROLE_FEED_FAST_BONUS 120
// 危険役の必須札を切る罰は bias に依らず効かせる。
#define DROP_DANGER_CRITICAL_NEAR_PENALTY 320
#define DROP_DANGER_CRITICAL_MID_PENALTY 160
#define DROP_DANGER_CRITICAL_FAR_PENALTY 80
#define DROP_DANGER_RELATED_NEAR_PENALTY 110
#define DROP_DANGER_RELATED_MID_PENALTY 55
#define DROP_DANGER_RELATED_FAR_PENALTY 25
#define DROP_DANGER_SCORE_UNIT 14
#define DROP_DANGER_REACH_UNIT 2
#define DROP_DANGER_VISIBLE_CAPTURE_BONUS 220
// 可視の相手手札にある 5点役の構成札が拾える月札を奪う加点。
#define DROP_FIVE_POINT_BLOCK_BASE_BONUS 900
#define DROP_FIVE_POINT_BLOCK_FULL_DENY_BONUS 1800
#define DROP_FIVE_POINT_BLOCK_REACH_UNIT 18
#define DROP_FIVE_POINT_BLOCK_SCORE_UNIT 220
#define DROP_EARLY_SAKE_THREAT_MAX 45
#define DROP_EARLY_SAKE_RISK7_MIN_DISTANCE 4
#define DROP_EARLY_SAKE_LEFT_OWN_MIN 6
#define DROP_EARLY_SAKE_LEFT_ROUNDS_MIN 3
#define DROP_EARLY_SAKE_BEHIND_LIMIT -15
#define DROP_EARLY_SAKE_COMPLETION_DELAY_MAX 3
#define DROP_7LINE_D1_BONUS 140000
#define DROP_7LINE_D2_BONUS 90000
#define DROP_7LINE_D3_BONUS 35000
#define DROP_7LINE_D4P_BONUS 0
#define DROP_7LINE_MARGIN2_BONUS 24000
#define DROP_VISIBLE_LIGHT_FINISH_BONUS 70000
#define DROP_VISIBLE_SAKE_FOLLOWUP_BONUS 250000
static int count_known_month_cards_for_player(int player, int month);
static int count_owned_month_cards(int player, int month);
static int player_has_hand_card(int player, int card_no);
static int count_floor_month_cards(int month);
static int hard_player_has_sake_cup(int player);
#define DROP_7LINE_MARGIN1_BONUS 12000
#define DROP_7LINE_MIN_REACH 10
#define DROP_7LINE_LOW_REACH_MULT 50
#define DROP_7LINE_PRESS_MULT 115
#define DROP_7LINE_BONUS_CLAMP_MAX 180000

#if HARD_RAPACIOUS_FALLBACK_ENABLE == 1
static int hard_should_force_rapacious_fallback_behind(const StrategyData* s)
{
    return (s && s->match_score_diff <= -HARD_RAPACIOUS_FALLBACK_BEHIND_THRESHOLD) ? ON : OFF;
}

static int hard_has_rapacious_fallback_sake_base(int player)
{
    return player >= 0 && player <= 1 && g.ai_model[player] == AI_MODEL_HARD && ai_debug_has_initial_sake(player);
}

static int hard_should_disable_rapacious_fallback_sake(const StrategyData* s)
{
    if (!s) {
        return ON;
    }
    return s->opponent_win_x2 == ON || s->opp_coarse_threat >= 85 || s->left_rounds <= 2;
}

static int hard_should_force_rapacious_fallback_sake_select_drop(int player, const StrategyData* s)
{
    if (!hard_has_rapacious_fallback_sake_base(player) || hard_should_disable_rapacious_fallback_sake(s)) {
        return OFF;
    }
    return g.koikoi[1 - player] ? OFF : ON;
}
#endif

static int hard_player_invent_has_card_id(int player, int card_id)
{
    int type;
    int i;

    if (player < 0 || player > 1) {
        return OFF;
    }
    for (type = 0; type < 4; type++) {
        for (i = 0; i < g.invent[player][type].num; i++) {
            Card* card = g.invent[player][type].cards[i];
            if (card && card->id == card_id) {
                return ON;
            }
        }
    }
    return OFF;
}

static int hard_player_has_sake_cup(int player)
{
    return player_has_hand_card(player, 35) || hard_player_invent_has_card_id(player, 35);
}

#if HARD_RAPACIOUS_FALLBACK_ENABLE == 1
static int hard_should_cancel_rapacious_fallback_sake_drop(int player, const StrategyData* s)
{
    int opp = 1 - player;

    if (!hard_should_force_rapacious_fallback_sake_select_drop(player, s)) {
        return OFF;
    }
    return hard_player_invent_has_card_id(opp, 11) && hard_player_invent_has_card_id(opp, 31);
}
#endif

// 高確率で役素材になる札の価値を押し上げる加点。
#define DROP_HOT_MATERIAL_BONUS 18000
// GREEDY 時に高確率で役素材になる札の価値をさらに押し上げる加点。
#define DROP_HOT_MATERIAL_GREEDY_BONUS 30000
// overpay 用の素材確保を押す加点。
#define DROP_OVERPAY_SETUP_BONUS 42000
// overpay が見えている時に base=5 即成立を少し抑える減点。
#define DROP_OVERPAY_IMMEDIATE_DELAY_PENALTY 36000
// 前半に高確度 5 点役をあえて遅らせる抑制/後押し。
#define DROP_EARLY_FIVE_POINT_DELAY_PENALTY 72000
#define DROP_EARLY_FIVE_POINT_DELAY_SETUP_BONUS 16000
#define DROP_EARLY_FIVE_POINT_DELAY_TURN_MAX 3
#define DROP_EARLY_FIVE_POINT_DELAY_MIN_REACH 20
#define DROP_EARLY_FIVE_POINT_DELAY_MAX_DELAY 3
// 3枚で成立する 5 点役は、2枚目でのリーチ化を特に強く、1枚目も中程度に押す。
#define DROP_THREE_CARD_FIVE_POINT_REACH_SETUP_BONUS 850
#define DROP_THREE_CARD_FIVE_POINT_REACH_REACH_UNIT 20
#define DROP_THREE_CARD_FIVE_POINT_FIRST_SETUP_BONUS 350
#define DROP_THREE_CARD_FIVE_POINT_FIRST_REACH_UNIT 8
#define DROP_THREE_CARD_FIVE_POINT_FAST_DELAY_BONUS 120
// 手札の役札を「今この公開情報で secure 化する」価値。
#define DROP_PUBLIC_ACCESS_SECURE_HIGH_BONUS 5200
#define DROP_PUBLIC_ACCESS_SECURE_MID_BONUS 2400
#define DROP_PUBLIC_ACCESS_SECURE_LAST_VISIBLE_BONUS 700
#define DROP_PUBLIC_ACCESS_SECURE_NO_LOCK_BONUS 700
#define DROP_PUBLIC_ACCESS_SECURE_NO_BACKUP_BONUS 500
#define DROP_PUBLIC_ACCESS_SECURE_FRAGILE_ROUTE_BONUS 2200
#define DROP_PUBLIC_ACCESS_SECURE_REDUNDANT_PENALTY 1800
// 同月4枚のうち3枚を把握している「月ロック」を取る加点。
#define DROP_MONTH_LOCK_BONUS 1500
// 月ロックを取り逃がす減点。
#define DROP_MONTH_LOCK_MISS_PENALTY 18000
// LIGHT ドメインの未ロック露出キーカードを相手手番前に確保する価値。
#define DROP_LIGHT_EXPOSED_KEYCARD_BONUS 3000
// SAKE ドメインの未ロック露出キーカードを相手手番前に確保する価値。
#define DROP_SAKE_EXPOSED_KEYCARD_BONUS 5000
// ISC ドメインの未ロック露出キーカードを相手手番前に確保する価値。
#define DROP_ISC_EXPOSED_KEYCARD_BONUS 2500
// AKATAN ドメインの未ロック露出キーカードを相手手番前に確保する価値。
#define DROP_AKATAN_EXPOSED_KEYCARD_BONUS 2500
// AOTAN ドメインの未ロック露出キーカードを相手手番前に確保する価値。
#define DROP_AOTAN_EXPOSED_KEYCARD_BONUS 2500
// 裏の本命札が高価値役のキーカードなら追加で押す加点。
#define DROP_MONTH_LOCK_HIDDEN_KEY_BONUS 28000
// 裏の本命札が相手危険役にも絡むならさらに押す加点。
#define DROP_MONTH_LOCK_HIDDEN_DENY_BONUS 64000
// combo7 生値へ掛ける基本重み。
#define DROP_COMBO7_WEIGHT 1
// GREEDY 時は combo7 を主項として押し上げる。
#define COMBO7_GREEDY_MULT 150
// BALANCED 時は tactical より少し強い程度に留める。
#define COMBO7_BALANCED_MULT 70
// DEFENSIVE 時は即勝ち・防御を崩さないよう抑える。
#define COMBO7_DEFENSIVE_MULT 0
// GREEDY かつ安全な押し込み局面では combo7 を主項寄りに昇格する。
#define COMBO7_PRIORITY_PUSH_MULT 130
// combo7 寄与差がこの値以下なら「重み変更で反転しやすい」局面とみなす。
#define COMBO7_MARGIN_FLIP_THRESHOLD 4000

// DropActionMC
#define DROP_ACTION_MC_SAMPLES 8
#define DROP_ACTION_MC_DIFF_WEIGHT 200
#define DROP_ACTION_MC_GREEDY_MULT 100
#define DROP_ACTION_MC_BALANCED_MULT 75
#define DROP_ACTION_MC_DEFENSIVE_MULT 20
#define DROP_ACTION_MC_TOP_K 2
#define DROP_ACTION_MC_GAP_THRESHOLD 30000
#define DROP_ACTION_MC_TRIGGER_QDIFF -600
#define DROP_ACTION_MC_Q_MARGIN 0

// EnvMC
#define ENV_MC_SAMPLES 8
#define ENV_MC_CLAMP 6000
#define ENV_MC_SCALE 4000
#define ENV_MC_GAP_THRESHOLD 2000
#define ENV_MC_GREEDY_BONUS 220
#define ENV_MC_GREEDY_PENALTY 120
#define ENV_MC_BALANCED_BONUS 150
#define ENV_MC_BALANCED_PENALTY 150
#define ENV_MC_DEFENCE_BONUS 100
#define ENV_MC_DEFENCE_PENALTY 220
#ifndef WATCH_DROP_METRICS_ENABLE
#define WATCH_DROP_METRICS_ENABLE 1
#endif

typedef struct {
    int valid;
    int index;
    int card_no;
    int taken_card_no;
    int action_mc_cluster;
    int value;
    int value_without_certain;
    int value_without_combo7;
    int action_mc_bonus;
    int action_mc_score_diff;
    int env_mc_bonus;
    int env_mc_E;
    int combo7_bonus;
    int combo7_plus1_rank;
    int value_without_keytarget;
    int keytarget_expose_penalty;
    int keytarget_expose_wid;
    int keytarget_expose_month;
    int risk_sum;
    int risk_min_delay;
    int self_min_delay;
    int completion_base;
    int completion_unrevealed;
    int visible_light_finish_bonus;
    int visible_light_finish_gain;
    int immediate_gain;
    int endgame_clinch_score;
    int endgame_needed;
    int endgame_k;
    int part_score;
    int part_speed;
    int part_deny;
    int part_safe;
    int part_keep;
    int part_tactical;
    int part_bonus_total;
    int opp_immediate_win;
    int opp_immediate_score;
    int opp_immediate_wid;
    int opp_immediate_x2;
    int opp7_dist;
    int opp7_threat;
    int opp7_certain;
    int certain_7plus;
    int near_7plus;
    int koikoi_opp_prune;
    int aim_type;
    int aim_wid;
    const char* prune_reason_code;
    int keep_fatal;
    int safety_fatal;
} DropCandidateScore;
enum {
    TRACE_AIM_NONE = 0,
    TRACE_AIM_IMMEDIATE,
    TRACE_AIM_SEVENLINE,
    TRACE_AIM_COMBO7,
    TRACE_AIM_MONTHLOCK,
    TRACE_AIM_OVERPAY,
    TRACE_AIM_DEFENCE7,
    TRACE_AIM_KOIKOIWIN,
};

typedef struct {
    int before_best_index;
    int before_second_index;
    int before_best_card_no;
    int before_second_card_no;
    int before_best_value;
    int before_second_value;
    int compare_active;
    int compare_triggered;
    int chosen_index;
    int chosen_card_no;
    int chosen_material;
    int best_qdiff;
    int second_qdiff;
    int trigger_qdiff;
} DropActionMcCompareResult;

typedef struct {
    int before_best_index;
    int before_second_index;
    int before_best_card_no;
    int before_second_card_no;
    int before_best_value;
    int before_second_value;
    int chosen_index;
    int chosen_card_no;
    int best_E;
    int second_E;
    int before_gap;
    int after_gap;
    int compare_triggered;
    int changed_choice;
} DropEnvMcCompareResult;

typedef struct {
    int capture_possible;
    int capture_quality;
    int leave_quality;
    int chosen_take_card_no;
} DropCaptureEval;

typedef struct {
    int ok;
    int wid;
    int delay;
    int reach;
    int margin;
    int score;
} AiSevenLine;

typedef struct {
    int base_bonus;
    int margin_bonus;
    int mult;
    int bonus;
    int low_reach;
    int press_applied;
} AiSevenLineBonus;

typedef struct {
    int d1_trigger;
    int d1_applied;
    int d1_changed;
    int action_mc_trigger;
    int action_mc_applied;
    int action_mc_changed;
    int phase2a_trigger;
    int phase2a_changed;
    int env_mc_trigger;
    int env_mc_changed;
    int final_best_index;
    int final_best_index_pre_d1;
    int final_best_index_phase2a_off;
    int chosen_rank;
    int chosen_gap_vs_second;
    const char* reason_code;
} DropTraceDecision;

static int collect_takeable_floor_cards_by_drop(int drop_card_no, int* out_list, int cap);
static int simulate_opponent_normal_turn_loss(int player, int* out_opp_score, int* out_is_seven_plus, int* out_opp_wid);
static int is_dangerous_risk_wid(const StrategyData* s, int wid);
static int calc_normal_drop_greedy_score(int player, int hand_index);
static int choose_best_drop_candidate(const DropCandidateScore* candidates, int count, int value_mode, int fallback_index);
static int collect_drop_choice_order_indices(const DropCandidateScore* candidates, int count, int value_mode, int fallback_index, int* out_indices, int cap);
static int build_greedy_fallback_pool(
    const DropCandidateScore* candidates, int count, int value_mode, int fallback_index, DropCandidateScore* out_pool, int* out_pool_count);
static int calc_fallback_take_priority(const DropCandidateScore* candidate);
static int choose_greedy_drop_candidate_from_pool(
    int player, const StrategyData* before, const DropCandidateScore* pool, int pool_count, int value_mode, int* out_chosen_pool_idx);
static void log_drop_fallback_debug(int player, int mode, AiPlan plan, const DropCandidateScore* pool, int pool_count, int top_k, int chosen_pool_idx,
                                    int chosen_orig_idx, int final_best_idx_pre_fb, int final_best_idx_post_fb);
static int drop_action_mc_mode_multiplier(int mode);
static int drop_action_mc_material_type(int card_no);
static const char* drop_action_mc_material_name(int material);
static int collect_drop_action_mc_top_indices(const DropCandidateScore* candidates, int count, int fallback_index, int* out_indices, int cap);
static int drop_action_mc_is_focus_card(const StrategyData* before, int card_no);
static void evaluate_drop_action_mc_compare(
    int player, const StrategyData* before, int mode, DropCandidateScore* candidates, int count, int fallback_index, DropActionMcCompareResult* out_result);
static int drop_env_mc_mode_bonus(int mode);
static int drop_env_mc_mode_penalty(int mode);
static const char* strategy_mode_name(int mode);
static const char* env_domain_name(AiEnvDomain domain);
static const char* env_plan_name(AiPlan plan);
static const char* env_category_name(int cat);
static const char* phase2a_reason_name(int reason);
static int trace_str_eq(const char* lhs, const char* rhs);
static void ai_env_cat_topN(const int cat_sum[ENV_CAT_MAX], int n, int out_idx[], int out_val[]);
static int apply_mult(int base, int mult);
static int plan_mult(const StrategyData* s);
static int survive_offence_mult(const StrategyData* s);
static int domain_mult(const StrategyData* s);
static int defence_mult(const StrategyData* s);
static int is_unlocked_rainman_state(int player);
static int calc_unlocked_rainman_hold_penalty(int player, int card_no, const StrategyData* before, int capture_possible);
static AiSevenLine find_best_seven_line(int player, const StrategyData* s);
static int clamp_int_local(int value, int min_value, int max_value);
static int calc_greedy_seven_plus_bonus_raw(int player, const StrategyData* s, AiSevenLine* out_line, AiSevenLineBonus* out_bonus);
static int calc_greedy_seven_plus_bonus_scaled(const StrategyData* s, const AiSevenLine* line, int raw_bonus, AiSevenLineBonus* out_bonus);
static int phase2a_domain_matches_cat(AiEnvDomain domain, int cat);
static int calc_domain_hot_bonus(int player, int card_no, const StrategyData* s);
static int find_drop_candidate_pos_by_index(const DropCandidateScore* candidates, int count, int index);
static int candidate_value_gap_by_index(const DropCandidateScore* candidates, int count, int idx_a, int idx_b);
static int choose_best_drop_candidate_from_sevenline_d1(const DropCandidateScore* candidates, const AiSevenLine* sevenlines, int count, int fallback_index,
                                                        int* out_d1_count);
static int calc_visible_light_finish_bonus(int player, const StrategyData* before, const StrategyData* after, int immediate_gain, int completion_base,
                                           int* out_wid, int* out_gain, int* out_hand_card_no, int* out_floor_card_no);
static int collect_drop_choice_order_indices_from_sevenline_d1(const DropCandidateScore* candidates, const AiSevenLine* sevenlines, int count, int fallback_index,
                                                               int* out_indices, int cap);
static void evaluate_drop_env_mc_compare(
    int player, const StrategyData* before, int mode, DropCandidateScore* candidates, int count, int fallback_index, DropEnvMcCompareResult* out_result);
static int count_floor_same_month(int month);
static int role_is_keytarget_expose_dangerous(const StrategyData* s, int wid);
static int find_unique_hidden_opp_keycard_for_role(int player, int wid, int* out_keycard_no);
static int find_opp_koikoi_keytarget_exposure(int player, int drop_card_no, const StrategyData* s, int* out_wid, int* out_keycard_no, int* out_penalty);
static void append_text_local(char* dst, int cap, int* pos, const char* src);
static void append_int_local(char* dst, int cap, int* pos, int value);
static void format_drop_fallback_take(char* dst, int cap, int card_no, int taken_card_no);
static void format_drop_feed(char* dst, int cap, const DropCandidateScore* candidate);
static void format_drop_feed_metric(char* dst, int cap, int player, int card_no, int taken_card_no);
static const char* trace_aim_type_name(int aim_type);
static const char* trace_reason_short(const DropTraceDecision* decision);
static void debug_assert_drop_fallback_pool_idx(int top_k, int chosen_pool_idx);
static int is_exposing_keycard_capture_target(int player, int drop_card_no, const StrategyData* s, int* out_wid, int* out_key_month, int* out_penalty);
static int sevenline_d1_loss_reason_from_phase2a(int reason);
static int trace_min_risk_delay(const StrategyData* s);
static int trace_max_risk_score(const StrategyData* s);
static const char* trace_side_name(int player);
static const char* trace_card_type_name(int card_no);
static const char* trace_mode_short(int mode);
static int drop_candidate_has_priority_stop_value(const DropCandidateScore* candidate);
static int should_keep_drop_completion_candidate(int player, const DropCandidateScore* base_candidate, const DropCandidateScore* override_candidate);
static int is_small_lead_danger_state(const StrategyData* s);
static int card_exists_in_hand(int player, int card_no);
static int card_exists_in_invent(int player, int card_no);
static int card_exists_on_floor(int card_no);
static int calc_access_keep_penalty_for_wid(int player, int card, int wid, const StrategyData* before);
static int is_light_capture_urgent_state(const StrategyData* s);
static int is_domain_capture_target(int player, int taken_card_no, const StrategyData* s);
static int calc_domain_capture_bonus(int player, int taken_card_no, const StrategyData* s);
static int calc_exposed_domain_keycard_capture_bonus(int player, int taken_card_no, const StrategyData* s);
static int calc_opp_five_point_block_bonus(int player, int taken_card_no, const StrategyData* before);
static int calc_visible_opp_role_feed_penalty(int player, int drop_card_no, const StrategyData* before);
static int has_public_same_month_role_followup(int player, int opp, int month, int wid, int exclude_card_no);
static int should_press_domain_capture(int player, int taken_card_no, const StrategyData* s);
static const char* trace_plan_short(AiPlan plan);
static int count_owned_type(int player, int type);
static int count_owned_kasu_total(int player);
static int hand_has_same_month_card_type_except(int player, int month, int type, int exclude_card_no);
static int calc_sake_cup_capture_material_bonus(int player, int drop_card_no, int taken_card_no, const StrategyData* before);
static void calc_drop_top2_feed(int player, int card_no, int taken_card_no, int* out_flag, int* out_month, int* out_recap);
static void emit_drop_trace(int player, const StrategyData* before, const DropCandidateScore* candidates, const AiSevenLine* sevenlines,
                            const AiSevenLineBonus* sevenline_bonuses, int count, int fallback_index, const DropTraceDecision* decision);

enum DropGreedyValueMode {
    DROP_GREEDY_VALUE_NORMAL = 0,
    DROP_GREEDY_VALUE_WITHOUT_CERTAIN = 1,
    DROP_GREEDY_VALUE_WITHOUT_COMBO7 = 2,
    DROP_GREEDY_VALUE_WITHOUT_ENDGAME = 3,
    DROP_GREEDY_VALUE_WITHOUT_KEYTARGET = 4,
};

static int drop_candidate_value(const DropCandidateScore* candidate, int value_mode)
{
    if (!candidate) {
        return INT_MIN;
    }
    switch (value_mode) {
        case DROP_GREEDY_VALUE_WITHOUT_CERTAIN: return candidate->value_without_certain;
        case DROP_GREEDY_VALUE_WITHOUT_COMBO7: return candidate->value_without_combo7;
        case DROP_GREEDY_VALUE_WITHOUT_ENDGAME: return candidate->value - candidate->endgame_clinch_score;
        case DROP_GREEDY_VALUE_WITHOUT_KEYTARGET: return candidate->value_without_keytarget;
        default: return candidate->value;
    }
}

static int calc_endgame_k(const StrategyData* s)
{
    int t;

    if (!s) {
        return 0;
    }
    t = s->left_rounds - 1;
    switch (t) {
        case 0: return 100;
        case 1: return 80;
        case 2: return 40;
        case 3: return 20;
        default: return 0;
    }
}

static int calc_endgame_needed(const StrategyData* s)
{
    if (!s) {
        return 0;
    }
    return (-s->match_score_diff + 1) > 0 ? (-s->match_score_diff + 1) : 0;
}

static int calc_endgame_clinch_score(const StrategyData* s, int immediate_gain, int* out_needed, int* out_k_end)
{
    int needed;
    int threshold;
    int excess;
    int shortfall;
    int k_end;

    needed = calc_endgame_needed(s);
    k_end = calc_endgame_k(s);
    if (out_needed) {
        *out_needed = needed;
    }
    if (out_k_end) {
        *out_k_end = k_end;
    }
#if !ENDGAME_ENABLE
    (void)immediate_gain;
    return 0;
#else
    if (k_end <= 0) {
        return 0;
    }
    threshold = needed == 0 ? 1 : needed;
    excess = (immediate_gain - threshold) > 0 ? (immediate_gain - threshold) : 0;
    shortfall = (threshold - immediate_gain) > 0 ? (threshold - immediate_gain) : 0;
    return (k_end * ENDGAME_WEIGHT_100 / 100) *
           (ENDGAME_CLINCH_BONUS * (immediate_gain >= threshold) - ENDGAME_EXCESS_PENALTY_UNIT * excess -
            ENDGAME_SHORTFALL_PENALTY_UNIT * shortfall);
#endif
}

static int is_final_round_non_clinch_immediate(const StrategyData* s, int immediate_gain)
{
    if (!s || immediate_gain <= 0) {
        return OFF;
    }
    if (s->left_rounds != 1 || s->match_score_diff >= 0) {
        return OFF;
    }
    return immediate_gain < calc_endgame_needed(s) ? ON : OFF;
}

static int trace_min_risk_delay(const StrategyData* s)
{
    int min_delay = RISK_DELAY_INVALID;

    if (!s) {
        return RISK_DELAY_INVALID;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (s->risk_reach_estimate[wid] <= 0 && s->risk_score[wid] <= 0) {
            continue;
        }
        if (s->risk_delay[wid] < min_delay) {
            min_delay = s->risk_delay[wid];
        }
    }
    return min_delay;
}

static int trace_max_risk_score(const StrategyData* s)
{
    int max_score = 0;

    if (!s) {
        return 0;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (s->risk_score[wid] > max_score) {
            max_score = s->risk_score[wid];
        }
    }
    return max_score;
}

static const char* trace_side_name(int player)
{
    return player ? "CPU" : "P1";
}

static const char* trace_card_type_name(int card_no)
{
    if (card_no < 0 || card_no >= 48) {
        return "n/a";
    }
    switch (card_types[card_no]) {
        case CARD_TYPE_KASU: return "KASU";
        case CARD_TYPE_TAN: return "TAN";
        case CARD_TYPE_TANE: return "TANE";
        case CARD_TYPE_GOKOU: return "GOKOU";
        default: return "UNKNOWN";
    }
}

static const char* trace_mode_short(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return "G";
        case MODE_DEFENSIVE: return "D";
        default: return "B";
    }
}

static const char* trace_plan_short(AiPlan plan)
{
    return plan == AI_PLAN_PRESS ? "P" : "S";
}

static void calc_drop_top2_feed(int player, int card_no, int taken_card_no, int* out_flag, int* out_month, int* out_recap)
{
    int month = 0;
    int flag = 0;
    int recap = 0;

    if (card_no >= 0) {
        month = card_no / 4 + 1;
    }
    if (taken_card_no < 0 && card_no >= 0 && drop_action_mc_material_type(card_no) >= 0) {
        flag = ON;
        recap = ai_count_unrevealed_same_month(1 - player, month - 1) > 0 ? ON : OFF;
    }
    if (out_flag) {
        *out_flag = flag;
    }
    if (out_month) {
        *out_month = month;
    }
    if (out_recap) {
        *out_recap = recap;
    }
}

static void emit_drop_trace(int player, const StrategyData* before, const DropCandidateScore* candidates, const AiSevenLine* sevenlines,
                            const AiSevenLineBonus* sevenline_bonuses, int count, int fallback_index, const DropTraceDecision* decision)
{
#if WATCH_TRACE_ENABLE
    int order[8];
    int full_order[8];
    int order_count;
    int full_order_count;
    int env_p1;
    int env_cpu;
    int top_value;
    int opp_risk_delay_min;
    int opp_risk_score_max;
    int chosen_pos = -1;
    const AiSevenLine* chosen_line = NULL;
    int chosen_rank = 0;
    int chosen_gap = 0;

    if (!g.auto_play || !before || !candidates || count <= 0 || !decision) {
        return;
    }

    order_count = collect_drop_choice_order_indices(candidates, count, DROP_GREEDY_VALUE_NORMAL, fallback_index, order, WATCH_TRACE_DROP_TOP_K);
    full_order_count = collect_drop_choice_order_indices(candidates, count, DROP_GREEDY_VALUE_NORMAL, fallback_index, full_order, 8);
    env_p1 = ai_env_score(0);
    env_cpu = ai_env_score(1);
    opp_risk_delay_min = trace_min_risk_delay(before);
    opp_risk_score_max = trace_max_risk_score(before);
    top_value = 0;
    chosen_pos = find_drop_candidate_pos_by_index(candidates, count, decision->final_best_index);
    if (chosen_pos >= 0 && sevenlines) {
        chosen_line = &sevenlines[chosen_pos];
    }

    if (order_count > 0) {
        int top_pos = find_drop_candidate_pos_by_index(candidates, count, order[0]);
        if (top_pos >= 0) {
            top_value = candidates[top_pos].value;
        }
    }
    if (full_order_count > 0) {
        int top_pos = find_drop_candidate_pos_by_index(candidates, count, full_order[0]);
        int second_pos = full_order_count > 1 ? find_drop_candidate_pos_by_index(candidates, count, full_order[1]) : -1;
        for (int i = 0; i < full_order_count; i++) {
            if (full_order[i] == decision->final_best_index) {
                chosen_rank = i + 1;
                break;
            }
        }
        if (chosen_pos >= 0 && second_pos >= 0 && top_pos >= 0) {
            chosen_gap = candidates[top_pos].value - candidates[second_pos].value;
        }
    }

    ai_putlog("[HARD-TRACE] side=%s round=%d turn=%d phase=drop mode=%s plan=%s domain=%s env=P1=%d CPU=%d diff=%d fallback=%d",
              trace_side_name(player), g.round + 1, 9 - g.own[player].num, strategy_mode_name(before->mode), env_plan_name(before->plan),
              env_domain_name(before->domain), env_p1, env_cpu, env_p1 - env_cpu, fallback_index);
    ai_putlog("[HARD-TOP] drop_top%d:", order_count);
    for (int rank = 0; rank < order_count; rank++) {
        int pos = find_drop_candidate_pos_by_index(candidates, count, order[rank]);
        int gap = 0;
        const DropCandidateScore* c;
        const AiSevenLine* line = NULL;
        const AiSevenLineBonus* bonus = NULL;

        if (pos < 0) {
            continue;
        }
        c = &candidates[pos];
        if (rank > 0) {
            gap = top_value - c->value;
        }
        if (sevenlines) {
            line = &sevenlines[pos];
        }
        if (sevenline_bonuses) {
            bonus = &sevenline_bonuses[pos];
        }

        ai_putlog("  #%d card=%d take=%s value=%d gap=%d mat=%s parts{score=%d speed=%d deny=%d safe=%d keep=%d tactical=%d bonus=%d}",
                  rank + 1, c->card_no, c->taken_card_no >= 0 ? "pair" : "n/a", c->value, gap, trace_card_type_name(c->card_no), c->part_score,
                  c->part_speed, c->part_deny, c->part_safe, c->part_keep, c->part_tactical, c->part_bonus_total);
        if (c->taken_card_no >= 0) {
            ai_putlog("    take_pair=%d,%d prune=%s flags{certain7=%d near7=%d koikoi_prune=%d} seven{ok=%d delay=%d margin=%d reach=%d wid=%s bonus=%d/%d/%d/%d}",
                      c->card_no, c->taken_card_no, c->prune_reason_code ? c->prune_reason_code : "NONE", c->certain_7plus, c->near_7plus,
                      c->koikoi_opp_prune, line ? line->ok : 0, line ? line->delay : 99, line ? line->margin : -99, line ? line->reach : 0,
                      (line && line->wid >= 0) ? winning_hands[line->wid].name : "NONE", bonus ? bonus->base_bonus : 0, bonus ? bonus->margin_bonus : 0,
                      bonus ? bonus->mult : 0, bonus ? bonus->bonus : 0);
        } else {
            ai_putlog("    take_pair=n/a prune=%s flags{certain7=%d near7=%d koikoi_prune=%d} seven{ok=%d delay=%d margin=%d reach=%d wid=%s bonus=%d/%d/%d/%d}",
                      c->prune_reason_code ? c->prune_reason_code : "NONE", c->certain_7plus, c->near_7plus, c->koikoi_opp_prune, line ? line->ok : 0,
                      line ? line->delay : 99, line ? line->margin : -99, line ? line->reach : 0, (line && line->wid >= 0) ? winning_hands[line->wid].name : "NONE",
                      bonus ? bonus->base_bonus : 0, bonus ? bonus->margin_bonus : 0, bonus ? bonus->mult : 0, bonus ? bonus->bonus : 0);
        }
    }
    ai_putlog("[HARD-REASON] chosen_rank=%d chosen_gap=%d reason=%s override{d1=%d/%d/%d actionmc=%d/%d/%d phase2a=%d/%d envmc=%d/%d} opp{koikoi=%d threat7=%d delaymin=%d scoremax=%d}",
              chosen_rank, chosen_gap, decision->reason_code ? decision->reason_code : "BASE_COMPARATOR",
              decision->d1_trigger, decision->d1_applied, decision->d1_changed, decision->action_mc_trigger, decision->action_mc_applied,
              decision->action_mc_changed, decision->phase2a_trigger, decision->phase2a_changed, decision->env_mc_trigger, decision->env_mc_changed,
              before->koikoi_opp, before->opp_coarse_threat, opp_risk_delay_min, opp_risk_score_max);
    ai_putlog("  seven{distance=%d margin=%d reach=%d wid=%s} final{index=%d pre_d1=%d phase2a_off=%d}",
              before->risk_7plus_distance, chosen_line ? chosen_line->margin : -99, chosen_line ? chosen_line->reach : 0,
              (chosen_line && chosen_line->wid >= 0) ? winning_hands[chosen_line->wid].name : "NONE",
              decision->final_best_index, decision->final_best_index_pre_d1, decision->final_best_index_phase2a_off);
#else
    (void)player;
    (void)before;
    (void)candidates;
    (void)sevenlines;
    (void)sevenline_bonuses;
    (void)count;
    (void)fallback_index;
    (void)decision;
#endif
}

static int sevenline_d1_loss_reason_from_phase2a(int reason)
{
    switch (reason) {
        case AI_PHASE2A_REASON_DEFENCE: return AI_SEVENLINE_D1_LOSS_DEFENCE;
        case AI_PHASE2A_REASON_COMBO7: return AI_SEVENLINE_D1_LOSS_COMBO7;
        case AI_PHASE2A_REASON_HOT: return AI_SEVENLINE_D1_LOSS_DOMAIN_HOT;
        default: return AI_SEVENLINE_D1_LOSS_OTHER;
    }
}

static int count_floor_same_month(int month)
{
    int count = 0;

    if (month < 0 || month >= 12) {
        return 0;
    }
    for (int i = 0; i < 48; i++) {
        Card* floor = g.deck.cards[i];
        if (floor && floor->month == month) {
            count++;
        }
    }
    return count;
}

static int role_is_keytarget_expose_dangerous(const StrategyData* s, int wid)
{
    if (!s || wid < 0 || wid >= WINNING_HAND_MAX || !ai_is_high_value_wid(wid)) {
        return OFF;
    }
    if (s->risk_reach_estimate[wid] <= 0 || s->risk_delay[wid] >= RISK_DELAY_INVALID) {
        return OFF;
    }
    if (winning_hands[wid].base_score < 5) {
        return OFF;
    }
    return (s->opp_coarse_threat >= DROP_2PLY_SEVEN_PLUS_GATE_THREAT && s->risk_delay[wid] <= 2) ? ON : OFF;
}

static int find_unique_hidden_opp_keycard_for_role(int player, int wid, int* out_keycard_no)
{
    int known[48];
    int hidden_count = 0;
    int hidden_keycard_no = -1;

    if (out_keycard_no) {
        *out_keycard_no = -1;
    }
    if (player < 0 || player > 1 || !ai_is_fixed_wid(wid) || ai_role_progress_level(1 - player, wid) <= 0) {
        return OFF;
    }

    vgs_memset(known, 0, sizeof(known));
    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card) {
            known[card->id] = ON;
        }
    }
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 4; t++) {
            CardSet* inv = &g.invent[p][t];
            for (int i = 0; i < inv->num; i++) {
                Card* card = inv->cards[i];
                if (card) {
                    known[card->id] = ON;
                }
            }
        }
    }
    for (int i = 0; i < 48; i++) {
        Card* floor = g.deck.cards[i];
        if (floor) {
            known[floor->id] = ON;
        }
    }

    for (int card_no = 0; card_no < 48; card_no++) {
        if (known[card_no] || !ai_is_keycard_for_role(card_no, wid)) {
            continue;
        }
        hidden_count++;
        hidden_keycard_no = card_no;
        if (hidden_count > 1) {
            return OFF;
        }
    }

    if (hidden_count != 1) {
        return OFF;
    }
    if (out_keycard_no) {
        *out_keycard_no = hidden_keycard_no;
    }
    return ON;
}

static int find_opp_koikoi_keytarget_exposure(int player, int drop_card_no, const StrategyData* s, int* out_wid, int* out_keycard_no, int* out_penalty)
{
    int best_wid = -1;
    int best_keycard_no = -1;
    int best_month = -1;
    int best_penalty = 0;

    if (out_wid) {
        *out_wid = -1;
    }
    if (out_keycard_no) {
        *out_keycard_no = -1;
    }
    if (out_penalty) {
        *out_penalty = 0;
    }
    if (!s || player < 0 || player > 1 || drop_card_no < 0 || drop_card_no >= 48) {
        return OFF;
    }
    if (s->koikoi_opp != ON) {
        return OFF;
    }
    if (s->opponent_win_x2 != ON) {
        return OFF;
    }
    if (s->opp_coarse_threat < DROP_2PLY_SEVEN_PLUS_GATE_THREAT && s->risk_7plus_distance > DROP_2PLY_SEVEN_PLUS_GATE_DISTANCE) {
        return OFF;
    }
    if (g.cards[drop_card_no].type != CARD_TYPE_KASU) {
        return OFF;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int keycard_no = -1;
        int month;
        int unrevealed_same_month;
        int penalty;

        if (!role_is_keytarget_expose_dangerous(s, wid)) {
            continue;
        }
        if (!find_unique_hidden_opp_keycard_for_role(player, wid, &keycard_no)) {
            continue;
        }

        month = g.cards[keycard_no].month;
        if (g.cards[drop_card_no].month != month) {
            continue;
        }
        if (count_floor_same_month(month) != 0) {
            continue;
        }

        unrevealed_same_month = ai_count_unrevealed_same_month(player, month);
        if (unrevealed_same_month != 1) {
            continue;
        }

        penalty = DROP_OPP_KOIKOI_KEYTARGET_EXPOSE_PENALTY;

        if (best_wid < 0 || s->risk_delay[wid] < s->risk_delay[best_wid] ||
            (s->risk_delay[wid] == s->risk_delay[best_wid] && winning_hands[wid].base_score > winning_hands[best_wid].base_score)) {
            best_wid = wid;
            best_keycard_no = keycard_no;
            best_month = month;
            best_penalty = penalty;
        }
    }

    if (best_wid < 0 || best_keycard_no < 0 || best_month < 0) {
        return OFF;
    }
    if (out_wid) {
        *out_wid = best_wid;
    }
    if (out_keycard_no) {
        *out_keycard_no = best_keycard_no;
    }
    if (out_penalty) {
        *out_penalty = best_penalty;
    }
    return ON;
}

static int is_exposing_keycard_capture_target(int player, int drop_card_no, const StrategyData* s, int* out_wid, int* out_key_month, int* out_penalty)
{
    int wid = -1;
    int keycard_no = -1;
    int penalty = 0;

    if (out_wid) {
        *out_wid = -1;
    }
    if (out_key_month) {
        *out_key_month = -1;
    }
    if (out_penalty) {
        *out_penalty = 0;
    }
    if (!find_opp_koikoi_keytarget_exposure(player, drop_card_no, s, &wid, &keycard_no, &penalty)) {
        return OFF;
    }
    if (out_wid) {
        *out_wid = wid;
    }
    if (out_key_month) {
        *out_key_month = g.cards[keycard_no].month;
    }
    if (out_penalty) {
        *out_penalty = penalty;
    }
    return ON;
}

static int choose_greedy_drop_candidate(int player, const StrategyData* before, const DropCandidateScore* candidates, int count, int value_mode, int fallback_index)
{
    DropCandidateScore pool[8];
    int pool_count = 0;

    if (!candidates || count <= 0) {
        return -1;
    }
    build_greedy_fallback_pool(candidates, count, value_mode, fallback_index, pool, &pool_count);
    return choose_greedy_drop_candidate_from_pool(player, before, pool, pool_count, value_mode, NULL);
}

static int has_live_sake_line(const StrategyData* s, int reach_hanami, int delay_hanami, int reach_tsukimi, int delay_tsukimi)
{
    if (!s) {
        return OFF;
    }
    if ((reach_hanami >= 10 && delay_hanami <= 4) || (reach_tsukimi >= 10 && delay_tsukimi <= 4)) {
        return ON;
    }
    return (s->reach[WID_ISC] >= 20 && s->delay[WID_ISC] <= 4) ? ON : OFF;
}

static int should_preserve_sake_cup_in_greedy_fallback(const StrategyData* before, const DropCandidateScore* candidate)
{
    Card* card;
    int self_live;
    int opp_live;

    if (!before || !candidate || candidate->card_no != 35 || candidate->taken_card_no >= 0) {
        return OFF;
    }
    if (before->left_own < DROP_EARLY_SAKE_LEFT_OWN_MIN || before->left_rounds < DROP_EARLY_SAKE_LEFT_ROUNDS_MIN) {
        return OFF;
    }
    if (before->koikoi_opp == ON || before->opponent_win_x2 == ON) {
        return OFF;
    }

    card = &g.cards[candidate->card_no];
    if (card->type != CARD_TYPE_TANE || card->month != 8) {
        return OFF;
    }

    self_live = has_live_sake_line(before, before->reach[WID_HANAMI], before->delay[WID_HANAMI], before->reach[WID_TSUKIMI], before->delay[WID_TSUKIMI]);
    opp_live = has_live_sake_line(before, before->risk_reach_estimate[WID_HANAMI], before->risk_delay[WID_HANAMI],
                                  before->risk_reach_estimate[WID_TSUKIMI], before->risk_delay[WID_TSUKIMI]);
    return (self_live || opp_live) ? ON : OFF;
}

static int should_keep_top_value_over_tan_domain_material_fallback(
    int player, const StrategyData* before, const DropCandidateScore* top, const DropCandidateScore* chosen, int top_value, int chosen_value)
{
    int target_wid;
    int material_card_no = -1;
    Card* material;

    if (player < 0 || player > 1 || !before || !top || !chosen) {
        return OFF;
    }
    if (before->plan != AI_PLAN_SURVIVE || before->left_own < 6) {
        return OFF;
    }
    if (before->domain == AI_ENV_DOMAIN_AKATAN) {
        target_wid = WID_AKATAN;
    } else if (before->domain == AI_ENV_DOMAIN_AOTAN) {
        target_wid = WID_AOTAN;
    } else {
        return OFF;
    }
    if (chosen->taken_card_no < 0 || top->taken_card_no >= 0) {
        return OFF;
    }
    if (chosen->immediate_gain > 0 || chosen->completion_base > 0 || chosen->aim_wid >= 0) {
        return OFF;
    }
    if (top->combo7_bonus <= chosen->combo7_bonus) {
        return OFF;
    }
    if (top->part_keep - chosen->part_keep < 200) {
        return OFF;
    }
    if (top_value - chosen_value < 3000) {
        return OFF;
    }

    if (g.cards[chosen->card_no].type == CARD_TYPE_TAN && ai_is_card_critical_for_wid(chosen->card_no, target_wid)) {
        material_card_no = chosen->card_no;
    } else if (chosen->taken_card_no >= 0 && g.cards[chosen->taken_card_no].type == CARD_TYPE_TAN &&
               ai_is_card_critical_for_wid(chosen->taken_card_no, target_wid)) {
        material_card_no = chosen->taken_card_no;
    }
    if (material_card_no < 0) {
        return OFF;
    }
    material = &g.cards[material_card_no];
    if (material->type != CARD_TYPE_TAN) {
        return OFF;
    }
    if (ai_would_complete_wid_by_taking_card(player, target_wid, chosen->card_no) ||
        (chosen->taken_card_no >= 0 && ai_would_complete_wid_by_taking_card(player, target_wid, chosen->taken_card_no))) {
        return OFF;
    }
    return ON;
}

static int calc_fallback_take_priority(const DropCandidateScore* candidate)
{
    Card* taken;
    int score = 0;

    if (!candidate || candidate->taken_card_no < 0 || candidate->taken_card_no >= 48) {
        return 0;
    }

    taken = &g.cards[candidate->taken_card_no];
    switch (taken->type) {
        case CARD_TYPE_GOKOU: score += 5000; break;
        case CARD_TYPE_TANE: score += (taken->month == 8 || taken->month == 5 || taken->month == 6 || taken->month == 9) ? 2800 : 1600; break;
        case CARD_TYPE_TAN:
            score += (taken->month == 0 || taken->month == 1 || taken->month == 2 || taken->month == 5 || taken->month == 8 || taken->month == 9) ? 1800 : 900;
            break;
        default: score += 300; break;
    }
    if (candidate->immediate_gain > 0) {
        score += candidate->immediate_gain * 220;
    }
    if (candidate->aim_wid >= 0 && candidate->aim_wid < WINNING_HAND_MAX && ai_is_high_value_wid(candidate->aim_wid)) {
        score += 1800 + winning_hands[candidate->aim_wid].base_score * 120;
    }
    return score;
}

static int build_greedy_fallback_pool(const DropCandidateScore* candidates, int count, int value_mode, int fallback_index, DropCandidateScore* out_pool,
                                      int* out_pool_count)
{
    DropCandidateScore temp[8];
    int order[8];
    int order_count = 0;
    int best_take_pos = -1;
    int best_take_priority = 0;

    if (out_pool_count) {
        *out_pool_count = 0;
    }
    if (!candidates || count <= 0 || !out_pool) {
        return 0;
    }

    vgs_memcpy(temp, candidates, sizeof(DropCandidateScore) * count);
    for (int i = 0; i < count; i++) {
        if (!temp[i].valid) {
            continue;
        }
        if ((temp[i].keep_fatal || temp[i].safety_fatal) && !(drop_candidate_value(&temp[i], value_mode) > 0 && temp[i].taken_card_no >= 0)) {
            temp[i].valid = OFF;
            continue;
        }
        temp[i].keep_fatal = OFF;
        temp[i].safety_fatal = OFF;
        {
            int take_priority = calc_fallback_take_priority(&temp[i]);
            if (take_priority > best_take_priority) {
                best_take_priority = take_priority;
                best_take_pos = i;
            }
        }
    }

    order_count = collect_drop_choice_order_indices(temp, count, value_mode, fallback_index, order, GREEDY_FALLBACK_TOP_K);
    for (int i = 0; i < order_count; i++) {
        int pos = find_drop_candidate_pos_by_index(candidates, count, order[i]);
        if (pos < 0) {
            continue;
        }
        out_pool[i] = candidates[pos];
    }
    if (best_take_pos >= 0 && best_take_priority > 0) {
        int already_in_pool = OFF;
        for (int i = 0; i < order_count; i++) {
            if (out_pool[i].index == candidates[best_take_pos].index) {
                already_in_pool = ON;
                break;
            }
        }
        if (!already_in_pool && order_count < 8) {
            out_pool[order_count++] = candidates[best_take_pos];
        }
    }
    if (out_pool_count) {
        *out_pool_count = order_count;
    }
    return order_count;
}

static int choose_greedy_drop_candidate_from_pool(
    int player, const StrategyData* before, const DropCandidateScore* pool, int pool_count, int value_mode, int* out_chosen_pool_idx)
{
    int greedy_best_pool_idx = -1;
    int greedy_best_score = INT_MIN;
    int top_value_pool_idx = -1;
    int top_value = INT_MIN;
    int best_immediate_gain = 0;

    if (out_chosen_pool_idx) {
        *out_chosen_pool_idx = -1;
    }
    if (!pool || pool_count <= 0) {
        return -1;
    }

    for (int i = 0; i < pool_count; i++) {
        if (pool[i].immediate_gain > best_immediate_gain) {
            best_immediate_gain = pool[i].immediate_gain;
        }
    }

    for (int i = 0; i < pool_count; i++) {
        int score = calc_normal_drop_greedy_score(player, pool[i].index);
        int value = drop_candidate_value(&pool[i], value_mode);
        int best_value = greedy_best_pool_idx >= 0 ? drop_candidate_value(&pool[greedy_best_pool_idx], value_mode) : INT_MIN;
        score += calc_fallback_take_priority(&pool[i]);
        if (should_preserve_sake_cup_in_greedy_fallback(before, &pool[i])) {
            score -= 5000;
        }
        if (top_value_pool_idx < 0 || value > top_value) {
            top_value_pool_idx = i;
            top_value = value;
        }
        if (before && before->koikoi_opp == ON && best_immediate_gain > 0) {
            int current_gain = pool[i].immediate_gain;
            int chosen_gain = greedy_best_pool_idx >= 0 ? pool[greedy_best_pool_idx].immediate_gain : 0;
            if (greedy_best_pool_idx < 0 || current_gain > chosen_gain ||
                (current_gain == chosen_gain && (score > greedy_best_score || (score == greedy_best_score && value > best_value)))) {
                greedy_best_pool_idx = i;
                greedy_best_score = score;
            }
        } else if (greedy_best_pool_idx < 0 || score > greedy_best_score || (score == greedy_best_score && value > best_value)) {
            greedy_best_pool_idx = i;
            greedy_best_score = score;
        }
    }

    if (greedy_best_pool_idx >= 0 && top_value_pool_idx >= 0) {
        int chosen_value = drop_candidate_value(&pool[greedy_best_pool_idx], value_mode);
        if (top_value - chosen_value > GREEDY_FALLBACK_MAX_VALUE_GAP) {
            greedy_best_pool_idx = top_value_pool_idx;
        } else if (before && greedy_best_pool_idx != top_value_pool_idx &&
                   should_keep_top_value_over_tan_domain_material_fallback(
                       player, before, &pool[top_value_pool_idx], &pool[greedy_best_pool_idx], top_value, chosen_value)) {
            greedy_best_pool_idx = top_value_pool_idx;
        } else if (before && greedy_best_pool_idx != top_value_pool_idx && pool[greedy_best_pool_idx].taken_card_no < 0 &&
                   pool[top_value_pool_idx].taken_card_no < 0 && top_value - chosen_value <= 1000 &&
                   g.cards[pool[greedy_best_pool_idx].card_no].type == CARD_TYPE_GOKOU &&
                   pool[greedy_best_pool_idx].part_keep < pool[top_value_pool_idx].part_keep &&
                   drop_action_mc_is_focus_card(before, pool[greedy_best_pool_idx].card_no)) {
            greedy_best_pool_idx = top_value_pool_idx;
        } else if (before && greedy_best_pool_idx != top_value_pool_idx && pool[greedy_best_pool_idx].taken_card_no < 0 &&
                   pool[top_value_pool_idx].taken_card_no < 0 && top_value - chosen_value <= 6000 &&
                   g.cards[pool[greedy_best_pool_idx].card_no].type == CARD_TYPE_GOKOU &&
                   g.cards[pool[top_value_pool_idx].card_no].type != CARD_TYPE_GOKOU) {
            greedy_best_pool_idx = top_value_pool_idx;
        }
    }

    if (out_chosen_pool_idx) {
        *out_chosen_pool_idx = greedy_best_pool_idx;
    }
    return greedy_best_pool_idx >= 0 ? pool[greedy_best_pool_idx].index : -1;
}

static void append_text_local(char* dst, int cap, int* pos, const char* src)
{
    if (!dst || cap <= 0 || !pos || !src) {
        return;
    }
    while (*src && *pos + 1 < cap) {
        dst[*pos] = *src;
        (*pos)++;
        src++;
    }
    dst[*pos] = '\0';
}

static void append_int_local(char* dst, int cap, int* pos, int value)
{
    char num[16];

    if (!dst || cap <= 0 || !pos) {
        return;
    }
    vgs_d32str(num, value);
    append_text_local(dst, cap, pos, num);
}

static void format_drop_fallback_take(char* dst, int cap, int card_no, int taken_card_no)
{
    int pos = 0;

    if (!dst || cap <= 0) {
        return;
    }
    dst[0] = '\0';
    if (taken_card_no < 0) {
        append_text_local(dst, cap, &pos, "n/a");
        return;
    }
    append_int_local(dst, cap, &pos, card_no);
    append_text_local(dst, cap, &pos, ",");
    append_int_local(dst, cap, &pos, taken_card_no);
}

static void format_drop_feed(char* dst, int cap, const DropCandidateScore* candidate)
{
    int pos = 0;

    if (!dst || cap <= 0) {
        return;
    }
    dst[0] = '\0';
    if (!candidate) {
        append_text_local(dst, cap, &pos, "win=OFF opp7=dist4+/th0/c0");
        return;
    }
    if (candidate->opp_immediate_win) {
        append_text_local(dst, cap, &pos, "win=ON(wid=");
        append_text_local(dst, cap, &pos,
                          (candidate->opp_immediate_wid >= 0 && candidate->opp_immediate_wid < WINNING_HAND_MAX) ? winning_hands[candidate->opp_immediate_wid].name
                                                                                                                 : "NONE");
        append_text_local(dst, cap, &pos, ",base=");
        append_int_local(dst, cap, &pos, candidate->opp_immediate_score);
        append_text_local(dst, cap, &pos, ",x2=");
        append_int_local(dst, cap, &pos, candidate->opp_immediate_x2);
        append_text_local(dst, cap, &pos, ")");
    } else {
        append_text_local(dst, cap, &pos, "win=OFF");
    }
    append_text_local(dst, cap, &pos, " opp7=dist");
    if (candidate->opp7_dist >= 4) {
        append_text_local(dst, cap, &pos, "4+");
    } else {
        append_int_local(dst, cap, &pos, candidate->opp7_dist);
    }
    append_text_local(dst, cap, &pos, "/th");
    append_int_local(dst, cap, &pos, candidate->opp7_threat);
    append_text_local(dst, cap, &pos, "/c");
    append_int_local(dst, cap, &pos, candidate->opp7_certain);
}

static void format_drop_feed_metric(char* dst, int cap, int player, int card_no, int taken_card_no)
{
    int pos = 0;
    int feed_flag = 0;
    int feed_month = 0;
    int feed_recap = 0;

    if (!dst || cap <= 0) {
        return;
    }
    calc_drop_top2_feed(player, card_no, taken_card_no, &feed_flag, &feed_month, &feed_recap);
    dst[0] = '\0';
    append_int_local(dst, cap, &pos, feed_flag);
    append_text_local(dst, cap, &pos, "/");
    append_int_local(dst, cap, &pos, feed_month);
    append_text_local(dst, cap, &pos, "/");
    append_int_local(dst, cap, &pos, feed_recap);
}

static const char* trace_aim_type_name(int aim_type)
{
    switch (aim_type) {
        case TRACE_AIM_IMMEDIATE: return "IMMEDIATE";
        case TRACE_AIM_SEVENLINE: return "SEVENLINE";
        case TRACE_AIM_COMBO7: return "COMBO7";
        case TRACE_AIM_MONTHLOCK: return "MONTHLOCK";
        case TRACE_AIM_OVERPAY: return "OVERPAY";
        case TRACE_AIM_DEFENCE7: return "DEFENCE7";
        case TRACE_AIM_KOIKOIWIN: return "KOIKOIWIN";
        default: return "NONE";
    }
}

static int trace_str_eq(const char* lhs, const char* rhs)
{
    if (!lhs || !rhs) {
        return OFF;
    }
    while (*lhs && *rhs) {
        if (*lhs != *rhs) {
            return OFF;
        }
        lhs++;
        rhs++;
    }
    return (*lhs == '\0' && *rhs == '\0') ? ON : OFF;
}

static const char* trace_reason_short(const DropTraceDecision* decision)
{
    if (!decision || !decision->reason_code || !decision->reason_code[0] || trace_str_eq(decision->reason_code, "BASE_COMPARATOR")) {
        return "comparator";
    }
    if (trace_str_eq(decision->reason_code, "GREEDY_FALLBACK")) {
        return "fallback";
    }
    if (trace_str_eq(decision->reason_code, "ACTION_MC_COMPARE") || trace_str_eq(decision->reason_code, "ENV_MC_COMPARE")) {
        return "mc";
    }
    if (decision->d1_changed) {
        return "sevenline";
    }
    if (decision->phase2a_changed) {
        return "phase2a";
    }
    return "override";
}

static void debug_assert_drop_fallback_pool_idx(int top_k, int chosen_pool_idx)
{
    if (!g.auto_play) {
        return;
    }
    if (top_k == 2 && chosen_pool_idx >= 0 && chosen_pool_idx != 0 && chosen_pool_idx != 1) {
        ai_putlog("[drop-fallback-debug] ASSERT top_k=%d chosen_pool_idx=%d", top_k, chosen_pool_idx);
        while (1) {
        }
    }
}

static void log_drop_fallback_debug(int player, int mode, AiPlan plan, const DropCandidateScore* pool, int pool_count, int top_k, int chosen_pool_idx,
                                    int chosen_orig_idx, int final_best_idx_pre_fb, int final_best_idx_post_fb)
{
#if WATCH_DROP_METRICS_ENABLE
    char pool_buf[960];
    const char* mode_short = trace_mode_short(mode);
    const char* plan_short = trace_plan_short(plan);
    int offset = 0;

    if (!g.auto_play) {
        return;
    }

    for (int i = 0; i < pool_count && offset < (int)sizeof(pool_buf); i++) {
        int feed_flag = 0;
        int feed_month = 0;
        int feed_recap = 0;
        char take_buf[32];

        calc_drop_top2_feed(player, pool[i].card_no, pool[i].taken_card_no, &feed_flag, &feed_month, &feed_recap);
        format_drop_fallback_take(take_buf, sizeof(take_buf), pool[i].card_no, pool[i].taken_card_no);
        if (i) {
            append_text_local(pool_buf, sizeof(pool_buf), &offset, " | ");
        }
        append_text_local(pool_buf, sizeof(pool_buf), &offset, "p");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, i);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, ":orig=");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, pool[i].index);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, " card=");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, pool[i].card_no);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, " v=");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, pool[i].value);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, " take=");
        append_text_local(pool_buf, sizeof(pool_buf), &offset, take_buf);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, " keep=");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, pool[i].part_keep);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, " safe=");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, pool[i].part_safe);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, " deny=");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, pool[i].part_deny);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, " feed=");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, feed_flag);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, "/");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, feed_month);
        append_text_local(pool_buf, sizeof(pool_buf), &offset, "/");
        append_int_local(pool_buf, sizeof(pool_buf), &offset, feed_recap);
    }
    pool_buf[(offset >= (int)sizeof(pool_buf)) ? (int)sizeof(pool_buf) - 1 : offset] = '\0';

    debug_assert_drop_fallback_pool_idx(top_k, chosen_pool_idx);

    ai_putlog("[drop-fallback-debug] seed=%u game=%d round=%d turn=%d player=%d mode=%s plan=%s top_k=%d pool_count=%d pool={%s} chosen_pool_idx=%d chosen_orig_idx=%d final_best_idx_pre_fb=%d final_best_idx_post_fb=%d",
              (unsigned int)ai_debug_current_seed(), ai_debug_current_game_index(), g.round + 1, 9 - g.own[player].num, player, mode_short, plan_short, top_k,
              pool_count, pool_buf, chosen_pool_idx, chosen_orig_idx, final_best_idx_pre_fb, final_best_idx_post_fb);
#else
    (void)player;
    (void)mode;
    (void)plan;
    (void)pool;
    (void)pool_count;
    (void)top_k;
    (void)chosen_pool_idx;
    (void)chosen_orig_idx;
    (void)final_best_idx_pre_fb;
    (void)final_best_idx_post_fb;
#endif
}

static int choose_best_drop_candidate(const DropCandidateScore* candidates, int count, int value_mode, int fallback_index)
{
    int best_pick = -1;
    int best_completion_base = -1;
    int best_completion_unrevealed = -1;
    int best_value = INT_MIN;
    int best_risk_sum = INT_MAX;
    int best_risk_min_delay = -1;
    int best_self_min_delay = 99;

    if (!candidates || count <= 0) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        int value;
        int fallback_distance;
        int best_fallback_distance;

        if (!candidates[i].valid || candidates[i].keep_fatal || candidates[i].safety_fatal) {
            continue;
        }
        value = drop_candidate_value(&candidates[i], value_mode);
        fallback_distance = candidates[i].index - fallback_index;
        if (fallback_distance < 0) {
            fallback_distance = -fallback_distance;
        }
        best_fallback_distance = best_pick < 0 ? INT_MAX : candidates[best_pick].index - fallback_index;
        if (best_fallback_distance < 0) {
            best_fallback_distance = -best_fallback_distance;
        }

        if (candidates[i].completion_base > 0) {
            if (best_completion_base < 0 || candidates[i].completion_base > best_completion_base ||
                (candidates[i].completion_base == best_completion_base &&
                 (candidates[i].completion_unrevealed > best_completion_unrevealed ||
                  (candidates[i].completion_unrevealed == best_completion_unrevealed &&
                   (value > best_value ||
                    (value == best_value &&
                     (candidates[i].risk_sum < best_risk_sum ||
                      (candidates[i].risk_sum == best_risk_sum &&
                       (candidates[i].self_min_delay < best_self_min_delay ||
                        (candidates[i].self_min_delay == best_self_min_delay && fallback_distance < best_fallback_distance)))))))))) {
                best_pick = i;
                best_completion_base = candidates[i].completion_base;
                best_completion_unrevealed = candidates[i].completion_unrevealed;
                best_value = value;
                best_risk_sum = candidates[i].risk_sum;
                best_risk_min_delay = candidates[i].risk_min_delay;
                best_self_min_delay = candidates[i].self_min_delay;
            }
            continue;
        }
        if (best_completion_base >= 0) {
            continue;
        }
        if (best_pick < 0 || value > best_value ||
            (value == best_value &&
             (candidates[i].risk_sum < best_risk_sum ||
              (candidates[i].risk_sum == best_risk_sum &&
               (candidates[i].risk_min_delay > best_risk_min_delay ||
                (candidates[i].risk_min_delay == best_risk_min_delay &&
                 (candidates[i].self_min_delay < best_self_min_delay ||
                  (candidates[i].self_min_delay == best_self_min_delay && fallback_distance < best_fallback_distance)))))))) {
            best_pick = i;
            best_value = value;
            best_risk_sum = candidates[i].risk_sum;
            best_risk_min_delay = candidates[i].risk_min_delay;
            best_self_min_delay = candidates[i].self_min_delay;
        }
    }
    return best_pick >= 0 ? candidates[best_pick].index : -1;
}

static int collect_drop_choice_order_indices(const DropCandidateScore* candidates, int count, int value_mode, int fallback_index, int* out_indices, int cap)
{
    DropCandidateScore temp[8];
    int collected = 0;

    if (!candidates || !out_indices || count <= 0 || cap <= 0) {
        return 0;
    }
    vgs_memcpy(temp, candidates, sizeof(DropCandidateScore) * count);
    while (collected < cap) {
        int chosen_index = choose_best_drop_candidate(temp, count, value_mode, fallback_index);
        if (chosen_index < 0) {
            break;
        }
        out_indices[collected++] = chosen_index;
        for (int i = 0; i < count; i++) {
            if (temp[i].index == chosen_index) {
                temp[i].valid = OFF;
                break;
            }
        }
    }
    return collected;
}

static int drop_action_mc_mode_multiplier(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return DROP_ACTION_MC_GREEDY_MULT;
        case MODE_DEFENSIVE: return DROP_ACTION_MC_DEFENSIVE_MULT;
        default: return DROP_ACTION_MC_BALANCED_MULT;
    }
}

static int drop_env_mc_mode_bonus(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return ENV_MC_GREEDY_BONUS;
        case MODE_DEFENSIVE: return ENV_MC_DEFENCE_BONUS;
        default: return ENV_MC_BALANCED_BONUS;
    }
}

static int drop_env_mc_mode_penalty(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return ENV_MC_GREEDY_PENALTY;
        case MODE_DEFENSIVE: return ENV_MC_DEFENCE_PENALTY;
        default: return ENV_MC_BALANCED_PENALTY;
    }
}

static int find_drop_candidate_pos_by_index(const DropCandidateScore* candidates, int count, int index)
{
    if (!candidates || count <= 0) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].index == index) {
            return i;
        }
    }
    return -1;
}

static int candidate_value_gap_by_index(const DropCandidateScore* candidates, int count, int idx_a, int idx_b)
{
    int pos_a = find_drop_candidate_pos_by_index(candidates, count, idx_a);
    int pos_b = find_drop_candidate_pos_by_index(candidates, count, idx_b);

    if (pos_a < 0 || pos_b < 0) {
        return 0;
    }
    return candidates[pos_a].value - candidates[pos_b].value;
}

static int choose_best_drop_candidate_from_sevenline_d1(const DropCandidateScore* candidates, const AiSevenLine* sevenlines, int count, int fallback_index,
                                                        int* out_d1_count)
{
    DropCandidateScore temp[8];
    int d1_count = 0;

    if (out_d1_count) {
        *out_d1_count = 0;
    }
    if (!candidates || !sevenlines || count <= 0) {
        return -1;
    }

    vgs_memcpy(temp, candidates, sizeof(DropCandidateScore) * count);
    for (int i = 0; i < count; i++) {
        if (sevenlines[i].ok && sevenlines[i].delay == 1) {
            d1_count++;
            continue;
        }
        temp[i].valid = OFF;
    }

    if (out_d1_count) {
        *out_d1_count = d1_count;
    }
    return choose_best_drop_candidate(temp, count, DROP_GREEDY_VALUE_NORMAL, fallback_index);
}

static int calc_visible_light_finish_bonus(int player, const StrategyData* before, const StrategyData* after, int immediate_gain, int completion_base,
                                           int* out_wid, int* out_gain, int* out_hand_card_no, int* out_floor_card_no)
{
    AiCombo7Eval eval;
    int best_gain = 0;
    int best_wid = -1;

    if (out_wid) {
        *out_wid = -1;
    }
    if (out_gain) {
        *out_gain = 0;
    }
    if (out_hand_card_no) {
        *out_hand_card_no = -1;
    }
    if (out_floor_card_no) {
        *out_floor_card_no = -1;
    }
    if (player < 0 || player > 1 || !before || !after) {
        return 0;
    }
    if (before->koikoi_opp == ON || before->opponent_win_x2 == ON) {
        return 0;
    }
    if (before->left_own > 3 || before->match_score_diff < 0) {
        return 0;
    }
    if (immediate_gain > 0 || completion_base > 0) {
        return 0;
    }
    ai_eval_combo7(player, g.stats[player].score + completion_base, after, &eval);
    if (eval.combo_reach < 100 || eval.combo_delay > 1 || eval.combo_score_sum < 7) {
        return 0;
    }
    for (int i = 0; i < eval.combo_count; i++) {
        int wid = eval.wids[i];
        int gain;
        if (wid != WID_AME_SHIKOU && wid != WID_SHIKOU && wid != WID_GOKOU) {
            continue;
        }
        gain = winning_hands[wid].base_score;
        if (gain > best_gain) {
            best_gain = gain;
            best_wid = wid;
        }
    }

    if (best_gain <= 0) {
        return 0;
    }
    if (out_wid) {
        *out_wid = best_wid;
    }
    if (out_gain) {
        *out_gain = best_gain;
    }
    return DROP_VISIBLE_LIGHT_FINISH_BONUS;
}

static int calc_visible_sake_followup_bonus(int player, int drop_card_no, const StrategyData* before, const StrategyData* after, int immediate_gain,
                                            int completion_base, int* out_wid)
{
    Card* drop;

    if (out_wid) {
        *out_wid = -1;
    }
    if (player < 0 || player > 1 || drop_card_no < 0 || drop_card_no >= 48 || !before || !after) {
        return 0;
    }
    if (g.koikoi[player] != ON) {
        return 0;
    }
    if (before->koikoi_opp == ON || before->opponent_win_x2 == ON) {
        return 0;
    }
    if (!hard_player_invent_has_card_id(player, 35)) {
        return 0;
    }
    if (immediate_gain > 0 || completion_base > 0) {
        return 0;
    }

    drop = &g.cards[drop_card_no];
    if (drop->type == CARD_TYPE_GOKOU) {
        return 0;
    }

    if (before->match_score_diff > -15) {
        return 0;
    }

    if (drop->month == 7 && drop->type != CARD_TYPE_GOKOU && player_has_hand_card(player, 31)) {
        if (out_wid) {
            *out_wid = WID_TSUKIMI;
        }
        return DROP_VISIBLE_SAKE_FOLLOWUP_BONUS;
    }
    if (drop->month == 2 && drop->type != CARD_TYPE_GOKOU && player_has_hand_card(player, 11)) {
        if (out_wid) {
            *out_wid = WID_HANAMI;
        }
        return DROP_VISIBLE_SAKE_FOLLOWUP_BONUS;
    }
    return 0;
}

static int calc_visible_sake_light_setup_bonus(int player, int drop_card_no, const StrategyData* before, const DropCaptureEval* capture_eval, int* out_wid)
{
    int bonus = 0;
    int wid = -1;

    if (out_wid) {
        *out_wid = -1;
    }
    if (player < 0 || player > 1 || drop_card_no < 0 || drop_card_no >= 48 || !before || !capture_eval || !capture_eval->capture_possible) {
        return 0;
    }
    if (!hard_player_has_sake_cup(player)) {
        return 0;
    }
    if (before->koikoi_opp == ON || before->opponent_win_x2 == ON) {
        return 0;
    }

    if (drop_card_no == 30 && capture_eval->chosen_take_card_no == 31 && !hard_player_invent_has_card_id(player, 31)) {
        wid = WID_TSUKIMI;
    } else if (drop_card_no == 10 && capture_eval->chosen_take_card_no == 11 && !hard_player_invent_has_card_id(player, 11)) {
        wid = WID_HANAMI;
    } else {
        return 0;
    }

    bonus += DROP_VISIBLE_SAKE_LIGHT_SETUP_BONUS;
    bonus += before->reach[wid] * 120;
    if (before->delay[wid] <= 3) {
        bonus += 6000;
    }

    for (int risk_wid = 0; risk_wid < WINNING_HAND_MAX; risk_wid++) {
        if (!is_dangerous_risk_wid(before, risk_wid)) {
            continue;
        }
        if (ai_is_card_critical_for_wid(capture_eval->chosen_take_card_no, risk_wid)) {
            bonus += DROP_VISIBLE_SAKE_LIGHT_DENY_BONUS;
            break;
        }
    }

    if (out_wid) {
        *out_wid = wid;
    }
    return bonus;
}

static int find_forced_visible_sake_setup_drop(int player, const StrategyData* before)
{
    if (player < 0 || player > 1 || !before) {
        return -1;
    }
    if (g.koikoi[player] != ON || before->koikoi_opp == ON || before->opponent_win_x2 == ON) {
        return -1;
    }
    if (before->match_score_diff > -15 || before->left_rounds > 5) {
        return -1;
    }
    if (!hard_player_invent_has_card_id(player, 35)) {
        return -1;
    }
    if (count_floor_month_cards(7) == 0 && player_has_hand_card(player, 31)) {
        for (int i = 0; i < g.own[player].num; i++) {
            Card* card = g.own[player].cards[i];
            if (!card) {
                continue;
            }
            if (card->month == 7 && card->type != CARD_TYPE_GOKOU) {
                return i;
            }
        }
    }
    if (count_floor_month_cards(2) == 0 && player_has_hand_card(player, 11)) {
        for (int i = 0; i < g.own[player].num; i++) {
            Card* card = g.own[player].cards[i];
            if (!card) {
                continue;
            }
            if (card->month == 2 && card->type != CARD_TYPE_GOKOU) {
                return i;
            }
        }
    }
    return -1;
}

static int drop_candidate_has_priority_stop_value(const DropCandidateScore* candidate)
{
    if (!candidate) {
        return OFF;
    }
    return (candidate->immediate_gain >= 7 || candidate->completion_base >= 7) ? ON : OFF;
}

static int should_keep_drop_completion_candidate(int player, const DropCandidateScore* base_candidate, const DropCandidateScore* override_candidate)
{
    if (player < 0 || player > 1 || g.koikoi[player] != ON) {
        return OFF;
    }
    if (!base_candidate || !override_candidate) {
        return OFF;
    }
    if (base_candidate->completion_base <= 0) {
        return OFF;
    }
    if (override_candidate->completion_base < base_candidate->completion_base) {
        return ON;
    }
    if (override_candidate->completion_base == base_candidate->completion_base &&
        override_candidate->completion_unrevealed < base_candidate->completion_unrevealed) {
        return ON;
    }
    return OFF;
}

static int collect_drop_choice_order_indices_from_sevenline_d1(const DropCandidateScore* candidates, const AiSevenLine* sevenlines, int count, int fallback_index,
                                                               int* out_indices, int cap)
{
    DropCandidateScore temp[8];
    int collected = 0;

    if (!candidates || !sevenlines || !out_indices || count <= 0 || cap <= 0) {
        return 0;
    }

    vgs_memcpy(temp, candidates, sizeof(DropCandidateScore) * count);
    for (int i = 0; i < count; i++) {
        if (!(sevenlines[i].ok && sevenlines[i].delay == 1)) {
            temp[i].valid = OFF;
        }
    }

    while (collected < cap) {
        int chosen_index = choose_best_drop_candidate(temp, count, DROP_GREEDY_VALUE_NORMAL, fallback_index);
        if (chosen_index < 0) {
            break;
        }
        out_indices[collected++] = chosen_index;
        for (int i = 0; i < count; i++) {
            if (temp[i].index == chosen_index) {
                temp[i].valid = OFF;
                break;
            }
        }
    }
    return collected;
}

static int drop_action_mc_material_type(int card_no)
{
    Card* card;

    if (card_no < 0 || card_no >= 48) {
        return AI_ACTION_MC_MATERIAL_OTHER_HIGH;
    }
    card = &g.cards[card_no];
    if (card->type == CARD_TYPE_GOKOU) {
        return card->month == 10 ? AI_ACTION_MC_MATERIAL_AME : AI_ACTION_MC_MATERIAL_LIGHT;
    }
    if (card->type == CARD_TYPE_TANE) {
        if (card->month == 8 || card->month == 2 || card->month == 7) {
            return AI_ACTION_MC_MATERIAL_SAKE;
        }
        if (card->month == 5 || card->month == 6) {
            return AI_ACTION_MC_MATERIAL_ISC;
        }
    }
    if (card->type == CARD_TYPE_TAN) {
        if (card->month == 0 || card->month == 1 || card->month == 2) {
            return AI_ACTION_MC_MATERIAL_AKATAN;
        }
        if (card->month == 5 || card->month == 8 || card->month == 9) {
            return AI_ACTION_MC_MATERIAL_AOTAN;
        }
    }
    return AI_ACTION_MC_MATERIAL_OTHER_HIGH;
}

static const char* drop_action_mc_material_name(int material)
{
    switch (material) {
        case AI_ACTION_MC_MATERIAL_LIGHT: return "LIGHT";
        case AI_ACTION_MC_MATERIAL_SAKE: return "SAKE";
        case AI_ACTION_MC_MATERIAL_AKATAN: return "AKATAN";
        case AI_ACTION_MC_MATERIAL_AOTAN: return "AOTAN";
        case AI_ACTION_MC_MATERIAL_ISC: return "ISC";
        case AI_ACTION_MC_MATERIAL_AME: return "AME";
        default: return "OTHER_HIGH";
    }
}

static int collect_drop_action_mc_top_indices(const DropCandidateScore* candidates, int count, int fallback_index, int* out_indices, int cap)
{
    int selected = 0;

    if (!candidates || !out_indices || cap <= 0) {
        return 0;
    }
    for (int pick = 0; pick < cap; pick++) {
        int best_i = -1;
        int best_value = INT_MIN;
        int best_fallback_distance = INT_MAX;
        for (int i = 0; i < count; i++) {
            int already = OFF;
            int value;
            int fallback_distance;
            if (!candidates[i].valid || candidates[i].keep_fatal || candidates[i].safety_fatal) {
                continue;
            }
            for (int j = 0; j < selected; j++) {
                if (out_indices[j] == i) {
                    already = ON;
                    break;
                }
            }
            if (already) {
                continue;
            }
            value = drop_candidate_value(&candidates[i], DROP_GREEDY_VALUE_NORMAL);
            fallback_distance = candidates[i].index - fallback_index;
            if (fallback_distance < 0) {
                fallback_distance = -fallback_distance;
            }
            if (best_i < 0 || value > best_value || (value == best_value && fallback_distance < best_fallback_distance)) {
                best_i = i;
                best_value = value;
                best_fallback_distance = fallback_distance;
            }
        }
        if (best_i < 0) {
            break;
        }
        out_indices[selected++] = best_i;
    }
    return selected;
}

static int drop_action_mc_is_focus_card(const StrategyData* before, int card_no)
{
    Card* card;

    if (!before || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    card = &g.cards[card_no];

    // 光札本体に加え、酒・赤短/青短・猪鹿蝶の高価値素材も対象にする。
    if (!(card->type == CARD_TYPE_GOKOU ||
          (card->type == CARD_TYPE_TANE &&
           (card->month == 5 || card->month == 6 || card->month == 7 || card->month == 8 || card->month == 9)) ||
          (card->type == CARD_TYPE_TAN &&
           (card->month == 0 || card->month == 1 || card->month == 2 || card->month == 5 || card->month == 8 || card->month == 9)))) {
        return OFF;
    }
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_score[i];
        if (wid >= 0 && wid < WINNING_HAND_MAX && ai_is_high_value_wid(wid) && before->reach[wid] > 0 &&
            ai_is_card_related_for_wid(card_no, wid)) {
            return ON;
        }
    }
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_speed[i];
        if (wid >= 0 && wid < WINNING_HAND_MAX && ai_is_high_value_wid(wid) && before->reach[wid] > 0 &&
            ai_is_card_related_for_wid(card_no, wid)) {
            return ON;
        }
    }
    return OFF;
}

static void evaluate_drop_env_mc_compare(
    int player, const StrategyData* before, int mode, DropCandidateScore* candidates, int count, int fallback_index, DropEnvMcCompareResult* out_result)
{
    AiActionMcDropCandidate env_candidates[2];
    int env_E[2];
    int choice_order[2];
    int choice_count;
    int best_pos;
    int second_pos;
    int bonus;
    int penalty;

    if (out_result) {
        vgs_memset(out_result, 0, sizeof(*out_result));
        out_result->before_best_index = -1;
        out_result->before_second_index = -1;
        out_result->before_best_card_no = -1;
        out_result->before_second_card_no = -1;
        out_result->chosen_index = -1;
        out_result->chosen_card_no = -1;
    }
    if (!candidates || count <= 1 || !out_result || !before) {
        return;
    }

    bonus = drop_env_mc_mode_bonus(mode);
    penalty = drop_env_mc_mode_penalty(mode);
    if ((bonus == 0 && penalty == 0) || mode != MODE_GREEDY) {
        return;
    }

    choice_count = collect_drop_choice_order_indices(candidates, count, DROP_GREEDY_VALUE_NORMAL, fallback_index, choice_order, 2);
    if (choice_count < 2) {
        return;
    }

    best_pos = find_drop_candidate_pos_by_index(candidates, count, choice_order[0]);
    second_pos = find_drop_candidate_pos_by_index(candidates, count, choice_order[1]);
    if (best_pos < 0 || second_pos < 0) {
        return;
    }

    out_result->before_best_index = choice_order[0];
    out_result->before_second_index = choice_order[1];
    out_result->before_best_card_no = candidates[best_pos].card_no;
    out_result->before_second_card_no = candidates[second_pos].card_no;
    out_result->before_best_value = candidates[best_pos].value;
    out_result->before_second_value = candidates[second_pos].value;
    out_result->before_gap = out_result->before_best_value - out_result->before_second_value;
    if (out_result->before_gap > ENV_MC_GAP_THRESHOLD) {
        return;
    }

    env_candidates[0].card_no = candidates[best_pos].card_no;
    env_candidates[0].taken_card_no = candidates[best_pos].taken_card_no;
    env_candidates[0].cluster = candidates[best_pos].action_mc_cluster;
    env_candidates[1].card_no = candidates[second_pos].card_no;
    env_candidates[1].taken_card_no = candidates[second_pos].taken_card_no;
    env_candidates[1].cluster = candidates[second_pos].action_mc_cluster;

    ai_debug_note_env_mc_trigger(player, mode);
    ai_eval_drop_env_mc(player, env_candidates, 2, ENV_MC_SAMPLES, env_E);
    for (int i = 0; i < 2; i++) {
        if (env_E[i] > ENV_MC_CLAMP) {
            env_E[i] = ENV_MC_CLAMP;
        } else if (env_E[i] < -ENV_MC_CLAMP) {
            env_E[i] = -ENV_MC_CLAMP;
        }
        ai_debug_note_env_mc_E(player, env_E[i]);
    }

    candidates[best_pos].env_mc_E = env_E[0];
    candidates[second_pos].env_mc_E = env_E[1];
    candidates[best_pos].env_mc_bonus = env_E[0] > 0 ? (env_E[0] * bonus) / ENV_MC_SCALE : (env_E[0] * penalty) / ENV_MC_SCALE;
    candidates[second_pos].env_mc_bonus = env_E[1] > 0 ? (env_E[1] * bonus) / ENV_MC_SCALE : (env_E[1] * penalty) / ENV_MC_SCALE;
    candidates[best_pos].value += candidates[best_pos].env_mc_bonus;
    candidates[second_pos].value += candidates[second_pos].env_mc_bonus;

    out_result->best_E = env_E[0];
    out_result->second_E = env_E[1];
    out_result->compare_triggered = ON;
    out_result->chosen_index = choose_best_drop_candidate(candidates, count, DROP_GREEDY_VALUE_NORMAL, fallback_index);
    out_result->chosen_card_no =
        (out_result->chosen_index >= 0 && out_result->chosen_index < g.own[player].num && g.own[player].cards[out_result->chosen_index])
            ? g.own[player].cards[out_result->chosen_index]->id
            : -1;
    out_result->after_gap = candidate_value_gap_by_index(candidates, count, choice_order[0], choice_order[1]);
    out_result->changed_choice = out_result->chosen_index == out_result->before_second_index ? ON : OFF;
    if (out_result->changed_choice) {
        ai_debug_note_env_mc_changed_choice(player, mode);
    }
}

static void evaluate_drop_action_mc_compare(
    int player, const StrategyData* before, int mode, DropCandidateScore* candidates, int count, int fallback_index, DropActionMcCompareResult* out_result)
{
    AiActionMcDropCandidate mc_candidates[DROP_ACTION_MC_TOP_K];
    int mc_score_diff[DROP_ACTION_MC_TOP_K];
    int choice_order[2];
    int choice_count;
    int gap;
    int focus_materials[2];
    int focus_count = 0;

    if (!candidates || count <= 1) {
        return;
    }
    if (out_result) {
        vgs_memset(out_result, 0, sizeof(*out_result));
        out_result->before_best_index = -1;
        out_result->before_second_index = -1;
        out_result->before_best_card_no = -1;
        out_result->before_second_card_no = -1;
        out_result->chosen_index = -1;
        out_result->chosen_card_no = -1;
        out_result->chosen_material = -1;
        out_result->best_qdiff = 0;
        out_result->second_qdiff = 0;
        out_result->trigger_qdiff = 0;
    }
    choice_count = collect_drop_choice_order_indices(candidates, count, DROP_GREEDY_VALUE_NORMAL, fallback_index, choice_order, 2);
    for (int i = 0; i < count; i++) {
        candidates[i].action_mc_bonus = 0;
        candidates[i].action_mc_score_diff = 0;
    }
    if (!out_result || choice_count < 2) {
        return;
    }
    if (choice_count > 0) {
        out_result->before_best_index = choice_order[0];
        for (int i = 0; i < count; i++) {
            if (candidates[i].index == choice_order[0]) {
                out_result->before_best_value = candidates[i].value;
                out_result->before_best_card_no = candidates[i].card_no;
                break;
            }
        }
    }
    if (choice_count > 1) {
        out_result->before_second_index = choice_order[1];
        for (int i = 0; i < count; i++) {
            if (candidates[i].index == choice_order[1]) {
                out_result->before_second_value = candidates[i].value;
                out_result->before_second_card_no = candidates[i].card_no;
                break;
            }
        }
    }

    if (!before || mode != MODE_GREEDY || before->koikoi_opp == ON || before->opp_coarse_threat >= 80) {
        return;
    }
    gap = out_result->before_best_value - out_result->before_second_value;
    if (gap > DROP_ACTION_MC_GAP_THRESHOLD) {
        return;
    }
    for (int k = 0; k < 2; k++) {
        int candidate_pos = -1;
        for (int i = 0; i < count; i++) {
            if (candidates[i].index == choice_order[k]) {
                candidate_pos = i;
                break;
            }
        }
        if (candidate_pos < 0) {
            return;
        }
        mc_candidates[k].card_no = candidates[candidate_pos].card_no;
        mc_candidates[k].taken_card_no = candidates[candidate_pos].taken_card_no;
        mc_candidates[k].cluster = candidates[candidate_pos].action_mc_cluster;
        if (drop_action_mc_is_focus_card(before, candidates[candidate_pos].card_no)) {
            focus_materials[focus_count++] = drop_action_mc_material_type(candidates[candidate_pos].card_no);
        }
    }
    if (focus_count <= 0) {
        return;
    }

    ai_debug_note_action_mc_trigger(player, 2);
    for (int i = 0; i < focus_count; i++) {
        ai_debug_note_action_mc_material_trigger(player, focus_materials[i]);
    }
    if (drop_action_mc_is_focus_card(before, mc_candidates[0].card_no)) {
        ai_debug_note_action_mc_applied_best(player);
    }
    ai_eval_drop_action_mc(player, mc_candidates, 2, DROP_ACTION_MC_SAMPLES, mc_score_diff);
    out_result->best_qdiff = mc_score_diff[0];
    out_result->second_qdiff = mc_score_diff[1];
    out_result->trigger_qdiff = mc_score_diff[0] < mc_score_diff[1] ? mc_score_diff[0] : mc_score_diff[1];
    ai_debug_note_action_mc_qdiff(player, out_result->trigger_qdiff);
    out_result->compare_triggered = ON;
    if (out_result->trigger_qdiff > DROP_ACTION_MC_TRIGGER_QDIFF) {
        return;
    }
    out_result->compare_active = ON;
    if (mc_score_diff[1] > mc_score_diff[0] + DROP_ACTION_MC_Q_MARGIN) {
        out_result->chosen_index = out_result->before_second_index;
        out_result->chosen_card_no = mc_candidates[1].card_no;
        out_result->chosen_material = drop_action_mc_material_type(mc_candidates[1].card_no);
    } else {
        out_result->chosen_index = out_result->before_best_index;
        out_result->chosen_card_no = mc_candidates[0].card_no;
        out_result->chosen_material = drop_action_mc_material_type(mc_candidates[0].card_no);
    }
}

static int is_want_card_for_player(int player, int card_no)
{
    if (card_no < 0 || card_no >= 48) {
        return OFF;
    }
    for (int i = 0; i < g.stats[player].want.num; i++) {
        Card* want = g.stats[player].want.cards[i];
        if (want && want->id == card_no) {
            return ON;
        }
    }
    return OFF;
}

static int secures_want_card_on_capture(int player, int own_card_no, int taken_card_no)
{
    return is_want_card_for_player(player, own_card_no) || is_want_card_for_player(player, taken_card_no);
}

static int calc_drop_opp_koikoi_win_bonus(int player, int drop_card_no, const DropCaptureEval* capture_eval)
{
    int takeable[48];
    int takeable_count;
    int any_want = OFF;

    if (g.koikoi[1 - player] != ON) {
        return 0;
    }
    if (capture_eval && capture_eval->capture_possible && secures_want_card_on_capture(player, drop_card_no, capture_eval->chosen_take_card_no)) {
        return DROP_OPP_KOIKOI_WIN_BONUS;
    }
    takeable_count = collect_takeable_floor_cards_by_drop(drop_card_no, takeable, 48);
    if (is_want_card_for_player(player, drop_card_no) && takeable_count > 0) {
        any_want = ON;
    }
    for (int i = 0; i < takeable_count; i++) {
        if (is_want_card_for_player(player, takeable[i])) {
            any_want = ON;
            break;
        }
    }
    return any_want ? -DROP_OPP_KOIKOI_MISS_PENALTY : 0;
}

static void simulate_capture_to_target(int player, Card* card, int target_deck, int* taken_card_no)
{
    if (!card || target_deck < 0 || target_deck >= 48) {
        return;
    }
    if (!g.deck.cards[target_deck]) {
        g.deck.cards[target_deck] = card;
        g.deck.num++;
        if (taken_card_no) {
            *taken_card_no = -1;
        }
        return;
    }

    Card* taken = g.deck.cards[target_deck];
    g.invent[player][card->type].cards[g.invent[player][card->type].num++] = card;
    g.invent[player][taken->type].cards[g.invent[player][taken->type].num++] = taken;
    g.deck.cards[target_deck] = NULL;
    if (g.deck.num > 0) {
        g.deck.num--;
    }
    if (taken_card_no) {
        *taken_card_no = taken->id;
    }
}

static int simulate_opponent_normal_turn_loss(int player, int* out_opp_score, int* out_is_seven_plus, int* out_opp_wid)
{
    int opp = 1 - player;
    StrategyData estimate;
    int immediate_win = OFF;
    int best_score = g.stats[opp].score;
    int best_wid = -1;

    ai_think_strategy(player, &estimate);
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int gain;
        int projected;

        if (estimate.risk_reach_estimate[wid] <= 0 || estimate.risk_delay[wid] > 2) {
            continue;
        }
        gain = estimate.risk_score[wid];
        if (gain <= 0 && ai_is_high_value_wid(wid)) {
            gain = winning_hands[wid].base_score;
        }
        projected = g.stats[opp].score + gain;
        if (best_wid < 0 || projected > best_score ||
            (projected == best_score && estimate.risk_delay[wid] < estimate.risk_delay[best_wid]) ||
            (projected == best_score && estimate.risk_delay[wid] == estimate.risk_delay[best_wid] &&
             estimate.risk_reach_estimate[wid] > estimate.risk_reach_estimate[best_wid])) {
            best_score = projected;
            best_wid = wid;
        }
    }

    if (best_wid >= 0 && best_score > g.stats[opp].score) {
        immediate_win = ON;
    }
    if (out_opp_score) {
        *out_opp_score = best_score;
    }
    if (out_is_seven_plus) {
        *out_is_seven_plus = best_score >= 7 ? ON : OFF;
    }
    if (out_opp_wid) {
        *out_opp_wid = best_wid;
    }
    return immediate_win;
}

static const char* strategy_mode_name(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return "GREEDY";
        case MODE_DEFENSIVE: return "DEFENSIVE";
        default: return "BALANCED";
    }
}

static const char* env_domain_name(AiEnvDomain domain)
{
    switch (domain) {
        case AI_ENV_DOMAIN_LIGHT: return "LIGHT";
        case AI_ENV_DOMAIN_SAKE: return "SAKE";
        case AI_ENV_DOMAIN_AKATAN: return "AKATAN";
        case AI_ENV_DOMAIN_AOTAN: return "AOTAN";
        case AI_ENV_DOMAIN_ISC: return "ISC";
        default: return "NONE";
    }
}

static const char* env_plan_name(AiPlan plan)
{
    return plan == AI_PLAN_PRESS ? "PRESS" : "SURVIVE";
}

static const char* env_category_name(int cat)
{
    switch (cat) {
        case ENV_CAT_1: return "CAT1";
        case ENV_CAT_2: return "CAT2";
        case ENV_CAT_3: return "CAT3";
        case ENV_CAT_4: return "CAT4";
        case ENV_CAT_5: return "CAT5";
        case ENV_CAT_6: return "CAT6";
        case ENV_CAT_7: return "CAT7";
        case ENV_CAT_8: return "CAT8";
        case ENV_CAT_9: return "CAT9";
        case ENV_CAT_10: return "CAT10";
        case ENV_CAT_11: return "CAT11";
        case ENV_CAT_12: return "CAT12";
        case ENV_CAT_NA: return "NA";
        default: return "UNKNOWN";
    }
}

static const char* phase2a_reason_name(int reason)
{
    switch (reason) {
        case AI_PHASE2A_REASON_7PLUS: return "7plus";
        case AI_PHASE2A_REASON_COMBO7: return "combo7";
        case AI_PHASE2A_REASON_HOT: return "hot";
        case AI_PHASE2A_REASON_DEFENCE: return "defence";
        default: return "none";
    }
}

static void ai_env_cat_topN(const int cat_sum[ENV_CAT_MAX], int n, int out_idx[], int out_val[])
{
    int used[ENV_CAT_MAX];

    if (!cat_sum || !out_idx || !out_val || n <= 0) {
        return;
    }

    vgs_memset(used, 0, sizeof(used));
    for (int rank = 0; rank < n; rank++) {
        int best_idx = -1;
        int best_abs = -1;
        int best_val = 0;
        for (int cat = 0; cat < ENV_CAT_MAX; cat++) {
            int v;
            int abs_v;
            if (used[cat]) {
                continue;
            }
            v = cat_sum[cat];
            abs_v = v < 0 ? -v : v;
            if (abs_v > best_abs) {
                best_idx = cat;
                best_abs = abs_v;
                best_val = v;
            }
        }
        out_idx[rank] = best_idx;
        out_val[rank] = best_val;
        if (best_idx >= 0) {
            used[best_idx] = ON;
        }
    }
}

static int apply_mult(int base, int mult)
{
    if (mult == 100) {
        return base;
    }
    return (base * mult) / 100;
}

static int plan_mult(const StrategyData* s)
{
    if (!PHASE2A_PLAN_DOMAIN_SCALING_ENABLE || !s) {
        return 100;
    }
    return s->plan == AI_PLAN_PRESS ? PLAN_PRESS_MULT : PLAN_SURVIVE_MULT;
}

static int survive_offence_mult(const StrategyData* s)
{
    int mult = plan_mult(s);

    if (!PHASE2A_PLAN_DOMAIN_SCALING_ENABLE || !s) {
        return 100;
    }
    if (s->plan != AI_PLAN_SURVIVE) {
        return mult;
    }
    return mult < SURVIVE_OFFENCE_FLOOR_MULT ? SURVIVE_OFFENCE_FLOOR_MULT : mult;
}

static int domain_mult(const StrategyData* s)
{
    if (!PHASE2A_PLAN_DOMAIN_SCALING_ENABLE || !s || s->plan != AI_PLAN_PRESS) {
        return 100;
    }
    switch (s->domain) {
        case AI_ENV_DOMAIN_LIGHT: return DOMAIN_LIGHT_MULT;
        case AI_ENV_DOMAIN_SAKE: return DOMAIN_SAKE_MULT;
        case AI_ENV_DOMAIN_AKATAN: return DOMAIN_AKATAN_MULT;
        case AI_ENV_DOMAIN_AOTAN: return DOMAIN_AOTAN_MULT;
        case AI_ENV_DOMAIN_ISC: return DOMAIN_ISC_MULT;
        default: return DOMAIN_NONE_MULT;
    }
}

static int defence_mult(const StrategyData* s)
{
    if (!PHASE2A_PLAN_DOMAIN_SCALING_ENABLE || !s || s->plan != AI_PLAN_SURVIVE) {
        return 100;
    }
    return PLAN_SURVIVE_DEFENCE_MULT;
}

static int phase2a_domain_matches_cat(AiEnvDomain domain, int cat)
{
    switch (domain) {
        case AI_ENV_DOMAIN_LIGHT: return cat == ENV_CAT_1 || cat == ENV_CAT_3 || cat == ENV_CAT_6;
        case AI_ENV_DOMAIN_SAKE: return cat == ENV_CAT_1 || cat == ENV_CAT_2 || cat == ENV_CAT_11 || cat == ENV_CAT_12;
        case AI_ENV_DOMAIN_AKATAN: return cat == ENV_CAT_4;
        case AI_ENV_DOMAIN_AOTAN: return cat == ENV_CAT_5;
        case AI_ENV_DOMAIN_ISC: return cat == ENV_CAT_7;
        default: return OFF;
    }
}

static int calc_domain_hot_bonus(int player, int card_no, const StrategyData* s)
{
    int cat;

    if (!PHASE2A_PLAN_DOMAIN_SCALING_ENABLE || !s || player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return 0;
    }
    if (s->plan != AI_PLAN_PRESS || s->mode != MODE_GREEDY || s->domain == AI_ENV_DOMAIN_NONE) {
        return 0;
    }

    cat = ai_env_effective_category_for_player(player, card_no);
    if (!phase2a_domain_matches_cat(s->domain, cat)) {
        return 0;
    }
    return apply_mult(DROP_DOMAIN_HOT_BONUS, domain_mult(s));
}

static int combo7_mode_multiplier(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return COMBO7_GREEDY_MULT;
        case MODE_DEFENSIVE: return COMBO7_DEFENSIVE_MULT;
        default: return COMBO7_BALANCED_MULT;
    }
}

static int calc_combo7_bonus_raw(int player, int round_score, const StrategyData* s, AiCombo7Eval* out_eval)
{
    AiCombo7Eval eval;
    int bonus;

    if (!s || s->mode == MODE_DEFENSIVE) {
        if (out_eval) {
            vgs_memset(out_eval, 0, sizeof(*out_eval));
            out_eval->combo_delay = 99;
            for (int i = 0; i < 3; i++) {
                out_eval->wids[i] = -1;
            }
        }
        return 0;
    }
    ai_eval_combo7(player, round_score, s, &eval);
    if (out_eval) {
        *out_eval = eval;
    }
    if (eval.value <= 0 || eval.combo_reach < 8 || eval.combo_delay > 6) {
        return 0;
    }
    if (round_score < 4 && s->mode != MODE_GREEDY) {
        return 0;
    }
    if (round_score <= 1 && eval.combo_count >= 3) {
        return 0;
    }
    bonus = eval.value * DROP_COMBO7_WEIGHT;
    bonus = (bonus * combo7_mode_multiplier(s ? s->mode : MODE_BALANCED)) / 100;
    if (s->mode == MODE_GREEDY && s->koikoi_opp == OFF && s->opp_coarse_threat < 60) {
        bonus = (bonus * COMBO7_PRIORITY_PUSH_MULT) / 100;
    }
    return bonus;
}

static int phase2a_should_neutralize_hot_override(AiEnvDomain domain)
{
    return domain == AI_ENV_DOMAIN_AKATAN || domain == AI_ENV_DOMAIN_AOTAN;
}

static int calc_combo7_bonus(int player, int round_score, const StrategyData* s, AiCombo7Eval* out_eval)
{
#if !PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
    return calc_combo7_bonus_raw(player, round_score, s, out_eval);
#else
    int bonus = calc_combo7_bonus_raw(player, round_score, s, out_eval);
    int mult;

    if (bonus <= 0 || !s) {
        return bonus;
    }
    mult = s->plan == AI_PLAN_PRESS ? plan_mult(s) : survive_offence_mult(s);
    return apply_mult(bonus, mult);
#endif
}

static void format_combo7_label(const AiCombo7Eval* eval, char* out)
{
    if (!out) {
        return;
    }
    vgs_strcpy(out, "-");
    if (!eval || eval->combo_count <= 0) {
        return;
    }
    out[0] = '\0';
    for (int i = 0; i < eval->combo_count; i++) {
        int wid = eval->wids[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX) {
            continue;
        }
        if (i > 0) {
            vgs_strcat(out, "+");
        }
        vgs_strcat(out, winning_hands[wid].name);
    }
}

static int is_overpay_aggressive_state(const StrategyData* s)
{
    if (!s) {
        return OFF;
    }
    return (s->mode == MODE_GREEDY || s->match_score_diff < 0 || s->left_rounds <= 2) ? ON : OFF;
}

static int is_overpay_defensive_state(const StrategyData* s)
{
    if (!s) {
        return ON;
    }
    return (s->koikoi_opp == ON || s->opp_coarse_threat >= 60 || s->opponent_win_x2 == ON) ? ON : OFF;
}

static int strategy_overpay_flag(const StrategyData* s, int wid)
{
    if (!s) {
        return OFF;
    }
    switch (wid) {
        case WID_AKATAN: return s->can_overpay_akatan;
        case WID_AOTAN: return s->can_overpay_aotan;
        case WID_ISC: return s->can_overpay_ino;
        default: return OFF;
    }
}

static int calc_overpay_delay_penalty(const StrategyData* s, int player, int month)
{
    int penalty = DROP_OVERPAY_IMMEDIATE_DELAY_PENALTY;

    if (!s || month < 0 || month >= 12) {
        return 0;
    }
    if (s->koikoi_opp == ON || s->opp_coarse_threat >= 60 || s->opponent_win_x2 == ON) {
        return 0;
    }
    if (ai_count_unrevealed_same_month(player, month) > 0) {
        return 0;
    }
    if (s->left_rounds <= 2 || s->match_score_diff < 0) {
        penalty /= 2;
    }
    return penalty;
}

static int is_delayable_five_point_wid(int wid)
{
    switch (wid) {
        case WID_SANKOU:
        case WID_HANAMI:
        case WID_TSUKIMI:
        case WID_ISC:
        case WID_AKATAN:
        case WID_AOTAN:
            return ON;
        default:
            return OFF;
    }
}

static int current_round_turn_for_player(int player)
{
    if (player < 0 || player > 1) {
        return 99;
    }
    return 9 - g.own[player].num;
}

static int count_critical_hand_cards_for_wid(int player, int wid, int exclude_card_no)
{
    int count = 0;

    if (player < 0 || player > 1) {
        return 0;
    }
    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (!card || card->id == exclude_card_no) {
            continue;
        }
        if (ai_is_card_critical_for_wid(card->id, wid)) {
            count++;
        }
    }
    return count;
}

static int is_month_locked_for_player(int player, int month)
{
    int known;
    int owned;

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return OFF;
    }
    known = count_known_month_cards_for_player(player, month);
    owned = count_owned_month_cards(player, month);
    return (known == 3 && owned >= 2) ? ON : OFF;
}

static int count_floor_month_cards(int month)
{
    int count = 0;

    if (month < 0 || month >= 12) {
        return 0;
    }
    for (int i = 0; i < 48; i++) {
        Card* card = g.deck.cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    return count;
}

static int player_has_hand_card(int player, int card_no)
{
    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->id == card_no) {
            return ON;
        }
    }
    return OFF;
}

static int is_hidden_month_lock_key_card_for_player(int player, int card_no)
{
    int month;

    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    if (!player_has_hand_card(player, card_no)) {
        return OFF;
    }
    month = g.cards[card_no].month;
    if (count_known_month_cards_for_player(player, month) != 3) {
        return OFF;
    }
    if (count_owned_month_cards(player, month) != 1) {
        return OFF;
    }
    if (count_floor_month_cards(month) != 2) {
        return OFF;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (ai_is_card_critical_for_wid(card_no, wid)) {
            return ON;
        }
    }
    return OFF;
}

static int is_secured_role_card_for_player(int player, int card_no)
{
    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* card = inv->cards[i];
            if (card && card->id == card_no) {
                return ON;
            }
        }
    }
    return is_month_locked_for_player(player, g.cards[card_no].month) || is_hidden_month_lock_key_card_for_player(player, card_no);
}

static int count_secured_components_for_wid(int player, int wid)
{
    int count = 0;

    switch (wid) {
        case WID_SANKOU:
            count += is_secured_role_card_for_player(player, 3) ? 1 : 0;
            count += is_secured_role_card_for_player(player, 11) ? 1 : 0;
            count += is_secured_role_card_for_player(player, 31) ? 1 : 0;
            count += is_secured_role_card_for_player(player, 47) ? 1 : 0;
            return count;
        case WID_HANAMI:
            return (is_secured_role_card_for_player(player, 11) ? 1 : 0) + (is_secured_role_card_for_player(player, 35) ? 1 : 0);
        case WID_TSUKIMI:
            return (is_secured_role_card_for_player(player, 31) ? 1 : 0) + (is_secured_role_card_for_player(player, 35) ? 1 : 0);
        case WID_ISC:
            return (is_secured_role_card_for_player(player, 23) ? 1 : 0) + (is_secured_role_card_for_player(player, 27) ? 1 : 0) +
                   (is_secured_role_card_for_player(player, 39) ? 1 : 0);
        case WID_AKATAN:
            return (is_secured_role_card_for_player(player, 2) ? 1 : 0) + (is_secured_role_card_for_player(player, 6) ? 1 : 0) +
                   (is_secured_role_card_for_player(player, 10) ? 1 : 0);
        case WID_AOTAN:
            return (is_secured_role_card_for_player(player, 22) ? 1 : 0) + (is_secured_role_card_for_player(player, 34) ? 1 : 0) +
                   (is_secured_role_card_for_player(player, 38) ? 1 : 0);
        default:
            return 0;
    }
}

static int required_component_count_for_wid(int wid)
{
    switch (wid) {
        case WID_SANKOU: return 3;
        case WID_HANAMI:
        case WID_TSUKIMI: return 2;
        case WID_ISC:
        case WID_AKATAN:
        case WID_AOTAN: return 3;
        default: return 0;
    }
}

static int hand_has_finishing_card_for_wid(int player, int wid, int exclude_card_no)
{
    int secured = count_secured_components_for_wid(player, wid);
    int required = required_component_count_for_wid(wid);

    if (secured != required - 1) {
        return OFF;
    }
    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (!card || card->id == exclude_card_no || !ai_is_card_critical_for_wid(card->id, wid)) {
            continue;
        }
        if (!is_secured_role_card_for_player(player, card->id)) {
            return ON;
        }
    }
    return OFF;
}

static int is_excluded_hand_card_finisher_for_wid(int player, int wid, int exclude_card_no)
{
    int secured;
    int required;

    if (player < 0 || player > 1 || exclude_card_no < 0 || exclude_card_no >= 48) {
        return OFF;
    }
    if (!ai_is_card_critical_for_wid(exclude_card_no, wid) || is_secured_role_card_for_player(player, exclude_card_no)) {
        return OFF;
    }

    secured = count_secured_components_for_wid(player, wid);
    required = required_component_count_for_wid(wid);
    return secured == required - 1 ? ON : OFF;
}

static int should_delay_early_five_point_completion(const StrategyData* before, int player, int wid, int exclude_card_no)
{
    int allow_excluded_finisher;

    if (!before || player < 0 || player > 1 || !is_delayable_five_point_wid(wid)) {
        return OFF;
    }
    allow_excluded_finisher = ((wid == WID_HANAMI || wid == WID_TSUKIMI) &&
                               is_excluded_hand_card_finisher_for_wid(player, wid, exclude_card_no))
                                  ? ON
                                  : OFF;
    if (g.koikoi[0] == ON || g.koikoi[1] == ON) {
        return OFF;
    }
    if (current_round_turn_for_player(player) > DROP_EARLY_FIVE_POINT_DELAY_TURN_MAX) {
        return OFF;
    }
    if (count_critical_hand_cards_for_wid(player, wid, exclude_card_no) <= 0 && !allow_excluded_finisher) {
        return OFF;
    }
    return hand_has_finishing_card_for_wid(player, wid, exclude_card_no) || allow_excluded_finisher;
}

static int calc_early_five_point_delay_bonus(int player, int drop_card_no, const StrategyData* before, const DropCaptureEval* capture_eval, int completion_wid)
{
    if (!before || player < 0 || player > 1) {
        return 0;
    }
    (void)drop_card_no;
    (void)capture_eval;
    (void)completion_wid;
    return 0;
}

static int count_known_month_cards_for_player(int player, int month)
{
    int count = 0;

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 4; t++) {
            CardSet* inv = &g.invent[p][t];
            for (int i = 0; i < inv->num; i++) {
                Card* card = inv->cards[i];
                if (card && card->month == month) {
                    count++;
                }
            }
        }
    }
    for (int i = 0; i < 48; i++) {
        Card* floor = g.deck.cards[i];
        if (floor && floor->month == month) {
            count++;
        }
    }
    return count;
}

static int count_owned_month_cards(int player, int month)
{
    int count = 0;

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* card = inv->cards[i];
            if (card && card->month == month) {
                count++;
            }
        }
    }
    return count;
}

static int calc_month_lock_bonus(int player, int month)
{
    int known = count_known_month_cards_for_player(player, month);
    int owned = count_owned_month_cards(player, month);
    int hidden;

    if (known <= 0) {
        return 0;
    }
    hidden = 4 - known;
    if (hidden != 1) {
        return 0;
    }
    if (owned >= 3) {
        return DROP_MONTH_LOCK_BONUS;
    }
    if (owned == 2) {
        return DROP_MONTH_LOCK_BONUS / 2;
    }
    return 0;
}

static int is_unlocked_rainman_state(int player)
{
    int month = 10;
    int known;
    int owned;

    if (player < 0 || player > 1) {
        return OFF;
    }
    known = count_known_month_cards_for_player(player, month);
    owned = count_owned_month_cards(player, month);
    if (known == 3 && owned >= 2) {
        return OFF;
    }
    return ON;
}

static int calc_unlocked_rainman_hold_penalty(int player, int card_no, const StrategyData* before, int capture_possible)
{
    if (!before || player < 0 || player > 1) {
        return 0;
    }
    if (card_no != 43 || capture_possible) {
        return 0;
    }
    if (!is_unlocked_rainman_state(player)) {
        return 0;
    }
    if ((before->reach[WID_AME_SHIKOU] <= 0 || before->delay[WID_AME_SHIKOU] >= 99) &&
        (before->reach[WID_GOKOU] <= 0 || before->delay[WID_GOKOU] >= 99) &&
        (before->risk_reach_estimate[WID_AME_SHIKOU] <= 0 || before->risk_delay[WID_AME_SHIKOU] >= 99) &&
        (before->risk_reach_estimate[WID_GOKOU] <= 0 || before->risk_delay[WID_GOKOU] >= 99)) {
        return 0;
    }
    return 220;
}

static int find_hidden_month_card_no(int player, int month)
{
    int known[48];

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return -1;
    }

    vgs_memset(known, 0, sizeof(known));
    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card) {
            known[card->id] = ON;
        }
    }
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 4; t++) {
            CardSet* inv = &g.invent[p][t];
            for (int i = 0; i < inv->num; i++) {
                Card* card = inv->cards[i];
                if (card) {
                    known[card->id] = ON;
                }
            }
        }
    }
    for (int i = 0; i < 48; i++) {
        Card* floor = g.deck.cards[i];
        if (floor) {
            known[floor->id] = ON;
        }
    }
    for (int i = 0; i < 48; i++) {
        if (!known[i] && g.cards[i].month == month) {
            return i;
        }
    }
    return -1;
}

static int calc_hidden_month_card_bonus(int hidden_card_no, const StrategyData* before, int* out_key_trigger, int* out_deny_trigger)
{
    int bonus = 0;

    if (out_key_trigger) {
        *out_key_trigger = OFF;
    }
    if (out_deny_trigger) {
        *out_deny_trigger = OFF;
    }

    if (hidden_card_no < 0 || hidden_card_no >= 48 || !before) {
        return 0;
    }

    for (int i = 0; i < 3; i++) {
        int wid = before->priority_score[i];
        if (wid >= 0 && wid < WINNING_HAND_MAX && ai_is_card_critical_for_wid(hidden_card_no, wid)) {
            bonus += DROP_MONTH_LOCK_HIDDEN_KEY_BONUS;
            if (out_key_trigger) {
                *out_key_trigger = ON;
            }
            break;
        }
    }
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_speed[i];
        if (wid >= 0 && wid < WINNING_HAND_MAX && ai_is_card_critical_for_wid(hidden_card_no, wid)) {
            bonus += DROP_MONTH_LOCK_HIDDEN_KEY_BONUS / 2;
            if (out_key_trigger) {
                *out_key_trigger = ON;
            }
            break;
        }
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_dangerous_risk_wid(before, wid)) {
            continue;
        }
        if (ai_is_card_critical_for_wid(hidden_card_no, wid)) {
            bonus += DROP_MONTH_LOCK_HIDDEN_DENY_BONUS;
            if (out_deny_trigger) {
                *out_deny_trigger = ON;
            }
            break;
        }
    }
    if (g.cards[hidden_card_no].type == CARD_TYPE_GOKOU || g.cards[hidden_card_no].type == CARD_TYPE_TANE) {
        bonus += DROP_MONTH_LOCK_HIDDEN_KEY_BONUS / 2;
    }
    return bonus;
}

static int calc_dead_month_kasu_release_bonus(int player, int card_no, int capture_possible)
{
    int month;
    int hidden_card_no;
    int opp_high_count = 0;

    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48 || capture_possible) {
        return 0;
    }
    if (g.cards[card_no].type != CARD_TYPE_KASU) {
        return 0;
    }

    month = g.cards[card_no].month;
    if (count_known_month_cards_for_player(player, month) != 3) {
        return 0;
    }

    hidden_card_no = find_hidden_month_card_no(player, month);
    if (hidden_card_no < 0 || g.cards[hidden_card_no].type != CARD_TYPE_KASU) {
        return 0;
    }

    for (int candidate = month * 4; candidate < month * 4 + 4; candidate++) {
        if (candidate == card_no || g.cards[candidate].type == CARD_TYPE_KASU) {
            continue;
        }
        if (card_exists_in_hand(player, candidate)) {
            return 0;
        }
        if (!card_exists_in_invent(1 - player, candidate)) {
            return 0;
        }
        opp_high_count++;
    }

    if (opp_high_count < 2) {
        return 0;
    }

    return 180 + opp_high_count * 60;
}

static int is_overpay_setup_card_for_wid(int card_no, int wid)
{
    Card* card;

    if (card_no < 0 || card_no >= 48) {
        return OFF;
    }
    card = &g.cards[card_no];

    switch (wid) {
        case WID_AKATAN:
        case WID_AOTAN:
            return card->type == CARD_TYPE_TAN ? ON : OFF;
        case WID_ISC:
            return card->type == CARD_TYPE_TANE ? ON : OFF;
        default:
            return OFF;
    }
}

static int calc_normal_drop_greedy_score(int player, int hand_index)
{
    Card* hand_card;
    int targets[48];
    int target_count;
    int best_priority = -1;
    int want_bonus = 0;
    static const int base_capture_priority[4] = {
        400, // CARD_TYPE_GOKOU
        300, // CARD_TYPE_TANE
        200, // CARD_TYPE_TAN
        100  // CARD_TYPE_KASU
    };
    static const int discard_priority_map[4] = {
        0, // CARD_TYPE_GOKOU
        1, // CARD_TYPE_TANE
        2, // CARD_TYPE_TAN
        3  // CARD_TYPE_KASU
    };

    if (hand_index < 0 || hand_index >= 8) {
        return INT_MIN;
    }
    hand_card = g.own[player].cards[hand_index];
    if (!hand_card) {
        return INT_MIN;
    }

    target_count = collect_target_deck_indexes(hand_card, targets);
    for (int i = 0; i < target_count; i++) {
        Card* deck_card = g.deck.cards[targets[i]];
        if (!deck_card) {
            continue;
        }
        int type = deck_card->type;
        int priority = base_capture_priority[type] + g.invent[player][type].num * 32;
        if (should_press_domain_capture(player, deck_card->id, &g.strategy[player])) {
            priority += calc_domain_capture_bonus(player, deck_card->id, &g.strategy[player]);
        } else if (type == CARD_TYPE_GOKOU) {
            priority += 400;
        }
        if (type == CARD_TYPE_TANE && g.invent[player][type].num >= 2) {
            priority += 1000;
        }
        if (type == CARD_TYPE_TAN && g.invent[player][type].num >= 2) {
            priority += 900;
        }
        if (type == CARD_TYPE_KASU && g.invent[player][type].num >= 5) {
            priority += 800;
        }
        for (int wi = 0; wi < g.stats[player].want.num; wi++) {
            Card* want_card = g.stats[player].want.cards[wi];
            if (want_card && want_card == deck_card) {
                priority += 2000;
                want_bonus = 1;
                break;
            }
        }
        if (priority > best_priority) {
            best_priority = priority;
        }
    }

    if (best_priority >= 0) {
        return best_priority + (want_bonus ? 10000 : 0);
    }
    return -discard_priority_map[hand_card->type] * 100 - hand_card->id;
}

static int is_dangerous_risk_wid(const StrategyData* s, int wid)
{
    return s->risk_reach_estimate[wid] > 0 && (s->risk_delay[wid] <= 4 || s->risk_score[wid] > 0);
}

static int calc_self_score_term(const StrategyData* s)
{
    int best = 0;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int ev = s->reach[wid] * s->score[wid];
        if (best < ev) {
            best = ev;
        }
    }
    return best;
}

static int calc_self_speed_term(const StrategyData* s)
{
    int best = 0;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int delay = s->delay[wid];
        if (delay >= 99) {
            continue;
        }
        int sev = (s->reach[wid] * s->score[wid]) / (delay + 1);
        if (best < sev) {
            best = sev;
        }
    }
    return best;
}

static int is_high_likelihood_line(const StrategyData* s, int wid)
{
    if (!s || wid < 0 || wid >= WINNING_HAND_MAX) {
        return OFF;
    }
    return (s->reach[wid] >= 25 && s->delay[wid] <= 3) || (s->reach[wid] >= 40 && s->delay[wid] <= 4) ? ON : OFF;
}

static int clamp_int_local(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static AiSevenLine find_best_seven_line(int player, const StrategyData* s)
{
    AiSevenLine best;
    int round_score = g.stats[player].score;

    vgs_memset(&best, 0, sizeof(best));
    best.wid = -1;
    best.delay = 99;

    if (!s) {
        return best;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int margin;

        if (s->reach[wid] <= 0 || s->score[wid] <= 0 || s->delay[wid] >= 99) {
            continue;
        }
        if (round_score + s->score[wid] < 7) {
            continue;
        }
        margin = round_score + s->score[wid] - 7;
        if (!best.ok || s->delay[wid] < best.delay || (s->delay[wid] == best.delay && margin > best.margin) ||
            (s->delay[wid] == best.delay && margin == best.margin && s->reach[wid] > best.reach) ||
            (s->delay[wid] == best.delay && margin == best.margin && s->reach[wid] == best.reach && s->score[wid] > best.score) ||
            (s->delay[wid] == best.delay && margin == best.margin && s->reach[wid] == best.reach && s->score[wid] == best.score && wid < best.wid)) {
            best.ok = ON;
            best.wid = wid;
            best.delay = s->delay[wid];
            best.reach = s->reach[wid];
            best.margin = margin;
            best.score = s->score[wid];
        }
    }
    return best;
}

static int calc_greedy_seven_plus_bonus_raw(int player, const StrategyData* s, AiSevenLine* out_line, AiSevenLineBonus* out_bonus)
{
    AiSevenLine line = find_best_seven_line(player, s);
    int base_bonus = 0;
    int margin_bonus = 0;
    int bonus = 0;
    int low_reach = OFF;

    if (out_line) {
        *out_line = line;
    }
    if (out_bonus) {
        vgs_memset(out_bonus, 0, sizeof(*out_bonus));
    }
    if (!line.ok) {
        return 0;
    }
    switch (line.delay) {
        case 1: base_bonus = DROP_7LINE_D1_BONUS; break;
        case 2: base_bonus = DROP_7LINE_D2_BONUS; break;
        case 3: base_bonus = DROP_7LINE_D3_BONUS; break;
        default: base_bonus = DROP_7LINE_D4P_BONUS; break;
    }
    if (line.delay <= 3) {
        if (line.margin >= 2) {
            margin_bonus = DROP_7LINE_MARGIN2_BONUS;
        } else if (line.margin == 1) {
            margin_bonus = DROP_7LINE_MARGIN1_BONUS;
        }
    }
    bonus = base_bonus + margin_bonus;
    if (line.delay <= 3 && line.reach < DROP_7LINE_MIN_REACH) {
        bonus = apply_mult(bonus, DROP_7LINE_LOW_REACH_MULT);
        low_reach = ON;
    }
    bonus = clamp_int_local(bonus, 0, DROP_7LINE_BONUS_CLAMP_MAX);
    if (out_bonus) {
        out_bonus->base_bonus = base_bonus;
        out_bonus->margin_bonus = margin_bonus;
        out_bonus->mult = low_reach ? DROP_7LINE_LOW_REACH_MULT : 100;
        out_bonus->bonus = bonus;
        out_bonus->low_reach = low_reach;
    }
    return bonus;
}

static int calc_greedy_seven_plus_bonus_scaled(const StrategyData* s, const AiSevenLine* line, int raw_bonus, AiSevenLineBonus* out_bonus)
{
    int mult = 100;
    int bonus = raw_bonus;
    int press_applied = OFF;

    if (bonus <= 0 || !s) {
        if (out_bonus) {
            out_bonus->mult = 100;
            out_bonus->bonus = bonus;
        }
        return bonus;
    }
    if (!line || line->delay <= 3) {
        mult = s->plan == AI_PLAN_PRESS ? plan_mult(s) : survive_offence_mult(s);
        if (s->plan == AI_PLAN_PRESS) {
            mult = (mult * DROP_7LINE_PRESS_MULT) / 100;
            press_applied = ON;
        }
    }
    bonus = apply_mult(bonus, mult);
    bonus = clamp_int_local(bonus, 0, DROP_7LINE_BONUS_CLAMP_MAX);
    if (out_bonus) {
        out_bonus->mult = (out_bonus->mult * mult) / 100;
        out_bonus->bonus = bonus;
        out_bonus->press_applied = press_applied;
    }
    return bonus;
}

static int calc_greedy_seven_plus_bonus(int player, const StrategyData* s)
{
#if !PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
    return calc_greedy_seven_plus_bonus_raw(player, s, NULL, NULL);
#else
    AiSevenLine line;
    int raw_bonus = calc_greedy_seven_plus_bonus_raw(player, s, &line, NULL);
    return calc_greedy_seven_plus_bonus_scaled(s, &line, raw_bonus, NULL);
#endif
}

static int calc_hot_material_bonus_raw(int card_no, const StrategyData* s)
{
    Card* card;
    int matched_bonus = 0;

    if (!s || card_no < 0 || card_no >= 48) {
        return 0;
    }
    card = &g.cards[card_no];

    if (card->type == CARD_TYPE_GOKOU) {
        if (card->month != 10) {
            if (is_high_likelihood_line(s, WID_GOKOU) || is_high_likelihood_line(s, WID_SHIKOU) || is_high_likelihood_line(s, WID_SANKOU)) {
                matched_bonus = DROP_HOT_MATERIAL_BONUS;
            }
        } else if (is_high_likelihood_line(s, WID_GOKOU) || is_high_likelihood_line(s, WID_AME_SHIKOU)) {
            matched_bonus = DROP_HOT_MATERIAL_BONUS / 2;
        }
    }
    if (card->type == CARD_TYPE_TANE && card->month == 8) {
        if (is_high_likelihood_line(s, WID_HANAMI) || is_high_likelihood_line(s, WID_TSUKIMI) || is_high_likelihood_line(s, WID_ISC)) {
            matched_bonus = DROP_HOT_MATERIAL_BONUS;
        }
    }
    if (card->type == CARD_TYPE_TANE && (card->month == 5 || card->month == 6 || card->month == 9)) {
        if (is_high_likelihood_line(s, WID_ISC)) {
            matched_bonus = DROP_HOT_MATERIAL_BONUS;
        }
    }
    if (card->type == CARD_TYPE_TAN && (card->month == 0 || card->month == 1 || card->month == 2)) {
        if (is_high_likelihood_line(s, WID_AKATAN) || is_high_likelihood_line(s, WID_DBTAN)) {
            matched_bonus = DROP_HOT_MATERIAL_BONUS;
        }
    }
    if (card->type == CARD_TYPE_TAN && (card->month == 5 || card->month == 8 || card->month == 9)) {
        if (is_high_likelihood_line(s, WID_AOTAN) || is_high_likelihood_line(s, WID_DBTAN)) {
            matched_bonus = DROP_HOT_MATERIAL_BONUS;
        }
    }
    return matched_bonus;
}

static int calc_hot_material_bonus_base(int card_no, const StrategyData* s)
{
    int matched_bonus = calc_hot_material_bonus_raw(card_no, s);
    int greedy_extra = 0;

    if (matched_bonus <= 0 || !s) {
        return 0;
    }
    if (s->mode == MODE_GREEDY) {
        greedy_extra = DROP_HOT_MATERIAL_GREEDY_BONUS - DROP_HOT_MATERIAL_BONUS;
        greedy_extra = (greedy_extra * matched_bonus) / DROP_HOT_MATERIAL_BONUS;
    }
    return matched_bonus + greedy_extra;
}

static int calc_hot_material_bonus(int card_no, const StrategyData* s)
{
#if !PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
    return calc_hot_material_bonus_base(card_no, s);
#else
    int matched_bonus = calc_hot_material_bonus_raw(card_no, s);
    int bonus = calc_hot_material_bonus_base(card_no, s);
    int greedy_extra = bonus - matched_bonus;

    if (bonus <= 0 || !s || s->mode != MODE_GREEDY) {
        return bonus;
    }
    return matched_bonus + apply_mult(greedy_extra, s->plan == AI_PLAN_PRESS ? plan_mult(s) : survive_offence_mult(s));
#endif
}

static int calc_opp_deny_term(const StrategyData* before, const StrategyData* after)
{
    int total = 0;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_dangerous_risk_wid(before, wid) && !is_dangerous_risk_wid(after, wid)) {
            continue;
        }
        int delta_score = before->risk_score[wid] - after->risk_score[wid];
        int delta_reach = before->risk_reach_estimate[wid] - after->risk_reach_estimate[wid];
        int delta_delay = after->risk_delay[wid] - before->risk_delay[wid];
        if (before->risk_delay[wid] >= RISK_DELAY_INVALID && after->risk_delay[wid] >= RISK_DELAY_INVALID) {
            delta_delay = 0;
        }
        total += delta_score * 100 + delta_reach * 3 + delta_delay * 35;
    }
    return total;
}

static int normalize_risk_7plus_distance(int distance)
{
    if (distance >= RISK_DELAY_INVALID) {
        return 6;
    }
    if (distance < 0) {
        return 0;
    }
    return distance;
}

static int is_emergency_one_point_wid(int wid)
{
    return wid == WID_TANE || wid == WID_TAN || wid == WID_KASU;
}

static int count_live_emergency_one_point_routes(const StrategyData* s, int* out_best_delay, int* out_total_reach)
{
    int route_count = 0;
    int best_delay = RISK_DELAY_INVALID;
    int total_reach = 0;

    if (!s) {
        if (out_best_delay) {
            *out_best_delay = RISK_DELAY_INVALID;
        }
        if (out_total_reach) {
            *out_total_reach = 0;
        }
        return 0;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_emergency_one_point_wid(wid)) {
            continue;
        }
        if (s->risk_reach_estimate[wid] <= 0 || s->risk_delay[wid] >= RISK_DELAY_INVALID) {
            continue;
        }
        route_count++;
        total_reach += s->risk_reach_estimate[wid];
        if (s->risk_delay[wid] < best_delay) {
            best_delay = s->risk_delay[wid];
        }
    }

    if (out_best_delay) {
        *out_best_delay = route_count > 0 ? best_delay : RISK_DELAY_INVALID;
    }
    if (out_total_reach) {
        *out_total_reach = total_reach;
    }
    return route_count;
}

static int calc_emergency_one_point_block_bonus(int player, const StrategyData* before, const StrategyData* after)
{
    int opp = 1 - player;
    int before_routes;
    int after_routes;
    int before_best_delay;
    int after_best_delay;
    int before_total_reach;
    int after_total_reach;
    int bonus = 0;

    if (!before || !after || player < 0 || player > 1) {
        return 0;
    }
    if (before->koikoi_opp != ON || g.stats[opp].score < 6) {
        return 0;
    }

    before_routes = count_live_emergency_one_point_routes(before, &before_best_delay, &before_total_reach);
    after_routes = count_live_emergency_one_point_routes(after, &after_best_delay, &after_total_reach);

    bonus += (before_routes - after_routes) * DROP_EMERGENCY_ONE_POINT_ROUTE_BONUS;
    bonus += (before_total_reach - after_total_reach) * DROP_EMERGENCY_ONE_POINT_REACH_UNIT;

    if (before_best_delay < RISK_DELAY_INVALID && after_best_delay < RISK_DELAY_INVALID) {
        bonus += (after_best_delay - before_best_delay) * DROP_EMERGENCY_ONE_POINT_DELAY_UNIT;
    }
    if (before_routes > 0 && after_routes == 0) {
        bonus += DROP_EMERGENCY_ONE_POINT_SHUTOUT_BONUS;
    }
    if (after_best_delay <= 1) {
        bonus -= DROP_EMERGENCY_ONE_POINT_STILL_CRITICAL_PENALTY;
    } else if (after_best_delay <= 2) {
        bonus -= DROP_EMERGENCY_ONE_POINT_STILL_NEAR_PENALTY;
    }
    return bonus;
}

static int calc_near_seven_plus_pressure_bonus_raw(const StrategyData* before, const StrategyData* after)
{
    int before_distance;
    int after_distance;
    int threat_delta;
    int bonus = 0;

    if (!before || !after) {
        return 0;
    }
    if (before->koikoi_opp != ON && before->opp_coarse_threat < DROP_2PLY_SEVEN_PLUS_GATE_THREAT &&
        before->risk_7plus_distance > 3) {
        return 0;
    }

    before_distance = normalize_risk_7plus_distance(before->risk_7plus_distance);
    after_distance = normalize_risk_7plus_distance(after->risk_7plus_distance);
    threat_delta = before->opp_coarse_threat - after->opp_coarse_threat;

    bonus += threat_delta * DROP_NEAR_SEVEN_PLUS_THREAT_UNIT;
    bonus += (after_distance - before_distance) * DROP_NEAR_SEVEN_PLUS_DISTANCE_UNIT;

    if (after->risk_7plus_distance <= 1) {
        bonus -= DROP_NEAR_SEVEN_PLUS_CRITICAL_PENALTY;
    } else if (after->risk_7plus_distance <= 2) {
        bonus -= DROP_NEAR_SEVEN_PLUS_NEAR_PENALTY;
    }
    if (after->opp_coarse_threat >= 80) {
        bonus -= DROP_NEAR_SEVEN_PLUS_HIGH_THREAT_PENALTY;
    } else if (after->opp_coarse_threat >= DROP_2PLY_SEVEN_PLUS_GATE_THREAT) {
        bonus -= DROP_NEAR_SEVEN_PLUS_MID_THREAT_PENALTY;
    }
    if (before->koikoi_opp == ON && after->risk_7plus_distance <= 2) {
        bonus -= DROP_NEAR_SEVEN_PLUS_KOIKOI_PENALTY;
    }
    return bonus;
}

static int calc_near_seven_plus_pressure_bonus(const StrategyData* before, const StrategyData* after)
{
#if !PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
    return calc_near_seven_plus_pressure_bonus_raw(before, after);
#else
    int bonus = calc_near_seven_plus_pressure_bonus_raw(before, after);

    if (!before) {
        return bonus;
    }
    return apply_mult(bonus, defence_mult(before));
#endif
}

static int calc_card_direct_risk_penalty(int player, int card_no, const StrategyData* s)
{
    int opp;
    Card* dropped;
    int penalty = 0;

    if (!s || player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return 0;
    }

    opp = 1 - player;
    dropped = &g.cards[card_no];

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int critical;
        int related;
        int role_progress;
        int unit = 0;

        role_progress = (ai_is_fixed_wid(wid) && ai_is_high_value_wid(wid)) ? ai_role_progress_level(opp, wid) : 0;
        if (!is_dangerous_risk_wid(s, wid) && role_progress <= 0) {
            continue;
        }

        critical = ai_is_card_critical_for_wid(card_no, wid);
        related = ai_is_card_related_for_wid(card_no, wid);
        if (!critical && !related) {
            continue;
        }

        if (critical) {
            if (s->risk_delay[wid] <= 2) {
                penalty -= DROP_DANGER_CRITICAL_NEAR_PENALTY;
            } else if (s->risk_delay[wid] <= 4) {
                penalty -= DROP_DANGER_CRITICAL_MID_PENALTY;
            } else {
                penalty -= DROP_DANGER_CRITICAL_FAR_PENALTY;
            }
            unit = DROP_DANGER_SCORE_UNIT;
            if (role_progress > 0 && has_public_same_month_role_followup(player, opp, dropped->month, wid, card_no)) {
                penalty -= DROP_DANGER_VISIBLE_CAPTURE_BONUS;
            }
        } else if (related) {
            if (s->risk_delay[wid] <= 2) {
                penalty -= DROP_DANGER_RELATED_NEAR_PENALTY;
            } else if (s->risk_delay[wid] <= 4) {
                penalty -= DROP_DANGER_RELATED_MID_PENALTY;
            } else {
                penalty -= DROP_DANGER_RELATED_FAR_PENALTY;
            }
            unit = DROP_DANGER_SCORE_UNIT / 2;
        }

        if (s->risk_score[wid] > 0) {
            penalty -= s->risk_score[wid] * unit;
        }
        if (s->risk_reach_estimate[wid] > 0) {
            penalty -= s->risk_reach_estimate[wid] * DROP_DANGER_REACH_UNIT;
        } else if (role_progress > 0 && critical) {
            penalty -= role_progress * DROP_DANGER_REACH_UNIT * 6;
        }
    }
    return penalty;
}

static int is_visible_feed_target_wid(int wid)
{
    switch (wid) {
        case WID_SANKOU:
        case WID_SHIKOU:
        case WID_AME_SHIKOU:
        case WID_GOKOU:
        case WID_AKATAN:
        case WID_AOTAN:
        case WID_ISC:
            return ON;
        default:
            return OFF;
    }
}

static int has_public_same_month_role_followup(int player, int opp, int month, int wid, int exclude_card_no)
{
    if (player < 0 || player > 1 || opp < 0 || opp > 1 || month < 0 || month >= 12) {
        return OFF;
    }

    for (int card_no = 0; card_no < 48; card_no++) {
        if (card_no == exclude_card_no || !ai_is_card_critical_for_wid(card_no, wid)) {
            continue;
        }
        if (g.cards[card_no].month != month) {
            continue;
        }
        if (card_exists_in_invent(player, card_no) || card_exists_in_invent(opp, card_no)) {
            continue;
        }
        return ON;
    }
    return OFF;
}

static int calc_visible_opp_role_feed_penalty(int player, int drop_card_no, const StrategyData* before)
{
    int opp;
    int best_penalty = 0;
    Card* dropped;

    if (!before || player < 0 || player > 1 || drop_card_no < 0 || drop_card_no >= 48) {
        return 0;
    }
    if (before->left_own > 6 || before->left_own < 2) {
        return 0;
    }

    opp = 1 - player;
    dropped = &g.cards[drop_card_no];

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int progress;

        if (!is_visible_feed_target_wid(wid)) {
            continue;
        }
        progress = ai_role_progress_level(opp, wid);
        if (progress <= 0 && before->risk_reach_estimate[wid] <= 0) {
            continue;
        }
        if (!ai_is_card_critical_for_wid(drop_card_no, wid) && !ai_is_card_related_for_wid(drop_card_no, wid)) {
            continue;
        }

        if (!has_public_same_month_role_followup(player, opp, dropped->month, wid, drop_card_no)) {
            continue;
        }

        {
            int penalty = DROP_VISIBLE_ROLE_FEED_BASE_PENALTY;
            penalty += winning_hands[wid].base_score * 10;
            penalty += progress * DROP_VISIBLE_ROLE_FEED_PROGRESS_UNIT;
            penalty += before->risk_reach_estimate[wid] * DROP_VISIBLE_ROLE_FEED_REACH_UNIT;
            penalty += before->risk_score[wid] * DROP_VISIBLE_ROLE_FEED_SCORE_UNIT;
            if (ai_is_card_critical_for_wid(drop_card_no, wid)) {
                penalty += DROP_VISIBLE_ROLE_FEED_CRITICAL_BONUS;
            } else {
                penalty += DROP_VISIBLE_ROLE_FEED_RELATED_BONUS;
            }
            if (before->risk_delay[wid] <= 2 || progress >= 2) {
                penalty += DROP_VISIBLE_ROLE_FEED_FAST_BONUS;
            }
            if (best_penalty < penalty) {
                best_penalty = penalty;
            }
        }
    }

    return -best_penalty;
}

static int calc_safety_term(int player, int card, const StrategyData* before)
{
    return calc_visible_opp_role_feed_penalty(player, card, before); // (A) safe は visible feed を主に扱い、危険札本体は別項で積む
}

static int calc_drop_card_direct_block_bonus(int card_no, const StrategyData* before)
{
    int bonus = 0;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_dangerous_risk_wid(before, wid)) {
            continue;
        }
        if (ai_is_card_critical_for_wid(card_no, wid)) {
            bonus += before->risk_delay[wid] <= 2 ? -300 : -150; // (A) 危険必須札を場に出す罰
        } else if (ai_is_card_related_for_wid(card_no, wid)) {
            bonus += before->risk_delay[wid] <= 2 ? -80 : -40;
        }
    }
    return bonus;
}

static int is_small_lead_danger_state(const StrategyData* s)
{
    if (!s) {
        return OFF;
    }
    return (s->match_score_diff >= 0 && s->match_score_diff <= 6) ? ON : OFF;
}

static int card_exists_in_hand(int player, int card_no)
{
    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    for (int i = 0; i < 8; i++) {
        Card* hand = g.own[player].cards[i];
        if (hand && hand->id == card_no) {
            return ON;
        }
    }
    return OFF;
}

static int card_exists_in_invent(int player, int card_no)
{
    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* owned = inv->cards[i];
            if (owned && owned->id == card_no) {
                return ON;
            }
        }
    }
    return OFF;
}

static int card_exists_on_floor(int card_no)
{
    if (card_no < 0 || card_no >= 48) {
        return OFF;
    }
    for (int i = 0; i < 48; i++) {
        Card* floor = g.deck.cards[i];
        if (floor && floor->id == card_no) {
            return ON;
        }
    }
    return OFF;
}

static int calc_access_keep_penalty_for_wid(int player, int card, int wid, const StrategyData* before)
{
    int missing_count;
    int month;
    int unrevealed_same_month;
    int owned_same_month;
    int hidden_missing = OFF;
    int missing_on_opp_invent = OFF;
    int missing_on_self_hand = OFF;
    int last_access = OFF;
    int trigger = OFF;
    int strong_state = OFF;
    int penalty = 0;
    int is_high = OFF;
    int missing_same_month = OFF;

    if (!before || player < 0 || player > 1 || card < 0 || card >= 48) {
        return 0;
    }

    switch (wid) {
        case WID_ISC:
        case WID_AKATAN:
        case WID_AOTAN:
            is_high = OFF;
            break;
        case WID_SANKOU:
        case WID_SHIKOU:
        case WID_AME_SHIKOU:
        case WID_GOKOU:
        case WID_HANAMI:
        case WID_TSUKIMI:
            is_high = ON;
            break;
        default: return 0;
    }

    missing_count = ai_wid_missing_count(player, wid);
    if (missing_count != 1) {
        return 0;
    }

    month = g.cards[card].month;
    for (int candidate = 0; candidate < 48; candidate++) {
        if (!ai_is_card_critical_for_wid(candidate, wid) || g.cards[candidate].month != month) {
            continue;
        }
        if (card_exists_in_invent(player, candidate)) {
            continue;
        }
        missing_same_month = ON;
        if (card_exists_in_invent(1 - player, candidate)) {
            missing_on_opp_invent = ON;
        } else if (card_exists_in_hand(player, candidate)) {
            missing_on_self_hand = ON;
        } else if (!card_exists_on_floor(candidate)) {
            hidden_missing = ON;
        }
        break;
    }
    if (!missing_same_month) {
        return 0;
    }
    if (missing_on_opp_invent) {
        if (!(is_high && before->koikoi_opp == ON && before->opponent_win_x2 == ON)) {
            return 0;
        }
        penalty += 120;
    }
    if (missing_on_self_hand) {
        return 0;
    }

    unrevealed_same_month = ai_count_unrevealed_same_month(player, month);
    owned_same_month = count_owned_month_cards(player, month);
    last_access = owned_same_month == 1 ? ON : OFF;
    trigger = hidden_missing || unrevealed_same_month > 0 || last_access;
    if (!trigger) {
        return 0;
    }

    strong_state = (before->koikoi_opp == ON || before->opponent_win_x2 == ON || before->opp_coarse_threat >= 80 || before->left_rounds <= 2 ||
                    is_small_lead_danger_state(before))
                       ? ON
                       : OFF;

    if (wid == WID_HANAMI || wid == WID_TSUKIMI) {
        if (!(hidden_missing && last_access)) {
            is_high = OFF;
        }
    }

    penalty += is_high ? 320 : 220;
    if (hidden_missing) {
        penalty += is_high ? 120 : 80;
    }
    if (unrevealed_same_month > 0) {
        penalty += is_high ? 90 : 60;
    }
    if (last_access) {
        penalty += is_high ? 160 : 100;
    }
    if (strong_state) {
        penalty += is_high ? 180 : 120;
    }
    return penalty;
}

static int calc_drop_keep_penalty_for_wid(int player, int card, int wid, const StrategyData* before)
{
    int critical = ai_is_card_critical_for_wid(card, wid);
    int related = ai_is_card_related_for_wid(card, wid);
    int is_threshold = (wid == WID_KASU || wid == WID_TAN || wid == WID_TANE);
    int access_penalty = calc_access_keep_penalty_for_wid(player, card, wid, before);

    if (!critical && !related) {
        return access_penalty;
    }

    if (is_threshold) {
        int penalty = critical ? 60 : 40; // (B) 加点役カテゴリ札は軽い caution
        if (before->delay[wid] <= 2 || before->reach[wid] >= 50) {
            penalty += 20;
        }
        return penalty > access_penalty ? penalty : access_penalty;
    }
    if (critical) {
        if (ai_is_high_value_wid(wid) && ai_would_complete_wid_by_taking_card(player, wid, card)) {
            return 1200 > access_penalty ? 1200 : access_penalty; // 手札に保持しているだけで完成札のケースは捨て禁止。
        }
        if (before->delay[wid] <= 1 && before->reach[wid] >= 35 && before->score[wid] >= 5) {
            return 800 > access_penalty ? 800 : access_penalty;
        }
        return (before->delay[wid] <= 2 ? 400 : 320) > access_penalty ? (before->delay[wid] <= 2 ? 400 : 320) : access_penalty;
    }
    if (before->delay[wid] > 3 && before->reach[wid] < 20) {
        return 120 > access_penalty ? 120 : access_penalty;
    }
    return 180 > access_penalty ? 180 : access_penalty;
}

static int clamp_keep_term(int raw_keep_term)
{
    if (raw_keep_term < KEEP_CLAMP_FATAL_MIN) {
        return KEEP_CLAMP_FATAL_MIN;
    }
    if (raw_keep_term < KEEP_CLAMP_MIN) {
        return KEEP_CLAMP_MIN;
    }
    return raw_keep_term;
}

static int calc_drop_keep_term(int player, int card, const StrategyData* before)
{
    int penalty = 0;
    int seen[WINNING_HAND_MAX];
    vgs_memset(seen, 0, sizeof(seen));

    for (int i = 0; i < 3; i++) {
        int wid = before->priority_speed[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX || seen[wid]) {
            continue;
        }
        seen[wid] = ON;
        int next = calc_drop_keep_penalty_for_wid(player, card, wid, before);
        if (penalty < next) {
            penalty = next;
        }
    }

    for (int i = 0; i < 3; i++) {
        int wid = before->priority_score[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX || seen[wid]) {
            continue;
        }
        seen[wid] = ON;
        int next = calc_drop_keep_penalty_for_wid(player, card, wid, before);
        if (penalty < next) {
            penalty = next;
        }
    }

    return clamp_keep_term(-penalty); // (B) keep は最悪候補の除外寄りに使う
}

static int calc_drop_no_take_gokou_penalty(int player, int card_no, const StrategyData* before, const StrategyData* after, int capture_possible)
{
    Card* card;
    int penalty = 0;
    int env_loss = 0;

    if (!before || !after || player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return 0;
    }
    if (capture_possible || before->left_own <= 1) {
        return 0;
    }

    card = &g.cards[card_no];
    if (card->type != CARD_TYPE_GOKOU) {
        return 0;
    }

    penalty += 6000;
    if (is_high_likelihood_line(before, WID_GOKOU) || is_high_likelihood_line(before, WID_SHIKOU) || is_high_likelihood_line(before, WID_AME_SHIKOU) ||
        is_high_likelihood_line(before, WID_SANKOU) || is_high_likelihood_line(after, WID_GOKOU) || is_high_likelihood_line(after, WID_SHIKOU) ||
        is_high_likelihood_line(after, WID_AME_SHIKOU) || is_high_likelihood_line(after, WID_SANKOU)) {
        penalty += 4000;
    }
    if (before->domain == AI_ENV_DOMAIN_LIGHT || after->domain == AI_ENV_DOMAIN_LIGHT) {
        penalty += before->plan == AI_PLAN_PRESS ? 4000 : 2500;
    }
    env_loss = before->env_total - after->env_total;
    if (env_loss > 0) {
        penalty += clamp_int_local(env_loss * 4, 0, 6000);
    }
    if (before->match_score_diff < 0 && before->left_rounds <= 2) {
        penalty += 1500;
    }
    return penalty;
}

static int sum_risk_score(const StrategyData* s)
{
    int total = 0;
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        total += s->risk_score[i];
    }
    return total;
}

static int min_valid_delay(const int arr[], const int reach_or_flag[], int n)
{
    int best = 99;
    for (int i = 0; i < n; i++) {
        if (arr[i] >= 99) {
            continue;
        }
        if (reach_or_flag && reach_or_flag[i] <= 0) {
            continue;
        }
        if (arr[i] < best) {
            best = arr[i];
        }
    }
    return best;
}

static int threshold_gain_bonus(int before, int after, int threshold)
{
    if (before < threshold && after >= threshold) {
        return 60;
    }
    if (before <= threshold - 2 && after == threshold - 1) {
        return 35;
    }
    if (before <= threshold - 3 && after == threshold - 2) {
        return 20;
    }
    return 0;
}

static int pre_completion_progress_bonus(int before, int after, int threshold, int unit)
{
    int capped_before;
    int capped_after;

    if (unit <= 0) {
        return 0;
    }
    capped_before = before >= threshold ? threshold - 1 : before;
    capped_after = after >= threshold ? threshold - 1 : after;
    if (capped_after <= capped_before) {
        return 0;
    }
    return (capped_after - capped_before) * unit;
}

static int calc_plain_role_progress_bonus_for_capture(int player, int first_card_no, int second_card_no)
{
    int before_tane;
    int before_tan;
    int before_kasu;
    int after_tane;
    int after_tan;
    int after_kasu;
    int bonus = 0;

    if (player < 0 || player > 1) {
        return 0;
    }

    before_tane = count_owned_type(player, CARD_TYPE_TANE);
    before_tan = count_owned_type(player, CARD_TYPE_TAN);
    before_kasu = count_owned_kasu_total(player);

    after_tane = before_tane;
    after_tan = before_tan;
    after_kasu = before_kasu;

    if (first_card_no >= 0 && first_card_no < 48) {
        Card* first = &g.cards[first_card_no];
        if (first->type == CARD_TYPE_TANE) {
            after_tane++;
        }
        if (first->type == CARD_TYPE_TAN) {
            after_tan++;
        }
        if (first->type == CARD_TYPE_KASU || (first->type == CARD_TYPE_TANE && first->month == 8)) {
            after_kasu++;
        }
    }
    if (second_card_no >= 0 && second_card_no < 48) {
        Card* second = &g.cards[second_card_no];
        if (second->type == CARD_TYPE_TANE) {
            after_tane++;
        }
        if (second->type == CARD_TYPE_TAN) {
            after_tan++;
        }
        if (second->type == CARD_TYPE_KASU || (second->type == CARD_TYPE_TANE && second->month == 8)) {
            after_kasu++;
        }
    }

    bonus += pre_completion_progress_bonus(before_tane, after_tane, 5, 120);
    bonus += pre_completion_progress_bonus(before_tan, after_tan, 5, 120);
    bonus += pre_completion_progress_bonus(before_kasu, after_kasu, 10, 60);
    return bonus;
}

static int is_three_card_five_point_wid(int wid)
{
    switch (wid) {
        case WID_AKATAN:
        case WID_AOTAN:
            return ON;
        default:
            return OFF;
    }
}

static int calc_drop_three_card_five_point_setup_bonus(int player, int first_card_no, int second_card_no, const StrategyData* before)
{
    int best_bonus = 0;

    if (!before || player < 0 || player > 1) {
        return 0;
    }
    if (before->mode == MODE_DEFENSIVE || before->mode == MODE_BALANCED) {
        return 0;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int after_missing;
        int bonus = 0;

        if (!is_three_card_five_point_wid(wid)) {
            continue;
        }
        if (before->score[wid] < 5 || before->reach[wid] <= 0 || before->delay[wid] >= 99) {
            continue;
        }
        if (!ai_is_card_critical_for_wid(first_card_no, wid) && !ai_is_card_critical_for_wid(second_card_no, wid)) {
            continue;
        }

        // simulate_drop_capture() 後の inventory 状態を見て、何枚目の確保になったかを評価する。
        after_missing = ai_wid_missing_count(player, wid);
        if (after_missing <= 0 || after_missing > 2) {
            continue;
        }

        if (after_missing == 1) {
            bonus = DROP_THREE_CARD_FIVE_POINT_REACH_SETUP_BONUS + before->reach[wid] * DROP_THREE_CARD_FIVE_POINT_REACH_REACH_UNIT;
            if (before->delay[wid] <= 3) {
                bonus += DROP_THREE_CARD_FIVE_POINT_FAST_DELAY_BONUS;
            }
        } else {
            bonus = DROP_THREE_CARD_FIVE_POINT_FIRST_SETUP_BONUS + before->reach[wid] * DROP_THREE_CARD_FIVE_POINT_FIRST_REACH_UNIT;
            if (before->delay[wid] <= 4) {
                bonus += DROP_THREE_CARD_FIVE_POINT_FAST_DELAY_BONUS / 2;
            }
        }

        if (best_bonus < bonus) {
            best_bonus = bonus;
        }
    }

    return best_bonus;
}

static int count_owned_type(int player, int type)
{
    if (player < 0 || player > 1 || type < CARD_TYPE_GOKOU || type > CARD_TYPE_KASU) {
        return 0;
    }
    return g.invent[player][type].num;
}

static int count_owned_kasu_total(int player)
{
    int total = count_owned_type(player, CARD_TYPE_KASU);
    CardSet* tane = &g.invent[player][CARD_TYPE_TANE];

    for (int i = 0; i < tane->num; i++) {
        Card* card = tane->cards[i];
        if (card && card->month == 8) {
            total++;
        }
    }
    return total;
}

static int hand_has_same_month_card_type_except(int player, int month, int type, int exclude_card_no)
{
    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return OFF;
    }

    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (!card || card->id == exclude_card_no) {
            continue;
        }
        if (card->month == month && card->type == type) {
            return ON;
        }
    }
    return OFF;
}

static int calc_sake_cup_capture_material_bonus(int player, int drop_card_no, int taken_card_no, const StrategyData* before)
{
    Card* drop;
    int before_tan;
    int bonus = 0;

    if (player < 0 || player > 1 || !before || drop_card_no < 0 || drop_card_no >= 48 || taken_card_no != 35) {
        return 0;
    }

    drop = &g.cards[drop_card_no];
    if (drop->month != 8 || (drop->type != CARD_TYPE_TAN && drop->type != CARD_TYPE_KASU)) {
        return 0;
    }

    // 盃を 9 月の短冊/カスどちらでも拾える可視 2 択だけを補正する。
    if (drop->type == CARD_TYPE_TAN) {
        if (!hand_has_same_month_card_type_except(player, drop->month, CARD_TYPE_KASU, drop_card_no)) {
            return 0;
        }
    } else {
        if (!hand_has_same_month_card_type_except(player, drop->month, CARD_TYPE_TAN, drop_card_no)) {
            return 0;
        }
    }

    before_tan = count_owned_type(player, CARD_TYPE_TAN);
    if (drop->type == CARD_TYPE_TAN) {
        if (before_tan == 4) {
            bonus += 220000;
        } else if (before_tan == 3) {
            bonus += 70000;
        }
        if (before->reach[WID_AOTAN] > 0 && ai_is_card_critical_for_wid(drop_card_no, WID_AOTAN)) {
            bonus += 18000 + before->reach[WID_AOTAN] * 120;
            if (before->delay[WID_AOTAN] <= 4) {
                bonus += 12000;
            }
        }
        if (before->reach[WID_TAN] > 0) {
            bonus += before->reach[WID_TAN] * 80;
        }
    } else {
        if (before_tan == 4) {
            bonus -= 220000;
        } else if (before_tan == 3) {
            bonus -= 70000;
        }
        if (before->reach[WID_AOTAN] > 0 && hand_has_same_month_card_type_except(player, drop->month, CARD_TYPE_TAN, drop_card_no)) {
            bonus -= 12000;
        }
    }

    return bonus;
}

static int count_owned_month_cards_excluding_card(int player, int month, int exclude_card_no)
{
    int count = 0;

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->id != exclude_card_no && card->month == month) {
            count++;
        }
    }
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* card = inv->cards[i];
            if (card && card->id != exclude_card_no && card->month == month) {
                count++;
            }
        }
    }
    return count;
}

static int calc_public_access_secure_bonus(int player, int drop_card_no, const StrategyData* before)
{
    int best_bonus = 0;
    int month;
    int known_same_month;
    int floor_same_month;
    int owned_same_month_without_drop;
    int already_secured;

    if (player < 0 || player > 1 || drop_card_no < 0 || drop_card_no >= 48 || !before) {
        return 0;
    }
    month = g.cards[drop_card_no].month;
    known_same_month = count_known_month_cards_for_player(player, month);
    floor_same_month = count_floor_month_cards(month);
    owned_same_month_without_drop = count_owned_month_cards_excluding_card(player, month, drop_card_no);
    already_secured = is_secured_role_card_for_player(player, drop_card_no);

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int bonus = 0;

        if (!ai_is_card_critical_for_wid(drop_card_no, wid)) {
            continue;
        }

        switch (wid) {
            case WID_HANAMI:
            case WID_TSUKIMI:
                break;
            default:
                continue;
        }

        if (already_secured) {
            bonus -= DROP_PUBLIC_ACCESS_SECURE_REDUNDANT_PENALTY;
        } else {
            bonus += DROP_PUBLIC_ACCESS_SECURE_HIGH_BONUS;
            if (floor_same_month <= 1) {
                bonus += DROP_PUBLIC_ACCESS_SECURE_LAST_VISIBLE_BONUS;
            }
            if (known_same_month < 3) {
                bonus += DROP_PUBLIC_ACCESS_SECURE_NO_LOCK_BONUS;
            }
            if (owned_same_month_without_drop == 0) {
                bonus += DROP_PUBLIC_ACCESS_SECURE_NO_BACKUP_BONUS;
            }
            if (floor_same_month <= 1 && known_same_month < 3 && owned_same_month_without_drop == 0) {
                bonus += DROP_PUBLIC_ACCESS_SECURE_FRAGILE_ROUTE_BONUS;
            }
            if (before->delay[wid] <= 3) {
                bonus += 500;
            }
            if (before->reach[wid] >= 35) {
                bonus += 700;
            }
        }

        if (best_bonus < bonus) {
            best_bonus = bonus;
        }
    }

    return best_bonus;
}

static int calc_capture_quality(int player, int drop_card_no, int taken_card_no, const StrategyData* before)
{
    Card* taken;
    int quality;
    int seen[WINNING_HAND_MAX];

    if (player < 0 || player > 1 || taken_card_no < 0 || taken_card_no >= 48 || !before) {
        return 0;
    }

    taken = &g.cards[taken_card_no];
    quality = 0;

    switch (taken->type) {
        case CARD_TYPE_GOKOU: quality += 520; break;
        case CARD_TYPE_TANE: quality += 340; break;
        case CARD_TYPE_TAN: quality += 220; break;
        case CARD_TYPE_KASU: quality += 100; break;
        default: break;
    }

    if (taken->type == CARD_TYPE_TANE && taken->month == 8) {
        quality += 180; // 盃は花見酒/月見酒/カスの起点になりやすい。
    }
    if (hard_player_has_sake_cup(player)) {
        if (taken->id == 31 && !hard_player_invent_has_card_id(player, 31) && player_has_hand_card(player, 30)) {
            quality += DROP_VISIBLE_SAKE_LIGHT_SETUP_BONUS / 4;
            for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
                if (is_dangerous_risk_wid(before, wid) && ai_is_card_critical_for_wid(31, wid)) {
                    quality += DROP_VISIBLE_SAKE_LIGHT_DENY_BONUS / 4;
                    break;
                }
            }
        } else if (taken->id == 11 && !hard_player_invent_has_card_id(player, 11) && player_has_hand_card(player, 10)) {
            quality += DROP_VISIBLE_SAKE_LIGHT_SETUP_BONUS / 4;
            for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
                if (is_dangerous_risk_wid(before, wid) && ai_is_card_critical_for_wid(11, wid)) {
                    quality += DROP_VISIBLE_SAKE_LIGHT_DENY_BONUS / 4;
                    break;
                }
            }
        }
    }
    if (should_press_domain_capture(player, taken_card_no, before)) {
        quality += calc_domain_capture_bonus(player, taken_card_no, before);
    } else if (taken->type == CARD_TYPE_GOKOU) {
        quality += 180;
    }
    if (taken->type == CARD_TYPE_TANE && (taken->month == 5 || taken->month == 6 || taken->month == 9)) {
        quality += 140; // 猪鹿蝶の素材を優遇。
    }
    quality += calc_exposed_domain_keycard_capture_bonus(player, taken_card_no, before);

    vgs_memset(seen, 0, sizeof(seen));
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_speed[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX || seen[wid]) {
            continue;
        }
        seen[wid] = ON;
        if (ai_would_complete_wid_by_taking_card(player, wid, taken_card_no)) {
            quality += ai_is_high_value_wid(wid) ? 900 + winning_hands[wid].base_score * 120 : 320;
        } else if (ai_is_card_critical_for_wid(taken_card_no, wid)) {
            quality += 220;
        } else if (ai_is_card_related_for_wid(taken_card_no, wid)) {
            quality += 80;
        }
    }
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_score[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX || seen[wid]) {
            continue;
        }
        seen[wid] = ON;
        if (ai_would_complete_wid_by_taking_card(player, wid, taken_card_no)) {
            quality += ai_is_high_value_wid(wid) ? 900 + winning_hands[wid].base_score * 120 : 320;
        } else if (ai_is_card_critical_for_wid(taken_card_no, wid)) {
            quality += 220;
        } else if (ai_is_card_related_for_wid(taken_card_no, wid)) {
            quality += 80;
        }
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_dangerous_risk_wid(before, wid)) {
            continue;
        }
        if (ai_is_card_critical_for_wid(taken_card_no, wid)) {
            quality += before->risk_delay[wid] <= 2 ? 360 : 180;
        } else if (ai_is_card_related_for_wid(taken_card_no, wid)) {
            quality += before->risk_delay[wid] <= 2 ? 110 : 55;
        }
    }

    if (taken->type == CARD_TYPE_TANE) {
        int before_tane = count_owned_type(player, CARD_TYPE_TANE);
        quality += threshold_gain_bonus(before_tane, before_tane + 1, 5) * 2;
    }
    if (taken->type == CARD_TYPE_TAN) {
        int before_tan = count_owned_type(player, CARD_TYPE_TAN);
        quality += threshold_gain_bonus(before_tan, before_tan + 1, 5) * 2;
    }
    if (taken->type == CARD_TYPE_KASU || (taken->type == CARD_TYPE_TANE && taken->month == 8)) {
        int before_kasu = count_owned_kasu_total(player);
        quality += threshold_gain_bonus(before_kasu, before_kasu + 1, 10) * 2;
    }

    quality += calc_drop_three_card_five_point_setup_bonus(player, drop_card_no, taken_card_no, before);
    quality += calc_plain_role_progress_bonus_for_capture(player, drop_card_no, taken_card_no);
    quality += calc_sake_cup_capture_material_bonus(player, drop_card_no, taken_card_no, before);
    quality += calc_public_access_secure_bonus(player, drop_card_no, before);
    quality += calc_month_lock_bonus(player, taken->month);

    return quality;
}

static int is_blocking_target_five_point_wid(int wid)
{
    switch (wid) {
        case WID_SANKOU:
        case WID_HANAMI:
        case WID_TSUKIMI:
        case WID_ISC:
        case WID_AKATAN:
        case WID_AOTAN:
            return ON;
        default:
            return OFF;
    }
}

static int calc_opp_five_point_block_bonus(int player, int taken_card_no, const StrategyData* before)
{
    int opp;
    int best_bonus = 0;
    Card* taken;

    if (!before || player < 0 || player > 1 || taken_card_no < 0 || taken_card_no >= 48) {
        return 0;
    }

    opp = 1 - player;
    taken = &g.cards[taken_card_no];

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_blocking_target_five_point_wid(wid) || before->risk_reach_estimate[wid] <= 0) {
            continue;
        }

        if (!ai_is_card_critical_for_wid(taken_card_no, wid)) {
            continue;
        }

        {
            int wid_bonus = DROP_FIVE_POINT_BLOCK_BASE_BONUS;
            wid_bonus += before->risk_reach_estimate[wid] * DROP_FIVE_POINT_BLOCK_REACH_UNIT;
            wid_bonus += before->risk_score[wid] * DROP_FIVE_POINT_BLOCK_SCORE_UNIT;
            if (before->risk_delay[wid] <= 2) {
                wid_bonus += 900;
            } else if (before->risk_delay[wid] <= 4) {
                wid_bonus += 450;
            }
            if (!has_public_same_month_role_followup(player, opp, taken->month, wid, taken_card_no)) {
                wid_bonus += DROP_FIVE_POINT_BLOCK_FULL_DENY_BONUS;
            }
            if (best_bonus < wid_bonus) {
                best_bonus = wid_bonus;
            }
        }
    }

    return best_bonus;
}

static int calc_leave_quality(int player, int left_card_no, const StrategyData* before)
{
    Card* left;
    int penalty;
    int unrevealed_same_month;

    if (player < 0 || player > 1 || left_card_no < 0 || left_card_no >= 48 || !before) {
        return 0;
    }

    left = &g.cards[left_card_no];
    penalty = 0;
    unrevealed_same_month = ai_count_unrevealed_same_month(player, left->month);

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (is_dangerous_risk_wid(before, wid)) {
            if (ai_is_card_critical_for_wid(left_card_no, wid)) {
                penalty -= before->risk_delay[wid] <= 2 ? 280 : 140;
            } else if (ai_is_card_related_for_wid(left_card_no, wid)) {
                penalty -= before->risk_delay[wid] <= 2 ? 90 : 45;
            }
        }

        if (!ai_is_fixed_wid(wid) || ai_wid_missing_count(player, wid) != 1) {
            continue;
        }
        if (ai_would_complete_wid_by_taking_card(player, wid, left_card_no)) {
            penalty -= ai_is_high_value_wid(wid) ? 280 : 120;
        }
    }

    if (unrevealed_same_month > 0) {
        penalty -= 40 + unrevealed_same_month * 25;
    }
    if (should_press_domain_capture(player, left_card_no, before)) {
        penalty -= calc_domain_capture_bonus(player, left_card_no, before);
    } else if (left->type == CARD_TYPE_GOKOU) {
        penalty -= 180;
    }
    penalty -= calc_exposed_domain_keycard_capture_bonus(player, left_card_no, before);
    if (count_known_month_cards_for_player(player, left->month) == 3) {
        penalty -= DROP_MONTH_LOCK_MISS_PENALTY;
    }

    return penalty;
}

static int calc_drop_take_immediate_gain(int player, int drop_card_no, int take_card_no, int* out_best_wid)
{
    int cards[2];

    if (out_best_wid) {
        *out_best_wid = -1;
    }
    if (player < 0 || player > 1 || drop_card_no < 0 || drop_card_no >= 48 || take_card_no < 0 || take_card_no >= 48) {
        return 0;
    }

    cards[0] = drop_card_no;
    cards[1] = take_card_no;
    return analyze_score_gain_with_card_ids(player, cards, 2, out_best_wid);
}

static void evaluate_drop_capture(int player, int drop_card_no, const StrategyData* before, DropCaptureEval* out_eval)
{
    int takeable[48];
    int takeable_count;

    if (!out_eval) {
        return;
    }

    vgs_memset(out_eval, 0, sizeof(*out_eval));
    takeable_count = collect_takeable_floor_cards_by_drop(drop_card_no, takeable, 48);
    if (takeable_count <= 0 || !before) {
        return;
    }

    out_eval->capture_possible = ON;

    if (takeable_count >= 3) {
        int total_quality = 0;
        for (int i = 0; i < takeable_count; i++) {
            total_quality += calc_capture_quality(player, drop_card_no, takeable[i], before);
        }
        out_eval->capture_quality = total_quality;
        out_eval->chosen_take_card_no = takeable[0];
        return;
    }

    {
        int best_total = INT_MIN;
        int best_immediate_gain = INT_MIN;
        int best_immediate_base = -1;
        for (int i = 0; i < takeable_count; i++) {
            int capture_quality = calc_capture_quality(player, drop_card_no, takeable[i], before);
            int leave_quality = 0;
            int immediate_wid = -1;
            int immediate_gain = calc_drop_take_immediate_gain(player, drop_card_no, takeable[i], &immediate_wid);
            int immediate_base = immediate_wid >= 0 ? winning_hands[immediate_wid].base_score : 0;
            if (is_final_round_non_clinch_immediate(before, immediate_gain)) {
                immediate_gain = -1;
                immediate_base = -1;
            }
            for (int j = 0; j < takeable_count; j++) {
                if (i == j) {
                    continue;
                }
                leave_quality += calc_leave_quality(player, takeable[j], before);
            }
            if (immediate_gain > best_immediate_gain ||
                (immediate_gain == best_immediate_gain &&
                 (immediate_base > best_immediate_base ||
                  (immediate_base == best_immediate_base &&
                   (capture_quality + leave_quality > best_total ||
                    (capture_quality + leave_quality == best_total && takeable[i] < out_eval->chosen_take_card_no)))))) {
                best_immediate_gain = immediate_gain;
                best_immediate_base = immediate_base;
                best_total = capture_quality + leave_quality;
                out_eval->capture_quality = capture_quality;
                out_eval->leave_quality = leave_quality;
                out_eval->chosen_take_card_no = takeable[i];
            }
        }
    }
}

static int is_light_capture_urgent_state(const StrategyData* s)
{
    if (!s) {
        return ON;
    }
    if (s->koikoi_opp == ON || s->opp_coarse_threat >= 85 || s->opponent_win_x2 == ON || s->left_rounds <= 2) {
        return ON;
    }
    if (s->match_score_diff >= 0 && s->match_score_diff <= 6) {
        return ON;
    }
    return OFF;
}

static int is_domain_capture_target(int player, int taken_card_no, const StrategyData* s)
{
    if (player < 0 || player > 1 || taken_card_no < 0 || taken_card_no >= 48 || !s) {
        return OFF;
    }
    switch (s->domain) {
        case AI_ENV_DOMAIN_LIGHT:
            return ai_is_card_critical_for_wid(taken_card_no, WID_SANKOU) || ai_is_card_critical_for_wid(taken_card_no, WID_SHIKOU) ||
                   ai_is_card_critical_for_wid(taken_card_no, WID_AME_SHIKOU) || ai_is_card_critical_for_wid(taken_card_no, WID_GOKOU) ||
                   ai_would_complete_wid_by_taking_card(player, WID_SANKOU, taken_card_no) ||
                   ai_would_complete_wid_by_taking_card(player, WID_SHIKOU, taken_card_no) ||
                   ai_would_complete_wid_by_taking_card(player, WID_AME_SHIKOU, taken_card_no) ||
                   ai_would_complete_wid_by_taking_card(player, WID_GOKOU, taken_card_no);
        case AI_ENV_DOMAIN_SAKE:
            return ai_is_card_critical_for_wid(taken_card_no, WID_HANAMI) || ai_is_card_critical_for_wid(taken_card_no, WID_TSUKIMI) ||
                   ai_would_complete_wid_by_taking_card(player, WID_HANAMI, taken_card_no) ||
                   ai_would_complete_wid_by_taking_card(player, WID_TSUKIMI, taken_card_no);
        case AI_ENV_DOMAIN_AKATAN:
            return ai_is_card_critical_for_wid(taken_card_no, WID_AKATAN) || ai_would_complete_wid_by_taking_card(player, WID_AKATAN, taken_card_no);
        case AI_ENV_DOMAIN_AOTAN:
            return ai_is_card_critical_for_wid(taken_card_no, WID_AOTAN) || ai_would_complete_wid_by_taking_card(player, WID_AOTAN, taken_card_no);
        case AI_ENV_DOMAIN_ISC:
            return ai_is_card_critical_for_wid(taken_card_no, WID_ISC) || ai_would_complete_wid_by_taking_card(player, WID_ISC, taken_card_no);
        default: return OFF;
    }
}

static int calc_domain_capture_bonus(int player, int taken_card_no, const StrategyData* s)
{
    int bonus = 0;

    if (player < 0 || player > 1 || taken_card_no < 0 || taken_card_no >= 48 || !s || !is_domain_capture_target(player, taken_card_no, s)) {
        return OFF;
    }
    switch (s->domain) {
        case AI_ENV_DOMAIN_LIGHT:
            bonus += 900;
            if (s->domain == AI_ENV_DOMAIN_LIGHT) {
                bonus += 1200;
            }
            if (ai_would_complete_wid_by_taking_card(player, WID_SANKOU, taken_card_no) ||
                ai_would_complete_wid_by_taking_card(player, WID_SHIKOU, taken_card_no) ||
                ai_would_complete_wid_by_taking_card(player, WID_AME_SHIKOU, taken_card_no) ||
                ai_would_complete_wid_by_taking_card(player, WID_GOKOU, taken_card_no)) {
                bonus += 2600;
            }
            break;
        case AI_ENV_DOMAIN_SAKE:
            bonus += 700;
            if (ai_would_complete_wid_by_taking_card(player, WID_HANAMI, taken_card_no) ||
                ai_would_complete_wid_by_taking_card(player, WID_TSUKIMI, taken_card_no)) {
                bonus += 2400;
            } else {
                bonus += 900;
            }
            break;
        case AI_ENV_DOMAIN_AKATAN:
        case AI_ENV_DOMAIN_AOTAN:
            bonus += 650;
            if ((s->domain == AI_ENV_DOMAIN_AKATAN && ai_would_complete_wid_by_taking_card(player, WID_AKATAN, taken_card_no)) ||
                (s->domain == AI_ENV_DOMAIN_AOTAN && ai_would_complete_wid_by_taking_card(player, WID_AOTAN, taken_card_no))) {
                bonus += 2200;
            } else {
                bonus += 800;
            }
            break;
        case AI_ENV_DOMAIN_ISC:
            bonus += 750;
            bonus += ai_would_complete_wid_by_taking_card(player, WID_ISC, taken_card_no) ? 2300 : 900;
            break;
        default: break;
    }
    return bonus;
}

static int calc_exposed_domain_keycard_capture_bonus(int player, int taken_card_no, const StrategyData* s)
{
    Card* taken;
    int is_keycard = OFF;
    int bonus = 0;

    if (player < 0 || player > 1 || taken_card_no < 0 || taken_card_no >= 48 || !s || s->domain == AI_ENV_DOMAIN_NONE) {
        return 0;
    }

    taken = &g.cards[taken_card_no];

    switch (s->domain) {
        case AI_ENV_DOMAIN_LIGHT:
            is_keycard = ai_is_card_critical_for_wid(taken_card_no, WID_SANKOU) || ai_is_card_critical_for_wid(taken_card_no, WID_SHIKOU) ||
                         ai_is_card_critical_for_wid(taken_card_no, WID_AME_SHIKOU) || ai_is_card_critical_for_wid(taken_card_no, WID_GOKOU);
            bonus = DROP_LIGHT_EXPOSED_KEYCARD_BONUS;
            break;
        case AI_ENV_DOMAIN_SAKE:
            is_keycard = ai_is_card_critical_for_wid(taken_card_no, WID_HANAMI) || ai_is_card_critical_for_wid(taken_card_no, WID_TSUKIMI) ||
                         ai_is_card_critical_for_wid(taken_card_no, WID_ISC);
            bonus = DROP_SAKE_EXPOSED_KEYCARD_BONUS;
            break;
        case AI_ENV_DOMAIN_AKATAN:
            is_keycard = ai_is_card_critical_for_wid(taken_card_no, WID_AKATAN);
            bonus = DROP_AKATAN_EXPOSED_KEYCARD_BONUS;
            break;
        case AI_ENV_DOMAIN_AOTAN:
            is_keycard = ai_is_card_critical_for_wid(taken_card_no, WID_AOTAN);
            bonus = DROP_AOTAN_EXPOSED_KEYCARD_BONUS;
            break;
        case AI_ENV_DOMAIN_ISC:
            is_keycard = ai_is_card_critical_for_wid(taken_card_no, WID_ISC);
            bonus = DROP_ISC_EXPOSED_KEYCARD_BONUS;
            break;
        default:
            return 0;
    }
    if (!is_keycard) {
        return 0;
    }
    if (count_known_month_cards_for_player(player, taken->month) != 3) {
        return 0;
    }
    if (count_owned_month_cards(player, taken->month) >= 3) {
        return 0;
    }

    return bonus;
}

static int should_press_domain_capture(int player, int taken_card_no, const StrategyData* s)
{
    int month_lock_ready;

    if (player < 0 || player > 1 || taken_card_no < 0 || taken_card_no >= 48 || !s) {
        return OFF;
    }
    if (!is_domain_capture_target(player, taken_card_no, s)) {
        return OFF;
    }
    month_lock_ready = count_known_month_cards_for_player(player, g.cards[taken_card_no].month) == 3 ? ON : OFF;
    if (!month_lock_ready) {
        return ON;
    }
    if (s->plan != AI_PLAN_PRESS) {
        return ON;
    }
    return is_light_capture_urgent_state(s);
}

static int simulate_drop_to_empty_slot(int player, int hand_index, Card* card)
{
    int slot = find_empty_deck_slot();
    if (slot < 0) {
        return -1;
    }
    g.own[player].cards[hand_index] = NULL;
    if (g.own[player].num > 0) {
        g.own[player].num--;
    }
    g.deck.cards[slot] = card;
    g.deck.num++;
    return slot;
}

static int simulate_drop_capture(int player, int hand_index, Card* card, const DropCaptureEval* capture_eval)
{
    int targetDecks[48];
    int targetDeckCount;

    if (!card) {
        return OFF;
    }

    targetDeckCount = collect_target_deck_indexes(card, targetDecks);
    if (targetDeckCount <= 0 || !capture_eval || !capture_eval->capture_possible) {
        return simulate_drop_to_empty_slot(player, hand_index, card) >= 0 ? ON : OFF;
    }

    g.own[player].cards[hand_index] = NULL;
    if (g.own[player].num > 0) {
        g.own[player].num--;
    }

    g.invent[player][card->type].cards[g.invent[player][card->type].num++] = card;
    if (targetDeckCount >= 3) {
        for (int i = targetDeckCount - 1; i >= 0; i--) {
            int deckIndex = targetDecks[i];
            Card* taken = g.deck.cards[deckIndex];
            if (!taken) {
                continue;
            }
            g.invent[player][taken->type].cards[g.invent[player][taken->type].num++] = taken;
            g.deck.cards[deckIndex] = NULL;
            if (g.deck.num > 0) {
                g.deck.num--;
            }
        }
        return ON;
    }

    for (int i = 0; i < targetDeckCount; i++) {
        int deckIndex = targetDecks[i];
        Card* taken = g.deck.cards[deckIndex];
        if (!taken || taken->id != capture_eval->chosen_take_card_no) {
            continue;
        }
        g.invent[player][taken->type].cards[g.invent[player][taken->type].num++] = taken;
        g.deck.cards[deckIndex] = NULL;
        if (g.deck.num > 0) {
            g.deck.num--;
        }
        return ON;
    }

    return OFF;
}

static int collect_takeable_floor_cards_by_drop(int drop_card_no, int* out_list, int cap)
{
    int count = 0;
    Card* drop = &g.cards[drop_card_no];

    if (!out_list || cap <= 0 || !drop) {
        return 0;
    }

    for (int i = 0; i < 48 && count < cap; i++) {
        Card* floor = g.deck.cards[i];
        if (floor && floor->month == drop->month) {
            out_list[count++] = floor->id;
        }
    }
    return count;
}

static int calc_drop_immediate_points_gain(int player, int drop_card_no, const DropCaptureEval* capture_eval, int* out_best_wid, int* out_take_card_no)
{
    int takeable[48];
    int cards[49];
    int card_count = 0;
    int takeable_count = 0;
    int best_wid = -1;
    int best_take = -1;
    int best_score = 0;

    if (!capture_eval || !capture_eval->capture_possible) {
        if (out_best_wid) {
            *out_best_wid = -1;
        }
        if (out_take_card_no) {
            *out_take_card_no = -1;
        }
        return 0;
    }

    if (collect_takeable_floor_cards_by_drop(drop_card_no, takeable, 48) >= 3) {
        takeable_count = collect_takeable_floor_cards_by_drop(drop_card_no, takeable, 48);
    } else if (capture_eval->chosen_take_card_no >= 0) {
        takeable[0] = capture_eval->chosen_take_card_no;
        takeable_count = 1;
    }
    cards[card_count++] = drop_card_no;
    for (int i = 0; i < takeable_count; i++) {
        cards[card_count++] = takeable[i];
    }
    best_score = analyze_score_gain_with_card_ids(player, cards, card_count, &best_wid);
    if (best_score > 0) {
        best_take = capture_eval->chosen_take_card_no >= 0 ? capture_eval->chosen_take_card_no : drop_card_no;
    }

    if (out_best_wid) {
        *out_best_wid = best_wid;
    }
    if (out_take_card_no) {
        *out_take_card_no = best_take;
    }
    return best_score;
}

static int should_delay_early_sake_completion(int player, int drop_card_no, const StrategyData* before, int completion_wid, int completion_take)
{
    Card* drop;

    if (player < 0 || player > 1 || !before || completion_take != drop_card_no) {
        return OFF;
    }
    if (completion_wid != WID_HANAMI && completion_wid != WID_TSUKIMI) {
        return OFF;
    }
    if (drop_card_no < 0 || drop_card_no >= 48) {
        return OFF;
    }

    drop = &g.cards[drop_card_no];
    if (drop->type != CARD_TYPE_GOKOU) {
        return OFF;
    }
    if (!hard_player_invent_has_card_id(player, 35)) {
        return OFF;
    }
    if (!((drop_card_no == 11 && completion_wid == WID_HANAMI) || (drop_card_no == 31 && completion_wid == WID_TSUKIMI))) {
        return OFF;
    }
    if (before->koikoi_opp == ON || before->opponent_win_x2 == ON) {
        return OFF;
    }
    if (before->opp_coarse_threat > DROP_EARLY_SAKE_THREAT_MAX || before->risk_7plus_distance < DROP_EARLY_SAKE_RISK7_MIN_DISTANCE) {
        return OFF;
    }
    if (before->left_own < DROP_EARLY_SAKE_LEFT_OWN_MIN || before->left_rounds < DROP_EARLY_SAKE_LEFT_ROUNDS_MIN) {
        return OFF;
    }
    if (before->match_score_diff <= DROP_EARLY_SAKE_BEHIND_LIMIT) {
        return OFF;
    }
    return before->delay[completion_wid] <= DROP_EARLY_SAKE_COMPLETION_DELAY_MAX ? ON : OFF;
}

static int calc_drop_completion_combo_bonus(int player, int drop_card_no, const StrategyData* before, const DropCaptureEval* capture_eval,
                                            int* out_best_wid, int* out_take_card_no)
{
    int takeable[48];
    int takeable_count = 0;
    int best_wid = -1;
    int best_take = -1;
    int best_base = 0;

    if (!capture_eval || !capture_eval->capture_possible) {
        if (out_best_wid) {
            *out_best_wid = -1;
        }
        if (out_take_card_no) {
            *out_take_card_no = -1;
        }
        return 0;
    }

    // 実際にこの手で回収する札だけを completion 対象にする。
    if (collect_takeable_floor_cards_by_drop(drop_card_no, takeable, 48) >= 3) {
        takeable_count = collect_takeable_floor_cards_by_drop(drop_card_no, takeable, 48);
    } else if (capture_eval->chosen_take_card_no >= 0) {
        takeable[0] = capture_eval->chosen_take_card_no;
        takeable_count = 1;
    }

    // 回収して自分の取り札になる drop 札自身も completion 対象に含める。
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!ai_is_fixed_wid(wid) || !ai_is_high_value_wid(wid)) {
            continue;
        }
        if (!ai_would_complete_wid_by_taking_card(player, wid, drop_card_no)) {
            continue;
        }
        if (winning_hands[wid].base_score > best_base) {
            best_base = winning_hands[wid].base_score;
            best_wid = wid;
            best_take = drop_card_no;
        }
    }

    // drop 後に同月回収できる場札の中から、固定高打点役を完成させる札を拾う。
    for (int i = 0; i < takeable_count; i++) {
        int take_card_no = takeable[i];
        for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
            if (!ai_is_fixed_wid(wid) || !ai_is_high_value_wid(wid)) {
                continue;
            }
            if (!ai_would_complete_wid_by_taking_card(player, wid, take_card_no)) {
                continue;
            }
            int base = winning_hands[wid].base_score;
            if (base > best_base) {
                best_base = base;
                best_wid = wid;
                best_take = take_card_no;
            }
        }
    }

    if (out_best_wid) {
        *out_best_wid = best_wid;
    }
    if (out_take_card_no) {
        *out_take_card_no = best_take;
    }
    if (best_base > 0 && should_delay_early_sake_completion(player, drop_card_no, before, best_wid, best_take)) {
        ai_putlog("[drop] delay early sake completion: wid=%s drop=%d delay=%d sake=ON", winning_hands[best_wid].name, drop_card_no,
                  before->delay[best_wid]);
        if (out_best_wid) {
            *out_best_wid = -1;
        }
        if (out_take_card_no) {
            *out_take_card_no = -1;
        }
        return 0;
    }
    if (best_base > 0 && before->match_score_diff >= 0 && should_delay_early_five_point_completion(before, player, best_wid, drop_card_no)) {
        ai_putlog("[drop] delay early five-point completion: wid=%s drop=%d delay=%d hand_keys=%d", winning_hands[best_wid].name, drop_card_no,
                  before->delay[best_wid], count_critical_hand_cards_for_wid(player, best_wid, drop_card_no));
        if (out_best_wid) {
            *out_best_wid = -1;
        }
        if (out_take_card_no) {
            *out_take_card_no = -1;
        }
        return -DROP_EARLY_FIVE_POINT_DELAY_PENALTY;
    }
    return best_base > 0 ? best_base * 20000 : 0;
}

static int calc_drop_overpay_bonus(int player, int drop_card_no, const StrategyData* before, const StrategyData* after,
                                   int completion_wid, int completion_take, int completion_base, const DropCaptureEval* capture_eval,
                                   int* out_wid, int* out_setup)
{
    if (out_wid) {
        *out_wid = -1;
    }
    if (out_setup) {
        *out_setup = OFF;
    }
    if (!before || !after || !is_overpay_aggressive_state(before) || is_overpay_defensive_state(before)) {
        return 0;
    }

    if (completion_wid >= 0 && ai_is_overpay_target_wid(completion_wid) && strategy_overpay_flag(before, completion_wid) && completion_base <= 5 &&
        completion_take >= 0) {
        int penalty = calc_overpay_delay_penalty(before, player, g.cards[completion_take].month);
        if (penalty <= 0) {
            return 0;
        }
        if (out_wid) {
            *out_wid = completion_wid;
        }
        return -penalty;
    }

    if (!capture_eval || !capture_eval->capture_possible || capture_eval->chosen_take_card_no < 0) {
        return 0;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!ai_is_overpay_target_wid(wid) || !strategy_overpay_flag(before, wid) || ai_wid_missing_count(player, wid) != 1) {
            continue;
        }
        if (!is_overpay_setup_card_for_wid(drop_card_no, wid) && !is_overpay_setup_card_for_wid(capture_eval->chosen_take_card_no, wid)) {
            continue;
        }
        if (strategy_overpay_flag(after, wid) && !ai_would_complete_wid_by_taking_card(player, wid, capture_eval->chosen_take_card_no)) {
            if (out_wid) {
                *out_wid = wid;
            }
            if (out_setup) {
                *out_setup = ON;
            }
            return DROP_OVERPAY_SETUP_BONUS;
        }
    }

    return 0;
}

/**
 * @brief CPU の手札 (g.own[player].cards) から切る札を選択
 * @param player 0: 自分, 1: CPU
 * @return g.own[player].cards のインデックス
 */
int ai_hard_drop(int player)
{
    ai_putlog("Thinking...");

    // 現局面を分析し、意思決定の重み (bias) を固定する。
    StrategyData str;
    ai_think_start();
    ai_think_strategy(player, &str);
    vgs_memcpy(&g.strategy[player], &str, sizeof(StrategyData));

    {
        int forced_visible_sake_setup = find_forced_visible_sake_setup_drop(player, &str);
        if (forced_visible_sake_setup >= 0) {
            ai_think_end();
            return forced_visible_sake_setup;
        }
    }

#if HARD_RAPACIOUS_FALLBACK_ENABLE == 1
    if (hard_should_force_rapacious_fallback_behind(&str) ||
        (hard_should_force_rapacious_fallback_sake_select_drop(player, &str) &&
         !hard_should_cancel_rapacious_fallback_sake_drop(player, &str))) {
        ai_think_end();
        return ai_hard_rapacious_fallback_drop(player);
    }
#endif

    int fallback_index = ai_normal_drop(player);

    int best_index = -1;
    int best_value = INT_MIN;
    int best_index_without_certain = -1;
    int best_value_without_certain = INT_MIN;
    int best_index_without_combo7 = -1;
    int best_value_without_combo7 = INT_MIN;
    int best_index_without_endgame = -1;
    int best_value_without_endgame = INT_MIN;
    int best_risk_sum_without_endgame = INT_MAX;
    int best_risk_min_delay_without_endgame = -1;
    int best_self_min_delay_without_endgame = 99;
    int best_index_without_keycard = -1;
    int best_value_without_keycard = INT_MIN;
    int best_fallback_distance_without_keycard = INT_MAX;
    int best_risk_sum_without_keycard = INT_MAX;
    int best_risk_min_delay_without_keycard = -1;
    int best_self_min_delay_without_keycard = 99;
    int best_risk_sum = INT_MAX;
    int best_risk_min_delay = -1;
    int best_self_min_delay = 99;
    int best_risk_sum_without_certain = INT_MAX;
    int best_risk_min_delay_without_certain = -1;
    int best_self_min_delay_without_certain = 99;
    int best_risk_sum_without_combo7 = INT_MAX;
    int best_risk_min_delay_without_combo7 = -1;
    int best_self_min_delay_without_combo7 = 99;
    int best_completion_base = -1;
    int best_completion_unrevealed = -1;
    int best_completion_base_without_certain = -1;
    int best_completion_unrevealed_without_certain = -1;
    int best_completion_base_without_combo7 = -1;
    int best_completion_unrevealed_without_combo7 = -1;
    int best_completion_base_without_keycard = -1;
    int best_completion_unrevealed_without_keycard = -1;
    int best_month_lock_bonus = 0;
    int best_overpay_bonus = 0;
    int best_hidden_key_trigger = OFF;
    int best_hidden_deny_trigger = OFF;
    int best_visible_light_finish_gain = 0;
    DropCandidateScore greedy_candidates[8];
    DropCandidateScore all_candidates[8];
    AiSevenLine candidate_sevenline[8];
    AiSevenLineBonus candidate_sevenline_bonus[8];
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
    DropCandidateScore phase2a_baseline_candidates[8];
    int phase2a_candidate_reason[8];
    AiSevenLine phase2a_candidate_sevenline[8];
    AiSevenLineBonus phase2a_candidate_sevenline_bonus[8];
#endif
    int greedy_candidate_count = 0;
    int all_candidate_count = 0;
    int any_month_lock_triggered = OFF;
    int any_overpay_delay_triggered = OFF;
    int any_hidden_key_triggered = OFF;
    int any_hidden_deny_triggered = OFF;
    int any_certain_seven_plus_triggered = OFF;
    int any_combo7_triggered = OFF;
    int any_keytarget_expose_triggered = OFF;
    int best_combo7_bonus = 0;
    int second_combo7_bonus = 0;
    int best_immediate_gain = 0;

    CardSet own_backup;
    CardSet deck_backup;
    CardSet invent_backup[4];

    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (!card) {
            continue;
        }

        vgs_memcpy(&own_backup, &g.own[player], sizeof(CardSet));
        vgs_memcpy(&deck_backup, &g.deck, sizeof(CardSet));
        vgs_memcpy(invent_backup, g.invent[player], sizeof(CardSet) * 4);

        StrategyData after;
        DropCaptureEval capture_eval;
        evaluate_drop_capture(player, card->id, &str, &capture_eval);
        int immediate_wid = -1;
        int immediate_take = -1;
        int immediate_gain = calc_drop_immediate_points_gain(player, card->id, &capture_eval, &immediate_wid, &immediate_take);
        int completion_wid = -1;
        int completion_take = -1;
        int completion_combo_bonus = calc_drop_completion_combo_bonus(player, card->id, &str, &capture_eval, &completion_wid, &completion_take);
        if (!simulate_drop_capture(player, i, card, &capture_eval)) {
            vgs_memcpy(&g.own[player], &own_backup, sizeof(CardSet));
            vgs_memcpy(&g.deck, &deck_backup, sizeof(CardSet));
            vgs_memcpy(g.invent[player], invent_backup, sizeof(CardSet) * 4);
            continue;
        }
        ai_think_strategy(player, &after);

        // 高打点見込み。
        int self_score_term = calc_self_score_term(&after);
        // 早上がり見込み。
        int self_speed_term = calc_self_speed_term(&after);
        // 相手の危険役をどれだけ鈍化/低減できるか。
        int opp_deny_term = calc_opp_deny_term(&str, &after) + calc_drop_card_direct_block_bonus(card->id, &str);
        // 危険札そのものを場へ出す罰は bias に依らず直接反映する。
        int danger_drop_penalty = calc_card_direct_risk_penalty(player, card->id, &str);
        // 危険札を直接切るリスクの抑制。
        int safety_term = calc_safety_term(player, card->id, &str);
        // 優先役の必須札を切る損失を、bias に依存しない独立項で反映。
        int keep_term = calc_drop_keep_term(player, card->id, &str);
        keep_term -= calc_unlocked_rainman_hold_penalty(player, card->id, &str, capture_eval.capture_possible);
        int no_take_gokou_penalty = calc_drop_no_take_gokou_penalty(player, card->id, &str, &after, capture_eval.capture_possible);
        int completion_base = completion_wid >= 0 ? winning_hands[completion_wid].base_score : 0;
        int completion_unrevealed = completion_take >= 0 ? ai_count_unrevealed_same_month(player, g.cards[completion_take].month) : 0;
        int endgame_needed = 0;
        int endgame_k = 0;
        int endgame_clinch_score = calc_endgame_clinch_score(&str, immediate_gain, &endgame_needed, &endgame_k);
        int keep_fatal = (completion_combo_bonus <= 0 && keep_term <= KEEP_FATAL_THRESHOLD) ? ON : OFF;
        if (completion_combo_bonus > 0 && keep_term < 0) {
            keep_term /= 4;
        }
        int capture_possible = capture_eval.capture_possible;
        int capture_term = capture_eval.capture_quality + capture_eval.leave_quality;
        int safety_fatal = safety_term <= SAFETY_FATAL_THRESHOLD ? ON : OFF;
        int opp_koikoi_win_bonus = calc_drop_opp_koikoi_win_bonus(player, card->id, &capture_eval);
        int greedy_seven_plus_bonus = 0;
        int hot_material_bonus = 0;
        int seven_plus_pressure_bonus = 0;
        int emergency_one_point_bonus = 0;
        int combo7_bonus = 0;
        int visible_light_finish_bonus = 0;
        int visible_light_finish_wid = -1;
        int visible_light_finish_gain = 0;
        int visible_light_finish_hand = -1;
        int visible_light_finish_floor = -1;
        int visible_sake_light_setup_bonus = 0;
        int visible_sake_light_setup_wid = -1;
        int visible_sake_followup_bonus = 0;
        int visible_sake_followup_wid = -1;
        AiSevenLine seven_line;
        AiSevenLineBonus seven_line_bonus;
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        int greedy_seven_plus_bonus_base = 0;
        int hot_material_bonus_base = 0;
        int phase2a_hot_material_bonus_base = 0;
        int seven_plus_pressure_bonus_base = 0;
        int combo7_bonus_base = 0;
        int domain_hot_bonus = 0;
        int phase2a_domain_hot_baseline_bonus = 0;
        int phase2a_delta_7plus = 0;
        int phase2a_delta_combo7 = 0;
        int phase2a_delta_domain_hot = 0;
        int phase2a_delta_defence = 0;
        int phase2a_reason = AI_PHASE2A_REASON_NONE;
#endif
        int keytarget_expose_penalty = 0;
        int keytarget_expose_wid = -1;
        int keytarget_expose_month = -1;
        AiCombo7Eval combo7_eval;
        int month_lock_bonus = 0;
        int hidden_month_card_no = -1;
        int hidden_key_trigger = OFF;
        int hidden_deny_trigger = OFF;
        int overpay_wid = -1;
        int overpay_setup = OFF;
        int dead_month_release_bonus = calc_dead_month_kasu_release_bonus(player, card->id, capture_eval.capture_possible);
        int overpay_bonus = calc_drop_overpay_bonus(player, card->id, &str, &after, completion_wid, completion_take, completion_base,
                                                    &capture_eval, &overpay_wid, &overpay_setup);
        int early_delay_bonus = calc_early_five_point_delay_bonus(player, card->id, &str, &capture_eval, completion_wid);
        int five_point_block_bonus = 0;
        if (capture_possible && should_press_domain_capture(player, card->id, &str)) {
            capture_term += calc_domain_capture_bonus(player, card->id, &str);
        }
        if (capture_possible && !safety_fatal && !keep_fatal) {
            capture_term += CAPTURE_DEFAULT_BONUS;
        }
        vgs_memset(&seven_line, 0, sizeof(seven_line));
        seven_line.wid = -1;
        vgs_memset(&seven_line_bonus, 0, sizeof(seven_line_bonus));
        if (str.mode == MODE_GREEDY) {
            greedy_seven_plus_bonus_base = calc_greedy_seven_plus_bonus_raw(player, &after, &seven_line, &seven_line_bonus);
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            greedy_seven_plus_bonus = calc_greedy_seven_plus_bonus_scaled(&after, &seven_line, greedy_seven_plus_bonus_base, &seven_line_bonus);
#else
            greedy_seven_plus_bonus = greedy_seven_plus_bonus_base;
#endif
            if (seven_line.ok) {
                ai_debug_note_sevenline_trigger(player, seven_line.margin, seven_line.reach, seven_line.delay, greedy_seven_plus_bonus,
                                                seven_line_bonus.low_reach, seven_line_bonus.press_applied);
            }
        }
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        combo7_bonus_base = calc_combo7_bonus_raw(player, g.stats[player].score + completion_base, &after, NULL);
#endif
        combo7_bonus = calc_combo7_bonus(player, g.stats[player].score + completion_base, &after, &combo7_eval);
        if (combo7_bonus > 0) {
            any_combo7_triggered = ON;
            if (combo7_bonus > best_combo7_bonus) {
                second_combo7_bonus = best_combo7_bonus;
                best_combo7_bonus = combo7_bonus;
            } else if (combo7_bonus > second_combo7_bonus) {
                second_combo7_bonus = combo7_bonus;
            }
        }
        if (capture_possible) {
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            hot_material_bonus_base += calc_hot_material_bonus_base(card->id, &after);
            hot_material_bonus_base += calc_hot_material_bonus_base(capture_eval.chosen_take_card_no, &after);
#endif
            hot_material_bonus += calc_hot_material_bonus(card->id, &after);
            hot_material_bonus += calc_hot_material_bonus(capture_eval.chosen_take_card_no, &after);
            five_point_block_bonus = calc_opp_five_point_block_bonus(player, capture_eval.chosen_take_card_no, &str);
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            domain_hot_bonus = calc_domain_hot_bonus(player, card->id, &after);
            if (phase2a_should_neutralize_hot_override(after.domain)) {
                phase2a_domain_hot_baseline_bonus = domain_hot_bonus;
            }
            hot_material_bonus += domain_hot_bonus;
#endif
            month_lock_bonus = calc_month_lock_bonus(player, g.cards[capture_eval.chosen_take_card_no].month);
            if (month_lock_bonus > 0) {
                any_month_lock_triggered = ON;
                hidden_month_card_no = find_hidden_month_card_no(player, g.cards[capture_eval.chosen_take_card_no].month);
                month_lock_bonus += calc_hidden_month_card_bonus(hidden_month_card_no, &str, &hidden_key_trigger, &hidden_deny_trigger);
                if (hidden_key_trigger) {
                    any_hidden_key_triggered = ON;
                }
                if (hidden_deny_trigger) {
                    any_hidden_deny_triggered = ON;
                }
            }
        }
        if (overpay_bonus < 0) {
            any_overpay_delay_triggered = ON;
        }
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        seven_plus_pressure_bonus_base = calc_near_seven_plus_pressure_bonus_raw(&str, &after);
#endif
        seven_plus_pressure_bonus = calc_near_seven_plus_pressure_bonus(&str, &after);
        emergency_one_point_bonus = calc_emergency_one_point_block_bonus(player, &str, &after);
        visible_light_finish_bonus = calc_visible_light_finish_bonus(player, &str, &after, immediate_gain, completion_base, &visible_light_finish_wid,
                                                                     &visible_light_finish_gain, &visible_light_finish_hand, &visible_light_finish_floor);
        visible_sake_light_setup_bonus = calc_visible_sake_light_setup_bonus(player, card->id, &str, &capture_eval, &visible_sake_light_setup_wid);
        visible_sake_followup_bonus =
            calc_visible_sake_followup_bonus(player, card->id, &str, &after, immediate_gain, completion_base, &visible_sake_followup_wid);
        if (opp_koikoi_win_bonus <= 0 &&
            is_exposing_keycard_capture_target(player, card->id, &str, &keytarget_expose_wid, &keytarget_expose_month, &keytarget_expose_penalty)) {
            any_keytarget_expose_triggered = ON;
        }

#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        phase2a_hot_material_bonus_base = hot_material_bonus_base + phase2a_domain_hot_baseline_bonus;
        if (phase2a_should_neutralize_hot_override(after.domain)) {
            phase2a_hot_material_bonus_base = hot_material_bonus;
        }
        phase2a_delta_7plus = greedy_seven_plus_bonus - greedy_seven_plus_bonus_base;
        phase2a_delta_combo7 = combo7_bonus - combo7_bonus_base;
        phase2a_delta_domain_hot = hot_material_bonus - phase2a_hot_material_bonus_base;
        phase2a_delta_defence = seven_plus_pressure_bonus - seven_plus_pressure_bonus_base;
        if (phase2a_delta_7plus != 0 || phase2a_delta_combo7 != 0 || phase2a_delta_domain_hot != 0 || phase2a_delta_defence != 0) {
            int best_abs = 0;
            if ((phase2a_delta_7plus < 0 ? -phase2a_delta_7plus : phase2a_delta_7plus) > best_abs) {
                best_abs = phase2a_delta_7plus < 0 ? -phase2a_delta_7plus : phase2a_delta_7plus;
                phase2a_reason = AI_PHASE2A_REASON_7PLUS;
            }
            if ((phase2a_delta_combo7 < 0 ? -phase2a_delta_combo7 : phase2a_delta_combo7) > best_abs) {
                best_abs = phase2a_delta_combo7 < 0 ? -phase2a_delta_combo7 : phase2a_delta_combo7;
                phase2a_reason = AI_PHASE2A_REASON_COMBO7;
            }
            if ((phase2a_delta_domain_hot < 0 ? -phase2a_delta_domain_hot : phase2a_delta_domain_hot) > best_abs) {
                best_abs = phase2a_delta_domain_hot < 0 ? -phase2a_delta_domain_hot : phase2a_delta_domain_hot;
                phase2a_reason = AI_PHASE2A_REASON_HOT;
            }
            if ((phase2a_delta_defence < 0 ? -phase2a_delta_defence : phase2a_delta_defence) > best_abs) {
                phase2a_reason = AI_PHASE2A_REASON_DEFENCE;
            }
            ai_debug_note_phase2a_trigger(player, after.plan, after.domain, phase2a_reason);
        }
#endif

        int offence_weight = str.bias.offence;
        int speed_weight = str.bias.speed;
        int defence_weight = str.bias.defence;
        int safety_weight = str.bias.defence;
        int capture_weight = CAPTURE_TACTICAL_WEIGHT;
        if (str.mode == MODE_DEFENSIVE) {
            defence_weight += DROP_MODE_DEFENCE_BONUS;
            safety_weight += DROP_MODE_DEFENCE_SAFETY_BONUS;
            capture_weight = 2;
        } else if (str.mode == MODE_GREEDY) {
            offence_weight += DROP_MODE_GREEDY_OFFENCE_BONUS;
            speed_weight += 5;
            capture_weight = 4;
        }

        int value = self_score_term * offence_weight + self_speed_term * speed_weight + opp_deny_term * defence_weight +
                    safety_term * safety_weight + keep_term + completion_combo_bonus + capture_term * capture_weight + opp_koikoi_win_bonus +
                    greedy_seven_plus_bonus + hot_material_bonus + five_point_block_bonus + overpay_bonus + early_delay_bonus + month_lock_bonus +
                    dead_month_release_bonus + seven_plus_pressure_bonus + emergency_one_point_bonus + combo7_bonus + visible_light_finish_bonus +
                    visible_sake_light_setup_bonus +
                    visible_sake_followup_bonus +
                    endgame_clinch_score -
                    keytarget_expose_penalty - no_take_gokou_penalty + danger_drop_penalty;
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        int value_base = self_score_term * offence_weight + self_speed_term * speed_weight + opp_deny_term * defence_weight +
                         safety_term * safety_weight + keep_term + completion_combo_bonus + capture_term * capture_weight + opp_koikoi_win_bonus +
                         greedy_seven_plus_bonus_base + phase2a_hot_material_bonus_base + five_point_block_bonus + overpay_bonus + early_delay_bonus + month_lock_bonus +
                         dead_month_release_bonus + seven_plus_pressure_bonus_base + combo7_bonus_base + visible_light_finish_bonus -
                         keytarget_expose_penalty - no_take_gokou_penalty +
                         danger_drop_penalty;
#endif

        int risk_sum = sum_risk_score(&after);
        int risk_min_delay = min_valid_delay(after.risk_delay, after.risk_reach_estimate, WINNING_HAND_MAX);
        int self_min_delay = min_valid_delay(after.delay, after.reach, WINNING_HAND_MAX);
        int opp_immediate_score = 0;
        int opp_immediate_seven_plus = OFF;
        int opp_immediate_win = OFF;
        int opp_immediate_wid = -1;
        int certain_seven_plus_penalty = 0;
        const char* prune_reason_code = "NONE";
        int completion_priority_bonus = completion_combo_bonus;
        if (is_final_round_non_clinch_immediate(&str, immediate_gain)) {
            immediate_gain = 0;
            immediate_wid = -1;
            endgame_clinch_score -= DROP_FINAL_NON_CLINCH_IMMEDIATE_PENALTY;
            value -= DROP_FINAL_NON_CLINCH_IMMEDIATE_PENALTY;
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            value_base -= DROP_FINAL_NON_CLINCH_IMMEDIATE_PENALTY;
#endif
            prune_reason_code = "FINAL_NON_CLINCH";
        }
        if (str.koikoi_opp == ON || str.opp_coarse_threat >= DROP_2PLY_SEVEN_PLUS_GATE_THREAT ||
            str.risk_7plus_distance <= DROP_2PLY_SEVEN_PLUS_GATE_DISTANCE) {
            opp_immediate_win = simulate_opponent_normal_turn_loss(player, &opp_immediate_score, &opp_immediate_seven_plus, &opp_immediate_wid);
            if (str.koikoi_opp == ON && opp_immediate_win) {
                value -= DROP_2PLY_KOIKOI_OPP_PRUNE_PENALTY;
                prune_reason_code = "PRUNE_KOIKOI_OPP";
            } else if (opp_immediate_seven_plus) {
                certain_seven_plus_penalty = (str.risk_7plus_distance <= DROP_2PLY_SEVEN_PLUS_GATE_DISTANCE &&
                                              str.opp_coarse_threat >= DROP_2PLY_SEVEN_PLUS_GATE_THREAT)
                                                 ? DROP_2PLY_CERTAIN_SEVEN_PLUS_PENALTY
                                                 : DROP_2PLY_SEVEN_PLUS_TIEBREAK_PENALTY;
                value -= certain_seven_plus_penalty;
                prune_reason_code = (certain_seven_plus_penalty == DROP_2PLY_CERTAIN_SEVEN_PLUS_PENALTY) ? "CERTAIN_7PLUS" : "NEAR_7PLUS";
                if (certain_seven_plus_penalty == DROP_2PLY_CERTAIN_SEVEN_PLUS_PENALTY) {
                    any_certain_seven_plus_triggered = ON;
                }
            }
        }
        int value_without_certain = value + certain_seven_plus_penalty;
        int value_without_combo7 = value - combo7_bonus;
        int value_without_keytarget = value + keytarget_expose_penalty;
        int part_score = self_score_term * offence_weight;
        int part_speed = self_speed_term * speed_weight;
        int part_deny = opp_deny_term * defence_weight;
        int part_safe = safety_term * safety_weight;
        int part_keep = keep_term - no_take_gokou_penalty;
        int part_tactical = completion_combo_bonus + capture_term * capture_weight;
        int part_bonus_total = value - (part_score + part_speed + part_deny + part_safe + part_keep + part_tactical);
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        int value_without_certain_base = value_base + certain_seven_plus_penalty;
        int value_without_combo7_base = value_base - combo7_bonus_base;
        int value_without_keytarget_base = value_base + keytarget_expose_penalty;
#endif
        int fallback_distance = i - fallback_index;
        if (fallback_distance < 0) {
            fallback_distance = -fallback_distance;
        }
        int best_fallback_distance = best_index < 0 ? INT_MAX : (best_index - fallback_index < 0 ? fallback_index - best_index : best_index - fallback_index);

        int better = OFF;
        if ((g.koikoi[player] == ON || str.koikoi_opp == ON) && (immediate_gain > 0 || best_immediate_gain > 0)) {
            if (immediate_gain > best_immediate_gain) {
                better = ON;
            } else if (immediate_gain == best_immediate_gain) {
                if (completion_priority_bonus > 0) {
                    if (best_completion_base < 0 || completion_base > best_completion_base) {
                        better = ON;
                    } else if (completion_base == best_completion_base) {
                        if (completion_unrevealed > best_completion_unrevealed) {
                            better = ON;
                        } else if (completion_unrevealed == best_completion_unrevealed) {
                            if (best_index < 0 || best_value < value) {
                                better = ON;
                            } else if (best_value == value) {
                                if (risk_sum < best_risk_sum) {
                                    better = ON;
                                } else if (risk_sum == best_risk_sum) {
                                    if (self_min_delay < best_self_min_delay) {
                                        better = ON;
                                    } else if (self_min_delay == best_self_min_delay && fallback_distance < best_fallback_distance) {
                                        better = ON;
                                    }
                                }
                            }
                        }
                    }
                } else if (best_index < 0 || best_value < value) {
                    better = ON;
                } else if (best_value == value) {
                    if (risk_sum < best_risk_sum) {
                        better = ON;
                    } else if (risk_sum == best_risk_sum) {
                        if (best_risk_min_delay < risk_min_delay) {
                            better = ON;
                        } else if (best_risk_min_delay == risk_min_delay) {
                            if (self_min_delay < best_self_min_delay) {
                                better = ON;
                            } else if (self_min_delay == best_self_min_delay && fallback_distance < best_fallback_distance) {
                                better = ON;
                            }
                        }
                    }
                }
            }
        } else if (visible_light_finish_bonus > 0 && best_completion_base > 0 && visible_light_finish_gain > best_completion_base) {
            if (visible_light_finish_gain > best_visible_light_finish_gain) {
                better = ON;
            } else if (visible_light_finish_gain == best_visible_light_finish_gain) {
                if (best_index < 0 || best_value < value) {
                    better = ON;
                } else if (best_value == value) {
                    if (risk_sum < best_risk_sum) {
                        better = ON;
                    } else if (risk_sum == best_risk_sum) {
                        if (self_min_delay < best_self_min_delay) {
                            better = ON;
                        } else if (self_min_delay == best_self_min_delay && fallback_distance < best_fallback_distance) {
                            better = ON;
                        }
                    }
                }
            }
        } else if (completion_priority_bonus > 0) {
            if (best_completion_base < 0 || completion_base > best_completion_base) {
                better = ON;
            } else if (completion_base == best_completion_base) {
                if (completion_unrevealed > best_completion_unrevealed) {
                    better = ON;
                } else if (completion_unrevealed == best_completion_unrevealed) {
                    if (best_index < 0 || best_value < value) {
                        better = ON;
                    } else if (best_value == value) {
                        if (risk_sum < best_risk_sum) {
                            better = ON;
                        } else if (risk_sum == best_risk_sum) {
                            if (self_min_delay < best_self_min_delay) {
                                better = ON;
                            } else if (self_min_delay == best_self_min_delay && fallback_distance < best_fallback_distance) {
                                better = ON;
                            }
                        }
                    }
                }
            }
        } else if (best_completion_base >= 0) {
            better = OFF;
        } else if (keep_fatal || safety_fatal) {
            better = OFF;
        } else if (best_index < 0 || best_value < value) {
            better = ON;
        } else if (best_value == value) {
            if (risk_sum < best_risk_sum) {
                better = ON;
            } else if (risk_sum == best_risk_sum) {
                if (best_risk_min_delay < risk_min_delay) {
                    better = ON;
                } else if (best_risk_min_delay == risk_min_delay) {
                    if (self_min_delay < best_self_min_delay) {
                        better = ON;
                    } else if (self_min_delay == best_self_min_delay && fallback_distance < best_fallback_distance) {
                        better = ON;
                    }
                }
            }
        }

        char combo7_label[96];
        format_combo7_label(&combo7_eval, combo7_label);
        if (month_lock_bonus > 0) {
            ai_putlog("[month-lock] month=%d own=%d known=%d hidden=1 hidden_card=%d reason=take_control", g.cards[capture_eval.chosen_take_card_no].month + 1,
                      count_owned_month_cards(player, g.cards[capture_eval.chosen_take_card_no].month),
                      count_known_month_cards_for_player(player, g.cards[capture_eval.chosen_take_card_no].month), hidden_month_card_no);
        }
        if (completion_combo_bonus > 0) {
            ai_putlog("[drop] completion combo: wid=%s base=%d drop=%d take=%d", winning_hands[completion_wid].name,
                      winning_hands[completion_wid].base_score, card->id, completion_take);
        }
        if (overpay_wid >= 0) {
            ai_putlog("[overpay] wid=%s setup=%s reason=%s", winning_hands[overpay_wid].name, overpay_setup ? "ON" : "OFF",
                      overpay_setup ? "hold_for_base6plus" : "deprioritize_base5_completion");
        }
        if (visible_light_finish_bonus > 0) {
            ai_putlog("[drop] visible light finish: wid=%s gain=%d hand=%d floor=%d", winning_hands[visible_light_finish_wid].name,
                      visible_light_finish_gain, visible_light_finish_hand, visible_light_finish_floor);
        }
        if (visible_sake_light_setup_bonus > 0) {
            ai_putlog("[drop] visible sake light setup: wid=%s drop=%d take=%d", winning_hands[visible_sake_light_setup_wid].name, card->id,
                      capture_eval.chosen_take_card_no);
        }
        if (visible_sake_followup_bonus > 0) {
            ai_putlog("[drop] visible sake followup: wid=%s drop=%d delay=%d reach=%d", winning_hands[visible_sake_followup_wid].name, card->id,
                      after.delay[visible_sake_followup_wid], after.reach[visible_sake_followup_wid]);
        }
        ai_putlog("drop[%d] card=%d value=%d (score=%d speed=%d deny=%d danger=%d safe=%d%s keep=%d:%s combo=%d cap=%d/%d/%d take=%d koikoi_win=%d atk7=%d emg1=%d hot=%d block5=%d overpay=%d delay5=%d monthlock=%d deadmonth=%d combo7=%d[%s reach=%d delay=%d sum=%d] end=%d need=%d k=%d imm=%d deny7=%d opp2ply=%d/%d)",
                  i,
                  card->id, value,
                  self_score_term, self_speed_term, opp_deny_term, danger_drop_penalty, safety_term, safety_fatal ? ":fatal" : "", keep_term,
                  keep_fatal ? "fatal" : (keep_term < 0 ? "caution" : "none"), completion_combo_bonus, capture_possible,
                  capture_eval.capture_quality, capture_eval.leave_quality, capture_eval.chosen_take_card_no, opp_koikoi_win_bonus,
                  greedy_seven_plus_bonus, emergency_one_point_bonus, hot_material_bonus, five_point_block_bonus, overpay_bonus, early_delay_bonus, month_lock_bonus,
                  dead_month_release_bonus,
                  combo7_bonus, combo7_label,
                  combo7_eval.combo_reach, combo7_eval.combo_delay, combo7_eval.combo_score_sum, endgame_clinch_score, endgame_needed, endgame_k, immediate_gain,
                  seven_plus_pressure_bonus,
                  opp_immediate_win, opp_immediate_seven_plus ? opp_immediate_score : 0);

        DropCandidateScore candidate_score;
        candidate_score.valid = ON;
        candidate_score.index = i;
        candidate_score.card_no = card->id;
        candidate_score.taken_card_no = capture_possible ? capture_eval.chosen_take_card_no : -1;
        candidate_score.action_mc_cluster = ai_strategy_primary_cluster(&after);
        candidate_score.value = value;
        candidate_score.value_without_certain = value_without_certain;
        candidate_score.value_without_combo7 = value_without_combo7;
        candidate_score.value_without_keytarget = value_without_keytarget;
        candidate_score.action_mc_bonus = 0;
        candidate_score.env_mc_bonus = 0;
        candidate_score.env_mc_E = 0;
        candidate_score.combo7_bonus = combo7_bonus;
        candidate_score.combo7_plus1_rank = combo7_eval.plus1_rank;
        candidate_score.keytarget_expose_penalty = keytarget_expose_penalty;
        candidate_score.keytarget_expose_wid = keytarget_expose_wid;
        candidate_score.keytarget_expose_month = keytarget_expose_month;
        candidate_score.risk_sum = risk_sum;
        candidate_score.risk_min_delay = risk_min_delay;
        candidate_score.self_min_delay = self_min_delay;
        candidate_score.completion_base = completion_base;
        candidate_score.completion_unrevealed = completion_unrevealed;
        candidate_score.visible_light_finish_bonus = visible_light_finish_bonus;
        candidate_score.visible_light_finish_gain = visible_light_finish_gain;
        candidate_score.immediate_gain = immediate_gain;
        candidate_score.endgame_clinch_score = endgame_clinch_score;
        candidate_score.endgame_needed = endgame_needed;
        candidate_score.endgame_k = endgame_k;
        candidate_score.part_score = part_score;
        candidate_score.part_speed = part_speed;
        candidate_score.part_deny = part_deny;
        candidate_score.part_safe = part_safe;
        candidate_score.part_keep = part_keep;
        candidate_score.part_tactical = part_tactical;
        candidate_score.part_bonus_total = part_bonus_total;
        candidate_score.opp_immediate_win = opp_immediate_win;
        candidate_score.opp_immediate_score = opp_immediate_win ? opp_immediate_score : 0;
        candidate_score.opp_immediate_wid = opp_immediate_win ? opp_immediate_wid : -1;
        candidate_score.opp_immediate_x2 = opp_immediate_win ? (((opp_immediate_score >= 7) ? 1 : 0) + (str.koikoi_mine == ON ? 1 : 0)) : 0;
        candidate_score.opp7_dist = after.risk_7plus_distance >= 4 ? 4 : (after.risk_7plus_distance <= 0 ? 1 : after.risk_7plus_distance);
        candidate_score.opp7_threat = after.opp_coarse_threat;
        candidate_score.opp7_certain = certain_seven_plus_penalty == DROP_2PLY_CERTAIN_SEVEN_PLUS_PENALTY ? ON : OFF;
        candidate_score.certain_7plus = certain_seven_plus_penalty == DROP_2PLY_CERTAIN_SEVEN_PLUS_PENALTY ? ON : OFF;
        candidate_score.near_7plus = (certain_seven_plus_penalty > 0 && !candidate_score.certain_7plus) ? ON : OFF;
        candidate_score.koikoi_opp_prune = (str.koikoi_opp == ON && opp_immediate_win) ? ON : OFF;
        candidate_score.aim_type = TRACE_AIM_NONE;
        candidate_score.aim_wid = -1;
        if (combo7_bonus > 0) {
            candidate_score.aim_type = TRACE_AIM_COMBO7;
            candidate_score.aim_wid = combo7_eval.combo_count > 0 ? combo7_eval.wids[0] : -1;
        }
        if (seven_line.ok && (greedy_seven_plus_bonus > 0 || seven_plus_pressure_bonus > 0)) {
            candidate_score.aim_type = TRACE_AIM_SEVENLINE;
            candidate_score.aim_wid = seven_line.wid;
        }
        if (month_lock_bonus > 0) {
            candidate_score.aim_type = TRACE_AIM_MONTHLOCK;
            candidate_score.aim_wid = -1;
        }
        if (overpay_bonus > 0 && overpay_wid >= 0) {
            candidate_score.aim_type = TRACE_AIM_OVERPAY;
            candidate_score.aim_wid = overpay_wid;
        }
        if (certain_seven_plus_penalty > 0 || (str.koikoi_opp == ON && opp_immediate_win)) {
            candidate_score.aim_type = TRACE_AIM_DEFENCE7;
            candidate_score.aim_wid = opp_immediate_wid;
        }
        if (opp_koikoi_win_bonus > 0) {
            candidate_score.aim_type = TRACE_AIM_KOIKOIWIN;
            candidate_score.aim_wid = immediate_wid;
        }
        if (immediate_gain > 0) {
            candidate_score.aim_type = TRACE_AIM_IMMEDIATE;
            candidate_score.aim_wid = immediate_wid >= 0 ? immediate_wid : completion_wid;
        }
        candidate_score.prune_reason_code = prune_reason_code;
        candidate_score.keep_fatal = keep_fatal;
        candidate_score.safety_fatal = safety_fatal;

        if (all_candidate_count < (int)(sizeof(all_candidates) / sizeof(all_candidates[0]))) {
            all_candidates[all_candidate_count++] = candidate_score;
        }

        {
            int better_without_endgame = OFF;
            int value_without_endgame = value - endgame_clinch_score;

            if (best_index_without_endgame < 0 || best_value_without_endgame < value_without_endgame) {
                better_without_endgame = ON;
            } else if (best_value_without_endgame == value_without_endgame) {
                if (risk_sum < best_risk_sum_without_endgame) {
                    better_without_endgame = ON;
                } else if (risk_sum == best_risk_sum_without_endgame) {
                    if (best_risk_min_delay_without_endgame < risk_min_delay) {
                        better_without_endgame = ON;
                    } else if (best_risk_min_delay_without_endgame == risk_min_delay) {
                        if (self_min_delay < best_self_min_delay_without_endgame) {
                            better_without_endgame = ON;
                        } else if (self_min_delay == best_self_min_delay_without_endgame && fallback_distance < best_fallback_distance) {
                            better_without_endgame = ON;
                        }
                    }
                }
            }
            if (better_without_endgame) {
                best_index_without_endgame = i;
                best_value_without_endgame = value_without_endgame;
                best_risk_sum_without_endgame = risk_sum;
                best_risk_min_delay_without_endgame = risk_min_delay;
                best_self_min_delay_without_endgame = self_min_delay;
            }
        }

        if (!keep_fatal && !safety_fatal && greedy_candidate_count < (int)(sizeof(greedy_candidates) / sizeof(greedy_candidates[0]))) {
            greedy_candidates[greedy_candidate_count] = candidate_score;
            candidate_sevenline[greedy_candidate_count] = seven_line;
            candidate_sevenline_bonus[greedy_candidate_count] = seven_line_bonus;
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            phase2a_baseline_candidates[greedy_candidate_count] = greedy_candidates[greedy_candidate_count];
            phase2a_baseline_candidates[greedy_candidate_count].value = value_base;
            phase2a_baseline_candidates[greedy_candidate_count].value_without_certain = value_without_certain_base;
            phase2a_baseline_candidates[greedy_candidate_count].value_without_combo7 = value_without_combo7_base;
            phase2a_baseline_candidates[greedy_candidate_count].value_without_keytarget = value_without_keytarget_base;
            phase2a_candidate_reason[greedy_candidate_count] = phase2a_reason;
            phase2a_candidate_sevenline[greedy_candidate_count] = seven_line;
            phase2a_candidate_sevenline_bonus[greedy_candidate_count] = seven_line_bonus;
#endif
            greedy_candidate_count++;
        }

        if (better) {
            best_index = i;
            best_value = value;
            best_immediate_gain = immediate_gain;
            best_risk_sum = risk_sum;
            best_risk_min_delay = risk_min_delay;
            best_self_min_delay = self_min_delay;
            best_month_lock_bonus = month_lock_bonus;
            best_overpay_bonus = overpay_bonus;
            best_hidden_key_trigger = hidden_key_trigger;
            best_hidden_deny_trigger = hidden_deny_trigger;
            best_visible_light_finish_gain = visible_light_finish_gain;
            if (completion_combo_bonus > 0) {
                best_completion_base = completion_base;
                best_completion_unrevealed = completion_unrevealed;
            }
        }
        {
            int best_fallback_distance_without =
                best_index_without_certain < 0 ? INT_MAX
                                               : (best_index_without_certain - fallback_index < 0 ? fallback_index - best_index_without_certain
                                                                                                  : best_index_without_certain - fallback_index);
            int better_without = OFF;
            if (completion_combo_bonus > 0) {
                if (best_completion_base_without_certain < 0 || completion_base > best_completion_base_without_certain) {
                    better_without = ON;
                } else if (completion_base == best_completion_base_without_certain) {
                    if (completion_unrevealed > best_completion_unrevealed_without_certain) {
                        better_without = ON;
                    } else if (completion_unrevealed == best_completion_unrevealed_without_certain) {
                        if (best_index_without_certain < 0 || best_value_without_certain < value_without_certain) {
                            better_without = ON;
                        } else if (best_value_without_certain == value_without_certain) {
                            if (risk_sum < best_risk_sum_without_certain) {
                                better_without = ON;
                            } else if (risk_sum == best_risk_sum_without_certain) {
                                if (self_min_delay < best_self_min_delay_without_certain) {
                                    better_without = ON;
                                } else if (self_min_delay == best_self_min_delay_without_certain &&
                                           fallback_distance < best_fallback_distance_without) {
                                    better_without = ON;
                                }
                            }
                        }
                    }
                }
            } else if (best_completion_base_without_certain >= 0) {
                better_without = OFF;
            } else if (keep_fatal || safety_fatal) {
                better_without = OFF;
            } else if (best_index_without_certain < 0 || best_value_without_certain < value_without_certain) {
                better_without = ON;
            } else if (best_value_without_certain == value_without_certain) {
                if (risk_sum < best_risk_sum_without_certain) {
                    better_without = ON;
                } else if (risk_sum == best_risk_sum_without_certain) {
                    if (best_risk_min_delay_without_certain < risk_min_delay) {
                        better_without = ON;
                    } else if (best_risk_min_delay_without_certain == risk_min_delay) {
                        if (self_min_delay < best_self_min_delay_without_certain) {
                            better_without = ON;
                        } else if (self_min_delay == best_self_min_delay_without_certain &&
                                   fallback_distance < best_fallback_distance_without) {
                            better_without = ON;
                        }
                    }
                }
            }
            if (better_without) {
                best_index_without_certain = i;
                best_value_without_certain = value_without_certain;
                best_risk_sum_without_certain = risk_sum;
                best_risk_min_delay_without_certain = risk_min_delay;
                best_self_min_delay_without_certain = self_min_delay;
                if (completion_priority_bonus > 0) {
                    best_completion_base_without_certain = completion_base;
                    best_completion_unrevealed_without_certain = completion_unrevealed;
                }
            }
        }

        {
            int best_fallback_distance_without_combo7 =
                best_index_without_combo7 < 0
                    ? INT_MAX
                    : (best_index_without_combo7 - fallback_index < 0 ? fallback_index - best_index_without_combo7
                                                                      : best_index_without_combo7 - fallback_index);
            int better_without_combo7 = OFF;
            if (completion_priority_bonus > 0) {
                if (best_completion_base_without_combo7 < 0 || completion_base > best_completion_base_without_combo7) {
                    better_without_combo7 = ON;
                } else if (completion_base == best_completion_base_without_combo7) {
                    if (completion_unrevealed > best_completion_unrevealed_without_combo7) {
                        better_without_combo7 = ON;
                    } else if (completion_unrevealed == best_completion_unrevealed_without_combo7) {
                        if (best_index_without_combo7 < 0 || best_value_without_combo7 < value_without_combo7) {
                            better_without_combo7 = ON;
                        } else if (best_value_without_combo7 == value_without_combo7) {
                            if (risk_sum < best_risk_sum_without_combo7) {
                                better_without_combo7 = ON;
                            } else if (risk_sum == best_risk_sum_without_combo7) {
                                if (best_risk_min_delay_without_combo7 < risk_min_delay) {
                                    better_without_combo7 = ON;
                                } else if (best_risk_min_delay_without_combo7 == risk_min_delay) {
                                    if (self_min_delay < best_self_min_delay_without_combo7) {
                                        better_without_combo7 = ON;
                                    } else if (self_min_delay == best_self_min_delay_without_combo7 &&
                                               fallback_distance < best_fallback_distance_without_combo7) {
                                        better_without_combo7 = ON;
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (best_completion_base_without_combo7 >= 0) {
                better_without_combo7 = OFF;
            } else if (keep_fatal || safety_fatal) {
                better_without_combo7 = OFF;
            } else if (best_index_without_combo7 < 0 || best_value_without_combo7 < value_without_combo7) {
                better_without_combo7 = ON;
            } else if (best_value_without_combo7 == value_without_combo7) {
                if (risk_sum < best_risk_sum_without_combo7) {
                    better_without_combo7 = ON;
                } else if (risk_sum == best_risk_sum_without_combo7) {
                    if (best_risk_min_delay_without_combo7 < risk_min_delay) {
                        better_without_combo7 = ON;
                    } else if (best_risk_min_delay_without_combo7 == risk_min_delay) {
                        if (self_min_delay < best_self_min_delay_without_combo7) {
                            better_without_combo7 = ON;
                        } else if (self_min_delay == best_self_min_delay_without_combo7 &&
                                   fallback_distance < best_fallback_distance_without_combo7) {
                            better_without_combo7 = ON;
                        }
                    }
                }
            }
            if (better_without_combo7) {
                best_index_without_combo7 = i;
                best_value_without_combo7 = value_without_combo7;
                best_risk_sum_without_combo7 = risk_sum;
                best_risk_min_delay_without_combo7 = risk_min_delay;
                best_self_min_delay_without_combo7 = self_min_delay;
                if (completion_priority_bonus > 0) {
                    best_completion_base_without_combo7 = completion_base;
                    best_completion_unrevealed_without_combo7 = completion_unrevealed;
                }
            }
        }
        {
            int better_without_keycard = OFF;
            if (completion_priority_bonus > 0) {
                if (best_completion_base_without_keycard < 0 || completion_base > best_completion_base_without_keycard) {
                    better_without_keycard = ON;
                } else if (completion_base == best_completion_base_without_keycard) {
                    if (completion_unrevealed > best_completion_unrevealed_without_keycard) {
                        better_without_keycard = ON;
                    } else if (completion_unrevealed == best_completion_unrevealed_without_keycard) {
                        if (best_index_without_keycard < 0 || best_value_without_keycard < value_without_keytarget) {
                            better_without_keycard = ON;
                        } else if (best_value_without_keycard == value_without_keytarget) {
                            if (risk_sum < best_risk_sum_without_keycard) {
                                better_without_keycard = ON;
                            } else if (risk_sum == best_risk_sum_without_keycard) {
                                if (best_risk_min_delay_without_keycard < risk_min_delay) {
                                    better_without_keycard = ON;
                                } else if (best_risk_min_delay_without_keycard == risk_min_delay) {
                                    if (self_min_delay < best_self_min_delay_without_keycard) {
                                        better_without_keycard = ON;
                                    } else if (self_min_delay == best_self_min_delay_without_keycard &&
                                               fallback_distance < best_fallback_distance_without_keycard) {
                                        better_without_keycard = ON;
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (best_completion_base_without_keycard >= 0) {
                better_without_keycard = OFF;
            } else if (keep_fatal || safety_fatal) {
                better_without_keycard = OFF;
            } else if (best_index_without_keycard < 0 || best_value_without_keycard < value_without_keytarget) {
                better_without_keycard = ON;
            } else if (best_value_without_keycard == value_without_keytarget) {
                if (risk_sum < best_risk_sum_without_keycard) {
                    better_without_keycard = ON;
                } else if (risk_sum == best_risk_sum_without_keycard) {
                    if (best_risk_min_delay_without_keycard < risk_min_delay) {
                        better_without_keycard = ON;
                    } else if (best_risk_min_delay_without_keycard == risk_min_delay) {
                        if (self_min_delay < best_self_min_delay_without_keycard) {
                            better_without_keycard = ON;
                        } else if (self_min_delay == best_self_min_delay_without_keycard &&
                                   fallback_distance < best_fallback_distance_without_keycard) {
                            better_without_keycard = ON;
                        }
                    }
                }
            }
            if (better_without_keycard) {
                best_index_without_keycard = i;
                best_value_without_keycard = value_without_keytarget;
                best_fallback_distance_without_keycard = fallback_distance;
                best_risk_sum_without_keycard = risk_sum;
                best_risk_min_delay_without_keycard = risk_min_delay;
                best_self_min_delay_without_keycard = self_min_delay;
                if (completion_priority_bonus > 0) {
                    best_completion_base_without_keycard = completion_base;
                    best_completion_unrevealed_without_keycard = completion_unrevealed;
                }
            }
        }

        vgs_memcpy(&g.own[player], &own_backup, sizeof(CardSet));
        vgs_memcpy(&g.deck, &deck_backup, sizeof(CardSet));
        vgs_memcpy(g.invent[player], invent_backup, sizeof(CardSet) * 4);
    }

    if (any_month_lock_triggered) {
        ai_debug_note_month_lock(player, best_month_lock_bonus > 0 && best_index != fallback_index ? ON : OFF);
    }
    if (any_hidden_key_triggered) {
        ai_debug_note_month_lock_hidden_key(player, best_hidden_key_trigger && best_index != fallback_index ? ON : OFF);
    }
    if (any_hidden_deny_triggered) {
        ai_debug_note_month_lock_hidden_deny(player, best_hidden_deny_trigger && best_index != fallback_index ? ON : OFF);
    }
    if (any_overpay_delay_triggered) {
        ai_debug_note_overpay_delay(player, best_overpay_bonus < 0 && best_index != fallback_index ? ON : OFF);
    }
    if (best_completion_base > 0 && best_completion_base <= 5 && all_candidate_count > 0) {
        int override_pos = -1;
        for (int i = 0; i < all_candidate_count; i++) {
            if (all_candidates[i].completion_base > 0) {
                continue;
            }
            if (all_candidates[i].value <= best_value) {
                continue;
            }
            if (override_pos < 0 || all_candidates[i].value > all_candidates[override_pos].value) {
                override_pos = i;
            }
        }
        if (override_pos >= 0) {
            best_index = all_candidates[override_pos].index;
            best_value = all_candidates[override_pos].value;
            best_immediate_gain = all_candidates[override_pos].immediate_gain;
            best_risk_sum = all_candidates[override_pos].risk_sum;
            best_risk_min_delay = all_candidates[override_pos].risk_min_delay;
            best_self_min_delay = all_candidates[override_pos].self_min_delay;
            best_visible_light_finish_gain = all_candidates[override_pos].visible_light_finish_gain;
            ai_putlog("[drop] override visible light finish: idx=%d gain=%d value=%d", best_index, best_visible_light_finish_gain, best_value);
        }
    }

    DropEnvMcCompareResult env_mc_result;
    int env_mc_forced_index = -1;
    vgs_memset(&env_mc_result, 0, sizeof(env_mc_result));
    env_mc_result.before_best_index = -1;
    if (greedy_candidate_count > 1) {
        evaluate_drop_env_mc_compare(player, &str, str.mode, greedy_candidates, greedy_candidate_count, fallback_index, &env_mc_result);
        if (env_mc_result.compare_triggered && env_mc_result.chosen_index >= 0) {
            env_mc_forced_index = env_mc_result.chosen_index;
        }
    }

    DropActionMcCompareResult action_mc_result;
    int action_mc_forced_index = -1;
    DropTraceDecision trace_decision;
    vgs_memset(&trace_decision, 0, sizeof(trace_decision));
    trace_decision.final_best_index = best_index;
    trace_decision.final_best_index_pre_d1 = best_index;
    trace_decision.final_best_index_phase2a_off = -1;
    trace_decision.reason_code = "BASE_COMPARATOR";
    vgs_memset(&action_mc_result, 0, sizeof(action_mc_result));
    action_mc_result.before_best_index = -1;
    if (greedy_candidate_count > 1) {
        evaluate_drop_action_mc_compare(player, &str, str.mode, greedy_candidates, greedy_candidate_count, fallback_index, &action_mc_result);
        if (action_mc_result.compare_active && action_mc_result.chosen_index >= 0) {
            action_mc_forced_index = action_mc_result.chosen_index;
        }
    }

    {
        int final_best_index = env_mc_forced_index >= 0 ? env_mc_forced_index : best_index;
        int final_best_index_pre_fb = final_best_index;
        int final_best_index_post_fb = final_best_index;
        DropCandidateScore greedy_fallback_pool[8];
        int greedy_fallback_pool_count = 0;
        int greedy_fallback_chosen_pool_idx = -1;
        int greedy_fallback_chosen_orig_idx = -1;
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        int final_best_index_phase2a_off = choose_best_drop_candidate(phase2a_baseline_candidates, greedy_candidate_count, DROP_GREEDY_VALUE_NORMAL, fallback_index);
#endif
        int final_best_index_without_certain = best_index_without_certain;
        int final_best_index_without_combo7 = best_index_without_combo7;
        int final_best_index_without_endgame = best_index_without_endgame;
        int final_best_index_without_keycard = best_index_without_keycard;
        int compare_gap = action_mc_result.before_best_value - action_mc_result.before_second_value;
        trace_decision.env_mc_trigger = env_mc_result.compare_triggered;
        trace_decision.env_mc_changed = env_mc_result.changed_choice;
        if (env_mc_forced_index >= 0) {
            trace_decision.reason_code = "ENV_MC_COMPARE";
        }
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
        trace_decision.final_best_index_phase2a_off = final_best_index_phase2a_off;
#endif
        if (str.mode != MODE_DEFENSIVE && str.greedy_need >= str.defensive_need && all_candidate_count > 0) {
            build_greedy_fallback_pool(all_candidates, all_candidate_count, DROP_GREEDY_VALUE_NORMAL, fallback_index, greedy_fallback_pool,
                                       &greedy_fallback_pool_count);
            greedy_fallback_chosen_orig_idx = choose_greedy_drop_candidate_from_pool(
                player, &str, greedy_fallback_pool, greedy_fallback_pool_count, DROP_GREEDY_VALUE_NORMAL, &greedy_fallback_chosen_pool_idx);
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            int greedy_choice_phase2a_off =
                choose_greedy_drop_candidate(player, &str, phase2a_baseline_candidates, greedy_candidate_count, DROP_GREEDY_VALUE_NORMAL, fallback_index);
#endif
            int greedy_choice_without =
                choose_greedy_drop_candidate(player, &str, all_candidates, all_candidate_count, DROP_GREEDY_VALUE_WITHOUT_CERTAIN, fallback_index);
            int greedy_choice_without_combo7 =
                choose_greedy_drop_candidate(player, &str, all_candidates, all_candidate_count, DROP_GREEDY_VALUE_WITHOUT_COMBO7, fallback_index);
            int greedy_choice_without_endgame =
                choose_greedy_drop_candidate(player, &str, all_candidates, all_candidate_count, DROP_GREEDY_VALUE_WITHOUT_ENDGAME, fallback_index);
            int greedy_choice_without_keytarget =
                choose_greedy_drop_candidate(player, &str, all_candidates, all_candidate_count, DROP_GREEDY_VALUE_WITHOUT_KEYTARGET, fallback_index);
            if (greedy_fallback_chosen_orig_idx >= 0) {
                int base_pos = find_drop_candidate_pos_by_index(all_candidates, all_candidate_count, final_best_index);
                int fallback_pos = find_drop_candidate_pos_by_index(all_candidates, all_candidate_count, greedy_fallback_chosen_orig_idx);
                int keep_base_completion = OFF;

                if (base_pos >= 0 && fallback_pos >= 0) {
                    int base_completion = all_candidates[base_pos].completion_base;
                    int fallback_completion = all_candidates[fallback_pos].completion_base;
                    if (base_completion > 0 &&
                        (fallback_completion < base_completion ||
                         (fallback_completion == base_completion &&
                          all_candidates[fallback_pos].completion_unrevealed < all_candidates[base_pos].completion_unrevealed))) {
                        keep_base_completion = ON;
                    }
                }

                if (!keep_base_completion) {
                    final_best_index = greedy_fallback_chosen_orig_idx;
                    final_best_index_post_fb = final_best_index;
                    ai_putlog("[drop] greedy fallback: top_k=%d chosen_index=%d", greedy_fallback_pool_count, greedy_fallback_chosen_orig_idx);
                    trace_decision.reason_code = "GREEDY_FALLBACK";
                } else {
                    ai_putlog("[drop] greedy fallback skipped: preserve completion base_idx=%d fallback_idx=%d", final_best_index,
                              greedy_fallback_chosen_orig_idx);
                }
            }
#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            if (greedy_choice_phase2a_off >= 0) {
                final_best_index_phase2a_off = greedy_choice_phase2a_off;
            }
#endif
            if (greedy_choice_without >= 0) {
                final_best_index_without_certain = greedy_choice_without;
            }
            if (greedy_choice_without_combo7 >= 0) {
                final_best_index_without_combo7 = greedy_choice_without_combo7;
            }
            if (greedy_choice_without_endgame >= 0) {
                final_best_index_without_endgame = greedy_choice_without_endgame;
            }
            if (greedy_choice_without_keytarget >= 0) {
                final_best_index_without_keycard = greedy_choice_without_keytarget;
            }
            log_drop_fallback_debug(player, str.mode, str.plan, greedy_fallback_pool, greedy_fallback_pool_count, greedy_fallback_pool_count,
                                    greedy_fallback_chosen_pool_idx, greedy_fallback_chosen_orig_idx, final_best_index_pre_fb,
                                    final_best_index_post_fb);
        }
        if (action_mc_forced_index >= 0) {
            int base_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, final_best_index);
            int action_mc_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, action_mc_forced_index);
            if (base_pos >= 0 && action_mc_pos >= 0 &&
                should_keep_drop_completion_candidate(player, &greedy_candidates[base_pos], &greedy_candidates[action_mc_pos])) {
                ai_putlog("[drop] action-mc skipped: preserve completion base_idx=%d mc_idx=%d", final_best_index, action_mc_forced_index);
            } else {
                final_best_index = action_mc_forced_index;
                trace_decision.reason_code = "ACTION_MC_COMPARE";
            }
        }
        {
            int final_best_index_pre_d1 = final_best_index;
            int d1_override_count = 0;
            int d1_override_index = -1;
            int d1_prev_pos = -1;
            int d1_new_pos = -1;

            trace_decision.final_best_index_pre_d1 = final_best_index_pre_d1;
            trace_decision.action_mc_trigger = action_mc_result.compare_triggered;
            trace_decision.action_mc_applied = action_mc_forced_index >= 0 ? ON : OFF;
            trace_decision.action_mc_changed =
                (action_mc_result.compare_active && action_mc_result.chosen_index >= 0 && action_mc_result.before_best_index != action_mc_result.chosen_index) ? ON : OFF;

            for (int i = 0; i < greedy_candidate_count; i++) {
                if (candidate_sevenline[i].ok && candidate_sevenline[i].delay == 1) {
                    d1_override_count++;
                }
            }
            trace_decision.d1_trigger = d1_override_count > 0 ? ON : OFF;
            if (d1_override_count > 0) {
                int final_pre_d1_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, final_best_index_pre_d1);
                if (str.koikoi_opp == ON) {
                    ai_debug_note_sevenline_d1_override(player, OFF, OFF);
                    ai_debug_note_sevenline_d1_override_blocked(player, AI_SEVENLINE_D1_OVERRIDE_BLOCK_KOIKOI_OPP);
                } else if (final_pre_d1_pos >= 0 && drop_candidate_has_priority_stop_value(&greedy_candidates[final_pre_d1_pos])) {
                    ai_debug_note_sevenline_d1_override(player, OFF, OFF);
                    ai_debug_note_sevenline_d1_override_blocked(player, AI_SEVENLINE_D1_OVERRIDE_BLOCK_INVALID);
                } else {
                    d1_override_index =
                        choose_best_drop_candidate_from_sevenline_d1(greedy_candidates, candidate_sevenline, greedy_candidate_count, fallback_index, NULL);
                    if (d1_override_index < 0) {
                        ai_debug_note_sevenline_d1_override(player, OFF, OFF);
                        ai_debug_note_sevenline_d1_override_blocked(player, AI_SEVENLINE_D1_OVERRIDE_BLOCK_INVALID);
                    } else {
                        int changed = d1_override_index != final_best_index ? ON : OFF;
                        trace_decision.d1_applied = ON;
                        if (changed) {
                            int current_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, final_best_index);
                            int override_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, d1_override_index);
                            if (current_pos >= 0 && override_pos >= 0 &&
                                should_keep_drop_completion_candidate(player, &greedy_candidates[current_pos], &greedy_candidates[override_pos])) {
                                changed = OFF;
                                ai_putlog("[d1-override] skipped: preserve completion prev=%d new=%d", final_best_index, d1_override_index);
                            }
                        }
                        trace_decision.d1_changed = changed;
                        ai_debug_note_sevenline_d1_override(player, ON, changed);
                        if (changed) {
                            int choice_before[2];
                            int choice_after[2];
                            int choice_before_count =
                                collect_drop_choice_order_indices(greedy_candidates, greedy_candidate_count, DROP_GREEDY_VALUE_NORMAL, fallback_index, choice_before, 2);
                            int choice_after_count = collect_drop_choice_order_indices_from_sevenline_d1(
                                greedy_candidates, candidate_sevenline, greedy_candidate_count, fallback_index, choice_after, 2);
                            int d1_gap_before =
                                choice_before_count >= 2 ? candidate_value_gap_by_index(greedy_candidates, greedy_candidate_count, choice_before[0], choice_before[1]) : 0;
                            int d1_gap_after =
                                choice_after_count >= 2 ? candidate_value_gap_by_index(greedy_candidates, greedy_candidate_count, choice_after[0], choice_after[1]) : 0;
                            d1_prev_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, final_best_index);
                            d1_new_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, d1_override_index);
                            ai_debug_note_sevenline_d1_gap(player, d1_gap_before, d1_gap_after);
                            ai_putlog("[d1-override] seed=%d game=%d round=%d turn=%d player=%d prev=%d value=%d new=%d value=%d d1_count=%d blocked=0 reason=none",
                                      ai_debug_current_seed(), ai_debug_current_game_index(), g.round + 1, 9 - g.own[player].num, player, final_best_index,
                                      d1_prev_pos >= 0 ? greedy_candidates[d1_prev_pos].value : 0, d1_override_index,
                                      d1_new_pos >= 0 ? greedy_candidates[d1_new_pos].value : 0, d1_override_count);
                            final_best_index = d1_override_index;
                        }
                    }
                }
            }

#if PHASE2A_PLAN_DOMAIN_SCALING_ENABLE
            {
                int phase2a_final_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, final_best_index_pre_d1);
                if (phase2a_final_pos >= 0 && phase2a_candidate_reason[phase2a_final_pos] != AI_PHASE2A_REASON_NONE &&
                    final_best_index_phase2a_off >= 0 && final_best_index_pre_d1 != final_best_index_phase2a_off) {
                    trace_decision.phase2a_trigger = ON;
                    trace_decision.phase2a_changed = ON;
                    for (int i = 0; i < greedy_candidate_count; i++) {
                        if (greedy_candidates[i].index == final_best_index_pre_d1) {
                            ai_debug_note_phase2a_changed_choice(player, str.plan, phase2a_candidate_reason[i]);
                            if (phase2a_candidate_reason[i] == AI_PHASE2A_REASON_7PLUS && phase2a_candidate_sevenline[i].ok) {
                                ai_debug_note_sevenline_changed(player, phase2a_candidate_sevenline[i].margin, phase2a_candidate_sevenline[i].delay);
                                ai_putlog("[sevenline] seed=%d round=%d turn=%d plan=%s best_wid=%d delay=%d reach=%d margin=%d base=%d mb=%d mult=%d bonus=%d before=%d after=%d",
                                          ai_debug_current_seed(), g.round + 1, 9 - g.own[player].num, env_plan_name(str.plan),
                                          phase2a_candidate_sevenline[i].wid, phase2a_candidate_sevenline[i].delay,
                                          phase2a_candidate_sevenline[i].reach, phase2a_candidate_sevenline[i].margin,
                                          phase2a_candidate_sevenline_bonus[i].base_bonus, phase2a_candidate_sevenline_bonus[i].margin_bonus,
                                          phase2a_candidate_sevenline_bonus[i].mult, phase2a_candidate_sevenline_bonus[i].bonus,
                                          final_best_index_phase2a_off, final_best_index_pre_d1);
                            }
                            ai_putlog("[phase2a] seed=%d round=%d turn=%d plan=%s domain=%s before=%d after=%d reason=%s",
                                      ai_debug_current_seed(), g.round + 1, 9 - g.own[player].num, env_plan_name(str.plan), env_domain_name(str.domain),
                                      final_best_index_phase2a_off, final_best_index_pre_d1, phase2a_reason_name(phase2a_candidate_reason[i]));
                            break;
                        }
                    }
                }
            }
            {
                int choice_order[2];
                int choice_count = collect_drop_choice_order_indices(greedy_candidates, greedy_candidate_count, DROP_GREEDY_VALUE_NORMAL, fallback_index, choice_order, 2);
                int d1_top2_index = -1;
                for (int i = 0; i < choice_count; i++) {
                    int pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, choice_order[i]);
                    if (pos >= 0 && phase2a_candidate_sevenline[pos].ok && phase2a_candidate_sevenline[pos].delay == 1) {
                        d1_top2_index = choice_order[i];
                        break;
                    }
                }
                if (d1_top2_index >= 0 && d1_top2_index != final_best_index_pre_d1) {
                    int final_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, final_best_index_pre_d1);
                    int loss_reason = AI_SEVENLINE_D1_LOSS_OTHER;
                    if (final_pos >= 0) {
                        loss_reason = sevenline_d1_loss_reason_from_phase2a(phase2a_candidate_reason[final_pos]);
                    }
                    ai_debug_note_sevenline_d1_loss_reason(player, loss_reason);
                }
            }
#endif
        }
        if (any_certain_seven_plus_triggered) {
            ai_debug_note_certain_seven_plus(player, final_best_index != final_best_index_without_certain ? ON : OFF);
        }
        if (any_combo7_triggered) {
            ai_debug_note_combo7(player, str.mode, final_best_index != final_best_index_without_combo7 ? ON : OFF);
            ai_debug_note_combo7_margin(player, best_combo7_bonus - second_combo7_bonus,
                                        (best_combo7_bonus - second_combo7_bonus) <= COMBO7_MARGIN_FLIP_THRESHOLD ? ON : OFF);
            for (int i = 0; i < greedy_candidate_count; i++) {
                if (greedy_candidates[i].index == final_best_index && greedy_candidates[i].combo7_plus1_rank > 0) {
                    ai_debug_note_best_plus1_used_rank(player, greedy_candidates[i].combo7_plus1_rank);
                    break;
                }
            }
        }
        if (any_keytarget_expose_triggered) {
            ai_debug_note_keytarget_expose(player, final_best_index != final_best_index_without_keycard ? ON : OFF);
            if (final_best_index != final_best_index_without_keycard && final_best_index_without_keycard >= 0) {
                DropCandidateScore* denied = NULL;
                int after_card_no = -1;
                for (int i = 0; i < greedy_candidate_count; i++) {
                    if (greedy_candidates[i].index == final_best_index_without_keycard) {
                        denied = &greedy_candidates[i];
                    }
                    if (greedy_candidates[i].index == final_best_index) {
                        after_card_no = greedy_candidates[i].card_no;
                    }
                }
                if (denied && denied->keytarget_expose_penalty > 0 && denied->keytarget_expose_wid >= 0 && denied->keytarget_expose_month >= 0) {
                    ai_putlog("[deny-expose] seed=%d round=%d turn=%d drop=%d key_month=%d key_role=%s penalty=%d before=%d after=%d",
                              ai_debug_current_seed(), g.round + 1, 9 - g.own[player].num, denied->card_no, denied->keytarget_expose_month + 1,
                              winning_hands[denied->keytarget_expose_wid].name, denied->keytarget_expose_penalty, denied->card_no, after_card_no);
                }
            }
        }
        ai_debug_note_endgame(player, calc_endgame_k(&str), final_best_index != final_best_index_without_endgame, calc_endgame_needed(&str));
        if (action_mc_result.compare_triggered) {
            ai_debug_note_action_mc_best_gap(player, compare_gap, action_mc_result.best_qdiff - action_mc_result.second_qdiff);
            if (action_mc_result.compare_active && action_mc_result.before_best_index >= 0 && final_best_index != action_mc_result.before_best_index) {
                ai_debug_note_action_mc_changed_choice(player, action_mc_result.chosen_material);
                ai_putlog("[action-mc-compare] seed=%d game=%d round=%d turn=%d player=%d best_idx=%d second_idx=%d gap=%d drop_before=%d drop_after=%d material=%s QA=%d QB=%d qdiffA=%d qdiffB=%d trigger_qdiff=%d q_margin=%d",
                          ai_debug_current_seed(), ai_debug_current_game_index(), g.round + 1, 9 - g.own[player].num, player,
                          action_mc_result.before_best_index, action_mc_result.before_second_index, compare_gap,
                          action_mc_result.before_best_card_no, action_mc_result.chosen_card_no, drop_action_mc_material_name(action_mc_result.chosen_material),
                          action_mc_result.best_qdiff, action_mc_result.second_qdiff, action_mc_result.best_qdiff, action_mc_result.second_qdiff,
                          action_mc_result.trigger_qdiff, DROP_ACTION_MC_Q_MARGIN);
            }
        }
        if (env_mc_result.compare_triggered && env_mc_result.changed_choice) {
            ai_putlog("[env-mc] seed=%d round=%d turn=%d player=%d mode=%s best=%d second=%d Ebest=%d Esecond=%d before_gap=%d after_gap=%d",
                      ai_debug_current_seed(), g.round + 1, 9 - g.own[player].num, player, strategy_mode_name(str.mode), env_mc_result.before_best_index,
                      env_mc_result.before_second_index, env_mc_result.best_E, env_mc_result.second_E, env_mc_result.before_gap, env_mc_result.after_gap);
        }
        best_index = final_best_index;
        trace_decision.final_best_index = final_best_index;
    }

    {
        int top2[2];
        int top2_count = collect_drop_choice_order_indices(greedy_candidates, greedy_candidate_count, DROP_GREEDY_VALUE_NORMAL, fallback_index, top2, 2);
        int best_pos = top2_count > 0 ? find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, top2[0]) : -1;
        int second_pos = top2_count > 1 ? find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, top2[1]) : -1;
#if WATCH_DROP_METRICS_ENABLE
        if (best_pos >= 0) {
            int gap = 0;
            int final_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, trace_decision.final_best_index);
            char best_take[24];
            char second_take[24];
            char best_feed[96];
            char second_feed[96];
            if (second_pos >= 0) {
                gap = greedy_candidates[best_pos].value - greedy_candidates[second_pos].value;
            }
            format_drop_fallback_take(best_take, sizeof(best_take), greedy_candidates[best_pos].card_no, greedy_candidates[best_pos].taken_card_no);
            format_drop_fallback_take(second_take, sizeof(second_take), second_pos >= 0 ? greedy_candidates[second_pos].card_no : -1,
                                      second_pos >= 0 ? greedy_candidates[second_pos].taken_card_no : -1);
            format_drop_feed(best_feed, sizeof(best_feed), &greedy_candidates[best_pos]);
            format_drop_feed(second_feed, sizeof(second_feed), second_pos >= 0 ? &greedy_candidates[second_pos] : NULL);
            ai_putlog(
                "[DROP-TOP2] best_idx=%d best_card=%d best_value=%d second_idx=%d second_card=%d second_value=%d gap=%d best_take=%s best_feed=%s second_take=%s second_feed=%s final_pick=%s why=%s aim=%s(wid=%s)",
                greedy_candidates[best_pos].index, greedy_candidates[best_pos].card_no, greedy_candidates[best_pos].value,
                second_pos >= 0 ? greedy_candidates[second_pos].index : -1, second_pos >= 0 ? greedy_candidates[second_pos].card_no : -1,
                second_pos >= 0 ? greedy_candidates[second_pos].value : 0, gap, best_take, best_feed, second_take, second_feed,
                trace_decision.final_best_index == greedy_candidates[best_pos].index
                    ? "best"
                    : (second_pos >= 0 && trace_decision.final_best_index == greedy_candidates[second_pos].index ? "second" : "override"),
                trace_reason_short(&trace_decision), trace_aim_type_name(final_pos >= 0 ? greedy_candidates[final_pos].aim_type : TRACE_AIM_NONE),
                (final_pos >= 0 && greedy_candidates[final_pos].aim_wid >= 0) ? winning_hands[greedy_candidates[final_pos].aim_wid].name : "NONE");
        }
#endif
    }

    // 思考結果をログへ出力（デバッグ用）
    ai_putlog("Priority-Speed:");
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        int w = str.priority_speed[i];
        if (str.reach[w] < 1) {
            break;
        }
        ai_putlog("- %s: reach=%d, delay=%d, score=%d", winning_hands[w].name, str.reach[w], str.delay[w], str.score[w]);
    }
    ai_putlog("Priority-Score:");
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        int w = str.priority_score[i];
        if (str.reach[w] < 1) {
            break;
        }
        ai_putlog("- %s: reach=%d, delay=%d, score=%d", winning_hands[w].name, str.reach[w], str.delay[w], str.score[w]);
    }
    ai_putlog("Risk:");
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        if (str.risk_reach_estimate[i]) {
            ai_putlog("- %s: reach=%d, delay=%d, score=%d", winning_hands[i].name, str.risk_reach_estimate[i], str.risk_delay[i], str.risk_score[i]);
        }
    }
    ai_putlog("Context:");
    ai_putlog("- mode=%s", strategy_mode_name(str.mode));
    ai_putlog("- match_score_diff=%d left_rounds=%d left_own=%d", str.match_score_diff, str.left_rounds, str.left_own);
    ai_putlog("- opp_coarse_threat=%d opp_exact_7plus_threat=%d risk_mult=%d", str.opp_coarse_threat, str.opp_exact_7plus_threat, str.risk_mult_pct);
    ai_putlog("- greedy_need=%d defensive_need=%d", str.greedy_need, str.defensive_need);
    ai_putlog("Bias:");
    ai_putlog("- offence: %d", str.bias.offence);
    ai_putlog("- speed: %d", str.bias.speed);
    ai_putlog("- defence: %d", str.bias.defence);
    {
        int top_idx[5];
        int top_val[5];
        ai_env_cat_topN(str.env_cat_sum, 5, top_idx, top_val);
        ai_putlog("Env:");
        ai_putlog("- total: %d", str.env_total);
        if (str.env_diff >= 0) {
            ai_putlog("- diff: +%d", str.env_diff);
        } else {
            ai_putlog("- diff: %d", str.env_diff);
        }
        ai_putlog("- domain: %s", env_domain_name(str.domain));
        ai_putlog("- plan: %s", env_plan_name(str.plan));
        ai_putlog("- cat_sum: CAT1=%d CAT2=%d CAT3=%d CAT4=%d CAT5=%d CAT6=%d CAT7=%d CAT11=%d CAT12=%d NA=%d",
                  str.env_cat_sum[ENV_CAT_1], str.env_cat_sum[ENV_CAT_2], str.env_cat_sum[ENV_CAT_3], str.env_cat_sum[ENV_CAT_4],
                  str.env_cat_sum[ENV_CAT_5], str.env_cat_sum[ENV_CAT_6], str.env_cat_sum[ENV_CAT_7], str.env_cat_sum[ENV_CAT_11],
                  str.env_cat_sum[ENV_CAT_12], str.env_cat_sum[ENV_CAT_NA]);
        ai_putlog("- cat_top: %s=%d %s=%d %s=%d %s=%d %s=%d",
                  env_category_name(top_idx[0]), top_val[0], env_category_name(top_idx[1]), top_val[1], env_category_name(top_idx[2]), top_val[2],
                  env_category_name(top_idx[3]), top_val[3], env_category_name(top_idx[4]), top_val[4]);
    }
    ai_putlog("Think End\n");

    ai_think_end();

    // 今回の分析結果をバックアップ
    vgs_memcpy(&g.strategy[player], &str, sizeof(StrategyData));

    if (best_index < 0) {
        ai_set_last_think_extra(player, "");
        return fallback_index;
    }
    {
        int chosen_pos = find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, best_index);
        int chosen_all_pos = chosen_pos;
        int order[2];
        int order_count = collect_drop_choice_order_indices(greedy_candidates, greedy_candidate_count, DROP_GREEDY_VALUE_NORMAL, fallback_index, order, 2);
        int second_pos = order_count > 1 ? find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, order[1]) : -1;
        int chosen_imm = chosen_pos >= 0 ? greedy_candidates[chosen_pos].immediate_gain : 0;
        int chosen_clinch = chosen_pos >= 0 ? greedy_candidates[chosen_pos].endgame_clinch_score : 0;
        int chosen_needed = chosen_pos >= 0 ? greedy_candidates[chosen_pos].endgame_needed : calc_endgame_needed(&str);
        int chosen_k = chosen_pos >= 0 ? greedy_candidates[chosen_pos].endgame_k : calc_endgame_k(&str);
        int second_imm = second_pos >= 0 ? greedy_candidates[second_pos].immediate_gain : 0;
        int second_clinch = second_pos >= 0 ? greedy_candidates[second_pos].endgame_clinch_score : 0;
        int imm_win_base = 0;
        int imm_win_wid = -1;
        int pre_best_idx = order_count > 0 ? order[0] : -1;
        int pre_best_value = chosen_pos >= 0 && pre_best_idx == greedy_candidates[chosen_pos].index
                                 ? greedy_candidates[chosen_pos].value
                                 : (order_count > 0 ? greedy_candidates[find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, order[0])].value : 0);
        int pre_second_idx = order_count > 1 ? order[1] : -1;
        int pre_second_value = second_pos >= 0 ? greedy_candidates[second_pos].value : 0;
        int pre_gap = (order_count > 1 && second_pos >= 0)
                          ? (greedy_candidates[find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, order[0])].value -
                             greedy_candidates[second_pos].value)
                          : 0;
        int final_value = 0;
        const char* final_by = "HARD";
        char best_take[24];
        char second_take[24];
        char best_feed_metric[24];
        char second_feed_metric[24];
        char base_extra[768];

        if (chosen_all_pos < 0) {
            chosen_all_pos = find_drop_candidate_pos_by_index(all_candidates, all_candidate_count, best_index);
        }
        if (chosen_all_pos >= 0) {
            if (chosen_pos >= 0) {
                final_value = greedy_candidates[chosen_pos].value;
            } else if (chosen_all_pos < all_candidate_count) {
                final_value = all_candidates[chosen_all_pos].value;
                chosen_imm = all_candidates[chosen_all_pos].immediate_gain;
                chosen_clinch = all_candidates[chosen_all_pos].endgame_clinch_score;
                chosen_needed = all_candidates[chosen_all_pos].endgame_needed;
                chosen_k = all_candidates[chosen_all_pos].endgame_k;
            }
        }
        for (int ci = 0; ci < all_candidate_count; ci++) {
            if (all_candidates[ci].immediate_gain > imm_win_base) {
                imm_win_base = all_candidates[ci].immediate_gain;
                imm_win_wid = all_candidates[ci].aim_type == TRACE_AIM_IMMEDIATE ? all_candidates[ci].aim_wid : all_candidates[ci].opp_immediate_wid;
                if (imm_win_wid < 0) {
                    imm_win_wid = all_candidates[ci].aim_wid;
                }
            }
        }
        if (trace_decision.final_best_index != pre_best_idx) {
            if (trace_decision.d1_changed) {
                final_by = "D1";
            } else if (trace_decision.action_mc_changed || trace_decision.env_mc_changed ||
                       trace_str_eq(trace_decision.reason_code, "ACTION_MC_COMPARE") || trace_str_eq(trace_decision.reason_code, "ENV_MC_COMPARE")) {
                final_by = "MC";
            } else if (trace_str_eq(trace_decision.reason_code, "GREEDY_FALLBACK")) {
                final_by = "FALLBACK";
            } else {
                final_by = "OTHER";
            }
        }
        format_drop_fallback_take(best_take, sizeof(best_take), order_count > 0 ? greedy_candidates[find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, order[0])].card_no : -1,
                                  order_count > 0 ? greedy_candidates[find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, order[0])].taken_card_no : -1);
        format_drop_fallback_take(second_take, sizeof(second_take), second_pos >= 0 ? greedy_candidates[second_pos].card_no : -1,
                                  second_pos >= 0 ? greedy_candidates[second_pos].taken_card_no : -1);
        format_drop_feed_metric(best_feed_metric, sizeof(best_feed_metric), player,
                                order_count > 0 ? greedy_candidates[find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, order[0])].card_no : -1,
                                order_count > 0 ? greedy_candidates[find_drop_candidate_pos_by_index(greedy_candidates, greedy_candidate_count, order[0])].taken_card_no : -1);
        format_drop_feed_metric(second_feed_metric, sizeof(second_feed_metric), player, second_pos >= 0 ? greedy_candidates[second_pos].card_no : -1,
                                second_pos >= 0 ? greedy_candidates[second_pos].taken_card_no : -1);

        ai_set_last_think_context(player, &str, chosen_needed, chosen_k, chosen_imm, second_imm, chosen_clinch, second_clinch);
        vgs_strcpy(base_extra, ai_get_last_think_extra(player));
        ai_set_last_think_extra(
            player,
            "%s <M pre_best=%d/%d pre_2nd=%d/%d gap=%d opp:%d/%d/%d/%d imm:%d/%s final=%d/%d by=%s best_take=%s best_feed=%s second_take=%s second_feed=%s>",
            base_extra, pre_best_idx, pre_best_value, pre_second_idx, pre_second_value, pre_gap, str.koikoi_opp, g.stats[1 - player].score,
            str.opponent_win_x2, str.opp_coarse_threat, imm_win_base, (imm_win_wid >= 0 && imm_win_wid < WINNING_HAND_MAX) ? winning_hands[imm_win_wid].name : "NONE",
            best_index, final_value, final_by, best_take, best_feed_metric, second_take, second_feed_metric);
    }
    return best_index;
}
