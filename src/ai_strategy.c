#include "ai.h"

typedef struct {
    int minimum;
    int count;
    struct {
        int key;
        int must;
    } cards[25];
} StrategyRequirement;

typedef struct {
    int gokou;
    int shikou;
    int tan;
    int isc;
    int tsukimi;
    int hanami;
    int akatan;
    int aotan;
    int tane;
    int kasu;
} StrategyCounts;

typedef struct {
    int self_best_ev;
    int self_best_speed_ev;
    int self_best_delay;
} StrategySelfBestMetrics;

typedef struct {
    int opp_best_risk_ev;
    int opp_best_risk_delay;
} StrategyOppBestMetrics;

enum {
    SLOC_NONE = 0,
    SLOC_OWN = 1 << 0,
    SLOC_HAND = 1 << 1,
    SLOC_FLOOR = 1 << 2,
    SLOC_OPP = 1 << 3,
    SLOC_EST = 1 << 4,
};

static StrategyRequirement strategy_requirements[WINNING_HAND_MAX];

static int can_complete_by_counts(int wid, const StrategyCounts* counts);
static void add_card_to_counts(StrategyCounts* counts, const Card* card);
static void add_zone_to_counts(StrategyCounts* counts, CardSet* set);
static int is_bonus_target_card(int wid, const Card* card);
static void init_strategy_requirements(void);
static int is_threshold_role_wid(int wid);
static void build_counts_for_player_fixed(int player, StrategyCounts* counts);
static void build_counts_for_player_plus_floor(int player, StrategyCounts* counts);
static int can_be_blocked_by_my_known_cards(int wid, int player);
static int calc_fixed_role_near_completion_threat(int wid, int player, const StrategyCounts* counts_opp_fixed, int* out_floor_missing,
                                                  int* out_est_missing);
static int calc_risk_delay(int wid, int player, const StrategyCounts* counts_opp_fixed);
static int calc_risk_reach_estimate(int wid, int player, const StrategyCounts* counts_opp_fixed, const StrategyCounts* counts_opp_plus_floor, int risk_delay);
static int calc_risk_score(int wid, int base_score, int risk_reach_estimate);
static int estimate_risk_7plus_distance(const StrategyData* data, int opp_current_score);
static int estimate_opp_coarse_threat(const StrategyData* data, int opp_current_score);
static int estimate_opp_exact_7plus_threat(const StrategyData* data, int opp_current_score);
static int risk_threat_multiplier_pct(int opp_coarse_threat, int koikoi_opp);
static int clamp_int(int x, int min, int max);
static void strategy_self_best_metrics(const StrategyData* data, StrategySelfBestMetrics* out_metrics);
static void strategy_opp_best_metrics(const StrategyData* data, StrategyOppBestMetrics* out_metrics);
static int strategy_linear_behind_bonus(int match_score_diff, int max_bonus);
static int strategy_behind_offence_tier(int match_score_diff);
static void strategy_calc_mode(StrategyData* data);
static void strategy_calc_bias(StrategyData* data);
static void ai_env_calc_cat_sum(int player, int out_sum[ENV_CAT_MAX]);
static void ai_env_add_cat_sum_from_set(int player, CardSet* set, int multiplier, int count, int out_sum[ENV_CAT_MAX]);
static AiEnvDomain ai_env_estimate_domain(const int cat_sum[ENV_CAT_MAX]);
static int strategy_has_7plus_line_with_delay_leq(const StrategyData* data, int round_score, int max_delay);
static AiPlan ai_env_estimate_plan(const StrategyData* data, int round_score, int* out_reason);
static int can_overpay_by_counts(const StrategyCounts* counts, int wid);
static int calc_role_score(int wid, const StrategyCounts* counts);

#define STRATEGY_REACH_MC_SAMPLES_MAX 32
#define STRATEGY_REACH_VSYNC_INTERVAL 1
#define STRATEGY_REACH_VSYNC_MIN_CALLS 20
#define STRATEGY_DELAY_INVALID 99

#define STRATEGY_RISK_FIXED_NEAR_COMPLETE_FLOOR_BONUS 22
#define STRATEGY_RISK_FIXED_NEAR_COMPLETE_EST_BONUS 14
#define STRATEGY_RISK_FIXED_NEAR_COMPLETE_DELAY_REDUCTION 1

#define STRATEGY_DEFENCE_KOIKOI_OPP_BONUS 25
#define STRATEGY_DEFENCE_RISK_DELAY_NEAR2_BONUS 30
#define STRATEGY_DEFENCE_RISK_DELAY_NEAR4_BONUS 20
#define STRATEGY_DEFENCE_RISK_DELAY_NEAR6_BONUS 10
#define STRATEGY_DEFENCE_RISK_EV_MEDIUM_BONUS 15
#define STRATEGY_DEFENCE_RISK_EV_HIGH_BONUS 25
#define STRATEGY_DEFENCE_LEFT_OWN_NEAR3_BONUS 10
#define STRATEGY_DEFENCE_LEFT_OWN_NEAR2_BONUS 20
#define STRATEGY_DEFENCE_LEFT_ROUNDS_NEAR2_BONUS 10
#define STRATEGY_DEFENCE_LEFT_ROUNDS_NEAR1_BONUS 15
#define STRATEGY_DEFENCE_OPP_WIN_X2_BONUS 30
#define STRATEGY_DEFENCE_RISK_7PLUS_NEAR2_BONUS 35
#define STRATEGY_DEFENCE_RISK_7PLUS_NEAR4_BONUS 20

#define STRATEGY_SPEED_LEFT_OWN_NEAR7_BONUS 5
#define STRATEGY_SPEED_LEFT_OWN_NEAR5_BONUS 15
#define STRATEGY_SPEED_LEFT_OWN_NEAR3_BONUS 25
#define STRATEGY_SPEED_OPP_DELAY_NEAR4_BONUS 15
#define STRATEGY_SPEED_OPP_DELAY_NEAR2_BONUS 25
#define STRATEGY_SPEED_SELF_DELAY_NEAR3_BONUS 10
#define STRATEGY_SPEED_SELF_DELAY_NEAR2_BONUS 20
#define STRATEGY_SPEED_KOIKOI_OPP_BONUS 10
#define STRATEGY_SPEED_KOIKOI_MINE_BONUS 10
#define STRATEGY_SPEED_LEFT_ROUNDS_NEAR1_BONUS 10
#define STRATEGY_SPEED_OPP_WIN_X2_BONUS 25
#define STRATEGY_SPEED_RISK_7PLUS_NEAR2_BONUS 30
#define STRATEGY_SPEED_RISK_7PLUS_NEAR4_BONUS 15

#define STRATEGY_OFFENCE_KOIKOI_MINE_BONUS 25
#define STRATEGY_OFFENCE_LEFT_ROUNDS_NEAR2_BONUS 15
#define STRATEGY_OFFENCE_LEFT_ROUNDS_NEAR1_BONUS 25
#define STRATEGY_OFFENCE_SELF_EV_LOW_BONUS 10
#define STRATEGY_OFFENCE_SELF_EV_MID_BONUS 20
#define STRATEGY_OFFENCE_SELF_EV_HIGH_BONUS 30
#define STRATEGY_OFFENCE_VS_DANGEROUS_OPP_BONUS 10
#define STRATEGY_OFFENCE_TOO_LATE_PENALTY 10
#define STRATEGY_OFFENCE_OPP_WIN_X2_PENALTY 15

#define STRATEGY_RISK_THREAT_EV_WEIGHT 15
#define STRATEGY_RISK_THREAT_KOIKOI_BONUS 25
#define STRATEGY_RISK_THREAT_DELAY_NEAR2_BONUS 40
#define STRATEGY_RISK_THREAT_DELAY_NEAR3_BONUS 25
#define STRATEGY_RISK_THREAT_DELAY_NEAR4_BONUS 10
#define STRATEGY_RISK_MULT_LOW_PCT 170
#define STRATEGY_RISK_MULT_MID_PCT 250
#define STRATEGY_RISK_MULT_HIGH_PCT 400
#define STRATEGY_RISK_MULT_KOIKOI_PCT 130

#define STRATEGY_GREEDY_BEHIND_FACTOR 6
#define STRATEGY_GREEDY_LEFT_ROUNDS_NEAR2_BONUS 28
#define STRATEGY_GREEDY_LEFT_ROUNDS_NEAR1_BONUS 40
#define STRATEGY_GREEDY_SELF_SCORE_NEAR5_BONUS 20
#define STRATEGY_GREEDY_SELF_SCORE_NEAR7_BONUS 35
#define STRATEGY_GREEDY_SELF_DELAY_NEAR2_BONUS 24
#define STRATEGY_GREEDY_SELF_DELAY_NEAR3_BONUS 14
#define STRATEGY_GREEDY_SELF_EV_HIGH_BONUS 24
#define STRATEGY_GREEDY_KOIKOI_MINE_BONUS 20
#define STRATEGY_FINAL_LEAD_GREEDY_PENALTY 36
#define STRATEGY_FINAL_LEAD_DEFENCE_BONUS 42
#define STRATEGY_BEHIND_LINEAR_MAX_DIFF 24
#define STRATEGY_BEHIND_GREEDY_BONUS_MAX 30
#define STRATEGY_BEHIND_DEFENCE_NEED_PENALTY_MAX 18

#define STRATEGY_DEFENSIVE_LEAD_FACTOR 2
#define STRATEGY_DEFENSIVE_KOIKOI_OPP_BONUS 18
#define STRATEGY_DEFENSIVE_THREAT_DIVISOR 2

#define STRATEGY_MODE_SWITCH_MARGIN 15
#define STRATEGY_MODE_GREEDY_OFFENCE_BONUS 40
#define STRATEGY_MODE_GREEDY_SPEED_BONUS 15
#define STRATEGY_MODE_GREEDY_DEFENCE_PENALTY 15
#define STRATEGY_MODE_DEFENCE_DEFENCE_BONUS 20
#define STRATEGY_MODE_DEFENCE_SPEED_BONUS 8
#define STRATEGY_MODE_DEFENCE_OFFENCE_PENALTY 10
#define STRATEGY_MODE_DEFENCE_OFFENCE_FLOOR 20
#define STRATEGY_MODE_DEFENCE_OFFENCE_FLOOR_BEHIND 30
#define STRATEGY_MODE_FORCE_DEFENCE_THREAT 80
#define STRATEGY_MODE_FORCE_DEFENCE_THREAT_KOIKOI 60
#define STRATEGY_MODE_FORCE_GREEDY_BEHIND 12
#define STRATEGY_MODE_FORCE_GREEDY_LEFT_ROUNDS 3
#define STRATEGY_MODE_FORCE_DEFENCE_FINAL_LEAD 1
#define STRATEGY_BIAS_FINAL_LEAD_OFFENCE_PENALTY 30
#define STRATEGY_BIAS_FINAL_LEAD_SPEED_BONUS 28
#define STRATEGY_BIAS_FINAL_LEAD_DEFENCE_BONUS 32
#define ENV_DOMAIN_MIN_THRESHOLD 300
#ifndef PLAN_PRESS_DIFF_THRESHOLD
#define PLAN_PRESS_DIFF_THRESHOLD 600
#endif
#ifndef PLAN_SURVIVE_DIFF_THRESHOLD
#define PLAN_SURVIVE_DIFF_THRESHOLD 800
#endif
#ifndef PLAN_PRESS_COMBO7_REACH_THRESHOLD
#define PLAN_PRESS_COMBO7_REACH_THRESHOLD 101
#endif
#ifndef PLAN_PRESS_SEVEN_LINE_DELAY_MAX
#define PLAN_PRESS_SEVEN_LINE_DELAY_MAX 3
#endif
#ifndef PLAN_PRESS_BLOCK_OPP_THREAT
#define PLAN_PRESS_BLOCK_OPP_THREAT 85
#endif

static const int STRATEGY_ENV_CAT_BASE[ENV_CAT_MAX] = {
    ENV_CAT1_BASE,
    ENV_CAT2_BASE,
    ENV_CAT3_BASE,
    ENV_CAT4_BASE,
    ENV_CAT5_BASE,
    ENV_CAT6_BASE,
    ENV_CAT7_BASE,
    ENV_CAT8_BASE,
    ENV_CAT9_BASE,
    ENV_CAT10_BASE,
    ENV_CAT11_BASE,
    ENV_CAT12_BASE,
    ENV_CATNA_BASE,
};

static void ai_env_add_cat_sum_from_set(int player, CardSet* set, int multiplier, int count, int out_sum[ENV_CAT_MAX])
{
    if (!set || !out_sum || multiplier == 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        Card* card = set->cards[i];
        int card_index;
        int cat;
        if (!card) {
            continue;
        }
        card_index = card_global_index(card);
        if (card_index < 0) {
            continue;
        }
        cat = ai_env_effective_category_for_player(player, card_index);
        if (cat < 0 || cat >= ENV_CAT_MAX) {
            continue;
        }
        out_sum[cat] += STRATEGY_ENV_CAT_BASE[cat] * multiplier;
    }
}

static void ai_env_calc_cat_sum(int player, int out_sum[ENV_CAT_MAX])
{
    int opp = 1 - player;

    if (!out_sum || player < 0 || player > 1) {
        return;
    }

    vgs_memset(out_sum, 0, sizeof(int) * ENV_CAT_MAX);

    for (int t = 0; t < 4; t++) {
        ai_env_add_cat_sum_from_set(player, &g.invent[player][t], 5, g.invent[player][t].num, out_sum);
        ai_env_add_cat_sum_from_set(player, &g.invent[opp][t], -5, g.invent[opp][t].num, out_sum);
    }
    ai_env_add_cat_sum_from_set(player, &g.own[player], 2, 8, out_sum);
    ai_env_add_cat_sum_from_set(player, &g.deck, 1, 48, out_sum);
}

static AiEnvDomain ai_env_estimate_domain(const int cat_sum[ENV_CAT_MAX])
{
    int light_score;
    int sake_score;
    int akatan_score;
    int aotan_score;
    int isc_score;
    int best_score;
    AiEnvDomain best_domain;

    if (!cat_sum) {
        return AI_ENV_DOMAIN_NONE;
    }

    light_score = cat_sum[ENV_CAT_1] + cat_sum[ENV_CAT_3] + cat_sum[ENV_CAT_6];
    sake_score = cat_sum[ENV_CAT_2] + cat_sum[ENV_CAT_1];
    akatan_score = cat_sum[ENV_CAT_4];
    aotan_score = cat_sum[ENV_CAT_5];
    isc_score = cat_sum[ENV_CAT_7];

    best_score = light_score;
    best_domain = AI_ENV_DOMAIN_LIGHT;
    if (sake_score > best_score) {
        best_score = sake_score;
        best_domain = AI_ENV_DOMAIN_SAKE;
    }
    if (akatan_score > best_score) {
        best_score = akatan_score;
        best_domain = AI_ENV_DOMAIN_AKATAN;
    }
    if (aotan_score > best_score) {
        best_score = aotan_score;
        best_domain = AI_ENV_DOMAIN_AOTAN;
    }
    if (isc_score > best_score) {
        best_score = isc_score;
        best_domain = AI_ENV_DOMAIN_ISC;
    }

    return best_score >= ENV_DOMAIN_MIN_THRESHOLD ? best_domain : AI_ENV_DOMAIN_NONE;
}

static int strategy_has_7plus_line_with_delay_leq(const StrategyData* data, int round_score, int max_delay)
{
    if (!data) {
        return OFF;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (data->score[wid] <= 0 || data->delay[wid] >= 99) {
            continue;
        }
        if (round_score + data->score[wid] >= 7 && data->delay[wid] <= max_delay) {
            return ON;
        }
    }

    return OFF;
}

static AiPlan ai_env_estimate_plan(const StrategyData* data, int round_score, int* out_reason)
{
    AiCombo7Eval combo7_eval;
    int press_by_diff;
    int press_by_combo7;
    int press_by_sevenline;

    if (out_reason) {
        *out_reason = -1;
    }
    if (!data) {
        return AI_PLAN_SURVIVE;
    }

    if (data->env_diff <= -PLAN_SURVIVE_DIFF_THRESHOLD) {
        if (out_reason) {
            *out_reason = AI_PLAN_REASON_SURVIVE_DIFF;
        }
        return AI_PLAN_SURVIVE;
    }

    ai_eval_combo7(0, round_score, data, &combo7_eval);
    press_by_diff = data->env_diff >= PLAN_PRESS_DIFF_THRESHOLD ? ON : OFF;
    press_by_combo7 = combo7_eval.combo_reach >= PLAN_PRESS_COMBO7_REACH_THRESHOLD ? ON : OFF;
    press_by_sevenline = strategy_has_7plus_line_with_delay_leq(data, round_score, PLAN_PRESS_SEVEN_LINE_DELAY_MAX);

    if (data->opp_coarse_threat >= PLAN_PRESS_BLOCK_OPP_THREAT && (press_by_diff || press_by_combo7 || press_by_sevenline)) {
        if (out_reason) {
            *out_reason = AI_PLAN_REASON_BLOCK_THREAT;
        }
        return AI_PLAN_SURVIVE;
    }

    if (press_by_diff) {
        if (out_reason) {
            *out_reason = AI_PLAN_REASON_DIFF;
        }
        return AI_PLAN_PRESS;
    }
    if (press_by_combo7) {
        if (out_reason) {
            *out_reason = AI_PLAN_REASON_COMBO7;
        }
        return AI_PLAN_PRESS;
    }
    if (press_by_sevenline) {
        if (out_reason) {
            *out_reason = AI_PLAN_REASON_SEVENLINE;
        }
        return AI_PLAN_PRESS;
    }

    return AI_PLAN_SURVIVE;
}

