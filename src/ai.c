#include "ai.h"
#include "ai_native.h"
#include <stdarg.h>
#include <limits.h>

#define HARD_SAKE_FALLBACK_ENABLE 1
#define HARD_SAKE_KOIKOI_SAFE_FALLBACK_ENABLE 0
#define HARD_SAKE_VISIBLE_SELECT_DROP_FALLBACK_ENABLE 0

static int (*koikoi_function[])(int player, int round_score) = {
    ai_normal_koikoi,
    ai_hard_koikoi,
    ai_stenv_koikoi,
    ai_mcenv_koikoi,
};
static int (*select_function[])(int player, Card* card) = {
    ai_normal_select,
    ai_hard_select,
    ai_stenv_select,
    ai_mcenv_select,
};
static int (*drop_function[])(int player) = {
    ai_normal_drop,
    ai_hard_drop,
    ai_stenv_drop,
    ai_mcenv_drop,
};
enum {
    AI_CARD_ID_SAKURA_GOKOU = 11,
    AI_CARD_ID_SAKE = 35,
    AI_CARD_ID_MOON_GOKOU = 31,
};
static AiDebugMetrics g_ai_debug_metrics;
static int g_ai_round_last_strategy_mode[2];
static int g_ai_round_has_strategy_mode[2];
static int g_ai_debug_seed;
static int g_ai_debug_game_index;
static char g_ai_log_buffer[1024];
static char g_ai_watch_min_action[2][256];
static char g_ai_watch_min_koikoi[2][256];
static char g_ai_last_think_extra[2][768];
static int g_ai_watch_turn[2];
static int g_ai_round_initial_sake[2];
typedef struct {
    int active;
    int actor;
    int opponent;
    int seed;
    int round;
    int round_max;
    int turn;
    int opponent_base_before;
} AiWatchAfterOppPending;
static AiWatchAfterOppPending g_ai_watch_after_opp[2];
static const int ENV_CAT_BASE[ENV_CAT_MAX] = {
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
static void ai_log_append_char(char* dst, int cap, int* pos, char ch);
static void ai_log_append_text(char* dst, int cap, int* pos, const char* src);
static void ai_log_format(char* dst, int cap, const char* fmt, va_list ap);
static int ai_env_opponent_has_combo_blocker(int player, int cat);
static int ai_env_add_card_value(int total, int player, Card* card, int multiplier);
static int ai_env_add_set_value(int total, int player, CardSet* set, int multiplier, int count);
static const char* ai_card_type_name(int type);
static void ai_watch_log_card_index_legend(void);
static const char* ai_watch_side_name(int player);
static const char* ai_watch_pick_wid_name(int wid);
static void ai_watch_clear_after_opp_pending(int player);
static void ai_watch_emit_after_opp(int actor, int opponent, const char* action, int base, int x2, int wid);
static int ai_card_set_has_card_id(const CardSet* set, int card_id);
static int ai_player_invent_has_card_id(int player, int card_id);
static int ai_has_visible_sake_key_card(int player);
static int ai_is_safe_sake_koikoi_fallback(int player);
static int ai_has_sake_fallback_base(int player);
static int ai_should_disable_sake_fallback(int player);
static int ai_should_force_stenv_sake_koikoi(int player);
static int ai_should_force_stenv_sake_select_drop(int player);
static int ai_should_cancel_sake_drop_fallback(int player);

int ai_env_category_from_card(int card_index)
{
    int month;
    int type;

    if (card_index < 0 || card_index >= 48) {
        return ENV_CAT_10;
    }

    month = card_index / 4;
    type = card_types[card_index];

    if (type == CARD_TYPE_GOKOU) {
        if (month == 2 || month == 7) {
            return ENV_CAT_1;
        }
        if (month == 0 || month == 11) {
            return ENV_CAT_3;
        }
        if (month == 10) {
            return ENV_CAT_6;
        }
        return ENV_CAT_1;
    }

    if (type == CARD_TYPE_TANE) {
        if (month == 8) {
            return ENV_CAT_2;
        }
        if (month == 5 || month == 6 || month == 9) {
            return ENV_CAT_7;
        }
        return ENV_CAT_9;
    }

    if (type == CARD_TYPE_TAN) {
        if (month == 0 || month == 1 || month == 2) {
            return ENV_CAT_4;
        }
        if (month == 5 || month == 8 || month == 9) {
            return ENV_CAT_5;
        }
        return ENV_CAT_8;
    }

    return ENV_CAT_10;
}

int ai_env_score(int player)
{
    int opp = 1 - player;
    int total = 0;

    if (player < 0 || player > 1) {
        return 0;
    }

    for (int t = 0; t < 4; t++) {
        total = ai_env_add_set_value(total, player, &g.invent[player][t], 5, g.invent[player][t].num);
        total = ai_env_add_set_value(total, player, &g.invent[opp][t], -5, g.invent[opp][t].num);
    }
    total = ai_env_add_set_value(total, player, &g.own[player], 2, 8);
    total = ai_env_add_set_value(total, player, &g.deck, 1, 48);
    return total;
}

static int ai_card_set_has_card_id(const CardSet* set, int card_id)
{
    int i;

    if (!set) {
        return OFF;
    }
    for (i = 0; i < set->num; i++) {
        Card* card = set->cards[i];
        if (card && card->id == card_id) {
            return ON;
        }
    }
    return OFF;
}

static int ai_has_sake_fallback_base(int player)
{
#if !HARD_SAKE_FALLBACK_ENABLE
    (void)player;
    return OFF;
#else
    if (player < 0 || player > 1) {
        return OFF;
    }
    return g.ai_model[player] == AI_MODEL_HARD && ai_debug_has_initial_sake(player);
#endif
}

static int ai_player_invent_has_card_id(int player, int card_id)
{
    int type;

    if (player < 0 || player > 1) {
        return OFF;
    }
    for (type = 0; type < 4; type++) {
        if (ai_card_set_has_card_id(&g.invent[player][type], card_id)) {
            return ON;
        }
    }
    return OFF;
}

static int ai_has_visible_sake_key_card(int player)
{
    if (player < 0 || player > 1) {
        return OFF;
    }
    if (ai_card_set_has_card_id(&g.own[player], AI_CARD_ID_SAKURA_GOKOU) || ai_card_set_has_card_id(&g.own[player], AI_CARD_ID_MOON_GOKOU)) {
        return ON;
    }
    if (ai_player_invent_has_card_id(player, AI_CARD_ID_SAKURA_GOKOU) || ai_player_invent_has_card_id(player, AI_CARD_ID_MOON_GOKOU)) {
        return ON;
    }
    if (ai_card_set_has_card_id(&g.deck, AI_CARD_ID_SAKURA_GOKOU) || ai_card_set_has_card_id(&g.deck, AI_CARD_ID_MOON_GOKOU)) {
        return ON;
    }
    return OFF;
}

static int ai_is_safe_sake_koikoi_fallback(int player)
{
    StrategyData* s;

    if (player < 0 || player > 1) {
        return OFF;
    }
    s = &g.strategy[player];
    return s->opponent_win_x2 == OFF && s->opp_coarse_threat < 60 && s->left_rounds >= 3;
}

static int ai_should_disable_sake_fallback(int player)
{
    StrategyData* s;

    if (player < 0 || player > 1) {
        return ON;
    }
    s = &g.strategy[player];
    return s->opponent_win_x2 == ON || s->opp_coarse_threat >= 85 || s->left_rounds <= 2;
}

static int ai_should_force_stenv_sake_koikoi(int player)
{
    if (!ai_has_sake_fallback_base(player)) {
        return OFF;
    }
    if (ai_should_disable_sake_fallback(player)) {
        return OFF;
    }
#if HARD_SAKE_KOIKOI_SAFE_FALLBACK_ENABLE
    return ai_is_safe_sake_koikoi_fallback(player);
#else
    return ON;
#endif
}

static int ai_should_force_stenv_sake_select_drop(int player)
{
    if (!ai_has_sake_fallback_base(player)) {
        return OFF;
    }
    if (ai_should_disable_sake_fallback(player)) {
        return OFF;
    }
#if HARD_SAKE_VISIBLE_SELECT_DROP_FALLBACK_ENABLE
    return ai_has_visible_sake_key_card(player);
#else
    return ON;
#endif
}

static int ai_should_cancel_sake_drop_fallback(int player)
{
    int opp = 1 - player;

    if (!ai_should_force_stenv_sake_select_drop(player)) {
        return OFF;
    }
    return ai_player_invent_has_card_id(opp, AI_CARD_ID_SAKURA_GOKOU) && ai_player_invent_has_card_id(opp, AI_CARD_ID_MOON_GOKOU);
}

static const char* ai_model_name(int model)
{
    switch (model) {
        case AI_MODEL_NORMAL: return "Normal";
        case AI_MODEL_HARD: return "Hard";
        case AI_MODEL_STENV: return "Stenv";
        case AI_MODEL_MCENV: return "Mcenv";
        default: return "Unknown";
    }
}

static const char* ai_card_type_name(int type)
{
    switch (type) {
        case CARD_TYPE_KASU: return "KASU";
        case CARD_TYPE_TAN: return "TAN";
        case CARD_TYPE_TANE: return "TANE";
        case CARD_TYPE_GOKOU: return "GOKOU";
        default: return "UNKNOWN";
    }
}

static void ai_watch_log_card_index_legend(void)
{
#if WATCH_TRACE_ENABLE
    static const char* month_names[12] = {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec",
    };

    ai_putlog("[CARD-INDEX] format=index:Month-Type month=(index/4)+1 type=KASU|TAN|TANE|GOKOU");
    for (int month = 0; month < 12; month++) {
        int base = month * 4;
        ai_putlog("[CARD-INDEX] %s: %d=%s %d=%s %d=%s %d=%s",
                  month_names[month],
                  base + 0, ai_card_type_name(card_types[base + 0]),
                  base + 1, ai_card_type_name(card_types[base + 1]),
                  base + 2, ai_card_type_name(card_types[base + 2]),
                  base + 3, ai_card_type_name(card_types[base + 3]));
    }
#endif
}

void ai_putlog_raw(const char* s)
{
    int len;

    if (!s) {
        return;
    }

    len = vgs_strlen(s);
    if (len >= (int)sizeof(g_ai_log_buffer) - 1) {
        len = (int)sizeof(g_ai_log_buffer) - 2;
    }

    vgs_memcpy(g_ai_log_buffer, s, len);
    if (len < 1 || g_ai_log_buffer[len - 1] != '\n') {
        g_ai_log_buffer[len++] = '\n';
    }
    g_ai_log_buffer[len] = '\0';

    if (!native_ai_available()) {
        if (len > 0 && g_ai_log_buffer[len - 1] == '\n') {
            g_ai_log_buffer[len - 1] = '\0';
        }
        vgs_putlog("%s", g_ai_log_buffer);
        return;
    }

#ifndef VGS_HOST_SIM
    *((volatile uint32_t*)OUT_WATCH_LOG_ADDR) = (uint32_t)g_ai_log_buffer;
    *((volatile uint32_t*)OUT_WATCH_LOG_CONTROL) = (uint32_t)((WATCH_LOG_COMMAND_WRITE << 16) | (len & 0xFFFF));
#endif
}

void ai_putlog(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    ai_log_format(g_ai_log_buffer, sizeof(g_ai_log_buffer), fmt, ap);
    va_end(ap);
    ai_putlog_raw(g_ai_log_buffer);
}

void ai_putlog_env(const char* fmt, ...)
{
    va_list ap;
    char body[768];
    int env_p1;
    int env_cpu;

    va_start(ap, fmt);
    ai_log_format(body, sizeof(body), fmt, ap);
    va_end(ap);

    env_p1 = ai_env_score(0);
    env_cpu = ai_env_score(1);
    ai_putlog("%s (Env: P1=%d, CPU=%d)", body, env_p1, env_cpu);
}

void ai_watch_log_begin(void)
{
    if (!g.auto_play) {
        return;
    }

    g_ai_watch_min_action[0][0] = '\0';
    g_ai_watch_min_action[1][0] = '\0';
    g_ai_watch_min_koikoi[0][0] = '\0';
    g_ai_watch_min_koikoi[1][0] = '\0';
    g_ai_last_think_extra[0][0] = '\0';
    g_ai_last_think_extra[1][0] = '\0';
    g_ai_watch_turn[0] = 0;
    g_ai_watch_turn[1] = 0;
    vgs_memset(g_ai_watch_after_opp, 0, sizeof(g_ai_watch_after_opp));

    if (native_ai_available()) {
#ifndef VGS_HOST_SIM
        *((volatile uint32_t*)OUT_WATCH_LOG_CONTROL) = (uint32_t)(WATCH_LOG_COMMAND_BEGIN << 16);
#endif
    }

    ai_putlog("Watch Start: P1=%s CPU=%s rounds=%d", ai_model_name(g.ai_model[0]), ai_model_name(g.ai_model[1]), g.round_max);
    ai_watch_log_card_index_legend();
}

void ai_watch_log_end(void)
{
    const int p1 = g.total_score[0];
    const int cpu = g.total_score[1];
    const char* winner = "Draw";
    int winner_index = -1;

    if (!g.auto_play) {
        return;
    }

    if (p1 > cpu) {
        winner = "P1";
        winner_index = 0;
    } else if (cpu > p1) {
        winner = "CPU";
        winner_index = 1;
    }

    ai_putlog("============================");
    ai_putlog("Game Finished: rounds=%d", g.round_max);
    ai_putlog("Final Score: P1=%d CPU=%d", p1, cpu);
    ai_putlog("Winner: %s <%s>", winner, winner_index < 0 ? "n/a" : ai_model_name(g.ai_model[winner_index]));
    ai_putlog("============================");

    if (native_ai_available()) {
#ifndef VGS_HOST_SIM
        *((volatile uint32_t*)OUT_WATCH_LOG_CONTROL) = (uint32_t)(WATCH_LOG_COMMAND_END << 16);
#endif
    }
}

void ai_watch_min_set_action(int player, const char* fmt, ...)
{
    va_list ap;

    if (player < 0 || player > 1 || !fmt) {
        return;
    }
    va_start(ap, fmt);
    ai_log_format(g_ai_watch_min_action[player], sizeof(g_ai_watch_min_action[player]), fmt, ap);
    va_end(ap);
}

void ai_watch_min_import_action(int player, const char* s)
{
    int i;

    if (player < 0 || player > 1) {
        return;
    }
    if (!s) {
        g_ai_watch_min_action[player][0] = '\0';
        return;
    }
    for (i = 0; i < (int)sizeof(g_ai_watch_min_action[player]) - 1 && s[i]; i++) {
        g_ai_watch_min_action[player][i] = s[i];
    }
    g_ai_watch_min_action[player][i] = '\0';
}

const char* ai_watch_min_get_action(int player)
{
    if (player < 0 || player > 1) {
        return "";
    }
    return g_ai_watch_min_action[player];
}

void ai_watch_min_clear_action(int player)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_watch_min_action[player][0] = '\0';
}

