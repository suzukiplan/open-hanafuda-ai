#include "ai.h"
// select 思考ログを出して選択理由を追えるようにする。
#include <limits.h>

typedef struct {
    int gokou;
    int shikou;
    int tsukimi;
    int hanami;
    int isc;
    int akatan;
    int aotan;
    int tane;
    int tan;
    int kasu;
} OwnYakuCounts;

#define HARD_RAPACIOUS_FALLBACK_ENABLE 1
#define HARD_RAPACIOUS_FALLBACK_BEHIND_THRESHOLD 32

// select前にstrategy更新
#define SELECT_STRATEGY_UPDATE 1

// tactical の局所評価は補助に留め、即成立役の獲得点を主導させる。
// tactical の局所読みを全体評価へ混ぜる基本重み。
#define TACTICAL_WEIGHT 10
// 即成立役の点を bias より優先して拾わせるための大きな重み。
#define IMMEDIATE_GAIN_WEIGHT 20000
// greedy fallback で Normal 評価にかける上位候補数。
#define GREEDY_FALLBACK_TOP_K 7
// DEFENSIVE 時に deny 系価値を追加で重くする量。
#define SELECT_MODE_DEFENCE_BONUS 20
// GREEDY 時に攻撃評価を少し押し上げる量。
#define SELECT_MODE_GREEDY_OFFENCE_BONUS 10
// 相手 KOIKOI 中に勝ち筋を取れる select を強く優遇する加点。
#define SELECT_OPP_KOIKOI_WIN_BONUS 100000
// 相手 KOIKOI 中の勝ち筋を見逃す select への減点。
#define SELECT_OPP_KOIKOI_MISS_PENALTY 40000
// 相手 KOIKOI 中に次手即負けを招く候補を実質除外する大減点。
#define SELECT_2PLY_KOIKOI_OPP_PRUNE_PENALTY 3000000
// 最終ラウンドで逆転不能な即時あがりを避ける大減点。
#define SELECT_FINAL_NON_CLINCH_IMMEDIATE_PENALTY 3000000
// 相手の次手 7+ を許す候補を tie-break レベルで落とす減点。
#define SELECT_2PLY_SEVEN_PLUS_TIEBREAK_PENALTY 117500
// 相手の次手 7+ が確定級の候補を強く弾く減点。
#define SELECT_2PLY_CERTAIN_SEVEN_PLUS_PENALTY 700000
// 次手 7+ の 2-ply 読みを起動する脅威閾値。
#define SELECT_2PLY_SEVEN_PLUS_GATE_THREAT 60
// 次手 7+ の 2-ply 読みを起動する推定距離閾値。
#define SELECT_2PLY_SEVEN_PLUS_GATE_DISTANCE 2
// 2〜3手先の 7+ 育成を直接潰す評価の、脅威差分 1 点あたりの重み。
#define SELECT_NEAR_SEVEN_PLUS_THREAT_UNIT 1200
// 2〜3手先の 7+ 育成を直接潰す評価の、距離差分 1 手あたりの重み。
#define SELECT_NEAR_SEVEN_PLUS_DISTANCE_UNIT 28000
// 相手 7+ 距離が 1 手まで縮んだままの候補への追加減点。
#define SELECT_NEAR_SEVEN_PLUS_CRITICAL_PENALTY 70000
// 相手 7+ 距離が 2 手以内のままの候補への追加減点。
#define SELECT_NEAR_SEVEN_PLUS_NEAR_PENALTY 26000
// 相手 7+ 脅威が高いままの候補への追加減点。
#define SELECT_NEAR_SEVEN_PLUS_HIGH_THREAT_PENALTY 32000
// 相手 7+ 脅威が中程度のままの候補への追加減点。
#define SELECT_NEAR_SEVEN_PLUS_MID_THREAT_PENALTY 14000
// 相手 KOIKOI 中に 7+ 距離を詰めたままの候補への追加減点。
#define SELECT_NEAR_SEVEN_PLUS_KOIKOI_PENALTY 40000
// 相手が 6 点以上で KOIKOI 中のとき、+1 ルート（タネ/タン/カス）を減らす価値。
#define SELECT_EMERGENCY_ONE_POINT_ROUTE_BONUS 900
#define SELECT_EMERGENCY_ONE_POINT_SHUTOUT_BONUS 1400
#define SELECT_EMERGENCY_ONE_POINT_REACH_UNIT 22
#define SELECT_EMERGENCY_ONE_POINT_DELAY_UNIT 380
#define SELECT_EMERGENCY_ONE_POINT_STILL_CRITICAL_PENALTY 500
#define SELECT_EMERGENCY_ONE_POINT_STILL_NEAR_PENALTY 180
// GREEDY で 2 手以内の 7+ 到達筋を強く押す加点。
#define SELECT_GREEDY_7PLUS_BONUS 80000
// GREEDY で 3 手目の 7+ 到達筋だけを軽く押す加点。
#define SELECT_GREEDY_7PLUS_NEAR_BONUS 25000
// GREEDY で遠い 7+ 到達筋にだけ最小限残す加点。
#define SELECT_GREEDY_7PLUS_FAR_BONUS 2000
// 高確率で役素材になる札の価値を押し上げる加点。
#define SELECT_HOT_MATERIAL_BONUS 18000
// GREEDY 時に高確率で役素材になる札の価値をさらに押し上げる加点。
#define SELECT_HOT_MATERIAL_GREEDY_BONUS 30000
// overpay 用の素材確保を tie-break を超えて押す加点。
#define SELECT_OVERPAY_SETUP_BONUS 42000
// overpay が見えている時に base=5 即成立を少し抑える減点。
#define SELECT_OVERPAY_IMMEDIATE_DELAY_PENALTY 36000
// 前半に高確度 5 点役をあえて遅らせる後押し。
#define SELECT_EARLY_FIVE_POINT_DELAY_PENALTY 120000
#define SELECT_EARLY_FIVE_POINT_DELAY_SETUP_BONUS 18000
#define SELECT_EARLY_FIVE_POINT_DELAY_TURN_MAX 3
#define SELECT_EARLY_FIVE_POINT_DELAY_MIN_REACH 20
#define SELECT_EARLY_FIVE_POINT_DELAY_MAX_DELAY 3
// 3枚で成立する 5 点役は、2枚目でのリーチ化を特に強く、1枚目も中程度に押す。
#define SELECT_THREE_CARD_FIVE_POINT_REACH_SETUP_BONUS 19000
#define SELECT_THREE_CARD_FIVE_POINT_REACH_REACH_UNIT 20
#define SELECT_THREE_CARD_FIVE_POINT_FIRST_SETUP_BONUS 12000
#define SELECT_THREE_CARD_FIVE_POINT_FIRST_REACH_UNIT 8
#define SELECT_THREE_CARD_FIVE_POINT_FAST_DELAY_BONUS 240

// 同月4枚のうち3枚を把握している「月ロック」を取る加点。
#define SELECT_MONTH_LOCK_BONUS 1500
// 月ロックを取り逃がす減点。
#define SELECT_MONTH_LOCK_MISS_PENALTY 18000
// 裏の本命札が高価値役のキーカードなら追加で押す加点。
#define SELECT_MONTH_LOCK_HIDDEN_KEY_BONUS 28000
// 裏の本命札が相手危険役にも絡むならさらに押す加点。
#define SELECT_MONTH_LOCK_HIDDEN_DENY_BONUS 64000
// combo7 生値へ掛ける基本重み。
#define SELECT_COMBO7_WEIGHT 1
static int count_known_month_cards_for_player(int player, int month);
static int count_owned_month_cards(int player, int month);
static int count_floor_month_cards(int month);
static int player_has_hand_card(int player, int card_no);
static int is_hidden_month_lock_key_card_for_player(int player, int card_no);
// GREEDY 時は combo7 を主項として押し上げる。
#define COMBO7_GREEDY_MULT 140
// BALANCED 時は tactical より少し強い程度に留める。
#define COMBO7_BALANCED_MULT 70
// DEFENSIVE 時は即勝ち・防御を崩さないよう抑える。
#define COMBO7_DEFENSIVE_MULT 0
// GREEDY かつ安全な押し込み局面では combo7 を主項寄りに昇格する。
#define COMBO7_PRIORITY_PUSH_MULT 130
// combo7 寄与差がこの値以下なら「重み変更で反転しやすい」局面とみなす。
#define COMBO7_MARGIN_FLIP_THRESHOLD 4000
typedef struct {
    int deck_index;
    int taken_card_no;
    int value;
    int value_without_certain;
    int value_without_combo7;
    int immediate_gain;
    int endgame_clinch_score;
    int endgame_needed;
    int endgame_k;
    int combo7_bonus;
    int combo7_plus1_rank;
    int risk_sum;
    int self_min_delay;
    int self_max_score;
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
    int seven_ok;
    int seven_delay;
    int seven_margin;
    int seven_reach;
    int seven_wid;
    int aim_type;
    int aim_wid;
    const char* prune_reason_code;
} SelectCandidateScore;
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
static int is_dangerous_risk_wid(const StrategyData* s, int wid);
static int calc_normal_select_greedy_score(int player, int deck_index);
static const char* strategy_mode_name(int mode);
static const char* env_domain_name(AiEnvDomain domain);
static const char* env_plan_name(AiPlan plan);
static int trace_str_eq(const char* lhs, const char* rhs);
static int collect_select_choice_order_indices(const SelectCandidateScore* candidates, int count, int value_mode, int fallback_index, int* out_indices, int cap);
static int trace_min_risk_delay(const StrategyData* s);
static int trace_max_risk_score(const StrategyData* s);
static const char* trace_side_name(int player);
static const char* trace_card_type_name(int card_no);
static const char* trace_aim_type_name(int aim_type);
static const char* trace_reason_short(const char* reason_code);
static void format_select_take(char* dst, int cap, int card_no, int taken_card_no);
static void format_select_feed(char* dst, int cap, const SelectCandidateScore* candidate);
static void describe_best_seven_plus_line(int round_score, const StrategyData* s, int* out_ok, int* out_delay, int* out_margin, int* out_reach, int* out_wid);
static void emit_select_trace(int player, Card* card, const StrategyData* before, const SelectCandidateScore* candidates, int count, int fallback_index,
                              int final_best_index, const char* reason_code);

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
    return OFF;
    if (!hard_has_rapacious_fallback_sake_base(player) || hard_should_disable_rapacious_fallback_sake(s)) {
        return OFF;
    }
    return g.koikoi[1 - player] ? OFF : ON;
}
#endif

static int is_runtime_card_ptr_valid(const Card* card)
{
    unsigned long addr;
    unsigned long base;
    unsigned long end;

    if (!card) {
        return OFF;
    }

    addr = (unsigned long)card;
    base = (unsigned long)&g.cards[0];
    end = (unsigned long)&g.cards[48];
    if (addr < base || addr >= end) {
        return OFF;
    }
    if (((addr - base) % sizeof(Card)) != 0u) {
        return OFF;
    }
    if (card->id < 0 || card->id >= 48) {
        return OFF;
    }
    if (&g.cards[card->id] != card) {
        return OFF;
    }
    return ON;
}

static Card* safe_deck_card_at(int deck_index)
{
    Card* card;

    if (deck_index < 0 || deck_index >= 48) {
        return NULL;
    }
    card = g.deck.cards[deck_index];
    return is_runtime_card_ptr_valid(card) ? card : NULL;
}