typedef struct {
    Card* hand[2][8];
    int hand_num[2];
    Card* captured[2][48];
    int captured_num[2];
    Card* table[48];
    int table_num;
    Card* floor[48];
    int floor_num;
    int floor_pos;
    StrategyCounts counts[2];
    int current_player;
} ReachSimState;

enum StrategyCluster {
    STRATEGY_CLUSTER_LIGHT = 0,
    STRATEGY_CLUSTER_HANAMI,
    STRATEGY_CLUSTER_TSUKIMI,
    STRATEGY_CLUSTER_ISC,
    STRATEGY_CLUSTER_DBTAN,
    STRATEGY_CLUSTER_TANE,
    STRATEGY_CLUSTER_TAN,
    STRATEGY_CLUSTER_KASU,
    STRATEGY_CLUSTER_MAX
};

static int strategy_cluster_of_wid(int wid)
{
    if (ai_is_disabled_wid_by_rules(wid)) {
        return STRATEGY_CLUSTER_KASU;
    }
    switch (wid) {
        case WID_GOKOU:
        case WID_SHIKOU:
        case WID_AME_SHIKOU:
        case WID_SANKOU:
            return STRATEGY_CLUSTER_LIGHT;
        case WID_HANAMI:
            return STRATEGY_CLUSTER_HANAMI;
        case WID_TSUKIMI:
            return STRATEGY_CLUSTER_TSUKIMI;
        case WID_ISC:
            return STRATEGY_CLUSTER_ISC;
        case WID_DBTAN:
        case WID_AKATAN:
        case WID_AOTAN:
            return STRATEGY_CLUSTER_DBTAN;
        case WID_TANE:
            return STRATEGY_CLUSTER_TANE;
        case WID_TAN:
            return STRATEGY_CLUSTER_TAN;
        case WID_KASU:
            return STRATEGY_CLUSTER_KASU;
        default:
            return STRATEGY_CLUSTER_KASU;
    }
}

int ai_strategy_primary_cluster(const StrategyData* data)
{
    int wid;

    if (!data) {
        return STRATEGY_CLUSTER_KASU;
    }
    wid = data->priority_score[0];
    if (wid < 0 || wid >= WINNING_HAND_MAX) {
        return STRATEGY_CLUSTER_KASU;
    }
    return strategy_cluster_of_wid(wid);
}

static int strategy_reach_sample_count(int hand_num)
{
    if (hand_num >= 7) {
        return STRATEGY_REACH_MC_SAMPLES_MAX;
    }
    return g.floor.num + hand_num;
}

static uint32_t strategy_rng_next(uint32_t* state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static int strategy_rand_range(uint32_t* state, int n)
{
    if (n <= 0) {
        return 0;
    }
    return (int)(strategy_rng_next(state) % (uint32_t)n);
}

static void strategy_pin_no_sake_hidden_cards_to_tail(Card** cards, int count)
{
    int tail = count - 1;

    if (!ai_is_no_sake_mode() || !cards || count <= 0) {
        return;
    }
    for (int target = 35; target >= 32; target -= 3) {
        for (int i = 0; i <= tail; i++) {
            if (cards[i] && cards[i]->id == target) {
                Card* tmp = cards[tail];
                cards[tail] = cards[i];
                cards[i] = tmp;
                tail--;
                break;
            }
        }
    }
}

static int strategy_capture_priority(const StrategyCounts* counts, const Card* card)
{
    if (!counts || !card) {
        return 0;
    }
    static const int base_capture_priority[4] = {
        400, // CARD_TYPE_GOKOU
        300, // CARD_TYPE_TANE
        200, // CARD_TYPE_TAN
        100  // CARD_TYPE_KASU
    };
    int type = card->type;
    int captured = 0;
    switch (type) {
        case CARD_TYPE_GOKOU: captured = counts->gokou; break;
        case CARD_TYPE_TANE: captured = counts->tane; break;
        case CARD_TYPE_TAN: captured = counts->tan; break;
        case CARD_TYPE_KASU: captured = counts->kasu; break;
        default: break;
    }
    int priority = base_capture_priority[type] + captured * 32;
    if (type == CARD_TYPE_TANE && captured >= 2) {
        priority += 1000;
    }
    if (type == CARD_TYPE_TAN && captured >= 2) {
        priority += 900;
    }
    if (type == CARD_TYPE_KASU && captured >= 5) {
        priority += 800;
    }
    return priority;
}

static int strategy_discard_priority(const Card* card)
{
    if (!card) {
        return -1;
    }
    static const int discard_priority_map[4] = {
        0, // CARD_TYPE_GOKOU
        1, // CARD_TYPE_TANE
        2, // CARD_TYPE_TAN
        3  // CARD_TYPE_KASU
    };
    return discard_priority_map[card->type];
}

static int strategy_cluster_card_bonus(int cluster, const Card* card)
{
    if (!card) {
        return 0;
    }
    if (ai_is_no_sake_mode() && (cluster == STRATEGY_CLUSTER_HANAMI || cluster == STRATEGY_CLUSTER_TSUKIMI)) {
        return 0;
    }
    switch (cluster) {
        case STRATEGY_CLUSTER_LIGHT:
            return card->type == CARD_TYPE_GOKOU ? 20 : 0;
        case STRATEGY_CLUSTER_HANAMI:
            return ((card->month == 2 && card->index == 3) || (card->month == 8 && card->index == 3)) ? 20 : 0;
        case STRATEGY_CLUSTER_TSUKIMI:
            return ((card->month == 7 && card->index == 3) || (card->month == 8 && card->index == 3)) ? 20 : 0;
        case STRATEGY_CLUSTER_ISC:
            return ((card->month == 5 && card->index == 3) || (card->month == 6 && card->index == 3) || (card->month == 9 && card->index == 3))
                       ? 20
                       : 0;
        case STRATEGY_CLUSTER_DBTAN:
            return card->type == CARD_TYPE_TAN ? 20 : 0;
        case STRATEGY_CLUSTER_TANE:
            return card->type == CARD_TYPE_TANE ? 20 : 0;
        case STRATEGY_CLUSTER_TAN:
            return card->type == CARD_TYPE_TAN ? 20 : 0;
        case STRATEGY_CLUSTER_KASU:
            return (card->type == CARD_TYPE_KASU || (card->type == CARD_TYPE_TANE && card->month == 8)) ? 20 : 0;
        default:
            return 0;
    }
}

static int strategy_role_progress(int wid, const StrategyCounts* counts)
{
    if (!counts) {
        return 0;
    }
    if (ai_is_disabled_wid_by_rules(wid)) {
        return 0;
    }
    if (can_complete_by_counts(wid, counts)) {
        return 100;
    }
    switch (wid) {
        case WID_GOKOU: return counts->gokou * 20;
        case WID_SHIKOU: return counts->shikou * 25;
        case WID_AME_SHIKOU: return counts->gokou > counts->shikou ? counts->gokou * 25 : counts->shikou * 20;
        case WID_SANKOU: return counts->shikou * 33;
        case WID_HANAMI: return counts->hanami * 50;
        case WID_TSUKIMI: return counts->tsukimi * 50;
        case WID_ISC: return counts->isc * 33;
        case WID_DBTAN: {
            int v = counts->akatan < counts->aotan ? counts->akatan : counts->aotan;
            return v * 33;
        }
        case WID_AKATAN: return counts->akatan * 33;
        case WID_AOTAN: return counts->aotan * 33;
        case WID_TANE: return counts->tane * 20;
        case WID_TAN: return counts->tan * 20;
        case WID_KASU: return counts->kasu * 10;
        default: return 0;
    }
}

static int strategy_cluster_progress(int cluster, const StrategyCounts* counts)
{
    if (!counts) {
        return 0;
    }
    int best = 0;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (strategy_cluster_of_wid(wid) != cluster) {
            continue;
        }
        int p = strategy_role_progress(wid, counts);
        if (p > best) {
            best = p;
        }
    }
    return best;
}

static int build_hidden_pool(int player, Card** hidden_pool, int hidden_cap)
{
    if (!hidden_pool || hidden_cap <= 0) {
        return 0;
    }
    int known[48];
    vgs_memset(known, 0, sizeof(known));
    int pool_num = 0;

    for (int i = 0; i < 8; i++) {
        Card* c = g.own[player].cards[i];
        if (!c) {
            continue;
        }
        int id = card_global_index(c);
        if (0 <= id && id < 48) {
            known[id] = ON;
        }
    }
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 4; t++) {
            for (int i = 0; i < 48; i++) {
                Card* c = g.invent[p][t].cards[i];
                if (!c) {
                    continue;
                }
                int id = card_global_index(c);
                if (0 <= id && id < 48) {
                    known[id] = ON;
                }
            }
        }
    }
    for (int i = 0; i < 48; i++) {
        Card* c = g.deck.cards[i];
        if (!c) {
            continue;
        }
        int id = card_global_index(c);
        if (0 <= id && id < 48) {
            known[id] = ON;
        }
    }

    for (int i = 0; i < 48 && pool_num < hidden_cap; i++) {
        if (known[i]) {
            continue;
        }
        hidden_pool[pool_num++] = &g.cards[i];
    }
    return pool_num;
}

/*
 * reach 定義の近似:
 * 観測可能情報を固定し、隠れ札プールをランダムに決定化して
 * 「相手手札 + 山札順序」をサンプルした状態を生成する。
 */
static int sample_determinization(int player, const Card** hidden_pool, int hidden_num, uint32_t* rng_state, ReachSimState* sim)
{
    if (!hidden_pool || !rng_state || !sim) {
        return OFF;
    }
    vgs_memset(sim, 0, sizeof(*sim));
    int opp = 1 - player;
    int opp_hand_num = g.own[opp].num;
    int floor_num = g.floor.num;
    int needed = opp_hand_num + floor_num;
    if (hidden_num < needed) {
        return OFF;
    }

    for (int i = 0; i < 8; i++) {
        Card* c = g.own[player].cards[i];
        if (!c) {
            continue;
        }
        sim->hand[player][sim->hand_num[player]++] = c;
    }
    for (int i = 0; i < 48; i++) {
        Card* c = g.deck.cards[i];
        if (!c) {
            continue;
        }
        sim->table[sim->table_num++] = c;
    }
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 4; t++) {
            for (int i = 0; i < g.invent[p][t].num; i++) {
                Card* c = g.invent[p][t].cards[i];
                if (!c) {
                    continue;
                }
                sim->captured[p][sim->captured_num[p]++] = c;
            }
            add_zone_to_counts(&sim->counts[p], &g.invent[p][t]);
        }
    }

    Card* shuffled[48];
    for (int i = 0; i < hidden_num; i++) {
        shuffled[i] = (Card*)hidden_pool[i];
    }
    for (int i = hidden_num - 1; i > 0; i--) {
        int j = strategy_rand_range(rng_state, i + 1);
        Card* tmp = shuffled[i];
        shuffled[i] = shuffled[j];
        shuffled[j] = tmp;
    }
    strategy_pin_no_sake_hidden_cards_to_tail(shuffled, hidden_num);
    int ptr = 0;
    for (int i = 0; i < opp_hand_num; i++) {
        sim->hand[opp][sim->hand_num[opp]++] = shuffled[ptr++];
    }
    sim->floor_num = floor_num;
    for (int i = 0; i < floor_num; i++) {
        sim->floor[i] = shuffled[ptr++];
    }
    sim->floor_pos = 0;
    sim->current_player = g.current_player;
    return ON;
}

static int collect_month_matches_from_table(const ReachSimState* sim, int month, int* indexes, int cap)
{
    if (!sim || !indexes || cap <= 0) {
        return 0;
    }
    int n = 0;
    for (int i = 0; i < sim->table_num && n < cap; i++) {
        Card* c = sim->table[i];
        if (c && c->month == month) {
            indexes[n++] = i;
        }
    }
    return n;
}

static void remove_table_index(ReachSimState* sim, int idx)
{
    if (!sim || idx < 0 || idx >= sim->table_num) {
        return;
    }
    for (int i = idx; i < sim->table_num - 1; i++) {
        sim->table[i] = sim->table[i + 1];
    }
    sim->table_num--;
}