void ai_watch_min_emit_action(int player)
{
    if (player < 0 || player > 1) {
        return;
    }
    if (!g_ai_watch_min_action[player][0]) {
        return;
    }
    ai_putlog_raw(g_ai_watch_min_action[player]);
    ai_watch_min_clear_action(player);
}

void ai_watch_min_set_koikoi(int player, const char* fmt, ...)
{
    va_list ap;

    if (player < 0 || player > 1 || !fmt) {
        return;
    }
    va_start(ap, fmt);
    ai_log_format(g_ai_watch_min_koikoi[player], sizeof(g_ai_watch_min_koikoi[player]), fmt, ap);
    va_end(ap);
}

void ai_watch_min_emit_koikoi(int player)
{
    if (player < 0 || player > 1) {
        return;
    }
    if (!g_ai_watch_min_koikoi[player][0]) {
        return;
    }
    ai_putlog_raw(g_ai_watch_min_koikoi[player]);
    g_ai_watch_min_koikoi[player][0] = '\0';
}

static int combo7_is_high_role_wid(int wid)
{
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

static int combo7_is_light_wid(int wid)
{
    return (wid == WID_SANKOU || wid == WID_SHIKOU || wid == WID_AME_SHIKOU || wid == WID_GOKOU) ? ON : OFF;
}

static int combo7_roles_conflict(int a, int b)
{
    if (a < 0 || b < 0) {
        return OFF;
    }
    if (a == b) {
        return ON;
    }
    if (combo7_is_light_wid(a) && combo7_is_light_wid(b)) {
        return ON;
    }
    if ((a == WID_ISC && b == WID_TANE) || (a == WID_TANE && b == WID_ISC)) {
        return ON;
    }
    if ((a == WID_DBTAN && (b == WID_AKATAN || b == WID_AOTAN || b == WID_TAN)) ||
        (b == WID_DBTAN && (a == WID_AKATAN || a == WID_AOTAN || a == WID_TAN))) {
        return ON;
    }
    if (((a == WID_AKATAN || a == WID_AOTAN) && b == WID_TAN) || ((b == WID_AKATAN || b == WID_AOTAN) && a == WID_TAN)) {
        return ON;
    }
    if ((a == WID_AKATAN && b == WID_AOTAN) || (a == WID_AOTAN && b == WID_AKATAN)) {
        return ON;
    }
    return OFF;
}

static int combo7_eval_better(const AiCombo7Eval* lhs, const AiCombo7Eval* rhs)
{
    if (!lhs) {
        return OFF;
    }
    if (!rhs || rhs->combo_count <= 0) {
        return lhs->combo_count > 0 ? ON : OFF;
    }
    if (lhs->value != rhs->value) {
        return lhs->value > rhs->value ? ON : OFF;
    }
    if (lhs->combo_reach != rhs->combo_reach) {
        return lhs->combo_reach > rhs->combo_reach ? ON : OFF;
    }
    if (lhs->combo_delay != rhs->combo_delay) {
        return lhs->combo_delay < rhs->combo_delay ? ON : OFF;
    }
    if (lhs->combo_score_sum != rhs->combo_score_sum) {
        return lhs->combo_score_sum > rhs->combo_score_sum ? ON : OFF;
    }
    return lhs->combo_count < rhs->combo_count ? ON : OFF;
}

static int combo7_compose_reach(int current, int next)
{
    int value;

    if (current <= 0 || next <= 0) {
        return 0;
    }
    // 6->7 補完は積より緩く、太い本線を潰しすぎない。
    value = (current * 2 + next + 1) / 3;
    if (value > 95) {
        value = 95;
    }
    return value;
}

static int combo7_top_plus_one_wids(const StrategyData* data, const int* used_wids, int used_count, int* out_wids, int cap)
{
    static const int plus_one_roles[] = {
        WID_TANE,
        WID_TAN,
        WID_KASU,
    };
    int best_wids[3];
    int best_scores[3];
    int best_reaches[3];
    int best_delays[3];
    int count = 0;

    if (!data || !out_wids || cap <= 0) {
        return 0;
    }
    for (int i = 0; i < cap && i < 3; i++) {
        best_wids[i] = -1;
        best_scores[i] = -1;
        best_reaches[i] = 0;
        best_delays[i] = 99;
    }
    for (int i = 0; i < (int)(sizeof(plus_one_roles) / sizeof(plus_one_roles[0])); i++) {
        int wid = plus_one_roles[i];
        int score;
        int conflict = OFF;
        if (data->reach[wid] <= 0 || data->score[wid] <= 0 || data->delay[wid] >= 99) {
            continue;
        }
        for (int j = 0; j < used_count; j++) {
            if (combo7_roles_conflict(wid, used_wids[j])) {
                conflict = ON;
                break;
            }
        }
        if (conflict) {
            continue;
        }
        score = (data->reach[wid] * 100) / (data->delay[wid] + 1);
        for (int slot = 0; slot < cap && slot < 3; slot++) {
            if (score > best_scores[slot] || (score == best_scores[slot] && data->reach[wid] > best_reaches[slot]) ||
                (score == best_scores[slot] && data->reach[wid] == best_reaches[slot] && data->delay[wid] < best_delays[slot])) {
                for (int move = (cap < 3 ? cap : 3) - 1; move > slot; move--) {
                    best_wids[move] = best_wids[move - 1];
                    best_scores[move] = best_scores[move - 1];
                    best_reaches[move] = best_reaches[move - 1];
                    best_delays[move] = best_delays[move - 1];
                }
                best_wids[slot] = wid;
                best_scores[slot] = score;
                best_reaches[slot] = data->reach[wid];
                best_delays[slot] = data->delay[wid];
                break;
            }
        }
    }
    for (int i = 0; i < cap && i < 3; i++) {
        out_wids[i] = best_wids[i];
        if (best_wids[i] >= 0) {
            count++;
        }
    }
    return count;
}

static void combo7_try_candidate(int round_score, const StrategyData* data, const int* wids, int count, AiCombo7Eval* best)
{
    AiCombo7Eval candidate;
    int used_count = count;
    int is_six_to_seven_fill = OFF;

    if (!data || !wids || count < 1 || count > 3 || !best) {
        return;
    }

    vgs_memset(&candidate, 0, sizeof(candidate));
    candidate.combo_delay = 0;
    candidate.combo_reach = 100;
    candidate.plus1_rank = 0;
    for (int i = 0; i < 3; i++) {
        candidate.wids[i] = -1;
    }

    for (int i = 0; i < count; i++) {
        int wid = wids[i];
        if (wid < 0 || wid >= WINNING_HAND_MAX) {
            return;
        }
        if (data->reach[wid] <= 0 || data->score[wid] <= 0 || data->delay[wid] >= 99) {
            return;
        }
        for (int j = 0; j < i; j++) {
            if (combo7_roles_conflict(wid, wids[j])) {
                return;
            }
        }
        candidate.combo_score_sum += data->score[wid];
        if (candidate.combo_delay < data->delay[wid]) {
            candidate.combo_delay = data->delay[wid];
        }
        if (i == 0) {
            candidate.combo_reach = data->reach[wid];
        } else {
            candidate.combo_reach = combo7_compose_reach(candidate.combo_reach, data->reach[wid]);
        }
        candidate.wids[i] = wid;
    }

    if (round_score + candidate.combo_score_sum == 6 && used_count < 3) {
        int plus1_wids[2];
        int plus1_count = combo7_top_plus_one_wids(data, candidate.wids, used_count, plus1_wids, 2);
        for (int p = 0; p < plus1_count; p++) {
            AiCombo7Eval filled = candidate;
            int plus1 = plus1_wids[p];
            if (plus1 < 0) {
                continue;
            }
            filled.wids[used_count] = plus1;
            filled.combo_score_sum += 1;
            filled.combo_reach = combo7_compose_reach(filled.combo_reach, data->reach[plus1]);
            if (filled.combo_delay < data->delay[plus1]) {
                filled.combo_delay = data->delay[plus1];
            }
            if (data->delay[plus1] <= 2 && filled.combo_delay > 0) {
                filled.combo_delay--;
            }
            filled.combo_count = used_count + 1;
            filled.plus1_rank = p + 1;
            filled.value = (filled.combo_reach * 1200) / (filled.combo_delay + 1);
            filled.value += filled.combo_score_sum * 800;
            filled.value += 2400;
            filled.value *= 2;
            if (data->koikoi_opp == ON) {
                filled.value = (filled.value * 60) / 100;
            } else if (data->opponent_win_x2 == ON) {
                filled.value = (filled.value * 80) / 100;
            }
            if (filled.value > 0 && combo7_eval_better(&filled, best)) {
                *best = filled;
            }
        }
        is_six_to_seven_fill = ON;
    }

    if (is_six_to_seven_fill) {
        return;
    }
    if (round_score + candidate.combo_score_sum < 7) {
        return;
    }

    candidate.combo_count = used_count;
    candidate.value = (candidate.combo_reach * 1200) / (candidate.combo_delay + 1);
    candidate.value += candidate.combo_score_sum * 800;
    if (is_six_to_seven_fill) {
        candidate.value += 2400;
    }
    candidate.value *= 2;
    if (data->koikoi_opp == ON) {
        candidate.value = (candidate.value * 60) / 100;
    } else if (data->opponent_win_x2 == ON) {
        candidate.value = (candidate.value * 80) / 100;
    }
    if (candidate.value <= 0) {
        return;
    }
    if (combo7_eval_better(&candidate, best)) {
        *best = candidate;
    }
}

void ai_eval_combo7(int player, int round_score, const StrategyData* data, AiCombo7Eval* out_eval)
{
    static const int high_roles[] = {
        WID_GOKOU,
        WID_SHIKOU,
        WID_AME_SHIKOU,
        WID_SANKOU,
        WID_HANAMI,
        WID_TSUKIMI,
        WID_ISC,
        WID_DBTAN,
        WID_AKATAN,
        WID_AOTAN,
    };
    static const int plus_one_roles[] = {
        WID_TANE,
        WID_TAN,
        WID_KASU,
    };

    (void)player;

    if (!out_eval) {
        return;
    }

    vgs_memset(out_eval, 0, sizeof(*out_eval));
    out_eval->combo_delay = 99;
    for (int i = 0; i < 3; i++) {
        out_eval->wids[i] = -1;
    }
    if (!data || round_score >= 7) {
        return;
    }

    for (int i = 0; i < (int)(sizeof(high_roles) / sizeof(high_roles[0])); i++) {
        int a = high_roles[i];
        if (!combo7_is_high_role_wid(a)) {
            continue;
        }
        {
            int single[1] = {a};
            combo7_try_candidate(round_score, data, single, 1, out_eval);
        }
        for (int j = i + 1; j < (int)(sizeof(high_roles) / sizeof(high_roles[0])); j++) {
            int pair[2] = {a, high_roles[j]};
            combo7_try_candidate(round_score, data, pair, 2, out_eval);
        }
        for (int j = 0; j < (int)(sizeof(plus_one_roles) / sizeof(plus_one_roles[0])); j++) {
            int pair[2] = {a, plus_one_roles[j]};
            int triple[3] = {a, -1, -1};
            combo7_try_candidate(round_score, data, pair, 2, out_eval);
            for (int k = j + 1; k < (int)(sizeof(plus_one_roles) / sizeof(plus_one_roles[0])); k++) {
                triple[1] = plus_one_roles[j];
                triple[2] = plus_one_roles[k];
                combo7_try_candidate(round_score, data, triple, 3, out_eval);
            }
        }
    }
    for (int i = 0; i < (int)(sizeof(plus_one_roles) / sizeof(plus_one_roles[0])); i++) {
        int single[1] = {plus_one_roles[i]};
        combo7_try_candidate(round_score, data, single, 1, out_eval);
    }
}

/**
 * @brief AI思考開始
 */
void ai_think_start(void)
{
    g.ai_thinking = 0;
    g.ai_think_end = OFF;
}

/**
 * @brief AI思考終了
 */
void ai_think_end(void)
{
    g.ai_thinking = 0;
    g.ai_think_end = ON;
}

/**
 * @brief AI思考フレーム数をインクリメントしてから vsync
 */
void ai_vsync(void)
{
    g.ai_thinking++;
    if (g.ai_thinking % 3) {
        vsync();
    } else {
        ; // 3回に1回はvsyncスキップ
    }
}

void ai_debug_metrics_reset(void)
{
    vgs_memset(&g_ai_debug_metrics, 0, sizeof(g_ai_debug_metrics));
    vgs_memset(g_ai_last_think_extra, 0, sizeof(g_ai_last_think_extra));
    g_ai_round_initial_sake[0] = OFF;
    g_ai_round_initial_sake[1] = OFF;
    g_ai_debug_metrics.action_mc_qdiff_min[0] = INT_MAX;
    g_ai_debug_metrics.action_mc_qdiff_min[1] = INT_MAX;
    g_ai_debug_metrics.action_mc_qdiff_max[0] = INT_MIN;
    g_ai_debug_metrics.action_mc_qdiff_max[1] = INT_MIN;
    g_ai_debug_metrics.action_mc_best_min_gap_before[0] = INT_MAX;
    g_ai_debug_metrics.action_mc_best_min_gap_before[1] = INT_MAX;
    g_ai_debug_metrics.action_mc_best_min_gap_after[0] = INT_MAX;
    g_ai_debug_metrics.action_mc_best_min_gap_after[1] = INT_MAX;
    g_ai_debug_metrics.env_mc_e_min[0] = INT_MAX;
    g_ai_debug_metrics.env_mc_e_min[1] = INT_MAX;
    g_ai_debug_metrics.env_mc_e_max[0] = INT_MIN;
    g_ai_debug_metrics.env_mc_e_max[1] = INT_MIN;
    g_ai_debug_metrics.sevenline_margin_min[0] = INT_MAX;
    g_ai_debug_metrics.sevenline_margin_min[1] = INT_MAX;
    g_ai_debug_metrics.sevenline_margin_max[0] = INT_MIN;
    g_ai_debug_metrics.sevenline_margin_max[1] = INT_MIN;
    g_ai_debug_metrics.sevenline_bonus_min[0] = INT_MAX;
    g_ai_debug_metrics.sevenline_bonus_min[1] = INT_MAX;
    g_ai_debug_metrics.sevenline_bonus_max[0] = INT_MIN;
    g_ai_debug_metrics.sevenline_bonus_max[1] = INT_MIN;
    g_ai_debug_metrics.sevenline_d1_gap_min_before[0] = INT_MAX;
    g_ai_debug_metrics.sevenline_d1_gap_min_before[1] = INT_MAX;
    g_ai_debug_metrics.sevenline_d1_gap_max_before[0] = INT_MIN;
    g_ai_debug_metrics.sevenline_d1_gap_max_before[1] = INT_MIN;
    g_ai_debug_metrics.sevenline_d1_gap_min_after[0] = INT_MAX;
    g_ai_debug_metrics.sevenline_d1_gap_min_after[1] = INT_MAX;
    g_ai_debug_metrics.sevenline_d1_gap_max_after[0] = INT_MIN;
    g_ai_debug_metrics.sevenline_d1_gap_max_after[1] = INT_MIN;
    ai_debug_reset_round_state();
}

void ai_debug_reset_round_state(void)
{
    g_ai_round_last_strategy_mode[0] = MODE_BALANCED;
    g_ai_round_last_strategy_mode[1] = MODE_BALANCED;
    g_ai_round_has_strategy_mode[0] = OFF;
    g_ai_round_has_strategy_mode[1] = OFF;
}

const AiDebugMetrics* ai_debug_metrics_get(void)
{
    return &g_ai_debug_metrics;
}

void ai_debug_set_run_context(int seed, int game_index)
{
    g_ai_debug_seed = seed;
    g_ai_debug_game_index = game_index;
}

int ai_debug_current_seed(void)
{
    return g_ai_debug_seed;
}

int ai_debug_current_game_index(void)
{
    return g_ai_debug_game_index;
}

static const char* ai_watch_side_name(int player)
{
    return player ? "CPU" : "P1";
}

static const char* ai_watch_pick_wid_name(int wid)
{
    if (wid >= 0 && wid < WINNING_HAND_MAX) {
        return winning_hands[wid].name;
    }
    return "NONE";
}

void ai_watch_prepare_turn(int player)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_watch_turn[player] = 9 - g.own[player].num;
    if (g_ai_watch_turn[player] < 1) {
        g_ai_watch_turn[player] = 1;
    }
}