enum SelectGreedyValueMode {
    SELECT_GREEDY_VALUE_NORMAL = 0,
    SELECT_GREEDY_VALUE_WITHOUT_CERTAIN = 1,
    SELECT_GREEDY_VALUE_WITHOUT_COMBO7 = 2,
    SELECT_GREEDY_VALUE_WITHOUT_ENDGAME = 3,
};

static int select_candidate_value(const SelectCandidateScore* candidate, int value_mode)
{
    if (!candidate) {
        return INT_MIN;
    }
    switch (value_mode) {
        case SELECT_GREEDY_VALUE_WITHOUT_CERTAIN: return candidate->value_without_certain;
        case SELECT_GREEDY_VALUE_WITHOUT_COMBO7: return candidate->value_without_combo7;
        case SELECT_GREEDY_VALUE_WITHOUT_ENDGAME: return candidate->value - candidate->endgame_clinch_score;
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

static const char* trace_reason_short(const char* reason_code)
{
    if (!reason_code || !reason_code[0] || trace_str_eq(reason_code, "BASE_COMPARATOR")) {
        return "comparator";
    }
    if (trace_str_eq(reason_code, "GREEDY_FALLBACK")) {
        return "fallback";
    }
    return "override";
}

static void format_select_take(char* dst, int cap, int card_no, int taken_card_no)
{
    char num[16];

    if (!dst || cap <= 0) {
        return;
    }
    if (card_no < 0 || taken_card_no < 0) {
        vgs_strcpy(dst, "n/a");
        return;
    }
    (void)cap;
    dst[0] = '\0';
    vgs_d32str(num, card_no);
    vgs_strcat(dst, num);
    vgs_strcat(dst, ",");
    vgs_d32str(num, taken_card_no);
    vgs_strcat(dst, num);
}

static void format_select_feed(char* dst, int cap, const SelectCandidateScore* candidate)
{
    char num[16];

    if (!dst || cap <= 0) {
        return;
    }
    if (!candidate) {
        vgs_strcpy(dst, "win=OFF opp7=dist4+/th0/c0");
        return;
    }
    (void)cap;
    dst[0] = '\0';
    if (candidate->opp_immediate_win) {
        vgs_strcat(dst, "win=ON(wid=");
        vgs_strcat(dst, (candidate->opp_immediate_wid >= 0 && candidate->opp_immediate_wid < WINNING_HAND_MAX)
                            ? winning_hands[candidate->opp_immediate_wid].name
                            : "NONE");
        vgs_strcat(dst, ",base=");
        vgs_d32str(num, candidate->opp_immediate_score);
        vgs_strcat(dst, num);
        vgs_strcat(dst, ",x2=");
        vgs_d32str(num, candidate->opp_immediate_x2);
        vgs_strcat(dst, num);
        vgs_strcat(dst, ")");
    } else {
        vgs_strcat(dst, "win=OFF");
    }
    vgs_strcat(dst, " opp7=dist");
    if (candidate->opp7_dist >= 4) {
        vgs_strcat(dst, "4+");
    } else {
        vgs_d32str(num, candidate->opp7_dist);
        vgs_strcat(dst, num);
    }
    vgs_strcat(dst, "/th");
    vgs_d32str(num, candidate->opp7_threat);
    vgs_strcat(dst, num);
    vgs_strcat(dst, "/c");
    vgs_d32str(num, candidate->opp7_certain);
    vgs_strcat(dst, num);
}

static int collect_select_choice_order_indices(const SelectCandidateScore* candidates, int count, int value_mode, int fallback_index, int* out_indices, int cap)
{
    int used[8];
    int collected = 0;

    if (!candidates || !out_indices || count <= 0 || cap <= 0) {
        return 0;
    }
    vgs_memset(used, 0, sizeof(used));
    while (collected < cap) {
        int chosen = -1;
        int chosen_value = INT_MIN;
        int chosen_risk = INT_MAX;
        int chosen_delay = 99;
        int chosen_score = 0;

        for (int i = 0; i < count; i++) {
            int value;

            if (used[i]) {
                continue;
            }
            value = select_candidate_value(&candidates[i], value_mode);
            if (chosen < 0 || value > chosen_value ||
                (value == chosen_value &&
                 (candidates[i].risk_sum < chosen_risk ||
                  (candidates[i].risk_sum == chosen_risk &&
                   (candidates[i].self_min_delay < chosen_delay ||
                    (candidates[i].self_min_delay == chosen_delay &&
                     (candidates[i].self_max_score > chosen_score ||
                      (candidates[i].self_max_score == chosen_score &&
                       ((candidates[i].deck_index == fallback_index && chosen != fallback_index) || candidates[i].deck_index < chosen))))))))) {
                chosen = candidates[i].deck_index;
                chosen_value = value;
                chosen_risk = candidates[i].risk_sum;
                chosen_delay = candidates[i].self_min_delay;
                chosen_score = candidates[i].self_max_score;
            }
        }

        if (chosen < 0) {
            break;
        }
        out_indices[collected++] = chosen;
        for (int i = 0; i < count; i++) {
            if (candidates[i].deck_index == chosen) {
                used[i] = ON;
                break;
            }
        }
    }
    return collected;
}

static int trace_min_risk_delay(const StrategyData* s)
{
    int min_delay = 99;

    if (!s) {
        return 99;
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

static void describe_best_seven_plus_line(int round_score, const StrategyData* s, int* out_ok, int* out_delay, int* out_margin, int* out_reach, int* out_wid)
{
    int best_wid = -1;
    int best_delay = 99;
    int best_reach = 0;
    int best_margin = -99;

    if (out_ok) {
        *out_ok = OFF;
    }
    if (out_delay) {
        *out_delay = 99;
    }
    if (out_margin) {
        *out_margin = -99;
    }
    if (out_reach) {
        *out_reach = 0;
    }
    if (out_wid) {
        *out_wid = -1;
    }
    if (!s) {
        return;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int total_score;

        if (s->reach[wid] <= 0 || s->score[wid] <= 0 || s->delay[wid] >= 99) {
            continue;
        }
        total_score = round_score + s->score[wid];
        if (total_score < 7) {
            continue;
        }
        if (best_wid < 0 || s->delay[wid] < best_delay || (s->delay[wid] == best_delay && s->reach[wid] > best_reach)) {
            best_wid = wid;
            best_delay = s->delay[wid];
            best_reach = s->reach[wid];
            best_margin = total_score - 7;
        }
    }
    if (best_wid >= 0) {
        if (out_ok) {
            *out_ok = ON;
        }
        if (out_delay) {
            *out_delay = best_delay;
        }
        if (out_margin) {
            *out_margin = best_margin;
        }
        if (out_reach) {
            *out_reach = best_reach;
        }
        if (out_wid) {
            *out_wid = best_wid;
        }
    }
}

static void emit_select_trace(int player, Card* card, const StrategyData* before, const SelectCandidateScore* candidates, int count, int fallback_index,
                              int final_best_index, const char* reason_code)
{
#if WATCH_TRACE_ENABLE
    int order[8];
    int full_order[8];
    int order_count;
    int full_order_count;
    int top_value = 0;
    int chosen_rank = 0;
    int chosen_gap = 0;
    int env_p1;
    int env_cpu;
    int chosen_pos = -1;
    int best_pos = -1;
    int second_pos = -1;
    char best_take[24];
    char second_take[24];
    char best_feed[96];
    char second_feed[96];

    if (!g.auto_play || !card || !before || !candidates || count <= 0) {
        return;
    }

    order_count = collect_select_choice_order_indices(candidates, count, SELECT_GREEDY_VALUE_NORMAL, fallback_index, order, WATCH_TRACE_SELECT_TOP_K);
    full_order_count = collect_select_choice_order_indices(candidates, count, SELECT_GREEDY_VALUE_NORMAL, fallback_index, full_order, 8);
    env_p1 = ai_env_score(0);
    env_cpu = ai_env_score(1);
    if (full_order_count > 0) {
        for (int i = 0; i < full_order_count; i++) {
            if (full_order[i] == final_best_index) {
                chosen_rank = i + 1;
                break;
            }
        }
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].deck_index == final_best_index) {
            chosen_pos = i;
            break;
        }
    }
    if (order_count > 0) {
        for (int i = 0; i < count; i++) {
            if (candidates[i].deck_index == order[0]) {
                top_value = candidates[i].value;
                best_pos = i;
                break;
            }
        }
    }
    if (order_count > 1) {
        int second_value = 0;
        for (int i = 0; i < count; i++) {
            if (candidates[i].deck_index == order[1]) {
                second_value = candidates[i].value;
                second_pos = i;
                break;
            }
        }
        chosen_gap = (chosen_rank == 1) ? (top_value - second_value) : (chosen_pos >= 0 ? top_value - candidates[chosen_pos].value : 0);
    }

    ai_putlog("[HARD-TRACE] side=%s round=%d turn=%d phase=select mode=%s plan=%s domain=%s env=P1=%d CPU=%d diff=%d fallback=%d",
              trace_side_name(player), g.round + 1, 9 - g.own[player].num, strategy_mode_name(before->mode), env_plan_name(before->plan),
              env_domain_name(before->domain), env_p1, env_cpu, env_p1 - env_cpu, fallback_index);
    ai_putlog("[HARD-TOP] select_top%d:", order_count);
    for (int rank = 0; rank < order_count; rank++) {
        for (int i = 0; i < count; i++) {
            if (candidates[i].deck_index == order[rank]) {
                int gap = rank > 0 ? top_value - candidates[i].value : 0;

                ai_putlog("  #%d deck=%d take=%d,%d value=%d gap=%d mat=%s parts{score=%d speed=%d deny=%d safe=%d keep=%d tactical=%d bonus=%d}",
                          rank + 1, candidates[i].deck_index, card->id, candidates[i].taken_card_no, candidates[i].value, gap,
                          trace_card_type_name(candidates[i].taken_card_no), candidates[i].part_score, candidates[i].part_speed, candidates[i].part_deny,
                          candidates[i].part_safe, candidates[i].part_keep, candidates[i].part_tactical, candidates[i].part_bonus_total);
                ai_putlog("    prune=%s flags{certain7=%d near7=%d koikoi_prune=%d} seven{ok=%d delay=%d margin=%d reach=%d wid=%s}",
                          candidates[i].prune_reason_code ? candidates[i].prune_reason_code : "NONE", candidates[i].certain_7plus, candidates[i].near_7plus,
                          candidates[i].koikoi_opp_prune, candidates[i].seven_ok, candidates[i].seven_delay, candidates[i].seven_margin,
                          candidates[i].seven_reach, candidates[i].seven_wid >= 0 ? winning_hands[candidates[i].seven_wid].name : "NONE");
                break;
            }
        }
    }
    ai_putlog("[HARD-REASON] chosen_rank=%d chosen_gap=%d reason=%s override{d1=0/0/0 actionmc=0/0/0 phase2a=0/0 envmc=0/0} opp{koikoi=%d threat7=%d delaymin=%d scoremax=%d}",
              chosen_rank, chosen_gap, reason_code ? reason_code : "BASE_COMPARATOR", before->koikoi_opp, before->opp_coarse_threat,
              trace_min_risk_delay(before), trace_max_risk_score(before));
    ai_putlog("  seven{distance=%d margin=%d reach=%d wid=%s} final{index=%d card=%d}",
              before->risk_7plus_distance, chosen_pos >= 0 ? candidates[chosen_pos].seven_margin : -99,
              chosen_pos >= 0 ? candidates[chosen_pos].seven_reach : 0,
              (chosen_pos >= 0 && candidates[chosen_pos].seven_wid >= 0) ? winning_hands[candidates[chosen_pos].seven_wid].name : "NONE", final_best_index, card->id);
    format_select_take(best_take, sizeof(best_take), card->id, best_pos >= 0 ? candidates[best_pos].taken_card_no : -1);
    format_select_take(second_take, sizeof(second_take), card->id, second_pos >= 0 ? candidates[second_pos].taken_card_no : -1);
    format_select_feed(best_feed, sizeof(best_feed), best_pos >= 0 ? &candidates[best_pos] : NULL);
    format_select_feed(second_feed, sizeof(second_feed), second_pos >= 0 ? &candidates[second_pos] : NULL);
    ai_putlog(
        "[SELECT-TOP2] best_idx=%d best_card=%d best_value=%d second_idx=%d second_card=%d second_value=%d gap=%d best_take=%s best_feed=%s second_take=%s second_feed=%s final_pick=%s why=%s aim=%s(wid=%s)",
        best_pos >= 0 ? candidates[best_pos].deck_index : -1, best_pos >= 0 ? candidates[best_pos].taken_card_no : -1, best_pos >= 0 ? candidates[best_pos].value : 0,
        second_pos >= 0 ? candidates[second_pos].deck_index : -1, second_pos >= 0 ? candidates[second_pos].taken_card_no : -1,
        second_pos >= 0 ? candidates[second_pos].value : 0, (best_pos >= 0 && second_pos >= 0) ? (candidates[best_pos].value - candidates[second_pos].value) : 0, best_take,
        best_feed, second_take, second_feed,
        (best_pos >= 0 && candidates[best_pos].deck_index == final_best_index) ? "best"
                                                                               : ((second_pos >= 0 && candidates[second_pos].deck_index == final_best_index)
                                                                                      ? "second"
                                                                                      : "override"),
        trace_reason_short(reason_code),
        trace_aim_type_name(chosen_pos >= 0 ? candidates[chosen_pos].aim_type : TRACE_AIM_NONE),
        (chosen_pos >= 0 && candidates[chosen_pos].aim_wid >= 0) ? winning_hands[candidates[chosen_pos].aim_wid].name : "NONE");
#else
    (void)player;
    (void)card;
    (void)before;
    (void)candidates;
    (void)count;
    (void)fallback_index;
    (void)final_best_index;
    (void)reason_code;
#endif
}

static int choose_greedy_select_candidate(int player, const StrategyData* before, const SelectCandidateScore* candidates, int count, int value_mode)
{
    SelectCandidateScore sorted[8];
    int top_k;
    int greedy_best = -1;
    int greedy_best_score = INT_MIN;
    int best_immediate_gain = 0;
    int force_stop_priority = before && before->koikoi_opp == ON;

    if (!candidates || count <= 0) {
        return -1;
    }
    vgs_memcpy(sorted, candidates, sizeof(SelectCandidateScore) * count);
    for (int a = 0; a < count - 1; a++) {
        for (int b = a + 1; b < count; b++) {
            int value_a = select_candidate_value(&sorted[a], value_mode);
            int value_b = select_candidate_value(&sorted[b], value_mode);
            if (value_b > value_a) {
                SelectCandidateScore tmp = sorted[a];
                sorted[a] = sorted[b];
                sorted[b] = tmp;
            }
        }
    }
    top_k = count < GREEDY_FALLBACK_TOP_K ? count : GREEDY_FALLBACK_TOP_K;
    for (int i = 0; i < top_k; i++) {
        if (sorted[i].immediate_gain > best_immediate_gain) {
            best_immediate_gain = sorted[i].immediate_gain;
        }
    }
    for (int i = 0; i < top_k; i++) {
        int score = calc_normal_select_greedy_score(player, sorted[i].deck_index);
        int value = select_candidate_value(&sorted[i], value_mode);
        int best_value = greedy_best >= 0 ? select_candidate_value(&sorted[greedy_best], value_mode) : INT_MIN;
        if (force_stop_priority && best_immediate_gain > 0) {
            int current_gain = sorted[i].immediate_gain;
            int chosen_gain = greedy_best >= 0 ? sorted[greedy_best].immediate_gain : 0;
            if (greedy_best < 0 || current_gain > chosen_gain ||
                (current_gain == chosen_gain && (score > greedy_best_score || (score == greedy_best_score && value > best_value)))) {
                greedy_best = i;
                greedy_best_score = score;
            }
        } else if (greedy_best < 0 || score > greedy_best_score || (score == greedy_best_score && value > best_value)) {
            greedy_best = i;
            greedy_best_score = score;
        }
    }
    return greedy_best >= 0 ? sorted[greedy_best].deck_index : -1;
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

static int combo7_mode_multiplier(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return COMBO7_GREEDY_MULT;
        case MODE_DEFENSIVE: return COMBO7_DEFENSIVE_MULT;
        default: return COMBO7_BALANCED_MULT;
    }
}

static int calc_combo7_bonus(int player, int round_score, const StrategyData* s, AiCombo7Eval* out_eval)
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
    bonus = eval.value * SELECT_COMBO7_WEIGHT;
    bonus = (bonus * combo7_mode_multiplier(s ? s->mode : MODE_BALANCED)) / 100;
    if (s->mode == MODE_GREEDY && s->koikoi_opp == OFF && s->opp_coarse_threat < 60) {
        bonus = (bonus * COMBO7_PRIORITY_PUSH_MULT) / 100;
    }
    return bonus;
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
    int penalty = SELECT_OVERPAY_IMMEDIATE_DELAY_PENALTY;

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

static int should_delay_early_five_point_completion(const StrategyData* before, int player, int wid, int exclude_card_no)
{
    if (!before || player < 0 || player > 1 || !is_delayable_five_point_wid(wid)) {
        return OFF;
    }
    if (g.koikoi[0] == ON || g.koikoi[1] == ON) {
        return OFF;
    }
    if (current_round_turn_for_player(player) > SELECT_EARLY_FIVE_POINT_DELAY_TURN_MAX) {
        return OFF;
    }
    if (count_critical_hand_cards_for_wid(player, wid, exclude_card_no) <= 0) {
        return OFF;
    }
    return hand_has_finishing_card_for_wid(player, wid, exclude_card_no);
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
        return SELECT_MONTH_LOCK_BONUS;
    }
    if (owned == 2) {
        return SELECT_MONTH_LOCK_BONUS / 2;
    }
    return 0;
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
            bonus += SELECT_MONTH_LOCK_HIDDEN_KEY_BONUS;
            if (out_key_trigger) {
                *out_key_trigger = ON;
            }
            break;
        }
    }
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_speed[i];
        if (wid >= 0 && wid < WINNING_HAND_MAX && ai_is_card_critical_for_wid(hidden_card_no, wid)) {
            bonus += SELECT_MONTH_LOCK_HIDDEN_KEY_BONUS / 2;
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
            bonus += SELECT_MONTH_LOCK_HIDDEN_DENY_BONUS;
            if (out_deny_trigger) {
                *out_deny_trigger = ON;
            }
            break;
        }
    }
    if (g.cards[hidden_card_no].type == CARD_TYPE_GOKOU || g.cards[hidden_card_no].type == CARD_TYPE_TANE) {
        bonus += SELECT_MONTH_LOCK_HIDDEN_KEY_BONUS / 2;
    }
    return bonus;
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