static int choose_capture_index_for_cluster(const ReachSimState* sim, int player, int cluster, const Card* played, const int* matches, int match_num)
{
    if (!sim || !played || !matches || match_num <= 0) {
        return 0;
    }
    StrategyCounts base = sim->counts[player];
    int best = 0;
    int best_score = -999999;
    for (int i = 0; i < match_num; i++) {
        Card* floor_card = sim->table[matches[i]];
        StrategyCounts next = base;
        add_card_to_counts(&next, played);
        add_card_to_counts(&next, floor_card);
        int score = strategy_capture_priority(&base, floor_card) * 4;
        score += strategy_capture_priority(&base, played);
        score += strategy_capture_priority(&base, floor_card);
        score += strategy_cluster_card_bonus(cluster, played) * 3;
        score += strategy_cluster_card_bonus(cluster, floor_card) * 3;
        score += (strategy_cluster_progress(cluster, &next) - strategy_cluster_progress(cluster, &base)) * 120;
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

static int choose_capture_index_greedy(const ReachSimState* sim, int player, const int* matches, int match_num)
{
    if (!sim || !matches || match_num <= 0) {
        return 0;
    }
    int best = 0;
    int best_score = -1;
    for (int i = 0; i < match_num; i++) {
        Card* floor_card = sim->table[matches[i]];
        int score = strategy_capture_priority(&sim->counts[player], floor_card);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

static void sim_capture_card(ReachSimState* sim, int player, Card* card)
{
    if (!sim || !card || player < 0 || player > 1) {
        return;
    }
    add_card_to_counts(&sim->counts[player], card);
    if (sim->captured_num[player] < 48) {
        sim->captured[player][sim->captured_num[player]++] = card;
    }
}

static void resolve_played_card(ReachSimState* sim, int player, int cluster, int cluster_mode, Card* card)
{
    if (!sim || !card || player < 0 || player > 1) {
        return;
    }
    int matches[4];
    int match_num = collect_month_matches_from_table(sim, card->month, matches, 4);
    if (match_num <= 0) {
        if (sim->table_num < 48) {
            sim->table[sim->table_num++] = card;
        }
        return;
    }
    if (match_num == 1) {
        sim_capture_card(sim, player, card);
        sim_capture_card(sim, player, sim->table[matches[0]]);
        remove_table_index(sim, matches[0]);
        return;
    }
    if (match_num == 2) {
        int pick = cluster_mode ? choose_capture_index_for_cluster(sim, player, cluster, card, matches, match_num)
                                : choose_capture_index_greedy(sim, player, matches, match_num);
        int t = matches[pick];
        sim_capture_card(sim, player, card);
        sim_capture_card(sim, player, sim->table[t]);
        remove_table_index(sim, t);
        return;
    }
    sim_capture_card(sim, player, card);
    for (int i = match_num - 1; i >= 0; i--) {
        sim_capture_card(sim, player, sim->table[matches[i]]);
        remove_table_index(sim, matches[i]);
    }
}

static int choose_hand_index_for_cluster(const ReachSimState* sim, int player, int cluster)
{
    if (!sim || player < 0 || player > 1 || sim->hand_num[player] <= 0) {
        return 0;
    }
    StrategyCounts base = sim->counts[player];
    int best_capture = -1;
    int best_capture_score = -999999;
    int best_discard = 0;
    int best_discard_score = -999999;
    for (int i = 0; i < sim->hand_num[player]; i++) {
        Card* card = sim->hand[player][i];
        int matches[4];
        int match_num = collect_month_matches_from_table(sim, card->month, matches, 4);
        if (match_num > 0) {
            int pick = 0;
            if (match_num == 2) {
                pick = choose_capture_index_for_cluster(sim, player, cluster, card, matches, match_num);
            }
            StrategyCounts next = base;
            add_card_to_counts(&next, card);
            if (match_num == 2) {
                add_card_to_counts(&next, sim->table[matches[pick]]);
            } else {
                for (int j = 0; j < match_num; j++) {
                    add_card_to_counts(&next, sim->table[matches[j]]);
                }
            }
            int score = 2000;
            int taken_priority = 0;
            if (match_num == 2) {
                taken_priority = strategy_capture_priority(&base, sim->table[matches[pick]]);
            } else if (match_num >= 1) {
                for (int j = 0; j < match_num; j++) {
                    int next_priority = strategy_capture_priority(&base, sim->table[matches[j]]);
                    if (next_priority > taken_priority) {
                        taken_priority = next_priority;
                    }
                }
            }
            score += taken_priority * 4;
            score += strategy_capture_priority(&base, card);
            score += strategy_cluster_card_bonus(cluster, card) * 3;
            score += (strategy_cluster_progress(cluster, &next) - strategy_cluster_progress(cluster, &base)) * 120;
            if (score > best_capture_score) {
                best_capture_score = score;
                best_capture = i;
            }
            continue;
        }

        {
            int score = strategy_discard_priority(card) * 16 - strategy_cluster_card_bonus(cluster, card) * 3;
            if (score > best_discard_score) {
                best_discard_score = score;
                best_discard = i;
            }
        }
    }
    if (best_capture >= 0) {
        return best_capture;
    }
    return best_discard;
}

/*
 * π_opp は ai_normal_* を直接使わず、既存通常AIの優先順位に近い貪欲則を使う:
 * - 取れるなら高優先カードを取る
 * - 取れないならカス優先で捨てる
 */
static int choose_hand_index_for_opponent(const ReachSimState* sim, int player)
{
    if (!sim || player < 0 || player > 1 || sim->hand_num[player] <= 0) {
        return 0;
    }
    int best_capture_i = -1;
    int best_capture_score = -1;
    int best_discard_i = 0;
    int best_discard_score = -1;
    for (int i = 0; i < sim->hand_num[player]; i++) {
        Card* card = sim->hand[player][i];
        int matches[4];
        int match_num = collect_month_matches_from_table(sim, card->month, matches, 4);
        if (match_num > 0) {
            int cap_score = -1;
            for (int m = 0; m < match_num; m++) {
                int p = strategy_capture_priority(&sim->counts[player], sim->table[matches[m]]);
                if (p > cap_score) {
                    cap_score = p;
                }
            }
            if (cap_score > best_capture_score) {
                best_capture_score = cap_score;
                best_capture_i = i;
            }
        } else {
            int d = strategy_discard_priority(card);
            if (d > best_discard_score) {
                best_discard_score = d;
                best_discard_i = i;
            }
        }
    }
    if (best_capture_i >= 0) {
        return best_capture_i;
    }
    return best_discard_i;
}

static Card* remove_hand_at(ReachSimState* sim, int player, int idx)
{
    if (!sim || player < 0 || player > 1 || idx < 0 || idx >= sim->hand_num[player]) {
        return NULL;
    }
    Card* c = sim->hand[player][idx];
    for (int i = idx; i < sim->hand_num[player] - 1; i++) {
        sim->hand[player][i] = sim->hand[player][i + 1];
    }
    sim->hand_num[player]--;
    return c;
}

/*
 * cluster ごとの π_wid でラウンド終端までプレイアウトし、
 * 途中で新規成立した役を reached[] に記録する。
 */
static void rollout_until_end_for_cluster(ReachSimState* sim, int target_player, int cluster, const int* already_completed, int* reached,
                                          int* completion_turn)
{
    if (!sim || !already_completed || !reached || !completion_turn) {
        return;
    }
    int guard = 48;
    int own_turn = 0;
    while ((sim->hand_num[0] > 0 || sim->hand_num[1] > 0) && guard-- > 0) {
        int p = sim->current_player;
        if (p < 0 || p > 1) {
            p = 0;
            sim->current_player = 0;
        }
        if (sim->hand_num[p] <= 0) {
            sim->current_player = 1 - p;
            continue;
        }

        int hand_idx = (p == target_player) ? choose_hand_index_for_cluster(sim, p, cluster) : choose_hand_index_for_opponent(sim, p);
        Card* hand_card = remove_hand_at(sim, p, hand_idx);
        resolve_played_card(sim, p, cluster, p == target_player, hand_card);

        if (sim->floor_pos < sim->floor_num) {
            Card* draw_card = sim->floor[sim->floor_pos++];
            resolve_played_card(sim, p, cluster, p == target_player, draw_card);
        }

        if (p == target_player) {
            own_turn++;
            for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
                if (already_completed[wid] || reached[wid]) {
                    continue;
                }
                if (can_complete_by_counts(wid, &sim->counts[target_player])) {
                    reached[wid] = ON;
                    completion_turn[wid] = own_turn;
                }
            }
        }
        sim->current_player = 1 - p;
    }
}

static int strategy_total_score_from_counts(const StrategyCounts* counts, int opponent_koikoi)
{
    int light_score = 0;
    int sake_score = 0;
    int tane_score = 0;
    int tan_score = 0;
    int kasu_score = 0;
    int total;

    if (!counts) {
        return 0;
    }
    for (int wid = WID_SANKOU; wid <= WID_GOKOU; wid++) {
        int score = calc_role_score(wid, counts);
        if (score > light_score) {
            light_score = score;
        }
    }
    if (!ai_is_no_sake_mode()) {
        sake_score += calc_role_score(WID_HANAMI, counts);
        sake_score += calc_role_score(WID_TSUKIMI, counts);
    }

    {
        int isc_score = calc_role_score(WID_ISC, counts);
        int plain_tane_score = calc_role_score(WID_TANE, counts);
        tane_score = isc_score > plain_tane_score ? isc_score : plain_tane_score;
    }
    {
        int dbtan_score = calc_role_score(WID_DBTAN, counts);
        int akatan_score = calc_role_score(WID_AKATAN, counts);
        int aotan_score = calc_role_score(WID_AOTAN, counts);
        int plain_tan_score = calc_role_score(WID_TAN, counts);
        tan_score = dbtan_score;
        if (akatan_score > tan_score) {
            tan_score = akatan_score;
        }
        if (aotan_score > tan_score) {
            tan_score = aotan_score;
        }
        if (plain_tan_score > tan_score) {
            tan_score = plain_tan_score;
        }
    }
    kasu_score = calc_role_score(WID_KASU, counts);

    total = light_score + sake_score + tane_score + tan_score + kasu_score;
    if (total >= 7 || opponent_koikoi == ON) {
        total *= 2;
    }
    return total;
}

static int find_hand_index_by_card_no(const ReachSimState* sim, int player, int card_no)
{
    if (!sim || player < 0 || player > 1) {
        return -1;
    }
    for (int i = 0; i < sim->hand_num[player]; i++) {
        Card* card = sim->hand[player][i];
        if (card && card->id == card_no) {
            return i;
        }
    }
    return -1;
}

static int choose_forced_capture_index(const ReachSimState* sim, const int* matches, int match_num, int taken_card_no)
{
    if (!sim || !matches || match_num <= 0) {
        return -1;
    }
    if (taken_card_no < 0) {
        return 0;
    }
    for (int i = 0; i < match_num; i++) {
        Card* table_card = sim->table[matches[i]];
        if (table_card && table_card->id == taken_card_no) {
            return i;
        }
    }
    return -1;
}

static int resolve_forced_played_card(ReachSimState* sim, int player, int cluster, Card* card, int taken_card_no)
{
    int matches[4];
    int match_num;

    (void)cluster;

    if (!sim || !card || player < 0 || player > 1) {
        return OFF;
    }
    match_num = collect_month_matches_from_table(sim, card->month, matches, 4);
    if (match_num <= 0) {
        if (sim->table_num < 48) {
            sim->table[sim->table_num++] = card;
            return ON;
        }
        return OFF;
    }
    if (match_num == 1) {
        sim_capture_card(sim, player, card);
        sim_capture_card(sim, player, sim->table[matches[0]]);
        remove_table_index(sim, matches[0]);
        return ON;
    }
    if (match_num == 2) {
        int pick = choose_forced_capture_index(sim, matches, match_num, taken_card_no);
        int table_index;
        if (pick < 0) {
            return OFF;
        }
        table_index = matches[pick];
        sim_capture_card(sim, player, card);
        sim_capture_card(sim, player, sim->table[table_index]);
        remove_table_index(sim, table_index);
        return ON;
    }
    sim_capture_card(sim, player, card);
    for (int i = match_num - 1; i >= 0; i--) {
        sim_capture_card(sim, player, sim->table[matches[i]]);
        remove_table_index(sim, matches[i]);
    }
    return ON;
}

static int apply_forced_drop_action(ReachSimState* sim, int player, int cluster, int card_no, int taken_card_no)
{
    int hand_index;
    Card* hand_card;

    if (!sim || player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    hand_index = find_hand_index_by_card_no(sim, player, card_no);
    if (hand_index < 0) {
        return OFF;
    }
    hand_card = remove_hand_at(sim, player, hand_index);
    if (!resolve_forced_played_card(sim, player, cluster, hand_card, taken_card_no)) {
        return OFF;
    }
    if (sim->floor_pos < sim->floor_num) {
        Card* draw_card = sim->floor[sim->floor_pos++];
        resolve_played_card(sim, player, cluster, ON, draw_card);
    }
    sim->current_player = 1 - player;
    return ON;
}

static void rollout_until_end_score(ReachSimState* sim, int target_player, int cluster, int* out_near_self_score, int* out_near_opp_score,
                                    int* out_self_score, int* out_opp_score)
{
    int guard = 48;
    int target_turns = 0;
    int near_captured = OFF;

    if (!sim) {
        return;
    }
    while ((sim->hand_num[0] > 0 || sim->hand_num[1] > 0) && guard-- > 0) {
        int p = sim->current_player;
        int hand_idx;
        Card* hand_card;

        if (p < 0 || p > 1) {
            p = 0;
            sim->current_player = 0;
        }
        if (sim->hand_num[p] <= 0) {
            sim->current_player = 1 - p;
            continue;
        }

        hand_idx = (p == target_player) ? choose_hand_index_for_cluster(sim, p, cluster) : choose_hand_index_for_opponent(sim, p);
        hand_card = remove_hand_at(sim, p, hand_idx);
        resolve_played_card(sim, p, cluster, p == target_player, hand_card);

        if (sim->floor_pos < sim->floor_num) {
            Card* draw_card = sim->floor[sim->floor_pos++];
            resolve_played_card(sim, p, cluster, p == target_player, draw_card);
        }
        if (p == target_player) {
            target_turns++;
            if (!near_captured && target_turns >= 2) {
                if (out_near_self_score) {
                    *out_near_self_score = strategy_total_score_from_counts(&sim->counts[target_player], g.koikoi[1 - target_player]);
                }
                if (out_near_opp_score) {
                    *out_near_opp_score = strategy_total_score_from_counts(&sim->counts[1 - target_player], g.koikoi[target_player]);
                }
                near_captured = ON;
            }
        }
        sim->current_player = 1 - p;
    }

    if (!near_captured) {
        if (out_near_self_score) {
            *out_near_self_score = strategy_total_score_from_counts(&sim->counts[target_player], g.koikoi[1 - target_player]);
        }
        if (out_near_opp_score) {
            *out_near_opp_score = strategy_total_score_from_counts(&sim->counts[1 - target_player], g.koikoi[target_player]);
        }
    }
    if (out_self_score) {
        *out_self_score = strategy_total_score_from_counts(&sim->counts[target_player], g.koikoi[1 - target_player]);
    }
    if (out_opp_score) {
        *out_opp_score = strategy_total_score_from_counts(&sim->counts[1 - target_player], g.koikoi[target_player]);
    }
}

static int sim_env_opponent_has_combo_blocker(const ReachSimState* sim, int player, int cat)
{
    int opp = 1 - player;

    if (!sim || player < 0 || player > 1) {
        return OFF;
    }
    for (int i = 0; i < sim->captured_num[opp]; i++) {
        Card* card = sim->captured[opp][i];
        if (!card) {
            continue;
        }
        if (cat == ENV_CAT_4 && card->type == CARD_TYPE_TAN && (card->month == 0 || card->month == 1 || card->month == 2)) {
            return ON;
        }
        if (cat == ENV_CAT_5 && card->type == CARD_TYPE_TAN && (card->month == 5 || card->month == 8 || card->month == 9)) {
            return ON;
        }
        if (cat == ENV_CAT_7 && card->type == CARD_TYPE_TANE && (card->month == 5 || card->month == 6 || card->month == 9)) {
            return ON;
        }
    }
    return OFF;
}

static int sim_env_effective_category(const ReachSimState* sim, int player, Card* card)
{
    int cat;
    int opp = 1 - player;
    int opp_light_lock_count = 0;
    int has_sake = OFF;
    int has_sakura = OFF;
    int has_moon = OFF;

    if (!sim || !card || player < 0 || player > 1) {
        return ENV_CAT_10;
    }

    cat = ai_env_category_from_card(card->id);
    for (int i = 0; i < sim->captured_num[opp]; i++) {
        Card* captured = sim->captured[opp][i];
        if (!captured) {
            continue;
        }
        if (!ai_is_no_sake_mode() && captured->type == CARD_TYPE_TANE && captured->month == 8) {
            has_sake = ON;
        }
        if (captured->type == CARD_TYPE_GOKOU) {
            if (captured->month == 2) {
                has_sakura = ON;
                opp_light_lock_count++;
            } else if (captured->month == 7) {
                has_moon = ON;
                opp_light_lock_count++;
            } else if (captured->month == 0 || captured->month == 11) {
                opp_light_lock_count++;
            }
        }
    }

    if (opp_light_lock_count >= 2 && card->type == CARD_TYPE_GOKOU &&
        (card->month == 0 || card->month == 2 || card->month == 7 || card->month == 10 || card->month == 11)) {
        return ENV_CAT_NA;
    }
    if (!ai_is_no_sake_mode() && cat == ENV_CAT_1 && has_sake) {
        return ENV_CAT_3;
    }
    if (!ai_is_no_sake_mode() && (cat == ENV_CAT_2 || cat == ENV_CAT_11)) {
        if (has_sakura && has_moon) {
            return ENV_CAT_12;
        }
        if (cat == ENV_CAT_2 && (has_sakura || has_moon)) {
            return ENV_CAT_11;
        }
    }
    if (cat == ENV_CAT_4 && sim_env_opponent_has_combo_blocker(sim, player, ENV_CAT_4)) {
        return ENV_CAT_8;
    }
    if (cat == ENV_CAT_5 && sim_env_opponent_has_combo_blocker(sim, player, ENV_CAT_5)) {
        return ENV_CAT_8;
    }
    if (cat == ENV_CAT_7 && sim_env_opponent_has_combo_blocker(sim, player, ENV_CAT_7)) {
        return ENV_CAT_9;
    }
    return cat;
}

static int sim_env_score(const ReachSimState* sim, int player)
{
    int total = 0;
    int opp = 1 - player;

    if (!sim || player < 0 || player > 1) {
        return 0;
    }
    for (int i = 0; i < sim->captured_num[player]; i++) {
        Card* card = sim->captured[player][i];
        if (!card) {
            continue;
        }
        total += STRATEGY_ENV_CAT_BASE[sim_env_effective_category(sim, player, card)] * 5;
    }
    for (int i = 0; i < sim->hand_num[player]; i++) {
        Card* card = sim->hand[player][i];
        if (!card) {
            continue;
        }
        total += STRATEGY_ENV_CAT_BASE[sim_env_effective_category(sim, player, card)] * 2;
    }
    for (int i = 0; i < sim->table_num; i++) {
        Card* card = sim->table[i];
        if (!card) {
            continue;
        }
        total += STRATEGY_ENV_CAT_BASE[sim_env_effective_category(sim, player, card)];
    }
    for (int i = 0; i < sim->captured_num[opp]; i++) {
        Card* card = sim->captured[opp][i];
        if (!card) {
            continue;
        }
        total -= STRATEGY_ENV_CAT_BASE[sim_env_effective_category(sim, player, card)] * 5;
    }
    return total;
}

static void rollout_until_env_horizon(ReachSimState* sim, int target_player, int cluster, int* out_env_self, int* out_env_opp)
{
    int guard = 48;
    int target_turns = 1;

    if (!sim) {
        return;
    }
    while ((sim->hand_num[0] > 0 || sim->hand_num[1] > 0) && guard-- > 0 && target_turns < 2) {
        int p = sim->current_player;
        int hand_idx;
        Card* hand_card;

        if (p < 0 || p > 1) {
            p = 0;
            sim->current_player = 0;
        }
        if (sim->hand_num[p] <= 0) {
            sim->current_player = 1 - p;
            continue;
        }

        hand_idx = (p == target_player) ? choose_hand_index_for_cluster(sim, p, cluster) : choose_hand_index_for_opponent(sim, p);
        hand_card = remove_hand_at(sim, p, hand_idx);
        resolve_played_card(sim, p, cluster, p == target_player, hand_card);
        if (sim->floor_pos < sim->floor_num) {
            Card* draw_card = sim->floor[sim->floor_pos++];
            resolve_played_card(sim, p, cluster, p == target_player, draw_card);
        }
        if (p == target_player) {
            target_turns++;
        }
        sim->current_player = 1 - p;
    }
    if (out_env_self) {
        *out_env_self = sim_env_score(sim, target_player);
    }
    if (out_env_opp) {
        *out_env_opp = sim_env_score(sim, 1 - target_player);
    }
}

int ai_eval_drop_env_mc(int player, const AiActionMcDropCandidate* candidates, int count, int sample_count, int* out_E_avg)
{
    Card* hidden_pool[48];
    int hidden_num;
    uint32_t rng_state;
    int total_e[8];
    int hit_count[8];

    if (!candidates || !out_E_avg || count <= 0 || count > 8 || player < 0 || player > 1) {
        return OFF;
    }
    if (sample_count <= 0) {
        sample_count = 8;
    }
    if (sample_count > 64) {
        sample_count = 64;
    }

    vgs_memset(total_e, 0, sizeof(total_e));
    vgs_memset(hit_count, 0, sizeof(hit_count));
    for (int i = 0; i < count; i++) {
        out_E_avg[i] = 0;
    }

    hidden_num = build_hidden_pool(player, hidden_pool, 48);
    if (hidden_num <= 0) {
        return OFF;
    }
    rng_state = (uint32_t)(g.frame * 2246822519u) ^ (uint32_t)(player * 3266489917u) ^ 0x6E6F77A5u;

    for (int s = 0; s < sample_count; s++) {
        ReachSimState base_sim;
        if (!sample_determinization(player, (const Card**)hidden_pool, hidden_num, &rng_state, &base_sim)) {
            continue;
        }
        for (int i = 0; i < count; i++) {
            ReachSimState sim = base_sim;
            int env0_p;
            int env0_o;
            int env2_p;
            int env2_o;

            env0_p = sim_env_score(&sim, player);
            env0_o = sim_env_score(&sim, 1 - player);
            if (!apply_forced_drop_action(&sim, player, candidates[i].cluster, candidates[i].card_no, candidates[i].taken_card_no)) {
                continue;
            }
            rollout_until_env_horizon(&sim, player, candidates[i].cluster, &env2_p, &env2_o);
            total_e[i] += (env2_p - env0_p) - (env2_o - env0_o);
            hit_count[i]++;
        }
    }

    for (int i = 0; i < count; i++) {
        if (hit_count[i] > 0) {
            out_E_avg[i] = total_e[i] / hit_count[i];
        }
    }
    return ON;
}

void ai_eval_drop_action_mc(int player, const AiActionMcDropCandidate* candidates, int count, int sample_count, int* out_score_diff)
{
    Card* hidden_pool[48];
    int hidden_num;
    uint32_t rng_state;
    int total_diff[8];
    int hit_count[8];

    if (!candidates || !out_score_diff || count <= 0 || count > 8 || player < 0 || player > 1) {
        return;
    }
    if (sample_count <= 0) {
        sample_count = strategy_reach_sample_count(g.own[player].num);
    }
    if (sample_count > 64) {
        sample_count = 64;
    }

    vgs_memset(total_diff, 0, sizeof(total_diff));
    vgs_memset(hit_count, 0, sizeof(hit_count));
    for (int i = 0; i < count; i++) {
        out_score_diff[i] = 0;
    }

    hidden_num = build_hidden_pool(player, hidden_pool, 48);
    if (hidden_num <= 0) {
        return;
    }
    rng_state = (uint32_t)(g.frame * 1103515245u) ^ (uint32_t)(player * 2654435761u) ^ 0xA57C3E2Du;

    for (int s = 0; s < sample_count; s++) {
        ReachSimState base_sim;
        if (!sample_determinization(player, (const Card**)hidden_pool, hidden_num, &rng_state, &base_sim)) {
            continue;
        }
        for (int i = 0; i < count; i++) {
            ReachSimState sim = base_sim;
            int near_self_score;
            int near_opp_score;
            int self_score;
            int opp_score;
            int mixed_diff;

            if (!apply_forced_drop_action(&sim, player, candidates[i].cluster, candidates[i].card_no, candidates[i].taken_card_no)) {
                continue;
            }
            rollout_until_end_score(&sim, player, candidates[i].cluster, &near_self_score, &near_opp_score, &self_score, &opp_score);
            mixed_diff = ((near_self_score - near_opp_score) * 7 + (self_score - opp_score) * 3) / 10;
            total_diff[i] += mixed_diff;
            hit_count[i]++;
        }
    }

    for (int i = 0; i < count; i++) {
        if (hit_count[i] > 0) {
            out_score_diff[i] = (total_diff[i] * 100) / hit_count[i];
        }
    }
}

static void update_reach_counters(int cluster, const int* reached, const int* completion_turn, int* reach_count, int* delay_sum, int* delay_hit)
{
    if (!reached || !completion_turn || !reach_count || !delay_sum || !delay_hit) {
        return;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (strategy_cluster_of_wid(wid) != cluster) {
            continue;
        }
        if (reached[wid]) {
            reach_count[wid]++;
            if (completion_turn[wid] > 0) {
                delay_sum[wid] += completion_turn[wid];
                delay_hit[wid]++;
            }
        }
    }
}

static inline int card_key(const Card* card)
{
    return (card->month << 2) | (card->index & 3);
}

int ai_is_card_critical_for_wid(int card_no, int wid)
{
    if (card_no < 0 || card_no >= 48 || wid < 0 || wid >= WINNING_HAND_MAX) {
        return OFF;
    }
    if (ai_is_disabled_wid_by_rules(wid)) {
        return OFF;
    }

    init_strategy_requirements();
    if (is_threshold_role_wid(wid)) {
        return OFF;
    }

    const StrategyRequirement* req = &strategy_requirements[wid];
    for (int i = 0; i < req->count; i++) {
        if (req->cards[i].key == card_no) {
            return ON;
        }
    }
    return OFF;
}

int ai_is_card_related_for_wid(int card_no, int wid)
{
    if (card_no < 0 || card_no >= 48 || wid < 0 || wid >= WINNING_HAND_MAX) {
        return OFF;
    }
    if (ai_is_disabled_wid_by_rules(wid)) {
        return OFF;
    }
    if (ai_is_card_critical_for_wid(card_no, wid)) {
        return ON;
    }

    Card* card = &g.cards[card_no];
    switch (wid) {
        case WID_GOKOU:
        case WID_SHIKOU:
        case WID_AME_SHIKOU:
        case WID_SANKOU:
            return card->type == CARD_TYPE_GOKOU ? ON : OFF;
        case WID_HANAMI:
        case WID_TSUKIMI:
            return (card->type == CARD_TYPE_GOKOU || card->type == CARD_TYPE_TANE) ? ON : OFF;
        case WID_ISC:
        case WID_TANE:
            return card->type == CARD_TYPE_TANE ? ON : OFF;
        case WID_DBTAN:
        case WID_AKATAN:
        case WID_AOTAN:
        case WID_TAN:
            return card->type == CARD_TYPE_TAN ? ON : OFF;
        case WID_KASU:
            return (card->type == CARD_TYPE_KASU || (card->type == CARD_TYPE_TANE && card->month == 8)) ? ON : OFF;
        default:
            return OFF;
    }
}

/*
 * 固定役の completion 判定は strategy_requirements[] を唯一の要件テーブルとして使う。
 * これにより Akatan/Aotan 専用分岐を増やさず、将来の固定高打点役も同じ評価経路へ乗せる。
 */
int ai_is_fixed_wid(int wid)
{
    if (ai_is_disabled_wid_by_rules(wid)) {
        return OFF;
    }
    switch (wid) {
        case WID_GOKOU:
        case WID_SHIKOU:
        case WID_AME_SHIKOU:
        case WID_SANKOU:
        case WID_HANAMI:
        case WID_TSUKIMI:
        case WID_ISC:
        case WID_DBTAN:
        case WID_AKATAN:
        case WID_AOTAN:
            return ON;
        default:
            return OFF;
    }
}

int ai_is_high_value_wid(int wid)
{
    if (wid < 0 || wid >= WINNING_HAND_MAX) {
        return OFF;
    }
    return winning_hands[wid].base_score >= 5 ? ON : OFF;
}

int ai_is_overpay_target_wid(int wid)
{
    switch (wid) {
        case WID_AKATAN:
        case WID_AOTAN:
        case WID_ISC:
            return ON;
        default:
            return OFF;
    }
}

static void build_owned_requirement_map(int player, int* owned_by_key)
{
    if (!owned_by_key || player < 0 || player > 1) {
        return;
    }
    vgs_memset(owned_by_key, 0, sizeof(int) * 48);
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* card = inv->cards[i];
            if (!card) {
                continue;
            }
            int key = card_key(card);
            if (0 <= key && key < 48) {
                owned_by_key[key] = ON;
            }
        }
    }
}

static int calc_fixed_wid_missing_from_owned_map(int wid, const int* owned_by_key)
{
    if (!owned_by_key || wid < 0 || wid >= WINNING_HAND_MAX || !ai_is_fixed_wid(wid)) {
        return 99;
    }

    init_strategy_requirements();
    const StrategyRequirement* req = &strategy_requirements[wid];
    if (req->count <= 0 || req->minimum <= 0) {
        return 99;
    }

    int owned = 0;
    int must_missing = 0;
    for (int i = 0; i < req->count; i++) {
        int key = req->cards[i].key;
        if (0 <= key && key < 48 && owned_by_key[key]) {
            owned++;
        } else if (req->cards[i].must) {
            must_missing++;
        }
    }

    int missing = must_missing;
    int remain = req->minimum - owned - must_missing;
    if (remain > 0) {
        missing += remain;
    }
    return missing < 0 ? 0 : missing;
}

int ai_wid_missing_count(int player, int wid)
{
    int owned_by_key[48];

    if (player < 0 || player > 1 || ai_is_disabled_wid_by_rules(wid)) {
        return 99;
    }
    build_owned_requirement_map(player, owned_by_key);
    return calc_fixed_wid_missing_from_owned_map(wid, owned_by_key);
}

int ai_would_complete_wid_by_taking_card(int player, int wid, int taken_card_no)
{
    int owned_by_key[48];

    if (player < 0 || player > 1 || taken_card_no < 0 || taken_card_no >= 48 || !ai_is_fixed_wid(wid)) {
        return OFF;
    }

    build_owned_requirement_map(player, owned_by_key);
    int before_missing = calc_fixed_wid_missing_from_owned_map(wid, owned_by_key);
    if (before_missing <= 0) {
        return OFF;
    }

    owned_by_key[taken_card_no] = ON;
    return calc_fixed_wid_missing_from_owned_map(wid, owned_by_key) == 0 ? ON : OFF;
}

int ai_overpay_possible_now(int player, int wid)
{
    StrategyCounts counts;

    if (player < 0 || player > 1 || !ai_is_overpay_target_wid(wid) || ai_wid_missing_count(player, wid) != 1) {
        return OFF;
    }

    build_counts_for_player_fixed(player, &counts);
    return can_overpay_by_counts(&counts, wid);
}

int ai_count_unrevealed_same_month(int player, int month)
{
    int known[48];

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return 0;
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
        Card* card = g.deck.cards[i];
        if (card) {
            known[card->id] = ON;
        }
    }

    int count = 0;
    for (int i = 0; i < 48; i++) {
        if (!known[i] && g.cards[i].month == month) {
            if (ai_is_no_sake_mode() && (i == 32 || i == 35)) {
                continue;
            }
            count++;
        }
    }
    return count;
}

int ai_is_keycard_for_role(int card_no, int role)
{
    return ai_is_card_critical_for_wid(card_no, role);
}

int ai_role_progress_level(int player, int role)
{
    int fixed_missing;
    int owned_gokou = g.invent[player][CARD_TYPE_GOKOU].num;
    int owned_non_rain_gokou = 0;

    if (player < 0 || player > 1 || !ai_is_fixed_wid(role) || !ai_is_high_value_wid(role)) {
        return 0;
    }

    for (int i = 0; i < g.invent[player][CARD_TYPE_GOKOU].num; i++) {
        Card* card = g.invent[player][CARD_TYPE_GOKOU].cards[i];
        if (card && card->month != 10) {
            owned_non_rain_gokou++;
        }
    }

    fixed_missing = ai_wid_missing_count(player, role);
    if (fixed_missing != 1) {
        return 0;
    }

    switch (role) {
        case WID_AKATAN:
        case WID_AOTAN:
        case WID_ISC:
        case WID_HANAMI:
        case WID_TSUKIMI:
            return 2;
        case WID_SANKOU:
            return owned_non_rain_gokou >= 2 ? 2 : 0;
        case WID_AME_SHIKOU:
            return owned_gokou >= 3 ? 2 : 0;
        case WID_SHIKOU:
            return owned_non_rain_gokou >= 3 ? 2 : 0;
        case WID_GOKOU:
            return owned_gokou >= 4 ? 2 : 0;
        default:
            break;
    }
    return 0;
}

int ai_has_obvious_alt_weak_card_in_hand(int player, int except_card_no)
{
    Card* except_card;

    if (player < 0 || player > 1 || except_card_no < 0 || except_card_no >= 48) {
        return OFF;
    }

    except_card = &g.cards[except_card_no];
    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        int key_for_high_role = OFF;
        if (!card || card->id == except_card_no || card->month != except_card->month) {
            continue;
        }
        for (int role = 0; role < WINNING_HAND_MAX; role++) {
            if (!ai_is_high_value_wid(role)) {
                continue;
            }
            if (ai_is_keycard_for_role(card->id, role) && ai_role_progress_level(player, role) > 0) {
                key_for_high_role = ON;
                break;
            }
        }
        if (key_for_high_role) {
            continue;
        }
        if (card->type < except_card->type) {
            continue;
        }
        return ON;
    }
    return OFF;
}