int ai_watch_current_turn(int player)
{
    if (player < 0 || player > 1) {
        return 0;
    }
    if (g_ai_watch_turn[player] > 0) {
        return g_ai_watch_turn[player];
    }
    return 9 - g.own[player].num;
}

void ai_debug_note_action_mc_trigger(int player, int candidate_count)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.action_mc_trigger_count[player]++;
    if (candidate_count > 0) {
        g_ai_debug_metrics.action_mc_total_candidates[player] += candidate_count;
    }
}

void ai_debug_note_action_mc_material_trigger(int player, int material)
{
    if (player < 0 || player > 1) {
        return;
    }
    if (0 <= material && material < AI_ACTION_MC_MATERIAL_MAX) {
        g_ai_debug_metrics.action_mc_trigger_by_material[player][material]++;
    }
}

void ai_debug_note_action_mc_qdiff(int player, int qdiff)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.action_mc_qdiff_total[player] += qdiff;
    g_ai_debug_metrics.action_mc_qdiff_count[player]++;
    if (qdiff < g_ai_debug_metrics.action_mc_qdiff_min[player]) {
        g_ai_debug_metrics.action_mc_qdiff_min[player] = qdiff;
    }
    if (qdiff > g_ai_debug_metrics.action_mc_qdiff_max[player]) {
        g_ai_debug_metrics.action_mc_qdiff_max[player] = qdiff;
    }
    if (qdiff < 0) {
        g_ai_debug_metrics.action_mc_qdiff_lt0_count[player]++;
    }
    if (qdiff <= -500) {
        g_ai_debug_metrics.action_mc_qdiff_le_500_count[player]++;
    }
    if (qdiff <= -1000) {
        g_ai_debug_metrics.action_mc_qdiff_le_1000_count[player]++;
    }
    if (qdiff <= -2000) {
        g_ai_debug_metrics.action_mc_qdiff_le_2000_count[player]++;
    }
    if (qdiff <= -4000) {
        g_ai_debug_metrics.action_mc_qdiff_le_4000_count[player]++;
    }
}