static int calc_normal_select_greedy_score(int player, int deck_index)
{
    Card* deck_card;
    int priority;
    static const int base_capture_priority[4] = {
        400, // CARD_TYPE_GOKOU
        300, // CARD_TYPE_TANE
        200, // CARD_TYPE_TAN
        100  // CARD_TYPE_KASU
    };

    if (deck_index < 0 || deck_index >= 48) {
        return INT_MIN;
    }
    deck_card = safe_deck_card_at(deck_index);
    if (!deck_card) {
        return INT_MIN;
    }

    priority = base_capture_priority[deck_card->type] + g.invent[player][deck_card->type].num * 32;
    if (deck_card->type == CARD_TYPE_TANE && g.invent[player][deck_card->type].num >= 2) {
        priority += 1000;
    }
    if (deck_card->type == CARD_TYPE_TAN && g.invent[player][deck_card->type].num >= 2) {
        priority += 900;
    }
    if (deck_card->type == CARD_TYPE_KASU && g.invent[player][deck_card->type].num >= 5) {
        priority += 800;
    }
    for (int wi = 0; wi < g.stats[player].want.num; wi++) {
        Card* want_card = g.stats[player].want.cards[wi];
        if (want_card && want_card == deck_card) {
            priority += 10000;
            break;
        }
    }
    return priority;
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

static int calc_greedy_seven_plus_bonus(int player, const StrategyData* s)
{
    int round_score = g.stats[player].score;
    int bonus = 0;

    if (!s) {
        return 0;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (s->reach[wid] <= 0 || s->score[wid] <= 0 || s->delay[wid] >= 99) {
            continue;
        }
        if (round_score + s->score[wid] >= 7) {
            if (s->delay[wid] <= 2) {
                bonus += SELECT_GREEDY_7PLUS_BONUS;
            } else if (s->delay[wid] == 3) {
                bonus += SELECT_GREEDY_7PLUS_NEAR_BONUS;
            } else {
                bonus += SELECT_GREEDY_7PLUS_FAR_BONUS;
            }
        }
    }
    return bonus;
}

static int calc_hot_material_bonus(int card_no, const StrategyData* s)
{
    Card* card;
    int bonus;

    if (!s || card_no < 0 || card_no >= 48) {
        return 0;
    }
    card = &g.cards[card_no];
    bonus = (s->mode == MODE_GREEDY) ? SELECT_HOT_MATERIAL_GREEDY_BONUS : SELECT_HOT_MATERIAL_BONUS;

    if (card->type == CARD_TYPE_GOKOU) {
        if (card->month != 10) {
            if (is_high_likelihood_line(s, WID_GOKOU) || is_high_likelihood_line(s, WID_SHIKOU) || is_high_likelihood_line(s, WID_SANKOU)) {
                return bonus;
            }
        } else if (is_high_likelihood_line(s, WID_GOKOU) || is_high_likelihood_line(s, WID_AME_SHIKOU)) {
            return bonus / 2;
        }
    }
    if (card->type == CARD_TYPE_TANE && card->month == 8) {
        if (is_high_likelihood_line(s, WID_HANAMI) || is_high_likelihood_line(s, WID_TSUKIMI) || is_high_likelihood_line(s, WID_ISC)) {
            return bonus;
        }
    }
    if (card->type == CARD_TYPE_TANE && (card->month == 5 || card->month == 6 || card->month == 9)) {
        if (is_high_likelihood_line(s, WID_ISC)) {
            return bonus;
        }
    }
    if (card->type == CARD_TYPE_TAN && (card->month == 0 || card->month == 1 || card->month == 2)) {
        if (is_high_likelihood_line(s, WID_AKATAN) || is_high_likelihood_line(s, WID_DBTAN)) {
            return bonus;
        }
    }
    if (card->type == CARD_TYPE_TAN && (card->month == 5 || card->month == 8 || card->month == 9)) {
        if (is_high_likelihood_line(s, WID_AOTAN) || is_high_likelihood_line(s, WID_DBTAN)) {
            return bonus;
        }
    }
    return 0;
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
        if (before->risk_delay[wid] >= 99 && after->risk_delay[wid] >= 99) {
            delta_delay = 0;
        }
        total += delta_score * 100 + delta_reach * 3 + delta_delay * 35;
    }
    return total;
}