int ai_keycard_drop_gives_opp_advantage(int player, int keycard_no)
{
    int known[48];
    Card* keycard;
    int owned_same_month = 0;
    int hidden_card_no = -1;

    if (player < 0 || player > 1 || keycard_no < 0 || keycard_no >= 48) {
        return OFF;
    }

    keycard = &g.cards[keycard_no];
    if (ai_count_unrevealed_same_month(player, keycard->month) != 1) {
        return OFF;
    }

    vgs_memset(known, 0, sizeof(known));
    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card) {
            known[card->id] = ON;
            if (card->month == keycard->month) {
                owned_same_month++;
            }
        }
    }
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* card = inv->cards[i];
            if (card) {
                known[card->id] = ON;
                if (card->month == keycard->month) {
                    owned_same_month++;
                }
            }
        }
    }
    if (owned_same_month < 2) {
        return OFF;
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
        Card* card = g.deck.cards[i];
        if (card) {
            known[card->id] = ON;
        }
    }
    for (int i = 0; i < 48; i++) {
        if (!known[i] && g.cards[i].month == keycard->month) {
            hidden_card_no = i;
            break;
        }
    }
    if (hidden_card_no < 0) {
        return OFF;
    }
    for (int role = 0; role < WINNING_HAND_MAX; role++) {
        if (!ai_is_high_value_wid(role) || ai_role_progress_level(player, role) <= 0) {
            continue;
        }
        if (ai_is_keycard_for_role(keycard_no, role) && ai_is_keycard_for_role(hidden_card_no, role)) {
            return ON;
        }
    }
    return OFF;
}

static void add_req_card(StrategyRequirement* req, int month, int index, int must)
{
    if (!req || req->count >= 25) {
        return;
    }
    req->cards[req->count].key = (month << 2) | (index & 3);
    req->cards[req->count].must = must ? ON : OFF;
    req->count++;
}