void ai_debug_note_action_mc_penalty_applied(int player, int material)
{
    if (player < 0 || player > 1) {
        return;
    }
    (void)material;
    g_ai_debug_metrics.action_mc_penalty_applied_count[player]++;
}

void ai_debug_note_action_mc_changed_choice(int player, int material)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.action_mc_changed_choice_count[player]++;
    if (0 <= material && material < AI_ACTION_MC_MATERIAL_MAX) {
        g_ai_debug_metrics.action_mc_changed_by_material[player][material]++;
    }
}

void ai_debug_note_action_mc_applied_best(int player)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.action_mc_applied_best_count[player]++;
}

void ai_debug_note_action_mc_best_gap(int player, int gap_before, int gap_after)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.action_mc_best_gap_count[player]++;
    if (gap_before < g_ai_debug_metrics.action_mc_best_min_gap_before[player]) {
        g_ai_debug_metrics.action_mc_best_min_gap_before[player] = gap_before;
    }
    if (gap_after < g_ai_debug_metrics.action_mc_best_min_gap_after[player]) {
        g_ai_debug_metrics.action_mc_best_min_gap_after[player] = gap_after;
    }
}

void ai_debug_note_month_lock(int player, int changed_choice)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.month_lock_trigger_count[player]++;
    if (changed_choice) {
        g_ai_debug_metrics.month_lock_changed_choice_count[player]++;
    }
}

