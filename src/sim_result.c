#include <stdio.h>

#include "ai.h"

typedef struct {
    int wins[2];
    int draws;
    int dealer_rounds[2];
    int dealer_wins[2];
    int player_rounds[2];
    int player_wins[2];
    int score_diff_sum[2];
    int win_score_sum[2];
    int round_wins[2][12];
    int round_win_score_sum[2][12];
    int reason_7plus[2];
    int reason_opponent_koikoi[2];
    int koikoi_count[2];
    int koikoi_success[2];
    int koikoi_failed[2];
    int initial_sake_round_win_count[2];
    int initial_sake_round_win_score_sum[2];
    int initial_sake_round_koikoi_count[2];
    int initial_sake_round_koikoi_win_count[2];
    int initial_sake_round_koikoi_up_sum[2];
    int bias_round_result[3][3];
    int yaku_counts[WINNING_HAND_MAX];
    int yaku_total_count;
} SimMetrics;

const char* ai_name(int model);
static SimMetrics g_sim_metrics;

void sim_metrics_reset(void)
{
    vgs_memset(&g_sim_metrics, 0, sizeof(g_sim_metrics));
}

const SimMetrics* sim_metrics_get(void)
{
    return &g_sim_metrics;
}

enum {
    BIAS_RESULT_WIN = 0,
    BIAS_RESULT_LOSE = 1,
    BIAS_RESULT_DRAW = 2,
};

static void note_bias_round_result(int player, int result)
{
    int has_mode = OFF;
    int mode = ai_debug_get_round_last_strategy_mode(player, &has_mode);

    if (!has_mode) {
        return;
    }
    if (mode < MODE_BALANCED || mode > MODE_DEFENSIVE) {
        return;
    }
    if (result < BIAS_RESULT_WIN || result > BIAS_RESULT_DRAW) {
        return;
    }
    g_sim_metrics.bias_round_result[mode][result]++;
}

static const char* multiplier_reason(int player)
{
    if (g.stats[player].score >= 7 && g.koikoi[1 - player]) {
        return "7+ and opponent_koikoi";
    }
    if (g.stats[player].score >= 7) {
        return "7+";
    }
    if (g.koikoi[1 - player]) {
        return "opponent_koikoi";
    }
    return "none";
}

static int round_multiplier(int player)
{
    int mul = 1;
    if (g.stats[player].score >= 7) {
        mul *= 2;
    }
    if (g.koikoi[1 - player]) {
        mul *= 2;
    }
    return mul;
}

static void note_yaku_counts(int player)
{
    for (int i = 0; i < g.stats[player].wh_count; i++) {
        WinningHand* wh = g.stats[player].wh[i];
        if (!wh) {
            continue;
        }
        if (wh->id < 0 || wh->id >= WINNING_HAND_MAX) {
            continue;
        }
        g_sim_metrics.yaku_counts[wh->id]++;
        g_sim_metrics.yaku_total_count++;
    }
}

static int is_draw_state(void)
{
    for (int i = 0; i < 2; i++) {
        if (g.koikoi[i]) {
            if (g.koikoi_score[i] < g.stats[i].score) {
                return OFF;
            }
        } else {
            if (0 < g.stats[i].score) {
                return OFF;
            }
        }
    }
    return ON;
}