static void init_strategy_requirements(void)
{
    if (strategy_requirements[WID_GOKOU].count) {
        return;
    }
    vgs_memset(strategy_requirements, 0, sizeof(strategy_requirements));

    strategy_requirements[WID_GOKOU].minimum = 5;
    add_req_card(&strategy_requirements[WID_GOKOU], 0, 3, OFF);
    add_req_card(&strategy_requirements[WID_GOKOU], 2, 3, OFF);
    add_req_card(&strategy_requirements[WID_GOKOU], 7, 3, OFF);
    add_req_card(&strategy_requirements[WID_GOKOU], 10, 3, OFF);
    add_req_card(&strategy_requirements[WID_GOKOU], 11, 3, OFF);

    strategy_requirements[WID_SHIKOU].minimum = 4;
    add_req_card(&strategy_requirements[WID_SHIKOU], 0, 3, OFF);
    add_req_card(&strategy_requirements[WID_SHIKOU], 2, 3, OFF);
    add_req_card(&strategy_requirements[WID_SHIKOU], 7, 3, OFF);
    add_req_card(&strategy_requirements[WID_SHIKOU], 11, 3, OFF);

    strategy_requirements[WID_AME_SHIKOU].minimum = 4;
    add_req_card(&strategy_requirements[WID_AME_SHIKOU], 10, 3, ON);
    add_req_card(&strategy_requirements[WID_AME_SHIKOU], 0, 3, OFF);
    add_req_card(&strategy_requirements[WID_AME_SHIKOU], 2, 3, OFF);
    add_req_card(&strategy_requirements[WID_AME_SHIKOU], 7, 3, OFF);
    add_req_card(&strategy_requirements[WID_AME_SHIKOU], 11, 3, OFF);

    strategy_requirements[WID_SANKOU].minimum = 3;
    add_req_card(&strategy_requirements[WID_SANKOU], 0, 3, OFF);
    add_req_card(&strategy_requirements[WID_SANKOU], 2, 3, OFF);
    add_req_card(&strategy_requirements[WID_SANKOU], 7, 3, OFF);
    add_req_card(&strategy_requirements[WID_SANKOU], 11, 3, OFF);

    strategy_requirements[WID_HANAMI].minimum = 2;
    add_req_card(&strategy_requirements[WID_HANAMI], 2, 3, OFF);
    add_req_card(&strategy_requirements[WID_HANAMI], 8, 3, OFF);

    strategy_requirements[WID_TSUKIMI].minimum = 2;
    add_req_card(&strategy_requirements[WID_TSUKIMI], 7, 3, OFF);
    add_req_card(&strategy_requirements[WID_TSUKIMI], 8, 3, OFF);

    strategy_requirements[WID_ISC].minimum = 3;
    add_req_card(&strategy_requirements[WID_ISC], 5, 3, OFF);
    add_req_card(&strategy_requirements[WID_ISC], 6, 3, OFF);
    add_req_card(&strategy_requirements[WID_ISC], 9, 3, OFF);

    strategy_requirements[WID_DBTAN].minimum = 6;
    add_req_card(&strategy_requirements[WID_DBTAN], 0, 2, OFF);
    add_req_card(&strategy_requirements[WID_DBTAN], 1, 2, OFF);
    add_req_card(&strategy_requirements[WID_DBTAN], 2, 2, OFF);
    add_req_card(&strategy_requirements[WID_DBTAN], 5, 2, OFF);
    add_req_card(&strategy_requirements[WID_DBTAN], 8, 2, OFF);
    add_req_card(&strategy_requirements[WID_DBTAN], 9, 2, OFF);

    strategy_requirements[WID_AKATAN].minimum = 3;
    add_req_card(&strategy_requirements[WID_AKATAN], 0, 2, OFF);
    add_req_card(&strategy_requirements[WID_AKATAN], 1, 2, OFF);
    add_req_card(&strategy_requirements[WID_AKATAN], 2, 2, OFF);

    strategy_requirements[WID_AOTAN].minimum = 3;
    add_req_card(&strategy_requirements[WID_AOTAN], 5, 2, OFF);
    add_req_card(&strategy_requirements[WID_AOTAN], 8, 2, OFF);
    add_req_card(&strategy_requirements[WID_AOTAN], 9, 2, OFF);

    strategy_requirements[WID_TANE].minimum = 5;
    add_req_card(&strategy_requirements[WID_TANE], 1, 3, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 3, 3, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 4, 3, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 5, 3, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 6, 3, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 7, 2, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 8, 3, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 9, 3, OFF);
    add_req_card(&strategy_requirements[WID_TANE], 10, 2, OFF);

    strategy_requirements[WID_TAN].minimum = 5;
    add_req_card(&strategy_requirements[WID_TAN], 0, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 1, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 2, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 3, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 4, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 5, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 6, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 8, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 9, 2, OFF);
    add_req_card(&strategy_requirements[WID_TAN], 10, 1, OFF);

    strategy_requirements[WID_KASU].minimum = 10;
    add_req_card(&strategy_requirements[WID_KASU], 0, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 0, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 1, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 1, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 2, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 2, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 3, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 3, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 4, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 4, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 5, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 5, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 6, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 6, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 7, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 7, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 8, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 8, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 8, 3, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 9, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 9, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 10, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 11, 0, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 11, 1, OFF);
    add_req_card(&strategy_requirements[WID_KASU], 11, 2, OFF);
}

static void add_card_to_counts(StrategyCounts* counts, const Card* card)
{
    if (!counts || !card) {
        return;
    }
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
                counts->hanami++;
                counts->tsukimi++;
                counts->kasu++;
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

static int is_additive_wid(int wid)
{
    return wid == WID_ISC || wid == WID_DBTAN || wid == WID_AKATAN || wid == WID_AOTAN || wid == WID_TANE || wid == WID_TAN ||
           wid == WID_KASU;
}

static int can_complete_by_counts(int wid, const StrategyCounts* counts)
{
    if (!counts) {
        return OFF;
    }
    if (ai_is_disabled_wid_by_rules(wid)) {
        return OFF;
    }
    switch (wid) {
        case WID_GOKOU:
            return counts->gokou >= 5 ? ON : OFF;
        case WID_SHIKOU:
            return counts->shikou >= 4 ? ON : OFF;
        case WID_AME_SHIKOU:
            return (counts->gokou >= 4 && counts->gokou > counts->shikou) ? ON : OFF;
        case WID_SANKOU:
            return counts->shikou >= 3 ? ON : OFF;
        case WID_HANAMI:
            return counts->hanami >= 2 ? ON : OFF;
        case WID_TSUKIMI:
            return counts->tsukimi >= 2 ? ON : OFF;
        case WID_ISC:
            return counts->isc >= 3 ? ON : OFF;
        case WID_DBTAN:
            return (counts->akatan >= 3 && counts->aotan >= 3) ? ON : OFF;
        case WID_AKATAN:
            return counts->akatan >= 3 ? ON : OFF;
        case WID_AOTAN:
            return counts->aotan >= 3 ? ON : OFF;
        case WID_TANE:
            return counts->tane >= 5 ? ON : OFF;
        case WID_TAN:
            return counts->tan >= 5 ? ON : OFF;
        case WID_KASU:
            return counts->kasu >= 10 ? ON : OFF;
        default:
            break;
    }
    return OFF;
}

static int calc_role_score(int wid, const StrategyCounts* counts)
{
    if (!counts || ai_is_disabled_wid_by_rules(wid) || !can_complete_by_counts(wid, counts)) {
        return 0;
    }
    switch (wid) {
        case WID_ISC:
            return 5 + (counts->tane - 3);
        case WID_DBTAN:
            return 10 + (counts->tan - 6);
        case WID_AKATAN:
        case WID_AOTAN:
            return 5 + (counts->tan - 3);
        case WID_TANE:
            return 1 + (counts->tane - 5);
        case WID_TAN:
            return 1 + (counts->tan - 5);
        case WID_KASU:
            return 1 + (counts->kasu - 10);
        default:
            return winning_hands[wid].base_score;
    }
}

static int can_overpay_by_counts(const StrategyCounts* counts, int wid)
{
    if (!counts) {
        return OFF;
    }

    switch (wid) {
        case WID_AKATAN:
        case WID_AOTAN:
            return counts->tan >= 3 ? ON : OFF;
        case WID_ISC:
            return counts->tane >= 3 ? ON : OFF;
        default:
            return OFF;
    }
}

static int calc_first_completion_score(int wid, int completed_own)
{
    if (is_additive_wid(wid) && completed_own) {
        return 1;
    }
    return winning_hands[wid].base_score;
}

static int evaluate_requirement(const StrategyRequirement* req, const int* loc_by_key, int allow_hand, int allow_floor, int allow_est,
                                int* hand_used, int* floor_used, int* est_used)
{
    if (!req || !loc_by_key) {
        return OFF;
    }
    int owned = 0;
    int must_hand = 0;
    int must_floor = 0;
    int must_est = 0;
    int opt_hand = 0;
    int opt_floor = 0;
    int opt_est = 0;
    for (int i = 0; i < req->count; i++) {
        int key = req->cards[i].key;
        int loc = (0 <= key && key < 48) ? loc_by_key[key] : SLOC_NONE;
        if (loc & SLOC_OWN) {
            owned++;
            continue;
        }
        if (loc & SLOC_OPP) {
            if (req->cards[i].must) {
                return OFF;
            }
            continue;
        }
        if (loc & SLOC_HAND) {
            if (req->cards[i].must) {
                must_hand++;
            } else {
                opt_hand++;
            }
            continue;
        }
        if (loc & SLOC_FLOOR) {
            if (req->cards[i].must) {
                must_floor++;
            } else {
                opt_floor++;
            }
            continue;
        }
        if (loc & SLOC_EST) {
            if (req->cards[i].must) {
                must_est++;
            } else {
                opt_est++;
            }
            continue;
        }
        if (req->cards[i].must) {
            return OFF;
        }
    }

    if ((!allow_hand && must_hand) || (!allow_floor && must_floor) || (!allow_est && must_est)) {
        return OFF;
    }

    int available = owned;
    if (allow_hand) {
        available += must_hand + opt_hand;
    }
    if (allow_floor) {
        available += must_floor + opt_floor;
    }
    if (allow_est) {
        available += must_est + opt_est;
    }
    if (available < req->minimum) {
        return OFF;
    }

    int hu = allow_hand ? must_hand : 0;
    int fu = allow_floor ? must_floor : 0;
    int eu = allow_est ? must_est : 0;
    int need = req->minimum - owned - hu - fu - eu;
    if (need < 0) {
        need = 0;
    }
    if (allow_hand && need > 0) {
        int take = opt_hand < need ? opt_hand : need;
        hu += take;
        need -= take;
    }
    if (allow_floor && need > 0) {
        int take = opt_floor < need ? opt_floor : need;
        fu += take;
        need -= take;
    }
    if (allow_est && need > 0) {
        int take = opt_est < need ? opt_est : need;
        eu += take;
        need -= take;
    }
    if (need > 0) {
        return OFF;
    }
    if (hand_used) {
        *hand_used = hu;
    }
    if (floor_used) {
        *floor_used = fu;
    }
    if (est_used) {
        *est_used = eu;
    }
    return ON;
}

static void add_zone_to_counts(StrategyCounts* counts, CardSet* set)
{
    if (!counts || !set) {
        return;
    }
    for (int i = 0; i < 48; i++) {
        add_card_to_counts(counts, set->cards[i]);
    }
}

static void add_hand_to_counts(StrategyCounts* counts, CardSet* hand)
{
    if (!counts || !hand) {
        return;
    }
    for (int i = 0; i < 8; i++) {
        add_card_to_counts(counts, hand->cards[i]);
    }
}

static void add_floor_range_to_counts(StrategyCounts* counts, CardSet* floor)
{
    if (!counts || !floor) {
        return;
    }
    if (floor->num <= 0) {
        return;
    }
    for (int i = 0; i < floor->num; i++) {
        int idx = floor->start + i;
        if (idx >= 48) {
            idx -= 48;
        }
        add_card_to_counts(counts, floor->cards[idx]);
    }
}

static void fill_location_by_set(int* loc_by_key, CardSet* set, int loc)
{
    if (!loc_by_key || !set) {
        return;
    }
    for (int i = 0; i < 48; i++) {
        Card* card = set->cards[i];
        if (!card) {
            continue;
        }
        int key = card_key(card);
        if (0 <= key && key < 48) {
            loc_by_key[key] = loc;
        }
    }
}

static void fill_location_by_set_if_none(int* loc_by_key, CardSet* set, int loc)
{
    if (!loc_by_key || !set) {
        return;
    }
    for (int i = 0; i < 48; i++) {
        Card* card = set->cards[i];
        if (!card) {
            continue;
        }
        int key = card_key(card);
        if (0 <= key && key < 48 && loc_by_key[key] == SLOC_NONE) {
            loc_by_key[key] = loc;
        }
    }
}

static void fill_location_by_hand(int* loc_by_key, CardSet* hand, int loc)
{
    if (!loc_by_key || !hand) {
        return;
    }
    for (int i = 0; i < 8; i++) {
        Card* card = hand->cards[i];
        if (!card) {
            continue;
        }
        int key = card_key(card);
        if (0 <= key && key < 48) {
            loc_by_key[key] = loc;
        }
    }
}

static void fill_location_by_hand_if_none(int* loc_by_key, CardSet* hand, int loc)
{
    if (!loc_by_key || !hand) {
        return;
    }
    for (int i = 0; i < 8; i++) {
        Card* card = hand->cards[i];
        if (!card) {
            continue;
        }
        int key = card_key(card);
        if (0 <= key && key < 48 && loc_by_key[key] == SLOC_NONE) {
            loc_by_key[key] = loc;
        }
    }
}

static void fill_location_by_floor_range_if_none(int* loc_by_key, CardSet* floor, int loc)
{
    if (!loc_by_key || !floor || floor->num <= 0) {
        return;
    }
    for (int i = 0; i < floor->num; i++) {
        int idx = floor->start + i;
        if (idx >= 48) {
            idx -= 48;
        }
        Card* card = floor->cards[idx];
        if (!card) {
            continue;
        }
        int key = card_key(card);
        if (0 <= key && key < 48 && loc_by_key[key] == SLOC_NONE) {
            loc_by_key[key] = loc;
        }
    }
}

static int is_threshold_role_wid(int wid)
{
    return wid == WID_TANE || wid == WID_TAN || wid == WID_KASU;
}

static int risk_unknown_penalty(int wid)
{
    if (is_threshold_role_wid(wid)) {
        return 25;
    }
    if (wid == WID_GOKOU || wid == WID_SHIKOU || wid == WID_AME_SHIKOU || wid == WID_SANKOU) {
        return 15;
    }
    return 10;
}

/*
 * risk 用の static 推定ロケーション:
 * - SLOC_OWN: 相手の取り札（確定）
 * - SLOC_FLOOR: 場札（確定）
 * - SLOC_OPP: 自分の手札/取り札（相手に渡らない確定）
 * - SLOC_EST: 未確定（相手手札/山札）
 */
static void build_opp_risk_locations(int player, int* loc_by_key)
{
    if (!loc_by_key || player < 0 || player > 1) {
        return;
    }
    int opp = 1 - player;
    for (int i = 0; i < 48; i++) {
        loc_by_key[i] = SLOC_NONE;
    }
    for (int t = 0; t < 4; t++) {
        fill_location_by_set(loc_by_key, &g.invent[player][t], SLOC_OPP);
    }
    fill_location_by_hand(loc_by_key, &g.own[player], SLOC_OPP);
    for (int t = 0; t < 4; t++) {
        fill_location_by_set(loc_by_key, &g.invent[opp][t], SLOC_OWN);
    }
    fill_location_by_set_if_none(loc_by_key, &g.deck, SLOC_FLOOR);
    for (int i = 0; i < 48; i++) {
        int key = card_key(&g.cards[i]);
        if (0 <= key && key < 48 && loc_by_key[key] == SLOC_NONE) {
            loc_by_key[key] = SLOC_EST;
        }
    }
}

static void build_counts_for_player_fixed(int player, StrategyCounts* counts)
{
    if (!counts || player < 0 || player > 1) {
        return;
    }
    vgs_memset(counts, 0, sizeof(*counts));
    for (int t = 0; t < 4; t++) {
        add_zone_to_counts(counts, &g.invent[player][t]);
    }
}

static void build_counts_for_player_plus_floor(int player, StrategyCounts* counts)
{
    if (!counts || player < 0 || player > 1) {
        return;
    }
    build_counts_for_player_fixed(player, counts);
    add_floor_range_to_counts(counts, &g.deck);
}

static int can_be_blocked_by_my_known_cards(int wid, int player)
{
    if (wid < 0 || wid >= WINNING_HAND_MAX || player < 0 || player > 1 || is_threshold_role_wid(wid)) {
        return OFF;
    }
    const StrategyRequirement* req = &strategy_requirements[wid];
    int loc_by_key[48];
    build_opp_risk_locations(player, loc_by_key);

    int available = 0;
    for (int i = 0; i < req->count; i++) {
        int key = req->cards[i].key;
        int loc = (0 <= key && key < 48) ? loc_by_key[key] : SLOC_NONE;
        if (loc & SLOC_OPP) {
            if (req->cards[i].must) {
                return ON;
            }
            continue;
        }
        available++;
    }
    return available < req->minimum ? ON : OFF;
}

static int calc_fixed_role_near_completion_threat(int wid, int player, const StrategyCounts* counts_opp_fixed, int* out_floor_missing,
                                                  int* out_est_missing)
{
    if (out_floor_missing) {
        *out_floor_missing = 0;
    }
    if (out_est_missing) {
        *out_est_missing = 0;
    }
    if (!counts_opp_fixed || wid < 0 || wid >= WINNING_HAND_MAX || player < 0 || player > 1 || is_threshold_role_wid(wid)) {
        return OFF;
    }
    if (can_complete_by_counts(wid, counts_opp_fixed)) {
        return OFF;
    }
    if (can_be_blocked_by_my_known_cards(wid, player)) {
        return OFF;
    }

    int loc_by_key[48];
    build_opp_risk_locations(player, loc_by_key);

    const StrategyRequirement* req = &strategy_requirements[wid];
    int opp = 1 - player;
    int opp_current_score = g.stats[opp].score;
    if (!ai_is_high_value_wid(wid) || req->minimum != 3) {
        return OFF;
    }
    if (opp_current_score + winning_hands[wid].base_score < 7 && g.koikoi[opp] == OFF) {
        return OFF;
    }

    int floor_missing = 0;
    int est_missing = 0;
    int unresolved = 0;
    int owned = 0;

    for (int i = 0; i < req->count; i++) {
        int key = req->cards[i].key;
        int loc = (0 <= key && key < 48) ? loc_by_key[key] : SLOC_NONE;
        if (loc & SLOC_OWN) {
            owned++;
            continue;
        }
        if (loc & SLOC_FLOOR) {
            floor_missing++;
            unresolved++;
            continue;
        }
        if (loc & SLOC_EST) {
            est_missing++;
            unresolved++;
            continue;
        }
        if (loc & SLOC_OPP) {
            if (req->cards[i].must) {
                return OFF;
            }
            continue;
        }
        if (req->cards[i].must) {
            return OFF;
        }
    }

    if (owned != req->minimum - 1) {
        return OFF;
    }
    if (owned + unresolved < req->minimum) {
        return OFF;
    }
    if (unresolved != 1) {
        return OFF;
    }

    if (out_floor_missing) {
        *out_floor_missing = floor_missing;
    }
    if (out_est_missing) {
        *out_est_missing = est_missing;
    }
    return ON;
}

static int calc_risk_delay(int wid, int player, const StrategyCounts* counts_opp_fixed)
{
    if (!counts_opp_fixed || wid < 0 || wid >= WINNING_HAND_MAX || player < 0 || player > 1) {
        return 99;
    }
    if (can_complete_by_counts(wid, counts_opp_fixed)) {
        return 0;
    }

    int loc_by_key[48];
    build_opp_risk_locations(player, loc_by_key);

    if (!is_threshold_role_wid(wid)) {
        if (can_be_blocked_by_my_known_cards(wid, player)) {
            return 99;
        }
        int floor_used = 0;
        int est_used = 0;
        if (!evaluate_requirement(&strategy_requirements[wid], loc_by_key, OFF, ON, ON, NULL, &floor_used, &est_used)) {
            return 99;
        }
        int delay = floor_used + est_used * 2;
        int near_floor_missing = 0;
        int near_est_missing = 0;
        if (calc_fixed_role_near_completion_threat(wid, player, counts_opp_fixed, &near_floor_missing, &near_est_missing) &&
            (near_floor_missing > 0 || near_est_missing > 0)) {
            delay -= STRATEGY_RISK_FIXED_NEAR_COMPLETE_DELAY_REDUCTION;
        }
        if (delay < 1) {
            delay = 1;
        }
        if (delay > 99) {
            delay = 99;
        }
        return delay;
    }

    int current = 0;
    int threshold = 0;
    switch (wid) {
        case WID_TANE:
            current = counts_opp_fixed->tane;
            threshold = 5;
            break;
        case WID_TAN:
            current = counts_opp_fixed->tan;
            threshold = 5;
            break;
        case WID_KASU:
            current = counts_opp_fixed->kasu;
            threshold = 10;
            break;
        default:
            return 99;
    }

    int need = threshold - current;
    if (need <= 0) {
        return 0;
    }
    int floor_targets = 0;
    int unknown_targets = 0;
    for (int i = 0; i < 48; i++) {
        Card* card = &g.cards[i];
        if (!is_bonus_target_card(wid, card)) {
            continue;
        }
        int key = card_key(card);
        int loc = (0 <= key && key < 48) ? loc_by_key[key] : SLOC_NONE;
        if (loc & SLOC_FLOOR) {
            floor_targets++;
        } else if (loc & SLOC_EST) {
            unknown_targets++;
        }
    }

    if (current + floor_targets + unknown_targets < threshold) {
        return 99;
    }
    int floor_used = floor_targets < need ? floor_targets : need;
    int est_used = need - floor_used;
    int delay = floor_used + est_used * 2;
    if (delay < 1) {
        delay = 1;
    }
    if (delay > 99) {
        delay = 99;
    }
    return delay;
}

static int calc_risk_reach_estimate(int wid, int player, const StrategyCounts* counts_opp_fixed, const StrategyCounts* counts_opp_plus_floor, int risk_delay)
{
    if (!counts_opp_fixed || !counts_opp_plus_floor || wid < 0 || wid >= WINNING_HAND_MAX || player < 0 || player > 1) {
        return 0;
    }
    if (can_complete_by_counts(wid, counts_opp_fixed)) {
        return 0; // 新規成立のみ対象
    }
    if (risk_delay >= 99) {
        return 0;
    }
    if (!is_threshold_role_wid(wid) && can_be_blocked_by_my_known_cards(wid, player)) {
        return 0;
    }
    int progress_fixed = strategy_role_progress(wid, counts_opp_fixed);
    int progress_floor = strategy_role_progress(wid, counts_opp_plus_floor);
    int risk = (progress_fixed * 70 + progress_floor * 30) / 100;
    risk -= risk_unknown_penalty(wid);
    int near_floor_missing = 0;
    int near_est_missing = 0;
    if (calc_fixed_role_near_completion_threat(wid, player, counts_opp_fixed, &near_floor_missing, &near_est_missing)) {
        risk += near_floor_missing * STRATEGY_RISK_FIXED_NEAR_COMPLETE_FLOOR_BONUS;
        risk += near_est_missing * STRATEGY_RISK_FIXED_NEAR_COMPLETE_EST_BONUS;
    }
    if (risk_delay <= 2) {
        risk += 5;
    }
    if (risk < 0) {
        risk = 0;
    } else if (risk > 100) {
        risk = 100;
    }
    return risk;
}

static int calc_risk_score(int wid, int base_score, int risk_reach_estimate)
{
    (void)wid;
    if (base_score <= 0 || risk_reach_estimate <= 0) {
        return 0;
    }
    int score = (base_score * risk_reach_estimate + 50) / 100;
    return score < 0 ? 0 : score;
}

static int estimate_risk_7plus_distance(const StrategyData* data, int opp_current_score)
{
    if (!data) {
        return 99;
    }
    if (opp_current_score >= 7) {
        return 0;
    }

    int best = 99;
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (ai_is_disabled_wid_by_rules(wid) || data->risk_reach_estimate[wid] <= 0 || data->risk_delay[wid] >= 99) {
            continue;
        }
        if (opp_current_score + winning_hands[wid].base_score < 7) {
            continue;
        }
        if (data->risk_delay[wid] < best) {
            best = data->risk_delay[wid];
        }
    }
    return best;
}