void ai_debug_note_month_lock_hidden_key(int player, int changed_choice)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.month_lock_hidden_key_trigger_count[player]++;
    if (changed_choice) {
        g_ai_debug_metrics.month_lock_hidden_key_changed_choice_count[player]++;
    }
}

void ai_debug_note_month_lock_hidden_deny(int player, int changed_choice)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.month_lock_hidden_deny_trigger_count[player]++;
    if (changed_choice) {
        g_ai_debug_metrics.month_lock_hidden_deny_changed_choice_count[player]++;
    }
}

void ai_debug_note_overpay_delay(int player, int changed_choice)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.overpay_delay_penalty_applied_count[player]++;
    if (changed_choice) {
        g_ai_debug_metrics.overpay_delay_changed_choice_count[player]++;
    }
}

void ai_debug_note_certain_seven_plus(int player, int changed_choice)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.certain_seven_plus_trigger_count[player]++;
    if (changed_choice) {
        g_ai_debug_metrics.certain_seven_plus_changed_choice_count[player]++;
    }
}

void ai_debug_note_sevenline_trigger(int player, int margin, int reach, int delay, int bonus, int low_reach, int press_applied)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.sevenline_trigger_count[player]++;
    g_ai_debug_metrics.sevenline_margin_total[player] += margin;
    g_ai_debug_metrics.sevenline_margin_count[player]++;
    if (margin < g_ai_debug_metrics.sevenline_margin_min[player]) {
        g_ai_debug_metrics.sevenline_margin_min[player] = margin;
    }
    if (margin > g_ai_debug_metrics.sevenline_margin_max[player]) {
        g_ai_debug_metrics.sevenline_margin_max[player] = margin;
    }
    if (margin <= 0) {
        g_ai_debug_metrics.sevenline_margin_hist[player][0]++;
    } else if (margin == 1) {
        g_ai_debug_metrics.sevenline_margin_hist[player][1]++;
        if (reach < 10) {
            g_ai_debug_metrics.sevenline_margin1_reach_hist[player][0]++;
        } else if (reach < 30) {
            g_ai_debug_metrics.sevenline_margin1_reach_hist[player][1]++;
        } else {
            g_ai_debug_metrics.sevenline_margin1_reach_hist[player][2]++;
        }
    } else {
        g_ai_debug_metrics.sevenline_margin_hist[player][2]++;
    }
    g_ai_debug_metrics.sevenline_bonus_total[player] += bonus;
    g_ai_debug_metrics.sevenline_bonus_count[player]++;
    if (bonus < g_ai_debug_metrics.sevenline_bonus_min[player]) {
        g_ai_debug_metrics.sevenline_bonus_min[player] = bonus;
    }
    if (bonus > g_ai_debug_metrics.sevenline_bonus_max[player]) {
        g_ai_debug_metrics.sevenline_bonus_max[player] = bonus;
    }
    if (low_reach) {
        g_ai_debug_metrics.sevenline_low_reach_count[player]++;
    }
    if (press_applied) {
        g_ai_debug_metrics.sevenline_press_applied_count[player]++;
    }
    if (delay <= 1) {
        g_ai_debug_metrics.sevenline_delay_hist[player][0]++;
    } else if (delay == 2) {
        g_ai_debug_metrics.sevenline_delay_hist[player][1]++;
    } else if (delay == 3) {
        g_ai_debug_metrics.sevenline_delay_hist[player][2]++;
    } else {
        g_ai_debug_metrics.sevenline_delay_hist[player][3]++;
    }
}

void ai_debug_note_sevenline_changed(int player, int margin, int delay)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.sevenline_changed_count[player]++;
    if (margin <= 0) {
        g_ai_debug_metrics.sevenline_changed_margin_hist[player][0]++;
    } else if (margin == 1) {
        g_ai_debug_metrics.sevenline_changed_margin_hist[player][1]++;
    } else {
        g_ai_debug_metrics.sevenline_changed_margin_hist[player][2]++;
    }
    if (delay <= 1) {
        g_ai_debug_metrics.sevenline_changed_delay_hist[player][0]++;
    } else if (delay == 2) {
        g_ai_debug_metrics.sevenline_changed_delay_hist[player][1]++;
    } else if (delay == 3) {
        g_ai_debug_metrics.sevenline_changed_delay_hist[player][2]++;
    } else {
        g_ai_debug_metrics.sevenline_changed_delay_hist[player][3]++;
    }
}

void ai_debug_note_sevenline_d1_loss_reason(int player, int reason)
{
    if (player < 0 || player > 1) {
        return;
    }
    if (reason >= 0 && reason < AI_SEVENLINE_D1_LOSS_MAX) {
        g_ai_debug_metrics.sevenline_d1_loss_reason_count[player][reason]++;
    }
}