static int normalize_risk_7plus_distance(int distance)
{
    if (distance >= 99) {
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
    int best_delay = 99;
    int total_reach = 0;

    if (!s) {
        if (out_best_delay) {
            *out_best_delay = 99;
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
        if (s->risk_reach_estimate[wid] <= 0 || s->risk_delay[wid] >= 99) {
            continue;
        }
        route_count++;
        total_reach += s->risk_reach_estimate[wid];
        if (s->risk_delay[wid] < best_delay) {
            best_delay = s->risk_delay[wid];
        }
    }

    if (out_best_delay) {
        *out_best_delay = route_count > 0 ? best_delay : 99;
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

    bonus += (before_routes - after_routes) * SELECT_EMERGENCY_ONE_POINT_ROUTE_BONUS;
    bonus += (before_total_reach - after_total_reach) * SELECT_EMERGENCY_ONE_POINT_REACH_UNIT;

    if (before_best_delay < 99 && after_best_delay < 99) {
        bonus += (after_best_delay - before_best_delay) * SELECT_EMERGENCY_ONE_POINT_DELAY_UNIT;
    }
    if (before_routes > 0 && after_routes == 0) {
        bonus += SELECT_EMERGENCY_ONE_POINT_SHUTOUT_BONUS;
    }
    if (after_best_delay <= 1) {
        bonus -= SELECT_EMERGENCY_ONE_POINT_STILL_CRITICAL_PENALTY;
    } else if (after_best_delay <= 2) {
        bonus -= SELECT_EMERGENCY_ONE_POINT_STILL_NEAR_PENALTY;
    }
    return bonus;
}

static int calc_near_seven_plus_pressure_bonus(const StrategyData* before, const StrategyData* after)
{
    int before_distance;
    int after_distance;
    int threat_delta;
    int bonus = 0;

    if (!before || !after) {
        return 0;
    }
    if (before->koikoi_opp != ON && before->opp_coarse_threat < SELECT_2PLY_SEVEN_PLUS_GATE_THREAT &&
        before->risk_7plus_distance > 3) {
        return 0;
    }

    before_distance = normalize_risk_7plus_distance(before->risk_7plus_distance);
    after_distance = normalize_risk_7plus_distance(after->risk_7plus_distance);
    threat_delta = before->opp_coarse_threat - after->opp_coarse_threat;

    bonus += threat_delta * SELECT_NEAR_SEVEN_PLUS_THREAT_UNIT;
    bonus += (after_distance - before_distance) * SELECT_NEAR_SEVEN_PLUS_DISTANCE_UNIT;

    if (after->risk_7plus_distance <= 1) {
        bonus -= SELECT_NEAR_SEVEN_PLUS_CRITICAL_PENALTY;
    } else if (after->risk_7plus_distance <= 2) {
        bonus -= SELECT_NEAR_SEVEN_PLUS_NEAR_PENALTY;
    }
    if (after->opp_coarse_threat >= 80) {
        bonus -= SELECT_NEAR_SEVEN_PLUS_HIGH_THREAT_PENALTY;
    } else if (after->opp_coarse_threat >= SELECT_2PLY_SEVEN_PLUS_GATE_THREAT) {
        bonus -= SELECT_NEAR_SEVEN_PLUS_MID_THREAT_PENALTY;
    }
    if (before->koikoi_opp == ON && after->risk_7plus_distance <= 2) {
        bonus -= SELECT_NEAR_SEVEN_PLUS_KOIKOI_PENALTY;
    }
    return bonus;
}

static int sum_risk_score(const StrategyData* s)
{
    int total = 0;
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        total += s->risk_score[i];
    }
    return total;
}

static int min_valid_delay_self(const StrategyData* s)
{
    int best = 99;
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        if (s->delay[i] >= 99 || s->reach[i] <= 0) {
            continue;
        }
        if (s->delay[i] < best) {
            best = s->delay[i];
        }
    }
    return best;
}

static int max_self_score(const StrategyData* s)
{
    int best = 0;
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        if (best < s->score[i]) {
            best = s->score[i];
        }
    }
    return best;
}

static int calc_card_deny_risk_gain(int card_no, const StrategyData* before)
{
    int gain = 0;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_dangerous_risk_wid(before, wid)) {
            continue;
        }
        if (ai_is_card_critical_for_wid(card_no, wid)) {
            gain += before->risk_delay[wid] <= 2 ? 300 : 150; // (A) 危険札回収を強化
        } else if (ai_is_card_related_for_wid(card_no, wid)) {
            gain += before->risk_delay[wid] <= 2 ? 80 : 40;
        }
    }
    return gain;
}

static void add_card_to_counts(OwnYakuCounts* counts, int card_no)
{
    if (!counts || card_no < 0 || card_no >= 48) {
        return;
    }
    Card* card = &g.cards[card_no];
    switch (card->type) {
        case CARD_TYPE_GOKOU:
            counts->gokou++;
            if (card->month != 10) {
                counts->shikou++;
            }
            if (card->month == 2) {
                counts->hanami++;
            }
            if (card->month == 7) {
                counts->tsukimi++;
            }
            break;
        case CARD_TYPE_TANE:
            counts->tane++;
            if (card->month == 5 || card->month == 6 || card->month == 9) {
                counts->isc++;
            }
            if (card->month == 8) {
                counts->kasu++;
                counts->hanami++;
                counts->tsukimi++;
            }
            break;
        case CARD_TYPE_TAN:
            counts->tan++;
            if (card->month == 0 || card->month == 1 || card->month == 2) {
                counts->akatan++;
            }
            if (card->month == 5 || card->month == 8 || card->month == 9) {
                counts->aotan++;
            }
            break;
        case CARD_TYPE_KASU:
            counts->kasu++;
            break;
        default:
            break;
    }
}

static void build_own_counts(int player, OwnYakuCounts* out_counts)
{
    vgs_memset(out_counts, 0, sizeof(OwnYakuCounts));
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* c = inv->cards[i];
            if (!c) {
                continue;
            }
            add_card_to_counts(out_counts, c->id);
        }
    }
}

static int is_wid_completed_by_counts(const OwnYakuCounts* c, int wid)
{
    switch (wid) {
        case WID_GOKOU:
            return c->gokou >= 5 ? ON : OFF;
        case WID_SHIKOU:
            return c->shikou >= 4 ? ON : OFF;
        case WID_AME_SHIKOU:
            return (c->gokou >= 4 && c->shikou < 4) ? ON : OFF;
        case WID_SANKOU:
            return c->shikou >= 3 ? ON : OFF;
        case WID_HANAMI:
            return c->hanami >= 2 ? ON : OFF;
        case WID_TSUKIMI:
            return c->tsukimi >= 2 ? ON : OFF;
        case WID_ISC:
            return c->isc >= 3 ? ON : OFF;
        case WID_DBTAN:
            return (c->akatan >= 3 && c->aotan >= 3) ? ON : OFF;
        case WID_AKATAN:
            return c->akatan >= 3 ? ON : OFF;
        case WID_AOTAN:
            return c->aotan >= 3 ? ON : OFF;
        case WID_TANE:
            return c->tane >= 5 ? ON : OFF;
        case WID_TAN:
            return c->tan >= 5 ? ON : OFF;
        case WID_KASU:
            return c->kasu >= 10 ? ON : OFF;
        default:
            return OFF;
    }
}

static int calc_completed_wid_score_from_counts(const OwnYakuCounts* c, int wid)
{
    if (!c || !is_wid_completed_by_counts(c, wid)) {
        return 0;
    }

    switch (wid) {
        case WID_ISC:
            return 5 + (c->tane - 3);
        case WID_AKATAN:
        case WID_AOTAN:
            return 5 + (c->tan - 3);
        case WID_TANE:
            return 1 + (c->tane - 5);
        case WID_TAN:
            return 1 + (c->tan - 5);
        case WID_KASU:
            return 1 + (c->kasu - 10);
        default:
            return winning_hands[wid].base_score;
    }
}