static int estimate_opp_coarse_threat(const StrategyData* data, int opp_current_score)
{
    StrategyOppBestMetrics opp_metrics;
    int threat = 0;

    if (!data) {
        return 0;
    }
    if (opp_current_score >= 7) {
        return 100;
    }

    strategy_opp_best_metrics(data, &opp_metrics);
    threat += opp_metrics.opp_best_risk_ev * STRATEGY_RISK_THREAT_EV_WEIGHT;
    if (opp_metrics.opp_best_risk_delay <= 2) {
        threat += STRATEGY_RISK_THREAT_DELAY_NEAR2_BONUS;
    } else if (opp_metrics.opp_best_risk_delay <= 3) {
        threat += STRATEGY_RISK_THREAT_DELAY_NEAR3_BONUS;
    } else if (opp_metrics.opp_best_risk_delay <= 4) {
        threat += STRATEGY_RISK_THREAT_DELAY_NEAR4_BONUS;
    }
    if (data->risk_7plus_distance <= 1) {
        threat += 25;
    } else if (data->risk_7plus_distance <= 2) {
        threat += 15;
    } else if (data->risk_7plus_distance <= 4) {
        threat += 5;
    }
    if (data->koikoi_opp == ON) {
        threat += STRATEGY_RISK_THREAT_KOIKOI_BONUS;
    }
    return clamp_int(threat, 0, 100);
}

static int estimate_opp_exact_7plus_threat(const StrategyData* data, int opp_current_score)
{
    int threat = 0;
    int best_risk_ev = 0;
    int best_risk_delay = STRATEGY_DELAY_INVALID;

    if (!data) {
        return 0;
    }
    if (opp_current_score >= 7) {
        return 100;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (ai_is_disabled_wid_by_rules(wid) || opp_current_score + winning_hands[wid].base_score < 7) {
            continue;
        }
        if (data->risk_score[wid] > best_risk_ev) {
            best_risk_ev = data->risk_score[wid];
        }
        if ((data->risk_reach_estimate[wid] > 0 || data->risk_score[wid] > 0) && data->risk_delay[wid] >= 0 && data->risk_delay[wid] < best_risk_delay) {
            best_risk_delay = data->risk_delay[wid];
        }
    }

    if (best_risk_ev <= 0 && best_risk_delay >= STRATEGY_DELAY_INVALID && data->risk_7plus_distance >= 99) {
        return 0;
    }

    threat += best_risk_ev * STRATEGY_RISK_THREAT_EV_WEIGHT;
    if (best_risk_delay <= 2) {
        threat += STRATEGY_RISK_THREAT_DELAY_NEAR2_BONUS;
    } else if (best_risk_delay <= 3) {
        threat += STRATEGY_RISK_THREAT_DELAY_NEAR3_BONUS;
    } else if (best_risk_delay <= 4) {
        threat += STRATEGY_RISK_THREAT_DELAY_NEAR4_BONUS;
    }
    if (data->risk_7plus_distance <= 1) {
        threat += 25;
    } else if (data->risk_7plus_distance <= 2) {
        threat += 15;
    } else if (data->risk_7plus_distance <= 4) {
        threat += 5;
    }
    if (data->koikoi_opp == ON) {
        threat += STRATEGY_RISK_THREAT_KOIKOI_BONUS;
    }
    return clamp_int(threat, 0, 100);
}

static int risk_threat_multiplier_pct(int opp_coarse_threat, int koikoi_opp)
{
    int multiplier = 100;

    if (opp_coarse_threat >= 80) {
        multiplier = STRATEGY_RISK_MULT_HIGH_PCT;
    } else if (opp_coarse_threat >= 60) {
        multiplier = STRATEGY_RISK_MULT_MID_PCT;
    } else if (opp_coarse_threat >= 40) {
        multiplier = STRATEGY_RISK_MULT_LOW_PCT;
    }
    if (koikoi_opp == ON) {
        multiplier = (multiplier * STRATEGY_RISK_MULT_KOIKOI_PCT + 50) / 100;
    }
    return multiplier;
}

static int is_bonus_target_card(int wid, const Card* card)
{
    if (!card) {
        return OFF;
    }
    switch (wid) {
        case WID_DBTAN:
        case WID_AKATAN:
        case WID_AOTAN:
        case WID_TAN:
            return card->type == CARD_TYPE_TAN ? ON : OFF;
        case WID_ISC:
        case WID_TANE:
            return card->type == CARD_TYPE_TANE ? ON : OFF;
        case WID_KASU:
            return (card->type == CARD_TYPE_KASU || (card->type == CARD_TYPE_TANE && card->month == 8)) ? ON : OFF;
        default:
            break;
    }
    return OFF;
}

static void count_bonus_targets(int wid, CardSet* hand, CardSet* floor, int* in_hand, int* in_floor)
{
    int hand_cnt = 0;
    int floor_cnt = 0;
    if (hand) {
        for (int i = 0; i < 8; i++) {
            if (is_bonus_target_card(wid, hand->cards[i])) {
                hand_cnt++;
            }
        }
    }
    if (floor) {
        for (int i = 0; i < 48; i++) {
            if (is_bonus_target_card(wid, floor->cards[i])) {
                floor_cnt++;
            }
        }
    }
    if (in_hand) {
        *in_hand = hand_cnt;
    }
    if (in_floor) {
        *in_floor = floor_cnt;
    }
}

static int count_bonus_targets_in_floor_range(int wid, CardSet* floor)
{
    if (!floor || floor->num <= 0) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < floor->num; i++) {
        int idx = floor->start + i;
        if (idx >= 48) {
            idx -= 48;
        }
        if (is_bonus_target_card(wid, floor->cards[idx])) {
            count++;
        }
    }
    return count;
}

static int count_bonus_targets_in_hand(int wid, CardSet* hand)
{
    if (!hand) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (is_bonus_target_card(wid, hand->cards[i])) {
            count++;
        }
    }
    return count;
}

static int count_month_in_hand(CardSet* hand, int month)
{
    if (!hand) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < 8; i++) {
        Card* card = hand->cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    return count;
}

static int count_month_in_set(CardSet* set, int month)
{
    if (!set) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < 48; i++) {
        Card* card = set->cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    return count;
}

static int count_month_in_invent(CardSet* invent, int month)
{
    int count = 0;
    for (int t = 0; t < 4; t++) {
        count += count_month_in_set(&invent[t], month);
    }
    return count;
}

static int count_month_in_deck(CardSet* deck, int month)
{
    return count_month_in_set(deck, month);
}

static void append_unique_month(int* months, int* month_count, int month)
{
    if (!months || !month_count || month < 0 || month >= 12) {
        return;
    }
    for (int i = 0; i < *month_count; i++) {
        if (months[i] == month) {
            return;
        }
    }
    if (*month_count < 12) {
        months[(*month_count)++] = month;
    }
}

static void collect_target_months_floor(int wid, int completed_own, const StrategyRequirement* req, const int* loc_by_key, CardSet* deck, int* months,
                                        int* month_count)
{
    if (!months || !month_count || !loc_by_key) {
        return;
    }
    *month_count = 0;
    if (is_additive_wid(wid) && completed_own) {
        if (!deck) {
            return;
        }
        for (int i = 0; i < 48; i++) {
            Card* card = deck->cards[i];
            if (is_bonus_target_card(wid, card)) {
                append_unique_month(months, month_count, card->month);
            }
        }
        return;
    }

    if (!req) {
        return;
    }
    for (int i = 0; i < req->count; i++) {
        int key = req->cards[i].key;
        if (key < 0 || key >= 48) {
            continue;
        }
        if (loc_by_key[key] & SLOC_FLOOR) {
            append_unique_month(months, month_count, key >> 2);
        }
    }
}

static void collect_target_months_hand(int wid, int completed_own, const StrategyRequirement* req, const int* loc_by_key, CardSet* hand, int* months,
                                       int* month_count)
{
    if (!months || !month_count || !loc_by_key) {
        return;
    }
    *month_count = 0;
    if (is_additive_wid(wid) && completed_own) {
        if (!hand) {
            return;
        }
        for (int i = 0; i < 8; i++) {
            Card* card = hand->cards[i];
            if (is_bonus_target_card(wid, card)) {
                append_unique_month(months, month_count, card->month);
            }
        }
        return;
    }

    if (!req) {
        return;
    }
    for (int i = 0; i < req->count; i++) {
        int key = req->cards[i].key;
        if (key < 0 || key >= 48) {
            continue;
        }
        if (loc_by_key[key] & SLOC_HAND) {
            append_unique_month(months, month_count, key >> 2);
        }
    }
}

static int promote_rate_from_floor_targets(int base_rate, int wid, int completed_own, const StrategyRequirement* req, const int* loc_by_key,
                                           CardSet* hand, CardSet* deck, CardSet* own_invent, CardSet* opp_invent)
{
    if (base_rate != 50) {
        return base_rate;
    }
    int months[12];
    int month_count = 0;
    collect_target_months_floor(wid, completed_own, req, loc_by_key, deck, months, &month_count);
    int rate = base_rate;
    for (int i = 0; i < month_count; i++) {
        int month = months[i];
        int hand_same = count_month_in_hand(hand, month);
        if (hand_same >= 1) {
            if (rate < 70) {
                rate = 70;
            }
            if (hand_same >= 2 && rate < 75) {
                rate = 75;
            }
            int known = hand_same + count_month_in_invent(own_invent, month) + count_month_in_invent(opp_invent, month);
            if (known >= 2) {
                rate = 100;
                break;
            }
        }
    }
    return rate;
}

static int promote_rate_from_hand_targets(int base_rate, int wid, int completed_own, const StrategyRequirement* req, const int* loc_by_key,
                                          CardSet* hand, CardSet* deck, CardSet* own_invent, CardSet* opp_invent)
{
    if (base_rate != 80) {
        return base_rate;
    }
    int months[12];
    int month_count = 0;
    collect_target_months_hand(wid, completed_own, req, loc_by_key, hand, months, &month_count);
    int rate = base_rate;
    for (int i = 0; i < month_count; i++) {
        int month = months[i];
        int deck_same = count_month_in_deck(deck, month);
        if (deck_same >= 1) {
            if (rate < 90) {
                rate = 90;
            }
            if (deck_same >= 2 && rate < 95) {
                rate = 95;
            }
            int known = count_month_in_hand(hand, month) + count_month_in_invent(own_invent, month) + count_month_in_invent(opp_invent, month);
            if (known >= 2) {
                rate = 100;
                break;
            }
        }
    }
    return rate;
}

static int better_speed(const StrategyData* data, int a, int b)
{
    if (data->reach[a] != data->reach[b]) {
        return data->reach[a] > data->reach[b];
    }
    if (data->delay[a] != data->delay[b]) {
        return data->delay[a] < data->delay[b];
    }
    if (data->score[a] != data->score[b]) {
        return data->score[a] > data->score[b];
    }
    return a < b;
}