void ai_debug_note_sevenline_d1_override(int player, int applied, int changed)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.sevenline_d1_override_trigger_count[player]++;
    if (applied) {
        g_ai_debug_metrics.sevenline_d1_override_applied_count[player]++;
    }
    if (changed) {
        g_ai_debug_metrics.sevenline_d1_override_changed_count[player]++;
    }
}

void ai_debug_note_sevenline_d1_override_blocked(int player, int reason)
{
    if (player < 0 || player > 1) {
        return;
    }
    if (reason >= 0 && reason < AI_SEVENLINE_D1_OVERRIDE_BLOCK_MAX) {
        g_ai_debug_metrics.sevenline_d1_override_blocked_count[player][reason]++;
    }
}

void ai_debug_note_sevenline_d1_gap(int player, int gap_before, int gap_after)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.sevenline_d1_gap_total_before[player] += gap_before;
    g_ai_debug_metrics.sevenline_d1_gap_total_after[player] += gap_after;
    g_ai_debug_metrics.sevenline_d1_gap_count[player]++;
    if (gap_before < g_ai_debug_metrics.sevenline_d1_gap_min_before[player]) {
        g_ai_debug_metrics.sevenline_d1_gap_min_before[player] = gap_before;
    }
    if (gap_before > g_ai_debug_metrics.sevenline_d1_gap_max_before[player]) {
        g_ai_debug_metrics.sevenline_d1_gap_max_before[player] = gap_before;
    }
    if (gap_after < g_ai_debug_metrics.sevenline_d1_gap_min_after[player]) {
        g_ai_debug_metrics.sevenline_d1_gap_min_after[player] = gap_after;
    }
    if (gap_after > g_ai_debug_metrics.sevenline_d1_gap_max_after[player]) {
        g_ai_debug_metrics.sevenline_d1_gap_max_after[player] = gap_after;
    }
}

void ai_debug_note_strategy_mode(int player, int mode)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_round_last_strategy_mode[player] = mode;
    g_ai_round_has_strategy_mode[player] = ON;
    switch (mode) {
        case MODE_GREEDY:
            g_ai_debug_metrics.mode_greedy_count[player]++;
            break;
        case MODE_DEFENSIVE:
            g_ai_debug_metrics.mode_defensive_count[player]++;
            break;
        default:
            g_ai_debug_metrics.mode_balanced_count[player]++;
            break;
    }
}

int ai_debug_get_round_last_strategy_mode(int player, int* has_mode)
{
    if (player < 0 || player > 1) {
        if (has_mode) {
            *has_mode = OFF;
        }
        return MODE_BALANCED;
    }
    if (has_mode) {
        *has_mode = g_ai_round_has_strategy_mode[player];
    }
    return g_ai_round_last_strategy_mode[player];
}

void ai_debug_note_combo7(int player, int mode, int changed_choice)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.combo7_trigger_count[player]++;
    if (changed_choice) {
        g_ai_debug_metrics.combo7_changed_choice_count[player]++;
    }
    switch (mode) {
        case MODE_GREEDY:
            g_ai_debug_metrics.combo7_greedy_trigger_count[player]++;
            if (changed_choice) {
                g_ai_debug_metrics.combo7_greedy_changed_choice_count[player]++;
            }
            break;
        case MODE_DEFENSIVE:
            g_ai_debug_metrics.combo7_defensive_trigger_count[player]++;
            if (changed_choice) {
                g_ai_debug_metrics.combo7_defensive_changed_choice_count[player]++;
            }
            break;
        default:
            g_ai_debug_metrics.combo7_balanced_trigger_count[player]++;
            if (changed_choice) {
                g_ai_debug_metrics.combo7_balanced_changed_choice_count[player]++;
            }
            break;
    }
}

void ai_debug_note_combo7_margin(int player, int margin, int flipped)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.combo7_margin_total[player] += margin;
    g_ai_debug_metrics.combo7_margin_count[player]++;
    if (flipped) {
        g_ai_debug_metrics.combo7_margin_flip_count[player]++;
    }
    if (margin < 5000) {
        g_ai_debug_metrics.combo7_margin_lt_5k_count[player]++;
    }
    if (margin < 10000) {
        g_ai_debug_metrics.combo7_margin_lt_10k_count[player]++;
    }
    if (margin < 20000) {
        g_ai_debug_metrics.combo7_margin_lt_20k_count[player]++;
    }
}

void ai_debug_note_best_plus1_used_rank(int player, int rank)
{
    if (player < 0 || player > 1 || rank < 1 || rank > 2) {
        return;
    }
    g_ai_debug_metrics.best_plus1_used_rank_count[player][rank - 1]++;
}

void ai_debug_note_koikoi_base6_push(int player, int applied, int flipped)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.koikoi_base6_push_trigger_count[player]++;
    if (applied) {
        g_ai_debug_metrics.koikoi_base6_push_applied_count[player]++;
    }
    if (flipped) {
        g_ai_debug_metrics.koikoi_base6_push_flipped_count[player]++;
    }
}

void ai_debug_note_koikoi_locked_threshold(int player, int applied)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.koikoi_locked_threshold_trigger_count[player]++;
    if (applied) {
        g_ai_debug_metrics.koikoi_locked_threshold_applied_count[player]++;
    }
}

void ai_debug_note_keytarget_expose(int player, int changed_choice)
{
    if (player < 0 || player > 1) {
        return;
    }

    g_ai_debug_metrics.keytarget_expose_trigger_count[player]++;
    if (changed_choice) {
        g_ai_debug_metrics.keytarget_expose_changed_choice_count[player]++;
    }
}

void ai_debug_note_env_mc_trigger(int player, int mode)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.env_mc_trigger_count[player]++;
    switch (mode) {
        case MODE_GREEDY:
            g_ai_debug_metrics.env_mc_mode_trigger_count[player][0]++;
            break;
        case MODE_DEFENSIVE:
            g_ai_debug_metrics.env_mc_mode_trigger_count[player][2]++;
            break;
        default:
            g_ai_debug_metrics.env_mc_mode_trigger_count[player][1]++;
            break;
    }
}

void ai_debug_note_env_mc_changed_choice(int player, int mode)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.env_mc_changed_choice_count[player]++;
    switch (mode) {
        case MODE_GREEDY:
            g_ai_debug_metrics.env_mc_mode_changed_choice_count[player][0]++;
            break;
        case MODE_DEFENSIVE:
            g_ai_debug_metrics.env_mc_mode_changed_choice_count[player][2]++;
            break;
        default:
            g_ai_debug_metrics.env_mc_mode_changed_choice_count[player][1]++;
            break;
    }
}

void ai_debug_note_env_mc_E(int player, int env_e)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.env_mc_e_total[player] += env_e;
    g_ai_debug_metrics.env_mc_e_count[player]++;
    if (env_e < g_ai_debug_metrics.env_mc_e_min[player]) {
        g_ai_debug_metrics.env_mc_e_min[player] = env_e;
    }
    if (env_e > g_ai_debug_metrics.env_mc_e_max[player]) {
        g_ai_debug_metrics.env_mc_e_max[player] = env_e;
    }
}

void ai_debug_note_plan_decision(int player, AiPlan plan, int reason)
{
    if (player < 0 || player > 1) {
        return;
    }
    if (plan >= 0 && plan < AI_PLAN_MAX) {
        g_ai_debug_metrics.plan_count[player][plan]++;
    }
    if (reason >= 0 && reason < AI_PLAN_REASON_MAX) {
        g_ai_debug_metrics.plan_reason_count[player][reason]++;
    }
}

void ai_debug_set_initial_sake(int player, int has_sake)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_round_initial_sake[player] = has_sake ? ON : OFF;
    if (has_sake) {
        g_ai_debug_metrics.initial_sake_round_count[player]++;
    }
}

int ai_debug_has_initial_sake(int player)
{
    if (player < 0 || player > 1) {
        return OFF;
    }
    return g_ai_round_initial_sake[player];
}