static int can_overpay_by_counts(const OwnYakuCounts* c, int wid)
{
    if (!c) {
        return OFF;
    }

    switch (wid) {
        case WID_AKATAN:
        case WID_AOTAN:
            return c->tan >= 3 ? ON : OFF;
        case WID_ISC:
            return c->tane >= 3 ? ON : OFF;
        default:
            return OFF;
    }
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

static int does_pair_immediately_complete_wid(int player, int drawn_card_no, int taken_card_no, int wid)
{
    OwnYakuCounts before;
    OwnYakuCounts after;
    build_own_counts(player, &before);
    after = before;
    add_card_to_counts(&after, drawn_card_no);
    add_card_to_counts(&after, taken_card_no);
    if (is_wid_completed_by_counts(&before, wid)) {
        return OFF;
    }
    return is_wid_completed_by_counts(&after, wid);
}

static int calc_select_early_five_point_delay_bonus(int player, int drawn_card_no, int taken_card_no, const StrategyData* before)
{
    int best_bonus = 0;

    if (!before || player < 0 || player > 1) {
        return 0;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int bonus = 0;

        if (!should_delay_early_five_point_completion(before, player, wid, -1)) {
            continue;
        }
        if (does_pair_immediately_complete_wid(player, drawn_card_no, taken_card_no, wid)) {
            if (best_bonus > -SELECT_EARLY_FIVE_POINT_DELAY_PENALTY) {
                best_bonus = -SELECT_EARLY_FIVE_POINT_DELAY_PENALTY;
            }
            continue;
        }
        if (ai_is_card_critical_for_wid(taken_card_no, wid)) {
            continue;
        }
        if (ai_is_card_critical_for_wid(drawn_card_no, wid) && count_critical_hand_cards_for_wid(player, wid, drawn_card_no) <= 0) {
            continue;
        }

        bonus = SELECT_EARLY_FIVE_POINT_DELAY_SETUP_BONUS;
        bonus += before->reach[wid] * 40;
        if (before->delay[wid] <= 2) {
            bonus += 2000;
        }
        if (best_bonus < bonus) {
            best_bonus = bonus;
        }
    }

    return best_bonus;
}

static int calc_immediate_points_gain(int player, int drawn_card_no, int taken_card_no, int* out_best_wid)
{
    int cards[2];

    cards[0] = drawn_card_no;
    cards[1] = taken_card_no;
    return analyze_score_gain_with_card_ids(player, cards, 2, out_best_wid);
}

static int calc_immediate_completion_bonus(int wid)
{
    int base_score = winning_hands[wid].base_score;
    switch (wid) {
        case WID_AKATAN:
        case WID_AOTAN:
        case WID_ISC:
        case WID_HANAMI:
        case WID_TSUKIMI:
        case WID_SANKOU:
        case WID_AME_SHIKOU:
        case WID_SHIKOU:
        case WID_GOKOU:
        case WID_DBTAN:
            return 200 + base_score * 120;
        case WID_KASU:
        case WID_TAN:
        case WID_TANE:
            return 60 + base_score * 40;
        default:
            return 120 + base_score * 80;
    }
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

static int is_three_card_five_point_wid(int wid)
{
    switch (wid) {
        case WID_SANKOU:
        case WID_ISC:
        case WID_AKATAN:
        case WID_AOTAN:
            return ON;
        default:
            return OFF;
    }
}

static int calc_select_three_card_five_point_setup_bonus(int player, int drawn_card_no, int taken_card_no, const StrategyData* before)
{
    int best_bonus = 0;

    if (!before || player < 0 || player > 1) {
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
        if (!ai_is_card_critical_for_wid(drawn_card_no, wid) && !ai_is_card_critical_for_wid(taken_card_no, wid)) {
            continue;
        }

        // simulate_select_capture() 後の inventory 状態を見て、何枚目の確保になったかを評価する。
        after_missing = ai_wid_missing_count(player, wid);
        if (after_missing <= 0 || after_missing > 2) {
            continue;
        }

        if (after_missing == 1) {
            bonus = SELECT_THREE_CARD_FIVE_POINT_REACH_SETUP_BONUS + before->reach[wid] * SELECT_THREE_CARD_FIVE_POINT_REACH_REACH_UNIT;
            if (before->delay[wid] <= 3) {
                bonus += SELECT_THREE_CARD_FIVE_POINT_FAST_DELAY_BONUS;
            }
        } else {
            bonus = SELECT_THREE_CARD_FIVE_POINT_FIRST_SETUP_BONUS + before->reach[wid] * SELECT_THREE_CARD_FIVE_POINT_FIRST_REACH_UNIT;
            if (before->delay[wid] <= 4) {
                bonus += SELECT_THREE_CARD_FIVE_POINT_FAST_DELAY_BONUS / 2;
            }
        }

        if (best_bonus < bonus) {
            best_bonus = bonus;
        }
    }

    return best_bonus;
}

static int calc_select_tactical_term(int player, int drawn_card_no, int taken_card_no, const StrategyData* before,
                                     int chosenDeckIndex, const int targetDecks[], int targetDeckCount)
{
    int term = 0;

    // 即役成立を最優先で評価。
    int immediate_bonus = 0;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (does_pair_immediately_complete_wid(player, drawn_card_no, taken_card_no, wid)) {
            int bonus = calc_immediate_completion_bonus(wid);
            if (immediate_bonus < bonus) {
                immediate_bonus = bonus;
            }
        }
    }
    term += immediate_bonus;

    // 優先役（速度/得点上位）への直接寄与を加点。
    int seen[WINNING_HAND_MAX];
    vgs_memset(seen, 0, sizeof(seen));
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_speed[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX || seen[wid]) {
            continue;
        }
        seen[wid] = ON;
        if (ai_is_card_critical_for_wid(taken_card_no, wid) || ai_is_card_critical_for_wid(drawn_card_no, wid)) {
            term += 120;
        } else if (ai_is_card_related_for_wid(taken_card_no, wid) || ai_is_card_related_for_wid(drawn_card_no, wid)) {
            term += 40;
        }
    }
    for (int i = 0; i < 3; i++) {
        int wid = before->priority_score[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX || seen[wid]) {
            continue;
        }
        seen[wid] = ON;
        if (ai_is_card_critical_for_wid(taken_card_no, wid) || ai_is_card_critical_for_wid(drawn_card_no, wid)) {
            term += 120;
        } else if (ai_is_card_related_for_wid(taken_card_no, wid) || ai_is_card_related_for_wid(drawn_card_no, wid)) {
            term += 40;
        }
    }

    // 相手危険役（短手）の必須札を回収できるなら加点、残すなら減点。
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!is_dangerous_risk_wid(before, wid)) {
            continue;
        }
        if (ai_is_card_critical_for_wid(taken_card_no, wid)) {
            term += before->risk_delay[wid] <= 2 ? 300 : 150;
        } else if (ai_is_card_related_for_wid(taken_card_no, wid)) {
            term += before->risk_delay[wid] <= 2 ? 80 : 40;
        }
        for (int i = 0; i < targetDeckCount; i++) {
            int deckIndex = targetDecks[i];
            if (deckIndex == chosenDeckIndex) {
                continue;
            }
            Card* left_card = safe_deck_card_at(deckIndex);
            if (left_card && ai_is_card_critical_for_wid(left_card->id, wid)) {
                term -= before->risk_delay[wid] <= 2 ? 220 : 110;
            } else if (left_card && ai_is_card_related_for_wid(left_card->id, wid)) {
                term -= before->risk_delay[wid] <= 2 ? 60 : 30;
            }
        }
    }

    // 優先役に必要な候補を取り逃がすケースを減点。
    for (int i = 0; i < targetDeckCount; i++) {
        int deckIndex = targetDecks[i];
        if (deckIndex == chosenDeckIndex) {
            continue;
        }
        Card* left_card = safe_deck_card_at(deckIndex);
        if (!left_card) {
            continue;
        }
        for (int pi = 0; pi < 3; pi++) {
            int wid = before->priority_speed[pi];
            if (wid < 0 || wid >= WINNING_HAND_MAX) {
                continue;
            }
            if (ai_is_card_critical_for_wid(left_card->id, wid) && !ai_is_card_critical_for_wid(taken_card_no, wid)) {
                term -= 80;
                break;
            }
        }
        for (int pi = 0; pi < 3; pi++) {
            int wid = before->priority_score[pi];
            if (wid < 0 || wid >= WINNING_HAND_MAX) {
                continue;
            }
            if (ai_is_card_critical_for_wid(left_card->id, wid) && !ai_is_card_critical_for_wid(taken_card_no, wid)) {
                term -= 80;
                break;
            }
        }

        // 役の完成札を残す取り逃がしを強く減点。
        for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
            if (does_pair_immediately_complete_wid(player, drawn_card_no, left_card->id, wid) &&
                !does_pair_immediately_complete_wid(player, drawn_card_no, taken_card_no, wid)) {
                term -= 200;
                break;
            }
        }
    }

    // 加点役（タネ/タン/カス）の閾値到達・接近を小加点。
    OwnYakuCounts before_counts;
    OwnYakuCounts after_counts;
    build_own_counts(player, &before_counts);
    after_counts = before_counts;
    add_card_to_counts(&after_counts, drawn_card_no);
    add_card_to_counts(&after_counts, taken_card_no);
    term += threshold_gain_bonus(before_counts.tane, after_counts.tane, 5);
    term += threshold_gain_bonus(before_counts.tan, after_counts.tan, 5);
    term += threshold_gain_bonus(before_counts.kasu, after_counts.kasu, 10);

    // 三光・猪鹿蝶・赤短・青短の 1 枚目/2 枚目確保を明示的に後押しする。
    term += calc_select_three_card_five_point_setup_bonus(player, drawn_card_no, taken_card_no, before);

    return term;
}

static int calc_select_overpay_bonus(int player, int drawn_card_no, int taken_card_no, const StrategyData* before,
                                     const int targetDecks[], int targetDeckCount, int* out_wid, int* out_setup)
{
    OwnYakuCounts after_counts;
    int bonus = 0;

    if (out_wid) {
        *out_wid = -1;
    }
    if (out_setup) {
        *out_setup = OFF;
    }
    if (!before || !is_overpay_aggressive_state(before) || is_overpay_defensive_state(before)) {
        return 0;
    }

    build_own_counts(player, &after_counts);
    add_card_to_counts(&after_counts, drawn_card_no);
    add_card_to_counts(&after_counts, taken_card_no);

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (!ai_is_overpay_target_wid(wid) || !strategy_overpay_flag(before, wid)) {
            continue;
        }

        if (does_pair_immediately_complete_wid(player, drawn_card_no, taken_card_no, wid)) {
            int score_after = calc_completed_wid_score_from_counts(&after_counts, wid);
            if (score_after <= 5) {
                int penalty = calc_overpay_delay_penalty(before, player, g.cards[taken_card_no].month);
                if (penalty <= 0) {
                    continue;
                }
                bonus -= penalty;
                if (out_wid) {
                    *out_wid = wid;
                }
                return bonus;
            }
            continue;
        }

        if (ai_wid_missing_count(player, wid) == 1 && can_overpay_by_counts(&after_counts, wid) &&
            (is_overpay_setup_card_for_wid(drawn_card_no, wid) || is_overpay_setup_card_for_wid(taken_card_no, wid))) {
            int left_completion_visible = OFF;
            for (int i = 0; i < targetDeckCount; i++) {
                Card* left = safe_deck_card_at(targetDecks[i]);
                if (left && left->id != taken_card_no && ai_would_complete_wid_by_taking_card(player, wid, left->id)) {
                    left_completion_visible = ON;
                    break;
                }
            }
            if (!left_completion_visible) {
                bonus += SELECT_OVERPAY_SETUP_BONUS;
                if (out_wid) {
                    *out_wid = wid;
                }
                if (out_setup) {
                    *out_setup = ON;
                }
                return bonus;
            }
        }
    }

    return 0;
}

static int simulate_select_capture(int player, Card* drawn_card, int deck_index, int* taken_card_no)
{
    if (!drawn_card || deck_index < 0 || deck_index >= 48) {
        return OFF;
    }
    Card* taken = safe_deck_card_at(deck_index);
    if (!taken) {
        return OFF;
    }
    g.invent[player][drawn_card->type].cards[g.invent[player][drawn_card->type].num++] = drawn_card;
    g.invent[player][taken->type].cards[g.invent[player][taken->type].num++] = taken;
    g.deck.cards[deck_index] = NULL;
    if (g.deck.num > 0) {
        g.deck.num--;
    }
    if (taken_card_no) {
        *taken_card_no = taken->id;
    }
    return ON;
}

static int are_target_deck_cards_same_type(const int targetDecks[], int targetDeckCount)
{
    if (!targetDecks || targetDeckCount < 2) {
        return OFF;
    }

    Card* first = safe_deck_card_at(targetDecks[0]);
    if (!first) {
        return OFF;
    }
    int type = first->type;

    for (int i = 1; i < targetDeckCount; i++) {
        Card* c = safe_deck_card_at(targetDecks[i]);
        if (!c || c->type != type) {
            return OFF;
        }
    }
    return ON;
}