static int better_score(const StrategyData* data, int a, int b)
{
    if (data->reach[a] != data->reach[b]) {
        return data->reach[a] > data->reach[b];
    }
    if (data->score[a] != data->score[b]) {
        return data->score[a] > data->score[b];
    }
    if (data->delay[a] != data->delay[b]) {
        return data->delay[a] < data->delay[b];
    }
    return a < b;
}

typedef struct {
    int sankou_ready;
    int shikou_ready;
    int ame_shikou_ready;
    int akatan_ready;
    int aotan_ready;
} PriorityRuleFlags;

static int must_be_higher_by_rule(int a, int b, const PriorityRuleFlags* flags)
{
    if (!flags) {
        return OFF;
    }
    if (!flags->sankou_ready) {
        if (a == WID_SANKOU && (b == WID_SHIKOU || b == WID_AME_SHIKOU)) {
            return ON;
        }
    }
    if (!flags->shikou_ready || !flags->ame_shikou_ready) {
        if ((a == WID_SHIKOU || a == WID_AME_SHIKOU) && b == WID_GOKOU) {
            return ON;
        }
    }
    if (!flags->akatan_ready || !flags->aotan_ready) {
        if ((a == WID_AKATAN || a == WID_AOTAN) && b == WID_DBTAN) {
            return ON;
        }
    }
    return OFF;
}

static int better_speed_with_rules(const StrategyData* data, int a, int b, const PriorityRuleFlags* flags)
{
    if (must_be_higher_by_rule(a, b, flags)) {
        return ON;
    }
    if (must_be_higher_by_rule(b, a, flags)) {
        return OFF;
    }
    return better_speed(data, a, b);
}

static int better_score_with_rules(const StrategyData* data, int a, int b, const PriorityRuleFlags* flags)
{
    if (must_be_higher_by_rule(a, b, flags)) {
        return ON;
    }
    if (must_be_higher_by_rule(b, a, flags)) {
        return OFF;
    }
    return better_score(data, a, b);
}

static void build_priorities(StrategyData* data, const StrategyCounts* own_counts)
{
    if (!data || !own_counts) {
        return;
    }
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        data->priority_speed[i] = i;
        data->priority_score[i] = i;
    }
    PriorityRuleFlags flags;
    flags.sankou_ready = can_complete_by_counts(WID_SANKOU, own_counts);
    flags.shikou_ready = can_complete_by_counts(WID_SHIKOU, own_counts);
    flags.ame_shikou_ready = can_complete_by_counts(WID_AME_SHIKOU, own_counts);
    flags.akatan_ready = can_complete_by_counts(WID_AKATAN, own_counts);
    flags.aotan_ready = can_complete_by_counts(WID_AOTAN, own_counts);

    for (int i = 0; i < WINNING_HAND_MAX - 1; i++) {
        for (int j = i + 1; j < WINNING_HAND_MAX; j++) {
            int ai = data->priority_speed[i];
            int aj = data->priority_speed[j];
            if (!better_speed_with_rules(data, ai, aj, &flags)) {
                data->priority_speed[i] = aj;
                data->priority_speed[j] = ai;
            }
        }
    }
    for (int i = 0; i < WINNING_HAND_MAX - 1; i++) {
        for (int j = i + 1; j < WINNING_HAND_MAX; j++) {
            int ai = data->priority_score[i];
            int aj = data->priority_score[j];
            if (!better_score_with_rules(data, ai, aj, &flags)) {
                data->priority_score[i] = aj;
                data->priority_score[j] = ai;
            }
        }
    }
}

static int clamp_int(int x, int min, int max)
{
    if (x < min) {
        return min;
    }
    if (x > max) {
        return max;
    }
    return x;
}

static int strategy_linear_behind_bonus(int match_score_diff, int max_bonus)
{
    int behind;

    if (max_bonus <= 0) {
        return 0;
    }
    behind = -match_score_diff;
    if (behind <= 0) {
        return 0;
    }
    if (behind >= STRATEGY_BEHIND_LINEAR_MAX_DIFF) {
        return max_bonus;
    }
    return (behind * max_bonus + STRATEGY_BEHIND_LINEAR_MAX_DIFF / 2) / STRATEGY_BEHIND_LINEAR_MAX_DIFF;
}

static int strategy_behind_offence_tier(int match_score_diff)
{
    int behind = -match_score_diff;

    if (behind >= 28) {
        return 3;
    }
    if (behind >= 20) {
        return 2;
    }
    if (behind >= 13) {
        return 1;
    }
    return 0;
}

static void strategy_self_best_metrics(const StrategyData* data, StrategySelfBestMetrics* out_metrics)
{
    if (!data || !out_metrics) {
        return;
    }
    out_metrics->self_best_ev = 0;
    out_metrics->self_best_speed_ev = 0;
    out_metrics->self_best_delay = STRATEGY_DELAY_INVALID;

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (ai_is_disabled_wid_by_rules(wid)) {
            continue;
        }
        int reach = data->reach[wid];
        int score = data->score[wid];
        int delay = data->delay[wid];
        int self_ev = reach * score;
        if (self_ev > out_metrics->self_best_ev) {
            out_metrics->self_best_ev = self_ev;
        }
        if (delay >= 0 && delay < STRATEGY_DELAY_INVALID) {
            int self_speed_ev = self_ev / (delay + 1);
            if (self_speed_ev > out_metrics->self_best_speed_ev) {
                out_metrics->self_best_speed_ev = self_speed_ev;
            }
        }
        if (reach > 0 && score > 0 && delay >= 0 && delay < out_metrics->self_best_delay) {
            out_metrics->self_best_delay = delay;
        }
    }
}

static void strategy_opp_best_metrics(const StrategyData* data, StrategyOppBestMetrics* out_metrics)
{
    if (!data || !out_metrics) {
        return;
    }
    out_metrics->opp_best_risk_ev = 0;
    out_metrics->opp_best_risk_delay = STRATEGY_DELAY_INVALID;

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (ai_is_disabled_wid_by_rules(wid)) {
            continue;
        }
        int risk_ev = data->risk_score[wid];
        int risk_delay = data->risk_delay[wid];
        if (risk_ev > out_metrics->opp_best_risk_ev) {
            out_metrics->opp_best_risk_ev = risk_ev;
        }
        if ((data->risk_reach_estimate[wid] > 0 || risk_ev > 0) && risk_delay >= 0 && risk_delay < out_metrics->opp_best_risk_delay) {
            out_metrics->opp_best_risk_delay = risk_delay;
        }
    }
}

static void strategy_calc_mode(StrategyData* data)
{
    StrategySelfBestMetrics self_metrics;
    int greedy_need = 0;
    int defensive_need = 0;
    int best_self_score = 0;
    int behind_greedy_bonus;
    int behind_defence_penalty;

    if (!data) {
        return;
    }

    strategy_self_best_metrics(data, &self_metrics);
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (ai_is_disabled_wid_by_rules(wid)) {
            continue;
        }
        if (data->score[wid] > best_self_score) {
            best_self_score = data->score[wid];
        }
    }

    if (data->match_score_diff < 0) {
        greedy_need += clamp_int(-data->match_score_diff * STRATEGY_GREEDY_BEHIND_FACTOR, 0, 32);
    } else if (data->match_score_diff > 0) {
        defensive_need += clamp_int(data->match_score_diff * STRATEGY_DEFENSIVE_LEAD_FACTOR, 0, 24);
    }

    behind_greedy_bonus = strategy_linear_behind_bonus(data->match_score_diff, STRATEGY_BEHIND_GREEDY_BONUS_MAX);
    behind_defence_penalty = strategy_linear_behind_bonus(data->match_score_diff, STRATEGY_BEHIND_DEFENCE_NEED_PENALTY_MAX);
    greedy_need += behind_greedy_bonus;
    defensive_need -= behind_defence_penalty;

    if (data->left_rounds <= 1 && data->match_score_diff > 0) {
        greedy_need -= STRATEGY_FINAL_LEAD_GREEDY_PENALTY;
        defensive_need += STRATEGY_FINAL_LEAD_DEFENCE_BONUS;
    }

    if (data->left_rounds <= 1) {
        greedy_need += STRATEGY_GREEDY_LEFT_ROUNDS_NEAR1_BONUS;
    } else if (data->left_rounds <= 2) {
        greedy_need += STRATEGY_GREEDY_LEFT_ROUNDS_NEAR2_BONUS;
    }

    if (best_self_score >= 7) {
        greedy_need += STRATEGY_GREEDY_SELF_SCORE_NEAR7_BONUS;
    } else if (best_self_score >= 5) {
        greedy_need += STRATEGY_GREEDY_SELF_SCORE_NEAR5_BONUS;
    }
    if (self_metrics.self_best_delay <= 2) {
        greedy_need += STRATEGY_GREEDY_SELF_DELAY_NEAR2_BONUS;
    } else if (self_metrics.self_best_delay <= 3) {
        greedy_need += STRATEGY_GREEDY_SELF_DELAY_NEAR3_BONUS;
    }
    if (self_metrics.self_best_ev >= 500) {
        greedy_need += STRATEGY_GREEDY_SELF_EV_HIGH_BONUS;
    }
    if (data->koikoi_mine == ON) {
        greedy_need += STRATEGY_GREEDY_KOIKOI_MINE_BONUS;
    }

    defensive_need += clamp_int(data->opp_coarse_threat / STRATEGY_DEFENSIVE_THREAT_DIVISOR, 0, 100);
    if (data->koikoi_opp == ON) {
        defensive_need += STRATEGY_DEFENSIVE_KOIKOI_OPP_BONUS;
    }

    data->greedy_need = clamp_int(greedy_need, 0, 100);
    data->defensive_need = clamp_int(defensive_need, 0, 100);

    if (data->match_score_diff <= -STRATEGY_MODE_FORCE_GREEDY_BEHIND && data->left_rounds <= STRATEGY_MODE_FORCE_GREEDY_LEFT_ROUNDS) {
        data->mode = MODE_GREEDY;
        return;
    }
    if (data->left_rounds <= STRATEGY_MODE_FORCE_DEFENCE_FINAL_LEAD && data->match_score_diff > 0) {
        data->mode = MODE_DEFENSIVE;
        return;
    }
    if (data->opp_coarse_threat >= STRATEGY_MODE_FORCE_DEFENCE_THREAT ||
        (data->koikoi_opp == ON && data->opp_coarse_threat >= STRATEGY_MODE_FORCE_DEFENCE_THREAT_KOIKOI)) {
        data->mode = MODE_DEFENSIVE;
        return;
    }
    if (data->defensive_need - data->greedy_need >= STRATEGY_MODE_SWITCH_MARGIN) {
        data->mode = MODE_DEFENSIVE;
    } else if (data->greedy_need - data->defensive_need >= STRATEGY_MODE_SWITCH_MARGIN) {
        data->mode = MODE_GREEDY;
    } else {
        data->mode = MODE_BALANCED;
    }
}

static void strategy_calc_bias(StrategyData* data)
{
    if (!data) {
        return;
    }
    StrategySelfBestMetrics self_metrics;
    StrategyOppBestMetrics opp_metrics;
    strategy_self_best_metrics(data, &self_metrics);
    strategy_opp_best_metrics(data, &opp_metrics);

    int defence = 0;
    if (data->koikoi_opp) {
        defence += STRATEGY_DEFENCE_KOIKOI_OPP_BONUS;
    }
    if (data->opponent_win_x2) {
        defence += STRATEGY_DEFENCE_OPP_WIN_X2_BONUS;
    }
    if (opp_metrics.opp_best_risk_delay <= 2) {
        defence += STRATEGY_DEFENCE_RISK_DELAY_NEAR2_BONUS;
    } else if (opp_metrics.opp_best_risk_delay <= 4) {
        defence += STRATEGY_DEFENCE_RISK_DELAY_NEAR4_BONUS;
    } else if (opp_metrics.opp_best_risk_delay <= 6) {
        defence += STRATEGY_DEFENCE_RISK_DELAY_NEAR6_BONUS;
    }
    if (opp_metrics.opp_best_risk_ev >= 5) {
        defence += STRATEGY_DEFENCE_RISK_EV_HIGH_BONUS;
    } else if (opp_metrics.opp_best_risk_ev >= 3) {
        defence += STRATEGY_DEFENCE_RISK_EV_MEDIUM_BONUS;
    }
    if (data->left_own <= 2) {
        defence += STRATEGY_DEFENCE_LEFT_OWN_NEAR2_BONUS;
    } else if (data->left_own <= 3) {
        defence += STRATEGY_DEFENCE_LEFT_OWN_NEAR3_BONUS;
    }
    if (data->left_rounds <= 1) {
        defence += STRATEGY_DEFENCE_LEFT_ROUNDS_NEAR1_BONUS;
    } else if (data->left_rounds <= 2) {
        defence += STRATEGY_DEFENCE_LEFT_ROUNDS_NEAR2_BONUS;
    }
    if (data->risk_7plus_distance <= 2) {
        defence += STRATEGY_DEFENCE_RISK_7PLUS_NEAR2_BONUS;
    } else if (data->risk_7plus_distance <= 4) {
        defence += STRATEGY_DEFENCE_RISK_7PLUS_NEAR4_BONUS;
    }

    int speed = 0;
    if (data->left_own <= 3) {
        speed += STRATEGY_SPEED_LEFT_OWN_NEAR3_BONUS;
    } else if (data->left_own <= 5) {
        speed += STRATEGY_SPEED_LEFT_OWN_NEAR5_BONUS;
    } else if (data->left_own <= 7) {
        speed += STRATEGY_SPEED_LEFT_OWN_NEAR7_BONUS;
    }
    if (opp_metrics.opp_best_risk_delay <= 2) {
        speed += STRATEGY_SPEED_OPP_DELAY_NEAR2_BONUS;
    } else if (opp_metrics.opp_best_risk_delay <= 4) {
        speed += STRATEGY_SPEED_OPP_DELAY_NEAR4_BONUS;
    }
    if (self_metrics.self_best_delay <= 2) {
        speed += STRATEGY_SPEED_SELF_DELAY_NEAR2_BONUS;
    } else if (self_metrics.self_best_delay <= 3) {
        speed += STRATEGY_SPEED_SELF_DELAY_NEAR3_BONUS;
    }
    if (data->koikoi_opp) {
        speed += STRATEGY_SPEED_KOIKOI_OPP_BONUS;
    }
    if (data->opponent_win_x2) {
        speed += STRATEGY_SPEED_OPP_WIN_X2_BONUS;
    }
    if (data->koikoi_mine) {
        speed += STRATEGY_SPEED_KOIKOI_MINE_BONUS;
    }
    if (data->left_rounds <= 1) {
        speed += STRATEGY_SPEED_LEFT_ROUNDS_NEAR1_BONUS;
    }
    if (data->risk_7plus_distance <= 2) {
        speed += STRATEGY_SPEED_RISK_7PLUS_NEAR2_BONUS;
    } else if (data->risk_7plus_distance <= 4) {
        speed += STRATEGY_SPEED_RISK_7PLUS_NEAR4_BONUS;
    }

    int offence = 0;
    if (data->koikoi_mine) {
        offence += STRATEGY_OFFENCE_KOIKOI_MINE_BONUS;
    }
    if (data->left_rounds <= 1) {
        offence += STRATEGY_OFFENCE_LEFT_ROUNDS_NEAR1_BONUS;
    } else if (data->left_rounds <= 2) {
        offence += STRATEGY_OFFENCE_LEFT_ROUNDS_NEAR2_BONUS;
    }
    if (self_metrics.self_best_ev >= 700) {
        offence += STRATEGY_OFFENCE_SELF_EV_HIGH_BONUS;
    } else if (self_metrics.self_best_ev >= 500) {
        offence += STRATEGY_OFFENCE_SELF_EV_MID_BONUS;
    } else if (self_metrics.self_best_ev >= 300) {
        offence += STRATEGY_OFFENCE_SELF_EV_LOW_BONUS;
    }
    if (data->koikoi_opp && (opp_metrics.opp_best_risk_ev >= 3 || opp_metrics.opp_best_risk_delay <= 4)) {
        offence += STRATEGY_OFFENCE_VS_DANGEROUS_OPP_BONUS;
    }
    if (data->opponent_win_x2) {
        offence -= STRATEGY_OFFENCE_OPP_WIN_X2_PENALTY;
    }
    if (data->left_own <= 2) {
        offence -= STRATEGY_OFFENCE_TOO_LATE_PENALTY;
    }

    if (data->left_rounds <= 1 && data->match_score_diff > 0) {
        offence -= STRATEGY_BIAS_FINAL_LEAD_OFFENCE_PENALTY;
        speed += STRATEGY_BIAS_FINAL_LEAD_SPEED_BONUS;
        defence += STRATEGY_BIAS_FINAL_LEAD_DEFENCE_BONUS;
    }

    // 総合点差（自分 - 相手）を終盤ほど強く効かせる。
    {
        int diff_clamped = clamp_int(data->match_score_diff, -10, 10);
        int diff_norm = diff_clamped * 10; // -100..+100
        int scale = 40;
        if (data->left_rounds <= 1) {
            scale = 100;
        } else if (data->left_rounds <= 2) {
            scale = 70;
        }

        if (diff_norm < 0) {
            int behind = -diff_norm;
            offence += (behind * 35 * scale + 5000) / 10000;
            defence -= (behind * 15 * scale + 5000) / 10000;
        } else if (diff_norm > 0) {
            defence += (diff_norm * 25 * scale + 5000) / 10000;
            speed += (diff_norm * 20 * scale + 5000) / 10000;
            offence -= (diff_norm * 20 * scale + 5000) / 10000;
        }
    }

    switch (strategy_behind_offence_tier(data->match_score_diff)) {
        case 3:
            offence += 35;
            speed += 18;
            defence -= 22;
            break;
        case 2:
            offence += 22;
            speed += 10;
            defence -= 12;
            break;
        case 1:
            offence += 10;
            speed += 5;
            defence -= 5;
            break;
        default:
            break;
    }

    switch (data->mode) {
        case MODE_GREEDY:
            offence += STRATEGY_MODE_GREEDY_OFFENCE_BONUS;
            speed += STRATEGY_MODE_GREEDY_SPEED_BONUS;
            defence -= STRATEGY_MODE_GREEDY_DEFENCE_PENALTY;
            break;
        case MODE_DEFENSIVE:
            defence += STRATEGY_MODE_DEFENCE_DEFENCE_BONUS;
            speed += STRATEGY_MODE_DEFENCE_SPEED_BONUS;
            offence -= STRATEGY_MODE_DEFENCE_OFFENCE_PENALTY;
            if (data->match_score_diff < 0) {
                offence = offence < STRATEGY_MODE_DEFENCE_OFFENCE_FLOOR_BEHIND ? STRATEGY_MODE_DEFENCE_OFFENCE_FLOOR_BEHIND : offence;
            } else {
                offence = offence < STRATEGY_MODE_DEFENCE_OFFENCE_FLOOR ? STRATEGY_MODE_DEFENCE_OFFENCE_FLOOR : offence;
            }
            break;
        default:
            break;
    }

    // バイアスは「局面と self/risk 集約指標」のみで算出し、候補手評価側で重みとして使う。
    data->bias.offence = clamp_int(offence, 0, 100);
    data->bias.speed = clamp_int(speed, 0, 100);
    data->bias.defence = clamp_int(defence, 0, 100);
}