void ai_debug_note_endgame(int player, int k_end, int changed_choice, int needed)
{
    int k_bucket = -1;
    int needed_bucket = 0;

    if (player < 0 || player > 1 || k_end <= 0) {
        return;
    }
    g_ai_debug_metrics.endgame_trigger_count[player]++;
    switch (k_end) {
        case 100: k_bucket = 0; break;
        case 80: k_bucket = 1; break;
        case 40: k_bucket = 2; break;
        case 20: k_bucket = 3; break;
        default: break;
    }
    if (k_bucket >= 0) {
        g_ai_debug_metrics.endgame_k_hist[player][k_bucket]++;
    }
    if (!changed_choice) {
        return;
    }
    g_ai_debug_metrics.endgame_changed_choice_count[player]++;
    if (needed <= 0) {
        needed_bucket = 0;
    } else if (needed <= 2) {
        needed_bucket = 1;
    } else if (needed <= 5) {
        needed_bucket = 2;
    } else {
        needed_bucket = 3;
    }
    g_ai_debug_metrics.endgame_changed_needed_hist[player][needed_bucket]++;
}

void ai_set_last_think_extra(int player, const char* fmt, ...)
{
    va_list ap;

    if (player < 0 || player > 1) {
        return;
    }
    g_ai_last_think_extra[player][0] = '\0';
    if (!fmt || !*fmt) {
        return;
    }
    va_start(ap, fmt);
    ai_log_format(g_ai_last_think_extra[player], sizeof(g_ai_last_think_extra[player]), fmt, ap);
    va_end(ap);
}

void ai_set_last_think_context(int player, const StrategyData* s, int needed, int k_end, int best_immediate_gain, int second_immediate_gain,
                               int best_clinch_score, int second_clinch_score)
{
    int turn;
    int left_hand;
    int is_parent;

    if (player < 0 || player > 1 || !s) {
        ai_set_last_think_extra(player, "");
        return;
    }

    turn = ai_watch_current_turn(player);
    left_hand = g.own[player].num;
    is_parent = g.oya == player ? ON : OFF;
    ai_set_last_think_extra(
        player,
        " seed=%d round=%d/%d turn=%d dealer=%s is_parent=%d left_rounds=%d left_hand=%d score_total_p1=%d score_total_cpu=%d round_score_p1=%d round_score_cpu=%d koikoi_p1=%d koikoi_cpu=%d opponent_win_x2=%d match_diff=%d needed=%d k_end=%d imm=%d/%d clinch=%d/%d",
        ai_debug_current_seed(), g.round + 1, s->round_max, turn, ai_watch_side_name(g.oya), is_parent, s->left_rounds, left_hand, g.total_score[0], g.total_score[1],
        g.stats[0].score, g.stats[1].score, g.koikoi[0], g.koikoi[1], s->opponent_win_x2, s->match_score_diff, needed, k_end, best_immediate_gain,
        second_immediate_gain, best_clinch_score, second_clinch_score);
}

const char* ai_get_last_think_extra(int player)
{
    if (player < 0 || player > 1 || !g_ai_last_think_extra[player][0]) {
        return "";
    }
    return g_ai_last_think_extra[player];
}

static void ai_watch_clear_after_opp_pending(int player)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_watch_after_opp[player].active = OFF;
}

void ai_watch_begin_after_opp_window(int player)
{
    AiWatchAfterOppPending* pending;

    if (!g.auto_play || player < 0 || player > 1) {
        return;
    }
    pending = &g_ai_watch_after_opp[player];
    pending->active = ON;
    pending->actor = player;
    pending->opponent = 1 - player;
    pending->seed = ai_debug_current_seed();
    pending->round = g.round + 1;
    pending->round_max = g.round_max;
    pending->turn = ai_watch_current_turn(player);
    pending->opponent_base_before = g.stats[1 - player].score;
}

static void ai_watch_emit_after_opp(int actor, int opponent, const char* action, int base, int x2, int wid)
{
    AiWatchAfterOppPending* pending;
    int base_delta;

    if (actor < 0 || actor > 1 || opponent < 0 || opponent > 1 || !action) {
        return;
    }
    pending = &g_ai_watch_after_opp[actor];
    if (!pending->active || pending->opponent != opponent) {
        return;
    }
    base_delta = g.stats[opponent].score - pending->opponent_base_before;
    if (base_delta < 0) {
        base_delta = 0;
    }
    ai_putlog("[AFTER-OPP] seed=%d side=%s round=%d/%d turn=%d after_opp_action=%s(base=%d,x2=%d,wid=%s) after_opp_base_delta=+%d",
              pending->seed, ai_watch_side_name(actor), pending->round, pending->round_max, pending->turn, action, base, x2, ai_watch_pick_wid_name(wid), base_delta);
    ai_watch_clear_after_opp_pending(actor);
}

void ai_watch_note_after_opp_none(int opponent)
{
    int actor = 1 - opponent;

    if (!g.auto_play) {
        return;
    }
    ai_watch_emit_after_opp(actor, opponent, "none", 0, 0, -1);
}

void ai_watch_note_after_opp_koikoi(int player, int round_score)
{
    int actor = 1 - player;
    int x2 = g.koikoi[1 - player] ? 1 : 0;

    if (!g.auto_play) {
        return;
    }
    ai_watch_emit_after_opp(actor, player, "koikoi", round_score, x2, g.stats[player].wh_count > 0 && g.stats[player].wh[0] ? g.stats[player].wh[0]->id : -1);
}

void ai_watch_note_after_opp_win(int player, int round_score)
{
    int actor = 1 - player;
    int x2 = ((g.stats[player].score >= 7) ? 1 : 0) + (g.koikoi[1 - player] ? 1 : 0);

    if (!g.auto_play) {
        return;
    }
    ai_watch_emit_after_opp(actor, player, "win", round_score, x2, g.stats[player].wh_count > 0 && g.stats[player].wh[0] ? g.stats[player].wh[0]->id : -1);
}

void ai_debug_note_phase2a_trigger(int player, AiPlan plan, AiEnvDomain domain, int reason)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.phase2a_trigger_count[player]++;
    if (plan >= 0 && plan < AI_PLAN_MAX) {
        g_ai_debug_metrics.phase2a_plan_count[player][plan]++;
    }
    if (domain >= 0 && domain < AI_ENV_DOMAIN_MAX) {
        g_ai_debug_metrics.phase2a_domain_count[player][domain]++;
    }
    if (reason >= AI_PHASE2A_REASON_7PLUS && reason <= AI_PHASE2A_REASON_DEFENCE) {
        g_ai_debug_metrics.phase2a_reason_trigger_count[player][reason]++;
        if (plan >= 0 && plan < AI_PLAN_MAX) {
            g_ai_debug_metrics.phase2a_reason_trigger_by_plan[player][plan][reason]++;
        }
    }
}

void ai_debug_note_phase2a_changed_choice(int player, AiPlan plan, int reason)
{
    if (player < 0 || player > 1) {
        return;
    }
    g_ai_debug_metrics.phase2a_changed_choice_count[player]++;
    if (reason >= AI_PHASE2A_REASON_7PLUS && reason <= AI_PHASE2A_REASON_DEFENCE) {
        g_ai_debug_metrics.phase2a_reason_changed_count[player][reason]++;
        if (plan >= 0 && plan < AI_PLAN_MAX) {
            g_ai_debug_metrics.phase2a_reason_changed_by_plan[player][plan][reason]++;
        }
    }
}

/**
 * @brief こいこいができる状態か判定
 * @param player 0: 自分, 1: CPU
 * @return ON: こいこいできる, OFF: こいこいできない
 */
int is_koikoi_enabled(int player)
{
    return !g.koikoi[0] && !g.koikoi[1] && 0 < g.own[player].num ? ON : OFF;
}