static int find_forced_completion_select(int player, const StrategyData* before, const int targetDecks[], int targetDeckCount)
{
    int best_index = -1;
    int best_wid = -1;
    int best_base = -1;

    // 高打点の固定役がこの1枚で新規成立するなら、通常評価より先に確定で拾う。
    for (int i = 0; i < targetDeckCount; i++) {
        Card* taken = safe_deck_card_at(targetDecks[i]);
        if (!taken) {
            continue;
        }
        for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
            if (!ai_is_fixed_wid(wid) || !ai_is_high_value_wid(wid)) {
                continue;
            }
            if (!ai_would_complete_wid_by_taking_card(player, wid, taken->id)) {
                continue;
            }
            if (ai_is_overpay_target_wid(wid) && before && is_overpay_aggressive_state(before) && !is_overpay_defensive_state(before) &&
                strategy_overpay_flag(before, wid) && ai_count_unrevealed_same_month(player, taken->month) <= 1) {
#ifdef LOGGING
                ai_putlog("[overpay] wid=%s setup=ON reason=skip_forced_completion_select", winning_hands[wid].name);
#endif
                continue;
            }
            if (should_delay_early_five_point_completion(before, player, wid, -1)) {
                ai_putlog("[select] skip early five-point forced completion: wid=%s taken=%d delay=%d hand_keys=%d",
                          winning_hands[wid].name, taken->id, before ? before->delay[wid] : -1,
                          count_critical_hand_cards_for_wid(player, wid, -1));
                continue;
            }
            int base = winning_hands[wid].base_score;
            if (base > best_base || (base == best_base && (best_index < 0 || targetDecks[i] < best_index))) {
                best_index = targetDecks[i];
                best_wid = wid;
                best_base = base;
            }
        }
    }

    if (best_index >= 0) {
        Card* taken = safe_deck_card_at(best_index);
        if (taken) {
            ai_putlog("[select] forced completion: wid=%s base=%d taken=%d", winning_hands[best_wid].name, best_base, taken->id);
        }
    }
    return best_index;
}

static int calc_left_completion_penalty(int player, int chosenDeckIndex, const int targetDecks[], int targetDeckCount)
{
    int penalty = 0;

    for (int i = 0; i < targetDeckCount; i++) {
        int deckIndex = targetDecks[i];
        if (deckIndex == chosenDeckIndex) {
            continue;
        }
        Card* left = safe_deck_card_at(deckIndex);
        if (!left) {
            continue;
        }
        for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
            if (!ai_is_fixed_wid(wid) || !ai_is_high_value_wid(wid)) {
                continue;
            }
            if (ai_wid_missing_count(player, wid) != 1) {
                continue;
            }
            if (!ai_would_complete_wid_by_taking_card(player, wid, left->id)) {
                continue;
            }
            penalty -= ai_count_unrevealed_same_month(player, left->month) > 0 ? 250 : 120;
            break;
        }
        if (count_known_month_cards_for_player(player, left->month) == 3) {
            penalty -= SELECT_MONTH_LOCK_MISS_PENALTY;
        }
    }
    return penalty;
}

/**
 * @brief CPU が指定カード（山札）に対する取り札の場位置 (g.deck.cards) を選択
 * @param player 0: 自分, 1: CPU
 * @param card 対象カード
 * @return g.deck.cards のインデックス
 */