void ai_think_strategy_mode(int player, StrategyData* data, int mode, const StrategyData* cache)
{
    if (!data || player < 0 || player > 1) {
        return;
    }
    int skip_reach = (mode & AI_THINK_STRATEGY_SKIP_REACH) != 0;
    int skip_risk = (mode & AI_THINK_STRATEGY_SKIP_RISK) != 0;
    int skip_priority = (mode & AI_THINK_STRATEGY_SKIP_PRIORITY) != 0;
    int skip_bias = (mode & AI_THINK_STRATEGY_SKIP_BIAS) != 0;

    init_strategy_requirements();
    vgs_memset(data, 0, sizeof(StrategyData));

    CardSet* hand = &g.own[player];
    CardSet* deck = &g.deck;
    CardSet* own_invent = g.invent[player];
    CardSet* opp_invent = g.invent[1 - player];
    int opp = 1 - player;
    int opp_current_score = g.stats[opp].score;

    int loc_by_key[48];
    for (int i = 0; i < 48; i++) {
        loc_by_key[i] = SLOC_NONE;
    }
    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        data->delay[i] = 99;
        data->risk_delay[i] = 99;
    }
    for (int t = 0; t < 4; t++) {
        fill_location_by_set(loc_by_key, &opp_invent[t], SLOC_OPP);
    }
    for (int t = 0; t < 4; t++) {
        fill_location_by_set(loc_by_key, &own_invent[t], SLOC_OWN);
    }
    fill_location_by_hand(loc_by_key, hand, SLOC_HAND);
    fill_location_by_set(loc_by_key, deck, SLOC_FLOOR);

    StrategyCounts own_counts;
    vgs_memset(&own_counts, 0, sizeof(own_counts));
    for (int t = 0; t < 4; t++) {
        add_zone_to_counts(&own_counts, &own_invent[t]);
    }
    int tan_blocked = can_complete_by_counts(WID_AKATAN, &own_counts) || can_complete_by_counts(WID_AOTAN, &own_counts) ||
                      can_complete_by_counts(WID_DBTAN, &own_counts);
    int tane_blocked = can_complete_by_counts(WID_ISC, &own_counts);
    int completed_own_flags[WINNING_HAND_MAX];
    vgs_memset(completed_own_flags, 0, sizeof(completed_own_flags));

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int can_hand = OFF;
        int can_floor = OFF;
        int can_both = OFF;
        int hand_used = 0;
        int floor_used = 0;
        int completed_own = can_complete_by_counts(wid, &own_counts);
        completed_own_flags[wid] = completed_own;

        if (is_additive_wid(wid) && completed_own) {
            int bonus_hand = 0;
            int bonus_floor = 0;
            count_bonus_targets(wid, hand, deck, &bonus_hand, &bonus_floor);
            can_hand = bonus_hand > 0 ? ON : OFF;
            can_floor = bonus_floor > 0 ? ON : OFF;
            can_both = (bonus_hand + bonus_floor) > 0 ? ON : OFF;
            if (can_both) {
                hand_used = can_hand ? 1 : 0;
                floor_used = can_hand ? 0 : 1;
                data->delay[wid] = hand_used + floor_used * 2;
            }
        } else {
            const StrategyRequirement* req = &strategy_requirements[wid];
            can_hand = evaluate_requirement(req, loc_by_key, ON, OFF, OFF, NULL, NULL, NULL);
            can_floor = evaluate_requirement(req, loc_by_key, OFF, ON, OFF, NULL, NULL, NULL);
            can_both = evaluate_requirement(req, loc_by_key, ON, ON, OFF, &hand_used, &floor_used, NULL);
            if (completed_own) {
                data->delay[wid] = 0;
            } else if (can_both) {
                data->delay[wid] = hand_used + floor_used * 2;
            }
        }

        int reachable_without_est = can_both || (is_additive_wid(wid) && completed_own && (can_hand || can_floor));
        if (completed_own && !is_additive_wid(wid)) {
            data->score[wid] = 0;
        } else if (!reachable_without_est) {
            data->delay[wid] = 99;
            data->score[wid] = 0;
        } else {
            data->score[wid] = calc_first_completion_score(wid, completed_own);
        }
    }

    if (!skip_reach) {
        /*
         * reach は「隠れ情報（相手手札 + 山札順序）を決定化MCでサンプルし、
         * π_wid（クラスタ方策）でロールアウトしたときにラウンド中で新規成立する確率 [%]」。
         */
        int reach_count[WINNING_HAND_MAX];
        int delay_sum[WINNING_HAND_MAX];
        int delay_hit[WINNING_HAND_MAX];
        int cluster_trials[STRATEGY_CLUSTER_MAX];
        int cluster_active[STRATEGY_CLUSTER_MAX];
        vgs_memset(reach_count, 0, sizeof(reach_count));
        vgs_memset(delay_sum, 0, sizeof(delay_sum));
        vgs_memset(delay_hit, 0, sizeof(delay_hit));
        vgs_memset(cluster_trials, 0, sizeof(cluster_trials));
        vgs_memset(cluster_active, 0, sizeof(cluster_active));

        for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
            if (completed_own_flags[wid]) {
                continue;
            }
            if (ai_is_disabled_wid_by_rules(wid)) {
                continue;
            }
            if (tan_blocked && wid == WID_TAN) {
                continue;
            }
            if (tane_blocked && wid == WID_TANE) {
                continue;
            }
            cluster_active[strategy_cluster_of_wid(wid)] = ON;
        }

        Card* hidden_pool[48];
        int hidden_num = build_hidden_pool(player, hidden_pool, 48);
        int sample_count = strategy_reach_sample_count(g.own[player].num);
        int sample_serial = 0;
        int vsync_calls = 0;
        uint32_t rng_state = (uint32_t)(0x9E3779B9u ^ (uint32_t)player ^ (uint32_t)(g.round << 8) ^ (uint32_t)(g.current_player << 16) ^
                                        (uint32_t)(g.own[player].num << 20) ^ (uint32_t)(g.floor.num << 24));

        for (int cluster = 0; cluster < STRATEGY_CLUSTER_MAX; cluster++) {
            if (!cluster_active[cluster]) {
                continue;
            }
            for (int sample = 0; sample < sample_count; sample++) {
                ReachSimState sim;
                int reached[WINNING_HAND_MAX];
                int completion_turn[WINNING_HAND_MAX];
                vgs_memset(reached, 0, sizeof(reached));
                for (int i = 0; i < WINNING_HAND_MAX; i++) {
                    completion_turn[i] = -1;
                }
                if (!sample_determinization(player, (const Card**)hidden_pool, hidden_num, &rng_state, &sim)) {
                    continue;
                }
                cluster_trials[cluster]++;
                rollout_until_end_for_cluster(&sim, player, cluster, completed_own_flags, reached, completion_turn);
                update_reach_counters(cluster, reached, completion_turn, reach_count, delay_sum, delay_hit);

                sample_serial++;
                // 重いMC計算中の処理猶予確保（例: 32x7=224試行時に3〜4回）
                if ((sample_serial % STRATEGY_REACH_VSYNC_INTERVAL) == 0) {
                    ai_vsync();
                    vsync_calls++;
                }
            }
        }

        // フレーム落ち回避のため、思考処理中に最低回数のvsyncを実行する。
        while (vsync_calls < STRATEGY_REACH_VSYNC_MIN_CALLS) {
            ai_vsync();
            vsync_calls++;
        }

        for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
            if (ai_is_disabled_wid_by_rules(wid)) {
                data->reach[wid] = 0;
                data->delay[wid] = 99;
                data->score[wid] = 0;
                continue;
            }
            if (completed_own_flags[wid]) {
                data->reach[wid] = 0;
                data->delay[wid] = 0;
                data->score[wid] = 0;
                continue;
            }
            int cluster = strategy_cluster_of_wid(wid);
            int trials = cluster_trials[cluster];
            if (trials <= 0) {
                data->reach[wid] = 0;
                data->delay[wid] = 99;
                data->score[wid] = 0;
                continue;
            }
            int v = (reach_count[wid] * 100 + trials / 2) / trials;
            if (v < 0) {
                v = 0;
            } else if (v > 100) {
                v = 100;
            }
            data->reach[wid] = v;
            if (data->reach[wid] > 0) {
                data->score[wid] = calc_first_completion_score(wid, OFF);
                if (delay_hit[wid] > 0) {
                    data->delay[wid] = (delay_sum[wid] + delay_hit[wid] / 2) / delay_hit[wid];
                } else {
                    data->delay[wid] = 99;
                }
            } else {
                data->delay[wid] = 99;
                data->score[wid] = 0;
            }
        }
        if (tan_blocked) {
            data->reach[WID_TAN] = 0;
            data->delay[WID_TAN] = 99;
            data->score[WID_TAN] = 0;
        }
        if (tane_blocked) {
            data->reach[WID_TANE] = 0;
            data->delay[WID_TANE] = 99;
            data->score[WID_TANE] = 0;
        }
    } else if (cache) {
        vgs_memcpy(data->reach, cache->reach, sizeof(data->reach));
    }

    /*
     * risk_* は相手視点の static 脅威推定:
     * 未知札（相手手札/山札）を counts には含めず、確定情報を中心に見積もる。
     */
    if (!skip_risk) {
        int opp = 1 - player;
        StrategyCounts opp_counts_fixed;
        StrategyCounts opp_counts_plus_floor;
        build_counts_for_player_fixed(opp, &opp_counts_fixed);
        build_counts_for_player_plus_floor(opp, &opp_counts_plus_floor);
        for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
            vsync();
            if (ai_is_disabled_wid_by_rules(wid)) {
                data->risk_delay[wid] = 99;
                data->risk_reach_estimate[wid] = 0;
                data->risk_score[wid] = 0;
                continue;
            }
            if (can_complete_by_counts(wid, &opp_counts_fixed)) {
                data->risk_delay[wid] = 99;
                data->risk_reach_estimate[wid] = 0;
                data->risk_score[wid] = 0;
                continue;
            }
            int risk_delay = calc_risk_delay(wid, player, &opp_counts_fixed);
            int risk_reach = calc_risk_reach_estimate(wid, player, &opp_counts_fixed, &opp_counts_plus_floor, risk_delay);
            if (risk_delay >= 99 || risk_reach <= 0) {
                data->risk_delay[wid] = 99;
                data->risk_reach_estimate[wid] = 0;
                data->risk_score[wid] = 0;
                continue;
            }
            int base_score = calc_first_completion_score(wid, OFF);
            data->risk_delay[wid] = risk_delay;
            data->risk_reach_estimate[wid] = risk_reach;
            data->risk_score[wid] = calc_risk_score(wid, base_score, risk_reach);
        }
    } else if (cache) {
        vgs_memcpy(data->risk_reach_estimate, cache->risk_reach_estimate, sizeof(data->risk_reach_estimate));
        vgs_memcpy(data->risk_delay, cache->risk_delay, sizeof(data->risk_delay));
        vgs_memcpy(data->risk_score, cache->risk_score, sizeof(data->risk_score));
    }

    // 戦況情報を設定
    data->left_rounds = g.round_max - g.round;
    data->round_max = g.round_max;
    data->left_own = 0;
    for (int i = 0; i < 48; i++) {
        if (g.own[player].cards[i]) {
            data->left_own++;
        }
    }
    data->koikoi_opp = g.koikoi[1 - player];
    data->koikoi_mine = g.koikoi[player];
    data->match_score_diff = g.total_score[player] - g.total_score[1 - player];
    data->base_now = g.stats[player].score;
    data->best_additional_1pt_reach = 0;
    data->best_additional_1pt_delay = 99;
    data->can_overpay_akatan = ai_overpay_possible_now(player, WID_AKATAN);
    data->can_overpay_aotan = ai_overpay_possible_now(player, WID_AOTAN);
    data->can_overpay_ino = ai_overpay_possible_now(player, WID_ISC);

    for (int wid = WID_TANE; wid <= WID_KASU; wid++) {
        if (data->reach[wid] <= 0 || data->score[wid] <= 0 || data->delay[wid] >= 99) {
            continue;
        }
        if (data->best_additional_1pt_reach < data->reach[wid]) {
            data->best_additional_1pt_reach = data->reach[wid];
        }
        if (data->delay[wid] < data->best_additional_1pt_delay) {
            data->best_additional_1pt_delay = data->delay[wid];
        }
    }
    if (data->best_additional_1pt_reach <= 0) {
        data->best_additional_1pt_delay = 99;
    }

    data->risk_7plus_distance = estimate_risk_7plus_distance(data, opp_current_score);
    data->opp_coarse_threat = estimate_opp_coarse_threat(data, opp_current_score);
    data->opp_exact_7plus_threat = estimate_opp_exact_7plus_threat(data, opp_current_score);
    data->opponent_win_x2 = (g.koikoi[opp] == ON || opp_current_score >= 7 || data->risk_7plus_distance <= 4) ? ON : OFF;
    data->risk_mult_pct = risk_threat_multiplier_pct(data->opp_coarse_threat, data->koikoi_opp);
    data->env_total = ai_env_score(player);
    data->env_diff = data->env_total - ai_env_score(opp);
    ai_env_calc_cat_sum(player, data->env_cat_sum);
    data->domain = ai_env_estimate_domain(data->env_cat_sum);
    {
        int plan_reason = -1;
        data->plan = ai_env_estimate_plan(data, g.stats[player].score, &plan_reason);
        ai_debug_note_plan_decision(player, data->plan, plan_reason);
    }
    {
        int multiplier = data->risk_mult_pct;
        if (multiplier > 100) {
            for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
                if (data->risk_score[wid] <= 0) {
                    continue;
                }
                data->risk_score[wid] = (data->risk_score[wid] * multiplier + 50) / 100;
            }
        }
    }

    if (!skip_priority) {
        build_priorities(data, &own_counts);
    } else if (cache) {
        vgs_memcpy(data->priority_speed, cache->priority_speed, sizeof(data->priority_speed));
        vgs_memcpy(data->priority_score, cache->priority_score, sizeof(data->priority_score));
    }

    vsync();

    strategy_calc_mode(data);
    ai_debug_note_strategy_mode(player, data->mode);
    if (!skip_bias) {
        strategy_calc_bias(data);
    } else if (cache) {
        data->bias = cache->bias;
    }
}

void ai_think_strategy(int player, StrategyData* data)
{
    ai_think_strategy_mode(player, data, 0, NULL);
}