int show_result(int current_player)
{
    if (is_draw_state()) {
        if (g.oya >= 0 && g.oya <= 1) {
            g_sim_metrics.dealer_rounds[g.oya]++;
            g_sim_metrics.player_rounds[1 - g.oya]++;
        }
        g.round_score[0][g.round] = 0;
        g.round_score[1][g.round] = 0;
        note_bias_round_result(0, BIAS_RESULT_DRAW);
        note_bias_round_result(1, BIAS_RESULT_DRAW);
        vgs_putlog("[Round %d] Draw", g.round + 1);
        g.koikoi[0] = 0;
        g.koikoi[1] = 0;
        g.koikoi_score[0] = 0;
        g.koikoi_score[1] = 0;
        return OFF;
    }

    int player = current_player;
    int base = g.stats[player].score;
    int koikoi_enabled = is_koikoi_enabled(player);
    int koikoi = ai_koikoi(player, base);
    if (koikoi_enabled && koikoi) {
        g_sim_metrics.koikoi_count[player]++;
        if (ai_debug_has_initial_sake(player)) {
            g_sim_metrics.initial_sake_round_koikoi_count[player]++;
        }
        g.koikoi[player] = ON;
        g.koikoi_score[player] = base;
        vgs_putlog("[Round %d] %s chooses KOIKOI (base=%d, own_win_requires>%d, opponent_win_x2=ON)", g.round + 1, player ? "CPU" : "P1",
                   base, base);
        ai_watch_note_after_opp_koikoi(player, base);
        return ON;
    }

    int mul = round_multiplier(player);
    int score = base * mul;
    if (g.oya >= 0 && g.oya <= 1) {
        g_sim_metrics.dealer_rounds[g.oya]++;
        g_sim_metrics.player_rounds[1 - g.oya]++;
        if (g.oya == player) {
            g_sim_metrics.dealer_wins[player]++;
        } else {
            g_sim_metrics.player_wins[player]++;
        }
    }
    note_bias_round_result(player, BIAS_RESULT_WIN);
    note_bias_round_result(1 - player, BIAS_RESULT_LOSE);
    note_yaku_counts(player);
    g_sim_metrics.round_wins[player][g.round]++;
    g_sim_metrics.round_win_score_sum[player][g.round] += score;
    if (ai_debug_has_initial_sake(player)) {
        g_sim_metrics.initial_sake_round_win_count[player]++;
        g_sim_metrics.initial_sake_round_win_score_sum[player] += score;
        if (g.koikoi[player]) {
            g_sim_metrics.initial_sake_round_koikoi_win_count[player]++;
            g_sim_metrics.initial_sake_round_koikoi_up_sum[player] += score - g.koikoi_score[player];
        }
    }
    g.round_score[player][g.round] = score;
    g.total_score[player] += score;
    if (g.stats[player].score >= 7) {
        g_sim_metrics.reason_7plus[player]++;
    }
    if (g.koikoi[1 - player]) {
        g_sim_metrics.reason_opponent_koikoi[player]++;
    }
    for (int p = 0; p < 2; p++) {
        if (!g.koikoi[p]) {
            continue;
        }
        if (p == player && g.koikoi_score[p] < g.stats[p].score) {
            g_sim_metrics.koikoi_success[p]++;
        } else {
            g_sim_metrics.koikoi_failed[p]++;
        }
    }

    vgs_putlog("[Round %d] Winner=%s base=%d x%d => +%d (reason=%s, Total P1=%d CPU=%d)", g.round + 1, player ? "CPU" : "P1", base, mul,
               score, multiplier_reason(player), g.total_score[0], g.total_score[1]);
    for (int i = 0; i < g.stats[player].wh_count; i++) {
        WinningHand* wh = g.stats[player].wh[i];
        if (!wh) {
            continue;
        }
        vgs_putlog("  - %s (%s): %d", wh->nameJ, wh->nameE, wh->base_score + g.stats[player].base_up[wh->id]);
    }
    ai_watch_note_after_opp_win(player, score);

    g.koikoi[0] = 0;
    g.koikoi[1] = 0;
    g.koikoi_score[0] = 0;
    g.koikoi_score[1] = 0;
    return OFF;
}

void show_game_result(void)
{
    const int p1 = g.total_score[0];
    const int cpu = g.total_score[1];
    if (p1 > cpu) {
        g_sim_metrics.wins[0]++;
        g_sim_metrics.win_score_sum[0] += p1;
    } else if (cpu > p1) {
        g_sim_metrics.wins[1]++;
        g_sim_metrics.win_score_sum[1] += cpu;
    } else {
        g_sim_metrics.draws++;
    }
    g_sim_metrics.score_diff_sum[0] += p1 - cpu;
    g_sim_metrics.score_diff_sum[1] += cpu - p1;
}