int ai_hard_select(int player, Card* card)
{
    const StrategyData* before;

    if (!card) {
        return find_empty_deck_slot();
    }

    int targetDecks[48];
    int targetDeckCount = collect_target_deck_indexes(card, targetDecks);
    if (!targetDeckCount) {
        return find_empty_deck_slot();
    }
    if (targetDeckCount < 2) {
        return targetDecks[0];
    }

#if SELECT_STRATEGY_UPDATE
    StrategyData before_work;
    ai_think_strategy(player, &before_work);
    vgs_memcpy(&g.strategy[player], &before_work, sizeof(StrategyData));
    before = &g.strategy[player];
#else
    before = &g.strategy[player];
#endif

#if HARD_RAPACIOUS_FALLBACK_ENABLE == 1
    if (hard_should_force_rapacious_fallback_behind(before) || hard_should_force_rapacious_fallback_sake_select_drop(player, before)) {
        return ai_hard_rapacious_fallback_select(player, card);
    }
#endif

    int forced_completion_index = find_forced_completion_select(player, before, targetDecks, targetDeckCount);
    if (forced_completion_index >= 0) {
        return forced_completion_index;
    }

    int fallback_index = ai_normal_select(player, card);
    if (are_target_deck_cards_same_type(targetDecks, targetDeckCount)) {
        return fallback_index;
    }

    int best_index = -1;
    int best_value = INT_MIN;
    int best_index_without_certain = -1;
    int best_value_without_certain = INT_MIN;
    int best_index_without_combo7 = -1;
    int best_value_without_combo7 = INT_MIN;
    int best_index_without_endgame = -1;
    int best_value_without_endgame = INT_MIN;
    int best_risk_sum_without_endgame = INT_MAX;
    int best_self_min_delay_without_endgame = 99;
    int best_self_max_score_without_endgame = -1;
    int best_index_without_keycard = -1;
    int best_value_without_keycard = INT_MIN;
    int best_risk_sum = INT_MAX;
    int best_self_min_delay = 99;
    int best_self_max_score = -1;
    int best_immediate_gain = 0;
    int best_risk_sum_without_certain = INT_MAX;
    int best_self_min_delay_without_certain = 99;
    int best_self_max_score_without_certain = -1;
    int best_risk_sum_without_combo7 = INT_MAX;
    int best_self_min_delay_without_combo7 = 99;
    int best_self_max_score_without_combo7 = -1;
    int best_month_lock_bonus = 0;
    int best_overpay_bonus = 0;
    int best_hidden_key_trigger = OFF;
    int best_hidden_deny_trigger = OFF;
    SelectCandidateScore greedy_candidates[8];
    int greedy_candidate_count = 0;
    int any_month_lock_triggered = OFF;
    int any_overpay_delay_triggered = OFF;
    int any_hidden_key_triggered = OFF;
    int any_hidden_deny_triggered = OFF;
    int any_certain_seven_plus_triggered = OFF;
    int any_combo7_triggered = OFF;
    int best_combo7_bonus = 0;
    int second_combo7_bonus = 0;

    CardSet deck_backup;
    CardSet invent_backup[4];

    ai_think_start();

    for (int i = 0; i < targetDeckCount; i++) {
        int deckIndex = targetDecks[i];
        int taken_card_no = -1;

        vgs_memcpy(&deck_backup, &g.deck, sizeof(CardSet));
        vgs_memcpy(invent_backup, g.invent[player], sizeof(CardSet) * 4);

        if (!simulate_select_capture(player, card, deckIndex, &taken_card_no)) {
            vgs_memcpy(&g.deck, &deck_backup, sizeof(CardSet));
            vgs_memcpy(g.invent[player], invent_backup, sizeof(CardSet) * 4);
            continue;
        }

        StrategyData after;
        ai_think_strategy(player, &after);

        int self_score_term = calc_self_score_term(&after);
        int self_speed_term = calc_self_speed_term(&after);
        int opp_deny_term = calc_opp_deny_term(before, &after) + calc_card_deny_risk_gain(taken_card_no, before);
        // tactical_term: 即役成立/優先役寄与/危険札回収/取り逃がしの局所価値を評価。
        int tactical_term = calc_select_tactical_term(player, card->id, taken_card_no, before, deckIndex, targetDecks, targetDeckCount);
        tactical_term += calc_left_completion_penalty(player, deckIndex, targetDecks, targetDeckCount);
        int immediate_wid = -1;
        int immediate_points_gain = calc_immediate_points_gain(player, card->id, taken_card_no, &immediate_wid);
        int final_non_clinch_immediate = is_final_round_non_clinch_immediate(before, immediate_points_gain);
        int endgame_needed = 0;
        int endgame_k = 0;
        int endgame_clinch_score = calc_endgame_clinch_score(before, immediate_points_gain, &endgame_needed, &endgame_k);
        int overpay_wid = -1;
        int overpay_setup = OFF;
        int overpay_bonus = calc_select_overpay_bonus(player, card->id, taken_card_no, before, targetDecks, targetDeckCount,
                                                      &overpay_wid, &overpay_setup);
        int early_delay_bonus = calc_select_early_five_point_delay_bonus(player, card->id, taken_card_no, before);
        if (immediate_wid >= 0 && is_delayable_five_point_wid(immediate_wid) &&
            should_delay_early_five_point_completion(before, player, immediate_wid, -1)) {
            early_delay_bonus -= SELECT_EARLY_FIVE_POINT_DELAY_PENALTY;
        }
        int hidden_month_card_no = -1;
        int hidden_key_trigger = OFF;
        int hidden_deny_trigger = OFF;
        int month_lock_bonus = calc_month_lock_bonus(player, g.cards[taken_card_no].month);
        if (month_lock_bonus > 0) {
            any_month_lock_triggered = ON;
            hidden_month_card_no = find_hidden_month_card_no(player, g.cards[taken_card_no].month);
            month_lock_bonus += calc_hidden_month_card_bonus(hidden_month_card_no, before, &hidden_key_trigger, &hidden_deny_trigger);
            if (hidden_key_trigger) {
                any_hidden_key_triggered = ON;
            }
            if (hidden_deny_trigger) {
                any_hidden_deny_triggered = ON;
            }
        }
        if (overpay_bonus < 0) {
            any_overpay_delay_triggered = ON;
        }
        int greedy_seven_plus_bonus = 0;
        int hot_material_bonus = 0;
        int opp_koikoi_win_bonus = 0;
        int seven_plus_pressure_bonus = calc_near_seven_plus_pressure_bonus(before, &after);
        int emergency_one_point_bonus = calc_emergency_one_point_block_bonus(player, before, &after);
        int combo7_bonus = 0;
        AiCombo7Eval combo7_eval;
        int certain_seven_plus_penalty = 0;
        int opp_immediate_score = 0;
        int opp_immediate_seven_plus = OFF;
        int opp_immediate_win = OFF;
        int opp_immediate_wid = -1;
        const char* prune_reason_code = "NONE";
        int seven_ok = OFF;
        int seven_delay = 99;
        int seven_margin = -99;
        int seven_reach = 0;
        int seven_wid = -1;
        if (final_non_clinch_immediate) {
            immediate_points_gain = 0;
            immediate_wid = -1;
            endgame_clinch_score -= SELECT_FINAL_NON_CLINCH_IMMEDIATE_PENALTY;
            prune_reason_code = "FINAL_NON_CLINCH";
        }
        if (before->koikoi_opp == ON) {
            if (secures_want_card_on_capture(player, card->id, taken_card_no) || immediate_points_gain > 0) {
                opp_koikoi_win_bonus += SELECT_OPP_KOIKOI_WIN_BONUS;
            } else if (!is_want_card_for_player(player, card->id)) {
                for (int ti = 0; ti < targetDeckCount; ti++) {
                    Card* deck_card = safe_deck_card_at(targetDecks[ti]);
                    if (deck_card && is_want_card_for_player(player, deck_card->id)) {
                        opp_koikoi_win_bonus -= SELECT_OPP_KOIKOI_MISS_PENALTY;
                        break;
                    }
                }
            }
        }
        if (before->opp_coarse_threat >= SELECT_2PLY_SEVEN_PLUS_GATE_THREAT || before->risk_7plus_distance <= SELECT_2PLY_SEVEN_PLUS_GATE_DISTANCE ||
            before->koikoi_opp == ON) {
            opp_immediate_win = simulate_opponent_normal_turn_loss(player, &opp_immediate_score, &opp_immediate_seven_plus, &opp_immediate_wid);
            if (before->koikoi_opp == ON && opp_immediate_win) {
                opp_koikoi_win_bonus -= SELECT_2PLY_KOIKOI_OPP_PRUNE_PENALTY;
                prune_reason_code = "PRUNE_KOIKOI_OPP";
            } else if (opp_immediate_seven_plus) {
                certain_seven_plus_penalty = (before->risk_7plus_distance <= SELECT_2PLY_SEVEN_PLUS_GATE_DISTANCE &&
                                              before->opp_coarse_threat >= SELECT_2PLY_SEVEN_PLUS_GATE_THREAT)
                                                 ? SELECT_2PLY_CERTAIN_SEVEN_PLUS_PENALTY
                                                 : SELECT_2PLY_SEVEN_PLUS_TIEBREAK_PENALTY;
                opp_koikoi_win_bonus -= certain_seven_plus_penalty;
                prune_reason_code = (certain_seven_plus_penalty == SELECT_2PLY_CERTAIN_SEVEN_PLUS_PENALTY) ? "CERTAIN_7PLUS" : "NEAR_7PLUS";
                if (certain_seven_plus_penalty == SELECT_2PLY_CERTAIN_SEVEN_PLUS_PENALTY) {
                    any_certain_seven_plus_triggered = ON;
                }
            }
        }
        if (before->mode == MODE_GREEDY) {
            greedy_seven_plus_bonus = calc_greedy_seven_plus_bonus(player, &after);
        }
        describe_best_seven_plus_line(g.stats[player].score + immediate_points_gain, &after, &seven_ok, &seven_delay, &seven_margin, &seven_reach, &seven_wid);
        hot_material_bonus = calc_hot_material_bonus(taken_card_no, &after);
        combo7_bonus = calc_combo7_bonus(player, g.stats[player].score + immediate_points_gain, &after, &combo7_eval);
        if (combo7_bonus > 0) {
            any_combo7_triggered = ON;
            if (combo7_bonus > best_combo7_bonus) {
                second_combo7_bonus = best_combo7_bonus;
                best_combo7_bonus = combo7_bonus;
            } else if (combo7_bonus > second_combo7_bonus) {
                second_combo7_bonus = combo7_bonus;
            }
        }

        int offence_weight = before->bias.offence;
        int speed_weight = before->bias.speed;
        int defence_weight = before->bias.defence;
        int tactical_weight = TACTICAL_WEIGHT;
        if (before->mode == MODE_DEFENSIVE) {
            defence_weight += SELECT_MODE_DEFENCE_BONUS;
            tactical_weight = 12;
        } else if (before->mode == MODE_GREEDY) {
            offence_weight += SELECT_MODE_GREEDY_OFFENCE_BONUS;
            speed_weight += 5;
            tactical_weight = 14;
        }

        int value = self_score_term * offence_weight + self_speed_term * speed_weight +
                    opp_deny_term * defence_weight + tactical_term * tactical_weight +
                    immediate_points_gain * IMMEDIATE_GAIN_WEIGHT + opp_koikoi_win_bonus +
                    greedy_seven_plus_bonus + hot_material_bonus + overpay_bonus + early_delay_bonus + month_lock_bonus + seven_plus_pressure_bonus +
                    emergency_one_point_bonus + combo7_bonus +
                    endgame_clinch_score;
        int value_without_certain = value + certain_seven_plus_penalty;
        int value_without_combo7 = value - combo7_bonus;
        int part_score = self_score_term * offence_weight;
        int part_speed = self_speed_term * speed_weight;
        int part_deny = opp_deny_term * defence_weight;
        int part_safe = 0;
        int part_keep = 0;
        int part_tactical = tactical_term * tactical_weight + immediate_points_gain * IMMEDIATE_GAIN_WEIGHT;
        int part_bonus_total = value - (part_score + part_speed + part_deny + part_safe + part_keep + part_tactical);
        int risk_sum = sum_risk_score(&after);
        int self_min_delay = min_valid_delay_self(&after);
        int self_max_score = max_self_score(&after);

        char combo7_label[96];
        format_combo7_label(&combo7_eval, combo7_label);
        if (month_lock_bonus > 0) {
            ai_putlog("[month-lock] month=%d own=%d known=%d hidden=1 hidden_card=%d reason=take_control", g.cards[taken_card_no].month + 1,
                      count_owned_month_cards(player, g.cards[taken_card_no].month),
                      count_known_month_cards_for_player(player, g.cards[taken_card_no].month), hidden_month_card_no);
        }
        if (overpay_wid >= 0) {
            ai_putlog("[overpay] wid=%s setup=%s reason=%s", winning_hands[overpay_wid].name, overpay_setup ? "ON" : "OFF",
                      overpay_setup ? "hold_for_base6plus" : "deprioritize_base5_completion");
        }
        ai_putlog("select[%d] take=%d value=%d (score=%d speed=%d deny=%d tactical=%d imm=%d end=%d need=%d k=%d koikoi_win=%d atk7=%d emg1=%d hot=%d overpay=%d delay5=%d monthlock=%d combo7=%d[%s reach=%d delay=%d sum=%d] deny7=%d opp2ply=%d/%d)",
                  deckIndex, taken_card_no, value, self_score_term, self_speed_term, opp_deny_term, tactical_term,
                  immediate_points_gain, endgame_clinch_score, endgame_needed, endgame_k, opp_koikoi_win_bonus, greedy_seven_plus_bonus, emergency_one_point_bonus,
                  hot_material_bonus, overpay_bonus, early_delay_bonus, month_lock_bonus,
                  combo7_bonus, combo7_label, combo7_eval.combo_reach, combo7_eval.combo_delay, combo7_eval.combo_score_sum,
                  seven_plus_pressure_bonus, opp_immediate_win, opp_immediate_seven_plus ? opp_immediate_score : 0);

        int better = OFF;
        if (best_index < 0) {
            better = ON;
        } else if ((g.koikoi[player] == ON || before->koikoi_opp == ON) && (immediate_points_gain > 0 || best_immediate_gain > 0)) {
            if (immediate_points_gain > best_immediate_gain) {
                better = ON;
            } else if (immediate_points_gain == best_immediate_gain) {
                int diff = value - best_value;
                int near = diff <= 50 && diff >= -50;
                if (!near) {
                    better = diff > 0 ? ON : OFF;
                } else {
                    if (risk_sum < best_risk_sum) {
                        better = ON;
                    } else if (risk_sum == best_risk_sum) {
                        if (self_min_delay < best_self_min_delay) {
                            better = ON;
                        } else if (self_min_delay == best_self_min_delay) {
                            if (self_max_score > best_self_max_score) {
                                better = ON;
                            } else if (self_max_score == best_self_max_score) {
                                if (deckIndex == fallback_index && best_index != fallback_index) {
                                    better = ON;
                                } else if (deckIndex < best_index) {
                                    better = ON;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            int diff = value - best_value;
            int near = diff <= 50 && diff >= -50;
            if (!near) {
                better = diff > 0 ? ON : OFF;
            } else {
                if (risk_sum < best_risk_sum) {
                    better = ON;
                } else if (risk_sum == best_risk_sum) {
                    if (self_min_delay < best_self_min_delay) {
                        better = ON;
                    } else if (self_min_delay == best_self_min_delay) {
                        if (self_max_score > best_self_max_score) {
                            better = ON;
                        } else if (self_max_score == best_self_max_score) {
                            if (deckIndex == fallback_index && best_index != fallback_index) {
                                better = ON;
                            } else if (deckIndex < best_index) {
                                better = ON;
                            }
                        }
                    }
                }
            }
        }

        if (better) {
            best_index = deckIndex;
            best_value = value;
            best_immediate_gain = immediate_points_gain;
            best_risk_sum = risk_sum;
            best_self_min_delay = self_min_delay;
            best_self_max_score = self_max_score;
            best_month_lock_bonus = month_lock_bonus;
            best_overpay_bonus = overpay_bonus;
            best_hidden_key_trigger = hidden_key_trigger;
            best_hidden_deny_trigger = hidden_deny_trigger;
        }
        {
            int better_without = OFF;
            if (best_index_without_certain < 0) {
                better_without = ON;
            } else {
                int diff = value_without_certain - best_value_without_certain;
                int near = diff <= 50 && diff >= -50;
                if (!near) {
                    better_without = diff > 0 ? ON : OFF;
                } else {
                    if (risk_sum < best_risk_sum_without_certain) {
                        better_without = ON;
                    } else if (risk_sum == best_risk_sum_without_certain) {
                        if (self_min_delay < best_self_min_delay_without_certain) {
                            better_without = ON;
                        } else if (self_min_delay == best_self_min_delay_without_certain) {
                            if (self_max_score > best_self_max_score_without_certain) {
                                better_without = ON;
                            } else if (self_max_score == best_self_max_score_without_certain) {
                                if (deckIndex == fallback_index && best_index_without_certain != fallback_index) {
                                    better_without = ON;
                                } else if (deckIndex < best_index_without_certain) {
                                    better_without = ON;
                                }
                            }
                        }
                    }
                }
            }
            if (better_without) {
                best_index_without_certain = deckIndex;
                best_value_without_certain = value_without_certain;
                best_risk_sum_without_certain = risk_sum;
                best_self_min_delay_without_certain = self_min_delay;
                best_self_max_score_without_certain = self_max_score;
            }
        }

        {
            int better_without_combo7 = OFF;
            if (best_index_without_combo7 < 0) {
                better_without_combo7 = ON;
            } else {
                int diff = value_without_combo7 - best_value_without_combo7;
                int near = diff <= 50 && diff >= -50;
                if (!near) {
                    better_without_combo7 = diff > 0 ? ON : OFF;
                } else {
                    if (risk_sum < best_risk_sum_without_combo7) {
                        better_without_combo7 = ON;
                    } else if (risk_sum == best_risk_sum_without_combo7) {
                        if (self_min_delay < best_self_min_delay_without_combo7) {
                            better_without_combo7 = ON;
                        } else if (self_min_delay == best_self_min_delay_without_combo7) {
                            if (self_max_score > best_self_max_score_without_combo7) {
                                better_without_combo7 = ON;
                            } else if (self_max_score == best_self_max_score_without_combo7) {
                                if (deckIndex == fallback_index && best_index_without_combo7 != fallback_index) {
                                    better_without_combo7 = ON;
                                } else if (deckIndex < best_index_without_combo7) {
                                    better_without_combo7 = ON;
                                }
                            }
                        }
                    }
                }
            }
            if (better_without_combo7) {
                best_index_without_combo7 = deckIndex;
                best_value_without_combo7 = value_without_combo7;
                best_risk_sum_without_combo7 = risk_sum;
                best_self_min_delay_without_combo7 = self_min_delay;
                best_self_max_score_without_combo7 = self_max_score;
            }
        }

        {
            int better_without_endgame = OFF;
            int value_without_endgame = value - endgame_clinch_score;

            if (best_index_without_endgame < 0) {
                better_without_endgame = ON;
            } else {
                int diff = value_without_endgame - best_value_without_endgame;
                int near = diff <= 50 && diff >= -50;
                if (!near) {
                    better_without_endgame = diff > 0 ? ON : OFF;
                } else {
                    if (risk_sum < best_risk_sum_without_endgame) {
                        better_without_endgame = ON;
                    } else if (risk_sum == best_risk_sum_without_endgame) {
                        if (self_min_delay < best_self_min_delay_without_endgame) {
                            better_without_endgame = ON;
                        } else if (self_min_delay == best_self_min_delay_without_endgame) {
                            if (self_max_score > best_self_max_score_without_endgame) {
                                better_without_endgame = ON;
                            } else if (self_max_score == best_self_max_score_without_endgame) {
                                if (deckIndex == fallback_index && best_index_without_endgame != fallback_index) {
                                    better_without_endgame = ON;
                                } else if (deckIndex < best_index_without_endgame) {
                                    better_without_endgame = ON;
                                }
                            }
                        }
                    }
                }
            }
            if (better_without_endgame) {
                best_index_without_endgame = deckIndex;
                best_value_without_endgame = value_without_endgame;
                best_risk_sum_without_endgame = risk_sum;
                best_self_min_delay_without_endgame = self_min_delay;
                best_self_max_score_without_endgame = self_max_score;
            }
        }

        if (greedy_candidate_count < (int)(sizeof(greedy_candidates) / sizeof(greedy_candidates[0]))) {
            greedy_candidates[greedy_candidate_count].deck_index = deckIndex;
            greedy_candidates[greedy_candidate_count].taken_card_no = taken_card_no;
            greedy_candidates[greedy_candidate_count].value = value;
            greedy_candidates[greedy_candidate_count].value_without_certain = value_without_certain;
            greedy_candidates[greedy_candidate_count].value_without_combo7 = value_without_combo7;
            greedy_candidates[greedy_candidate_count].immediate_gain = immediate_points_gain;
            greedy_candidates[greedy_candidate_count].endgame_clinch_score = endgame_clinch_score;
            greedy_candidates[greedy_candidate_count].endgame_needed = endgame_needed;
            greedy_candidates[greedy_candidate_count].endgame_k = endgame_k;
            greedy_candidates[greedy_candidate_count].combo7_bonus = combo7_bonus;
            greedy_candidates[greedy_candidate_count].combo7_plus1_rank = combo7_eval.plus1_rank;
            greedy_candidates[greedy_candidate_count].risk_sum = risk_sum;
            greedy_candidates[greedy_candidate_count].self_min_delay = self_min_delay;
            greedy_candidates[greedy_candidate_count].self_max_score = self_max_score;
            greedy_candidates[greedy_candidate_count].part_score = part_score;
            greedy_candidates[greedy_candidate_count].part_speed = part_speed;
            greedy_candidates[greedy_candidate_count].part_deny = part_deny;
            greedy_candidates[greedy_candidate_count].part_safe = part_safe;
            greedy_candidates[greedy_candidate_count].part_keep = part_keep;
            greedy_candidates[greedy_candidate_count].part_tactical = part_tactical;
            greedy_candidates[greedy_candidate_count].part_bonus_total = part_bonus_total;
            greedy_candidates[greedy_candidate_count].opp_immediate_win = opp_immediate_win;
            greedy_candidates[greedy_candidate_count].opp_immediate_score = opp_immediate_win ? opp_immediate_score : 0;
            greedy_candidates[greedy_candidate_count].opp_immediate_wid = opp_immediate_win ? opp_immediate_wid : -1;
            greedy_candidates[greedy_candidate_count].opp_immediate_x2 =
                opp_immediate_win ? (((opp_immediate_score >= 7) ? 1 : 0) + (before->koikoi_mine == ON ? 1 : 0)) : 0;
            greedy_candidates[greedy_candidate_count].opp7_dist =
                after.risk_7plus_distance >= 4 ? 4 : (after.risk_7plus_distance <= 0 ? 1 : after.risk_7plus_distance);
            greedy_candidates[greedy_candidate_count].opp7_threat = after.opp_coarse_threat;
            greedy_candidates[greedy_candidate_count].opp7_certain = certain_seven_plus_penalty == SELECT_2PLY_CERTAIN_SEVEN_PLUS_PENALTY ? ON : OFF;
            greedy_candidates[greedy_candidate_count].certain_7plus = certain_seven_plus_penalty == SELECT_2PLY_CERTAIN_SEVEN_PLUS_PENALTY ? ON : OFF;
            greedy_candidates[greedy_candidate_count].near_7plus =
                (certain_seven_plus_penalty > 0 && certain_seven_plus_penalty != SELECT_2PLY_CERTAIN_SEVEN_PLUS_PENALTY) ? ON : OFF;
            greedy_candidates[greedy_candidate_count].koikoi_opp_prune = (before->koikoi_opp == ON && opp_immediate_win) ? ON : OFF;
            greedy_candidates[greedy_candidate_count].seven_ok = seven_ok;
            greedy_candidates[greedy_candidate_count].seven_delay = seven_delay;
            greedy_candidates[greedy_candidate_count].seven_margin = seven_margin;
            greedy_candidates[greedy_candidate_count].seven_reach = seven_reach;
            greedy_candidates[greedy_candidate_count].seven_wid = seven_wid;
            greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_NONE;
            greedy_candidates[greedy_candidate_count].aim_wid = -1;
            if (combo7_bonus > 0) {
                greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_COMBO7;
                greedy_candidates[greedy_candidate_count].aim_wid = combo7_eval.combo_count > 0 ? combo7_eval.wids[0] : -1;
            }
            if (seven_ok && (greedy_seven_plus_bonus > 0 || seven_plus_pressure_bonus > 0)) {
                greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_SEVENLINE;
                greedy_candidates[greedy_candidate_count].aim_wid = seven_wid;
            }
            if (month_lock_bonus > 0) {
                greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_MONTHLOCK;
                greedy_candidates[greedy_candidate_count].aim_wid = -1;
            }
            if (overpay_bonus > 0 && overpay_wid >= 0) {
                greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_OVERPAY;
                greedy_candidates[greedy_candidate_count].aim_wid = overpay_wid;
            }
            if (certain_seven_plus_penalty > 0 || (before->koikoi_opp == ON && opp_immediate_win)) {
                greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_DEFENCE7;
                greedy_candidates[greedy_candidate_count].aim_wid = opp_immediate_wid;
            }
            if (opp_koikoi_win_bonus > 0) {
                greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_KOIKOIWIN;
                greedy_candidates[greedy_candidate_count].aim_wid = immediate_wid;
            }
            if (immediate_points_gain > 0) {
                greedy_candidates[greedy_candidate_count].aim_type = TRACE_AIM_IMMEDIATE;
                greedy_candidates[greedy_candidate_count].aim_wid = immediate_wid;
            }
            greedy_candidates[greedy_candidate_count].prune_reason_code = prune_reason_code;
            greedy_candidate_count++;
        }

        vgs_memcpy(&g.deck, &deck_backup, sizeof(CardSet));
        vgs_memcpy(g.invent[player], invent_backup, sizeof(CardSet) * 4);
    }

    ai_think_end();

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

    {
        int final_best_index = best_index;
        int final_best_index_without_certain = best_index_without_certain;
        int final_best_index_without_combo7 = best_index_without_combo7;
        int final_best_index_without_endgame = best_index_without_endgame;
        const char* reason_code = "BASE_COMPARATOR";
        if (before->mode != MODE_DEFENSIVE && before->greedy_need >= before->defensive_need && greedy_candidate_count > 0) {
            int greedy_choice = choose_greedy_select_candidate(player, before, greedy_candidates, greedy_candidate_count, SELECT_GREEDY_VALUE_NORMAL);
            int greedy_choice_without = choose_greedy_select_candidate(player, before, greedy_candidates, greedy_candidate_count, SELECT_GREEDY_VALUE_WITHOUT_CERTAIN);
            int greedy_choice_without_combo7 =
                choose_greedy_select_candidate(player, before, greedy_candidates, greedy_candidate_count, SELECT_GREEDY_VALUE_WITHOUT_COMBO7);
            int greedy_choice_without_endgame =
                choose_greedy_select_candidate(player, before, greedy_candidates, greedy_candidate_count, SELECT_GREEDY_VALUE_WITHOUT_ENDGAME);
            if (greedy_choice >= 0) {
                final_best_index = greedy_choice;
                reason_code = "GREEDY_FALLBACK";
            }
            if (greedy_choice_without >= 0) {
                final_best_index_without_certain = greedy_choice_without;
            }
            if (greedy_choice_without_combo7 >= 0) {
                final_best_index_without_combo7 = greedy_choice_without_combo7;
            }
            if (greedy_choice_without_endgame >= 0) {
                final_best_index_without_endgame = greedy_choice_without_endgame;
            }
        }
        if (any_certain_seven_plus_triggered) {
            ai_debug_note_certain_seven_plus(player, final_best_index != final_best_index_without_certain ? ON : OFF);
        }
        if (any_combo7_triggered) {
            ai_debug_note_combo7(player, before->mode, final_best_index != final_best_index_without_combo7 ? ON : OFF);
            ai_debug_note_combo7_margin(player, best_combo7_bonus - second_combo7_bonus,
                                        (best_combo7_bonus - second_combo7_bonus) <= COMBO7_MARGIN_FLIP_THRESHOLD ? ON : OFF);
            for (int i = 0; i < greedy_candidate_count; i++) {
                if (greedy_candidates[i].deck_index == final_best_index && greedy_candidates[i].combo7_plus1_rank > 0) {
                    ai_debug_note_best_plus1_used_rank(player, greedy_candidates[i].combo7_plus1_rank);
                    break;
                }
            }
        }
        ai_debug_note_endgame(player, calc_endgame_k(before), final_best_index != final_best_index_without_endgame, calc_endgame_needed(before));
        best_index = final_best_index;
        {
            int chosen_pos = -1;
            int second_pos = -1;
            int order[2];
            int order_count = collect_select_choice_order_indices(greedy_candidates, greedy_candidate_count, SELECT_GREEDY_VALUE_NORMAL, fallback_index, order, 2);

            for (int i = 0; i < greedy_candidate_count; i++) {
                if (greedy_candidates[i].deck_index == best_index) {
                    chosen_pos = i;
                }
                if (order_count > 1 && greedy_candidates[i].deck_index == order[1]) {
                    second_pos = i;
                }
            }
            ai_set_last_think_context(player, before, chosen_pos >= 0 ? greedy_candidates[chosen_pos].endgame_needed : calc_endgame_needed(before),
                                      chosen_pos >= 0 ? greedy_candidates[chosen_pos].endgame_k : calc_endgame_k(before),
                                      chosen_pos >= 0 ? greedy_candidates[chosen_pos].immediate_gain : 0,
                                      second_pos >= 0 ? greedy_candidates[second_pos].immediate_gain : 0,
                                      chosen_pos >= 0 ? greedy_candidates[chosen_pos].endgame_clinch_score : 0,
                                      second_pos >= 0 ? greedy_candidates[second_pos].endgame_clinch_score : 0);
        }
        emit_select_trace(player, card, before, greedy_candidates, greedy_candidate_count, fallback_index, best_index, reason_code);
    }

    if (best_index < 0) {
        ai_set_last_think_extra(player, "");
        return fallback_index;
    }
    ai_putlog("[select] mode=%s round=%d/%d match_score_diff=%d left_rounds=%d left_own=%d opp_coarse_threat=%d opp_exact_7plus_threat=%d risk_mult=%d greedy_need=%d defensive_need=%d best=%d fallback=%d koikoi_opp=%d",
              strategy_mode_name(before->mode), g.round + 1, before->round_max, before->match_score_diff, before->left_rounds, before->left_own,
              before->opp_coarse_threat, before->opp_exact_7plus_threat, before->risk_mult_pct, before->greedy_need, before->defensive_need, best_index,
              fallback_index, before->koikoi_opp);
    return best_index;
}