/**
 * @brief こいこいを する or しない の判断
 * @param player 0: 自分, 1: CPU
 * @param koikoi_enabled ON: こいこい可能な状態, OFF: こいこい不可の状態
 * @param round_score 現在のラウンドスコア
 * @return ON: する, OFF: しない
 */
int ai_koikoi(int player, int round_score)
{
    int result;

    if (!is_koikoi_enabled(player)) {
        return OFF; // こいこい出来ない (どちらかがこいこい中 or 手札がない)
    }
    if (g.own[player].num >= 8) {
        return ON; // 手札が8枚以上ある（あり得ないケース）
    }
    if (g.own[player].num <= 0) {
        return OFF; // 手札がもう無い（「こいこい」できない）
    }

    // 最終ラウンドの場合の特殊な判定
    if (g.round == g.round_max - 1) {
        int op_score = g.total_score[1 - player];
        int my_score = g.total_score[player];
        if (op_score < my_score + round_score) {
            return OFF; // 既に勝っているので無駄な「こいこい」をしない
        } else {
            return ON; // まだ負けているので無条件で「こいこい」する
        }
    }

    if (native_ai_execute(NATIVE_AI_COMMAND_KOIKOI, player, round_score, &result)) {
        return result;
    }

    // 以下、思考ルーチン
    return koikoi_function[g.ai_model[player]](player, round_score);
}

/**
 * @brief CPU が指定カードに対する取り札の場位置 (g.deck.cards) を選択
 * @param player 0: 自分, 1: CPU
 * @param card 対象カード
 * @return g.deck.cards のインデックス
 */
int ai_select(int player, Card* card)
{
    int result;

    if (g_ai_watch_turn[player] <= 0) {
        ai_watch_prepare_turn(player);
    }

    if (native_ai_execute(NATIVE_AI_COMMAND_SELECT, player, card ? card->id : -1, &result)) {
        return result;
    }
    return select_function[g.ai_model[player]](player, card);
}

/**
 * @brief CPU の手札 (g.own[player].cards) から切る札を選択
 * @param player 0: 自分, 1: CPU
 * @return g.own[player].cards のインデックス
 */
int ai_drop(int player)
{
    int result;

    ai_watch_prepare_turn(player);

    if (native_ai_execute(NATIVE_AI_COMMAND_DROP, player, 0, &result)) {
        return result;
    }
    return drop_function[g.ai_model[player]](player);
}

static void ai_log_append_char(char* dst, int cap, int* pos, char ch)
{
    if (*pos + 1 >= cap) {
        return;
    }
    dst[*pos] = ch;
    (*pos)++;
    dst[*pos] = '\0';
}

static void ai_log_append_text(char* dst, int cap, int* pos, const char* src)
{
    if (!src) {
        src = "(null)";
    }
    while (*src && *pos + 1 < cap) {
        dst[*pos] = *src;
        (*pos)++;
        src++;
    }
    dst[*pos] = '\0';
}

static void ai_log_format(char* dst, int cap, const char* fmt, va_list ap)
{
    int pos = 0;

    if (cap < 1) {
        return;
    }
    dst[0] = '\0';
    while (*fmt && pos + 1 < cap) {
        if (fmt[0] == '%' && fmt[1] == 'd') {
            char num[16];
            vgs_d32str(num, va_arg(ap, int32_t));
            ai_log_append_text(dst, cap, &pos, num);
            fmt += 2;
        } else if (fmt[0] == '%' && fmt[1] == 'u') {
            char num[16];
            vgs_u32str(num, va_arg(ap, uint32_t));
            ai_log_append_text(dst, cap, &pos, num);
            fmt += 2;
        } else if (fmt[0] == '%' && fmt[1] == 's') {
            ai_log_append_text(dst, cap, &pos, va_arg(ap, const char*));
            fmt += 2;
        } else {
            ai_log_append_char(dst, cap, &pos, *fmt);
            fmt++;
        }
    }
}

int ai_env_effective_category_for_player(int player, int card_index)
{
    int cat = ai_env_category_from_card(card_index);
    int opp = 1 - player;
    int has_sake = OFF;
    int has_sakura = OFF;
    int has_moon = OFF;
    int opp_light_lock_count = 0;
    int month = card_index / 4;

    if (player < 0 || player > 1) {
        return cat;
    }

    for (int i = 0; i < g.invent[opp][CARD_TYPE_TANE].num; i++) {
        Card* card = g.invent[opp][CARD_TYPE_TANE].cards[i];
        if (!card) {
            continue;
        }
        if (card->month == 8) {
            has_sake = ON;
        }
    }
    for (int i = 0; i < g.invent[opp][CARD_TYPE_GOKOU].num; i++) {
        Card* card = g.invent[opp][CARD_TYPE_GOKOU].cards[i];
        if (!card) {
            continue;
        }
        if (card->month == 2) {
            has_sakura = ON;
            opp_light_lock_count++;
        } else if (card->month == 7) {
            has_moon = ON;
            opp_light_lock_count++;
        } else if (card->month == 0 || card->month == 11) {
            opp_light_lock_count++;
        }
    }

    if (opp_light_lock_count >= 2 &&
        (month == 0 || month == 2 || month == 7 || month == 10 || month == 11) &&
        card_types[card_index] == CARD_TYPE_GOKOU) {
        return ENV_CAT_NA;
    }
    if (cat == ENV_CAT_1 && has_sake) {
        return ENV_CAT_3;
    }
    if (cat == ENV_CAT_2) {
        if (has_sakura && has_moon) {
            return ENV_CAT_12;
        }
        if (has_sakura || has_moon) {
            return ENV_CAT_11;
        }
    }
    if (cat == ENV_CAT_11 && has_sakura && has_moon) {
        return ENV_CAT_12;
    }
    if (cat == ENV_CAT_4 && ai_env_opponent_has_combo_blocker(player, ENV_CAT_4)) {
        return ENV_CAT_8;
    }
    if (cat == ENV_CAT_5 && ai_env_opponent_has_combo_blocker(player, ENV_CAT_5)) {
        return ENV_CAT_8;
    }
    if (cat == ENV_CAT_7 && ai_env_opponent_has_combo_blocker(player, ENV_CAT_7)) {
        return ENV_CAT_9;
    }
    return cat;
}

static int ai_env_opponent_has_combo_blocker(int player, int cat)
{
    int opp = 1 - player;
    CardSet* tane = &g.invent[opp][CARD_TYPE_TANE];
    CardSet* tan = &g.invent[opp][CARD_TYPE_TAN];

    if (cat == ENV_CAT_4) {
        for (int i = 0; i < tan->num; i++) {
            Card* card = tan->cards[i];
            if (card && (card->month == 0 || card->month == 1 || card->month == 2)) {
                return ON;
            }
        }
        return OFF;
    }

    if (cat == ENV_CAT_5) {
        for (int i = 0; i < tan->num; i++) {
            Card* card = tan->cards[i];
            if (card && (card->month == 5 || card->month == 8 || card->month == 9)) {
                return ON;
            }
        }
        return OFF;
    }

    if (cat == ENV_CAT_7) {
        for (int i = 0; i < tane->num; i++) {
            Card* card = tane->cards[i];
            if (card && (card->month == 5 || card->month == 6 || card->month == 9)) {
                return ON;
            }
        }
        return OFF;
    }

    return OFF;
}

static int ai_env_add_card_value(int total, int player, Card* card, int multiplier)
{
    int card_index;
    int cat;

    if (!card || multiplier == 0) {
        return total;
    }

    card_index = card_global_index(card);
    if (card_index < 0) {
        return total;
    }

    cat = ai_env_effective_category_for_player(player, card_index);
    if (cat < 0 || cat >= ENV_CAT_MAX) {
        return total;
    }
    return total + ENV_CAT_BASE[cat] * multiplier;
}

static int ai_env_add_set_value(int total, int player, CardSet* set, int multiplier, int count)
{
    if (!set) {
        return total;
    }
    for (int i = 0; i < count; i++) {
        total = ai_env_add_card_value(total, player, set->cards[i], multiplier);
    }
    return total;
}
