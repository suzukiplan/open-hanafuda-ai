#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
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

typedef struct {
    int seed;
    int applied_best;
    int penalty_applied;
    int changed_choice;
    int min_gap_before;
    int min_gap_after;
} SeedSearchResult;

void sim_metrics_reset(void);
const SimMetrics* sim_metrics_get(void);
void vgs_set_log_enabled(int enabled);

static int parse_or_default(const char* s, int def)
{
    if (!s || !*s) {
        return def;
    }
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (!end || *end != '\0') {
        return def;
    }
    return (int)v;
}

static int parse_option_value(int argc, char** argv, int* index, int def)
{
    if (!index || *index + 1 >= argc) {
        return def;
    }
    (*index)++;
    return parse_or_default(argv[*index], def);
}

static void print_usage(const char* argv0)
{
    const char* prog = (argv0 && *argv0) ? argv0 : "ai_sim";
    fprintf(stderr,
            "usage: %s -0 <ai_model> -1 <ai_model> -r <rounds> [-l <loops>] [--seed=<seed>] [--sake=<0|1>] [--log=<path>] [--find-seed-actionmc-best=<N> "
            "--seed-start=<S> --seed-count=<M>]\n",
            prog);
}

static const char* strategy_mode_name(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return "GREEDY";
        case MODE_DEFENSIVE: return "DEFENSIVE";
        default: return "BALANCED";
    }
}

static void configure_game(int ai0, int ai1, int sake_enabled)
{
    vgs_memset(&g, 0, sizeof(g));
    g.online_mode = OFF;
    g.auto_play = ON;
    g.no_sake = sake_enabled ? OFF : ON;
    g_no_sake_latched = g.no_sake;
    g.ai_model[0] = ai0;
    g.ai_model[1] = ai1;
    g.bg_color = 0;
}

const char* ai_name(int model)
{
    switch (model) {
        case AI_MODEL_NORMAL: return "Normal";
        case AI_MODEL_HARD: return "Hard";
        case AI_MODEL_STENV: return "Stenv";
        case AI_MODEL_MCENV: return "Mcenv";
        default: return "Unknown";
    }
}

static void run_single_game(int ai0, int ai1, int round_max, int seed, int game_index, int sake_enabled)
{
    sim_metrics_reset();
    ai_debug_metrics_reset();
    configure_game(ai0, ai1, sake_enabled);
    ai_debug_set_run_context(seed, game_index);
    vgs_srand((uint32_t)seed);
    game(round_max);
}

static int seed_search_result_better(const SeedSearchResult* lhs, const SeedSearchResult* rhs)
{
    if (lhs->applied_best != rhs->applied_best) {
        return lhs->applied_best > rhs->applied_best;
    }
    if (lhs->penalty_applied != rhs->penalty_applied) {
        return lhs->penalty_applied > rhs->penalty_applied;
    }
    if (lhs->min_gap_after != rhs->min_gap_after) {
        return lhs->min_gap_after < rhs->min_gap_after;
    }
    if (lhs->min_gap_before != rhs->min_gap_before) {
        return lhs->min_gap_before < rhs->min_gap_before;
    }
    return lhs->seed < rhs->seed;
}

static void update_seed_search_top(SeedSearchResult* top, int top_n, const SeedSearchResult* candidate)
{
    int insert_at = -1;

    for (int i = 0; i < top_n; i++) {
        if (top[i].seed == 0 && top[i].applied_best == 0 && top[i].penalty_applied == 0 && top[i].changed_choice == 0 && top[i].min_gap_before == 0 &&
            top[i].min_gap_after == 0) {
            insert_at = i;
            break;
        }
        if (seed_search_result_better(candidate, &top[i])) {
            insert_at = i;
            break;
        }
    }
    if (insert_at < 0) {
        return;
    }
    for (int i = top_n - 1; i > insert_at; i--) {
        top[i] = top[i - 1];
    }
    top[insert_at] = *candidate;
}

static int run_action_mc_best_seed_search(int ai0, int ai1, int round_max, int seed_start, int seed_count, int top_n, int sake_enabled)
{
    SeedSearchResult* top;

    if (top_n <= 0 || seed_count <= 0) {
        return 1;
    }
    top = (SeedSearchResult*)calloc((size_t)top_n, sizeof(SeedSearchResult));
    if (!top) {
        fprintf(stderr, "failed to allocate seed search buffer\n");
        return 1;
    }
    vgs_set_log_enabled(OFF);
    for (int i = 0; i < seed_count; i++) {
        int current_seed = seed_start + i;
        SeedSearchResult result;
        const AiDebugMetrics* debug_metrics;
        run_single_game(ai0, ai1, round_max, current_seed, 0, sake_enabled);
        debug_metrics = ai_debug_metrics_get();
        result.seed = current_seed;
        result.applied_best = debug_metrics->action_mc_applied_best_count[1];
        result.penalty_applied = debug_metrics->action_mc_penalty_applied_count[1];
        result.changed_choice = debug_metrics->action_mc_changed_choice_count[1];
        result.min_gap_before =
            debug_metrics->action_mc_best_gap_count[1] ? debug_metrics->action_mc_best_min_gap_before[1] : 999999999;
        result.min_gap_after =
            debug_metrics->action_mc_best_gap_count[1] ? debug_metrics->action_mc_best_min_gap_after[1] : 999999999;
        if (result.applied_best >= 1) {
            update_seed_search_top(top, top_n, &result);
        }
    }

    printf("ACTION_MC_BEST_SEEDS top=%d seed_start=%d seed_count=%d rounds=%d\n", top_n, seed_start, seed_count, round_max);
    for (int i = 0; i < top_n; i++) {
        if (top[i].applied_best <= 0) {
            break;
        }
        printf("seed=%d applied_best=%d penalty=%d changed=%d min_gap_before=%d min_gap_after=%d\n", top[i].seed, top[i].applied_best,
               top[i].penalty_applied, top[i].changed_choice, top[i].min_gap_before, top[i].min_gap_after);
    }
    free(top);
    return 0;
}

int main(int argc, char** argv)
{
    const int ai_default = AI_MODEL_HARD;
    const int rounds_default = 12;
    int ai_model[2] = {ai_default, ai_default};
    int round_max = rounds_default;
    int loops = 1;
    int seed = (int)time(NULL);
    const char* log_path = NULL;
    int find_seed_actionmc_best = 0;
    int seed_start = 1;
    int seed_count = 0;
    int parse_error = OFF;
    int sake_enabled = 1;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "-0") == 0) {
            if (i + 1 >= argc) {
                parse_error = ON;
                break;
            }
            ai_model[0] = parse_option_value(argc, argv, &i, ai_model[0]);
        } else if (strcmp(arg, "-1") == 0) {
            if (i + 1 >= argc) {
                parse_error = ON;
                break;
            }
            ai_model[1] = parse_option_value(argc, argv, &i, ai_model[1]);
        } else if (strcmp(arg, "-r") == 0) {
            if (i + 1 >= argc) {
                parse_error = ON;
                break;
            }
            round_max = parse_option_value(argc, argv, &i, round_max);
        } else if (strcmp(arg, "-l") == 0) {
            if (i + 1 >= argc) {
                parse_error = ON;
                break;
            }
            loops = parse_option_value(argc, argv, &i, loops);
        } else if (strncmp(arg, "--seed=", 7) == 0) {
            seed = parse_or_default(arg + 7, seed);
        } else if (strncmp(arg, "--log=", 6) == 0) {
            log_path = arg + 6;
        } else if (strncmp(arg, "--sake=", 7) == 0) {
            sake_enabled = parse_or_default(arg + 7, sake_enabled) ? 1 : 0;
        } else if (strcmp(arg, "--log") == 0) {
            if (i + 1 >= argc) {
                parse_error = ON;
                break;
            }
            log_path = argv[++i];
        } else if (strcmp(arg, "--sake") == 0) {
            sake_enabled = parse_option_value(argc, argv, &i, sake_enabled) ? 1 : 0;
        } else if (strncmp(arg, "--find-seed-actionmc-best=", 26) == 0) {
            find_seed_actionmc_best = parse_or_default(arg + 26, 0);
        } else if (strcmp(arg, "--find-seed-actionmc-best") == 0) {
            find_seed_actionmc_best = parse_option_value(argc, argv, &i, find_seed_actionmc_best);
        } else if (strncmp(arg, "--seed-start=", 13) == 0) {
            seed_start = parse_or_default(arg + 13, seed_start);
        } else if (strcmp(arg, "--seed-start") == 0) {
            seed_start = parse_option_value(argc, argv, &i, seed_start);
        } else if (strncmp(arg, "--seed-count=", 13) == 0) {
            seed_count = parse_or_default(arg + 13, seed_count);
        } else if (strcmp(arg, "--seed-count") == 0) {
            seed_count = parse_option_value(argc, argv, &i, seed_count);
        } else {
            parse_error = ON;
            break;
        }
    }

    if (parse_error) {
        print_usage(argv[0]);
        return 1;
    }

    if (ai_model[0] < AI_MODEL_NORMAL || ai_model[0] >= AI_MODEL_MAX) {
        ai_model[0] = ai_default;
    }
    if (ai_model[1] < AI_MODEL_NORMAL || ai_model[1] >= AI_MODEL_MAX) {
        ai_model[1] = ai_default;
    }
    if (round_max <= 0 || round_max > 12) {
        round_max = rounds_default;
    }
    if (loops <= 0) {
        loops = 1;
    }
    if (find_seed_actionmc_best > 0) {
        if (seed_count <= 0) {
            seed_count = 100;
        }
        return run_action_mc_best_seed_search(ai_model[0], ai_model[1], round_max, seed_start, seed_count, find_seed_actionmc_best, sake_enabled);
    }
    if (!vgs_watch_log_set_base_path(log_path)) {
        return 1;
    }

    sim_metrics_reset();
    ai_debug_metrics_reset();
    vgs_set_log_enabled(loops >= 2 ? OFF : ON);

    for (int game_index = 0; game_index < loops; game_index++) {
        if (!vgs_watch_log_begin_game()) {
            vgs_watch_log_shutdown();
            return 1;
        }
        configure_game(ai_model[0], ai_model[1], sake_enabled);
        ai_debug_set_run_context(seed + game_index, game_index);
        vgs_srand((uint32_t)(seed + game_index));
        game(round_max);
        if (!vgs_watch_log_finalize_game(game_index, g.total_score[0], g.total_score[1])) {
            vgs_watch_log_shutdown();
            return 1;
        }
    }
    vgs_watch_log_shutdown();

    const SimMetrics* metrics = sim_metrics_get();
    const AiDebugMetrics* debug_metrics = ai_debug_metrics_get();
    double p1_win_rate = (metrics->wins[0] * 100.0) / loops;
    double cpu_win_rate = (metrics->wins[1] * 100.0) / loops;
    double p1_dealer_win_rate = metrics->dealer_rounds[0] ? (metrics->dealer_wins[0] * 100.0) / metrics->dealer_rounds[0] : 0.0;
    double cpu_dealer_win_rate = metrics->dealer_rounds[1] ? (metrics->dealer_wins[1] * 100.0) / metrics->dealer_rounds[1] : 0.0;
    double p1_player_win_rate = metrics->player_rounds[0] ? (metrics->player_wins[0] * 100.0) / metrics->player_rounds[0] : 0.0;
    double cpu_player_win_rate = metrics->player_rounds[1] ? (metrics->player_wins[1] * 100.0) / metrics->player_rounds[1] : 0.0;
    double p1_avg_win_score = metrics->wins[0] ? metrics->win_score_sum[0] / (double)metrics->wins[0] : 0.0;
    double cpu_avg_win_score = metrics->wins[1] ? metrics->win_score_sum[1] / (double)metrics->wins[1] : 0.0;
    double p1_avg_diff = metrics->score_diff_sum[0] / (double)loops;
    double cpu_avg_diff = metrics->score_diff_sum[1] / (double)loops;
    double p1_koikoi_success = metrics->koikoi_count[0] ? (metrics->koikoi_success[0] * 100.0) / metrics->koikoi_count[0] : 0.0;
    double cpu_koikoi_success = metrics->koikoi_count[1] ? (metrics->koikoi_success[1] * 100.0) / metrics->koikoi_count[1] : 0.0;
    double p1_combo7_margin_avg =
        debug_metrics->combo7_margin_count[0] ? (double)debug_metrics->combo7_margin_total[0] / debug_metrics->combo7_margin_count[0] : 0.0;
    double cpu_combo7_margin_avg =
        debug_metrics->combo7_margin_count[1] ? (double)debug_metrics->combo7_margin_total[1] / debug_metrics->combo7_margin_count[1] : 0.0;
    double p1_sevenline_margin_avg =
        debug_metrics->sevenline_margin_count[0] ? (double)debug_metrics->sevenline_margin_total[0] / debug_metrics->sevenline_margin_count[0] : 0.0;
    double cpu_sevenline_margin_avg =
        debug_metrics->sevenline_margin_count[1] ? (double)debug_metrics->sevenline_margin_total[1] / debug_metrics->sevenline_margin_count[1] : 0.0;
    double p1_sevenline_bonus_avg =
        debug_metrics->sevenline_bonus_count[0] ? (double)debug_metrics->sevenline_bonus_total[0] / debug_metrics->sevenline_bonus_count[0] : 0.0;
    double cpu_sevenline_bonus_avg =
        debug_metrics->sevenline_bonus_count[1] ? (double)debug_metrics->sevenline_bonus_total[1] / debug_metrics->sevenline_bonus_count[1] : 0.0;
    double p1_d1_gap_before_avg =
        debug_metrics->sevenline_d1_gap_count[0] ? (double)debug_metrics->sevenline_d1_gap_total_before[0] / debug_metrics->sevenline_d1_gap_count[0] : 0.0;
    double p1_d1_gap_after_avg =
        debug_metrics->sevenline_d1_gap_count[0] ? (double)debug_metrics->sevenline_d1_gap_total_after[0] / debug_metrics->sevenline_d1_gap_count[0] : 0.0;
    double cpu_d1_gap_before_avg =
        debug_metrics->sevenline_d1_gap_count[1] ? (double)debug_metrics->sevenline_d1_gap_total_before[1] / debug_metrics->sevenline_d1_gap_count[1] : 0.0;
    double cpu_d1_gap_after_avg =
        debug_metrics->sevenline_d1_gap_count[1] ? (double)debug_metrics->sevenline_d1_gap_total_after[1] / debug_metrics->sevenline_d1_gap_count[1] : 0.0;
    double p1_action_mc_qdiff_avg =
        debug_metrics->action_mc_qdiff_count[0] ? (double)debug_metrics->action_mc_qdiff_total[0] / debug_metrics->action_mc_qdiff_count[0] : 0.0;
    double cpu_action_mc_qdiff_avg =
        debug_metrics->action_mc_qdiff_count[1] ? (double)debug_metrics->action_mc_qdiff_total[1] / debug_metrics->action_mc_qdiff_count[1] : 0.0;
    int p1_action_mc_qdiff_min =
        debug_metrics->action_mc_qdiff_count[0] ? debug_metrics->action_mc_qdiff_min[0] : 0;
    int p1_action_mc_qdiff_max =
        debug_metrics->action_mc_qdiff_count[0] ? debug_metrics->action_mc_qdiff_max[0] : 0;
    int cpu_action_mc_qdiff_min =
        debug_metrics->action_mc_qdiff_count[1] ? debug_metrics->action_mc_qdiff_min[1] : 0;
    int cpu_action_mc_qdiff_max =
        debug_metrics->action_mc_qdiff_count[1] ? debug_metrics->action_mc_qdiff_max[1] : 0;
    double p1_env_mc_e_avg = debug_metrics->env_mc_e_count[0] ? (double)debug_metrics->env_mc_e_total[0] / debug_metrics->env_mc_e_count[0] : 0.0;
    double cpu_env_mc_e_avg = debug_metrics->env_mc_e_count[1] ? (double)debug_metrics->env_mc_e_total[1] / debug_metrics->env_mc_e_count[1] : 0.0;
    int p1_env_mc_e_min = debug_metrics->env_mc_e_count[0] ? debug_metrics->env_mc_e_min[0] : 0;
    int p1_env_mc_e_max = debug_metrics->env_mc_e_count[0] ? debug_metrics->env_mc_e_max[0] : 0;
    int cpu_env_mc_e_min = debug_metrics->env_mc_e_count[1] ? debug_metrics->env_mc_e_min[1] : 0;
    int cpu_env_mc_e_max = debug_metrics->env_mc_e_count[1] ? debug_metrics->env_mc_e_max[1] : 0;

    printf("SUMMARY games=%d seed=%d rounds=%d\n", loops, seed, round_max);
    printf("P1 model=%s wins=%d avg_diff=%.3f reason_7plus=%d reason_opponent_koikoi=%d koikoi=%d koikoi_success=%d koikoi_success_rate=%.2f%%\n",
           ai_name(ai_model[0]), metrics->wins[0], p1_avg_diff, metrics->reason_7plus[0], metrics->reason_opponent_koikoi[0],
           metrics->koikoi_count[0], metrics->koikoi_success[0], p1_koikoi_success);
    printf("CPU model=%s wins=%d avg_diff=%.3f reason_7plus=%d reason_opponent_koikoi=%d koikoi=%d koikoi_success=%d koikoi_success_rate=%.2f%%\n",
           ai_name(ai_model[1]), metrics->wins[1], cpu_avg_diff, metrics->reason_7plus[1], metrics->reason_opponent_koikoi[1],
           metrics->koikoi_count[1], metrics->koikoi_success[1], cpu_koikoi_success);
    printf("DRAW draws=%d\n", metrics->draws);
    if (loops >= 2) {
        printf("Round Win Ratio:\n");
        printf("- P1: Dealer = %d/%d (%.2f%%), Player = %d/%d (%.2f%%)\n", metrics->dealer_wins[0], metrics->dealer_rounds[0], p1_dealer_win_rate,
               metrics->player_wins[0], metrics->player_rounds[0], p1_player_win_rate);
        printf("- CPU: Dealer = %d/%d (%.2f%%), Player = %d/%d (%.2f%%)\n", metrics->dealer_wins[1], metrics->dealer_rounds[1], cpu_dealer_win_rate,
               metrics->player_wins[1], metrics->player_rounds[1], cpu_player_win_rate);
        printf("Yaku Ratio:\n");
        for (int i = 0; i < WINNING_HAND_MAX; i++) {
            double yaku_ratio = metrics->yaku_total_count ? (metrics->yaku_counts[i] * 100.0) / metrics->yaku_total_count : 0.0;
            printf("- %s: %d (%.2f%%)\n", winning_hands[i].nameJ, metrics->yaku_counts[i], yaku_ratio);
        }
    }
    printf("Sake round summary: 1P=%d, CPU=%d\n", debug_metrics->initial_sake_round_count[0], debug_metrics->initial_sake_round_count[1]);
    printf(" - 1P detail: win=%d (%.2f%%), average=%.2fpts koikoi-cnt=%d koikoi-win=%d koikoi-up=%.2fpts\n", metrics->initial_sake_round_win_count[0],
           debug_metrics->initial_sake_round_count[0] > 0
               ? (metrics->initial_sake_round_win_count[0] * 100.0) / debug_metrics->initial_sake_round_count[0]
               : 0.0,
           metrics->initial_sake_round_win_count[0] > 0
               ? metrics->initial_sake_round_win_score_sum[0] / (double)metrics->initial_sake_round_win_count[0]
               : 0.0,
           metrics->initial_sake_round_koikoi_count[0], metrics->initial_sake_round_koikoi_win_count[0],
           metrics->initial_sake_round_koikoi_win_count[0] > 0 ? metrics->initial_sake_round_koikoi_up_sum[0] / (double)metrics->initial_sake_round_koikoi_win_count[0] : 0.0);
    printf(" - CPU detail: win=%d (%.2f%%), average=%.2fpts koikoi-cnt=%d koikoi-win=%d koikoi-up=%.2fpts\n", metrics->initial_sake_round_win_count[1],
           debug_metrics->initial_sake_round_count[1] > 0
               ? (metrics->initial_sake_round_win_count[1] * 100.0) / debug_metrics->initial_sake_round_count[1]
               : 0.0,
           metrics->initial_sake_round_win_count[1] > 0
               ? metrics->initial_sake_round_win_score_sum[1] / (double)metrics->initial_sake_round_win_count[1]
               : 0.0,
           metrics->initial_sake_round_koikoi_count[1], metrics->initial_sake_round_koikoi_win_count[1],
           metrics->initial_sake_round_koikoi_win_count[1] ? metrics->initial_sake_round_koikoi_up_sum[1] / (double)metrics->initial_sake_round_koikoi_win_count[1] : 0.0);
    printf("KOIKOI_BASE6_PUSH P1=%d/%d/%d CPU=%d/%d/%d\n", debug_metrics->koikoi_base6_push_trigger_count[0],
           debug_metrics->koikoi_base6_push_applied_count[0], debug_metrics->koikoi_base6_push_flipped_count[0],
           debug_metrics->koikoi_base6_push_trigger_count[1], debug_metrics->koikoi_base6_push_applied_count[1],
           debug_metrics->koikoi_base6_push_flipped_count[1]);
    printf("KOIKOI_LOCKED_THRESHOLD P1=%d/%d CPU=%d/%d\n", debug_metrics->koikoi_locked_threshold_trigger_count[0],
           debug_metrics->koikoi_locked_threshold_applied_count[0], debug_metrics->koikoi_locked_threshold_trigger_count[1],
           debug_metrics->koikoi_locked_threshold_applied_count[1]);
#if 0
           printf("DEBUG P1 action_mc=%d/%d/%d/%d/%d qdiff=%.2f/%d/%d/%d/%d/%d/%d/%d action_mc_mat=%d,%d,%d,%d,%d,%d,%d/%d,%d,%d,%d,%d,%d,%d month_lock=%d/%d month_lock_hidden_key=%d/%d month_lock_hidden_deny=%d/%d overpay_delay=%d/%d certain_7plus=%d/%d mode=%d/%d/%d combo7=%d/%d g=%d/%d b=%d/%d d=%d/%d combo7_margin=%.2f/%d/%d/%d/%d plus1_rank=%d/%d koikoi_base6_push=%d/%d/%d keytarget_expose=%d/%d env_mc=%d/%d env_mc_E=%.2f/%d/%d env_mc_mode=%d/%d,%d/%d,%d/%d plan_count=%d/%d plan_reason=%d,%d,%d,%d,%d phase2a=%d/%d phase2a_plan=%d/%d phase2a_domain=%d,%d,%d,%d,%d,%d phase2a_reason=%d/%d,%d/%d,%d/%d,%d/%d phase2a_reason_press=%d/%d,%d/%d,%d/%d,%d/%d phase2a_reason_survive=%d/%d,%d/%d,%d/%d,%d/%d phase2a_press_reason_7plus_changed=%d sevenline=%d/%d sevenline_margin=%.2f/%d/%d sevenline_margin_hist=%d/%d/%d sevenline_margin1_reach_hist=%d/%d/%d sevenline_delay_hist=%d/%d/%d/%d sevenline_changed_delay_hist=%d/%d/%d/%d sevenline_changed_margin_hist=%d/%d/%d sevenline_bonus_stats=%.2f/%d/%d sevenline_low_reach_count=%d sevenline_press_applied_count=%d sevenline_d1_loss_reason=%d/%d/%d/%d sevenline_d1_override=%d/%d/%d sevenline_d1_override_blocked=%d/%d d1_gap_before=%.2f/%d/%d d1_gap_after=%.2f/%d/%d d1_gap_count=%d\n",
           debug_metrics->action_mc_trigger_count[0], debug_metrics->action_mc_penalty_applied_count[0],
           debug_metrics->action_mc_changed_choice_count[0], debug_metrics->action_mc_applied_best_count[0],
           debug_metrics->action_mc_total_candidates[0],
           p1_action_mc_qdiff_avg, p1_action_mc_qdiff_min, p1_action_mc_qdiff_max, debug_metrics->action_mc_qdiff_lt0_count[0],
           debug_metrics->action_mc_qdiff_le_500_count[0], debug_metrics->action_mc_qdiff_le_1000_count[0],
           debug_metrics->action_mc_qdiff_le_2000_count[0], debug_metrics->action_mc_qdiff_le_4000_count[0],
           debug_metrics->action_mc_trigger_by_material[0][AI_ACTION_MC_MATERIAL_LIGHT],
           debug_metrics->action_mc_trigger_by_material[0][AI_ACTION_MC_MATERIAL_SAKE],
           debug_metrics->action_mc_trigger_by_material[0][AI_ACTION_MC_MATERIAL_AKATAN],
           debug_metrics->action_mc_trigger_by_material[0][AI_ACTION_MC_MATERIAL_AOTAN],
           debug_metrics->action_mc_trigger_by_material[0][AI_ACTION_MC_MATERIAL_ISC],
           debug_metrics->action_mc_trigger_by_material[0][AI_ACTION_MC_MATERIAL_AME],
           debug_metrics->action_mc_trigger_by_material[0][AI_ACTION_MC_MATERIAL_OTHER_HIGH],
           debug_metrics->action_mc_changed_by_material[0][AI_ACTION_MC_MATERIAL_LIGHT],
           debug_metrics->action_mc_changed_by_material[0][AI_ACTION_MC_MATERIAL_SAKE],
           debug_metrics->action_mc_changed_by_material[0][AI_ACTION_MC_MATERIAL_AKATAN],
           debug_metrics->action_mc_changed_by_material[0][AI_ACTION_MC_MATERIAL_AOTAN],
           debug_metrics->action_mc_changed_by_material[0][AI_ACTION_MC_MATERIAL_ISC],
           debug_metrics->action_mc_changed_by_material[0][AI_ACTION_MC_MATERIAL_AME],
           debug_metrics->action_mc_changed_by_material[0][AI_ACTION_MC_MATERIAL_OTHER_HIGH],
           debug_metrics->month_lock_trigger_count[0], debug_metrics->month_lock_changed_choice_count[0],
           debug_metrics->month_lock_hidden_key_trigger_count[0], debug_metrics->month_lock_hidden_key_changed_choice_count[0],
           debug_metrics->month_lock_hidden_deny_trigger_count[0], debug_metrics->month_lock_hidden_deny_changed_choice_count[0],
           debug_metrics->overpay_delay_penalty_applied_count[0], debug_metrics->overpay_delay_changed_choice_count[0],
           debug_metrics->certain_seven_plus_trigger_count[0], debug_metrics->certain_seven_plus_changed_choice_count[0],
           debug_metrics->mode_greedy_count[0], debug_metrics->mode_balanced_count[0], debug_metrics->mode_defensive_count[0],
           debug_metrics->combo7_trigger_count[0], debug_metrics->combo7_changed_choice_count[0],
           debug_metrics->combo7_greedy_trigger_count[0], debug_metrics->combo7_greedy_changed_choice_count[0],
           debug_metrics->combo7_balanced_trigger_count[0], debug_metrics->combo7_balanced_changed_choice_count[0],
           debug_metrics->combo7_defensive_trigger_count[0], debug_metrics->combo7_defensive_changed_choice_count[0],
           p1_combo7_margin_avg, debug_metrics->combo7_margin_flip_count[0], debug_metrics->combo7_margin_lt_5k_count[0],
           debug_metrics->combo7_margin_lt_10k_count[0], debug_metrics->combo7_margin_lt_20k_count[0],
           debug_metrics->best_plus1_used_rank_count[0][0], debug_metrics->best_plus1_used_rank_count[0][1],
           debug_metrics->koikoi_base6_push_trigger_count[0], debug_metrics->koikoi_base6_push_applied_count[0],
           debug_metrics->koikoi_base6_push_flipped_count[0],
           debug_metrics->keytarget_expose_trigger_count[0], debug_metrics->keytarget_expose_changed_choice_count[0],
           debug_metrics->env_mc_trigger_count[0], debug_metrics->env_mc_changed_choice_count[0],
           p1_env_mc_e_avg, p1_env_mc_e_min, p1_env_mc_e_max,
           debug_metrics->env_mc_mode_trigger_count[0][0], debug_metrics->env_mc_mode_changed_choice_count[0][0],
           debug_metrics->env_mc_mode_trigger_count[0][1], debug_metrics->env_mc_mode_changed_choice_count[0][1],
           debug_metrics->env_mc_mode_trigger_count[0][2], debug_metrics->env_mc_mode_changed_choice_count[0][2],
           debug_metrics->plan_count[0][AI_PLAN_PRESS], debug_metrics->plan_count[0][AI_PLAN_SURVIVE],
           debug_metrics->plan_reason_count[0][AI_PLAN_REASON_DIFF], debug_metrics->plan_reason_count[0][AI_PLAN_REASON_COMBO7],
           debug_metrics->plan_reason_count[0][AI_PLAN_REASON_SEVENLINE], debug_metrics->plan_reason_count[0][AI_PLAN_REASON_BLOCK_THREAT],
           debug_metrics->plan_reason_count[0][AI_PLAN_REASON_SURVIVE_DIFF],
           debug_metrics->phase2a_trigger_count[0], debug_metrics->phase2a_changed_choice_count[0],
           debug_metrics->phase2a_plan_count[0][AI_PLAN_PRESS], debug_metrics->phase2a_plan_count[0][AI_PLAN_SURVIVE],
           debug_metrics->phase2a_domain_count[0][AI_ENV_DOMAIN_NONE], debug_metrics->phase2a_domain_count[0][AI_ENV_DOMAIN_LIGHT],
           debug_metrics->phase2a_domain_count[0][AI_ENV_DOMAIN_SAKE], debug_metrics->phase2a_domain_count[0][AI_ENV_DOMAIN_AKATAN],
           debug_metrics->phase2a_domain_count[0][AI_ENV_DOMAIN_AOTAN], debug_metrics->phase2a_domain_count[0][AI_ENV_DOMAIN_ISC],
           debug_metrics->phase2a_reason_trigger_count[0][AI_PHASE2A_REASON_7PLUS], debug_metrics->phase2a_reason_changed_count[0][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->phase2a_reason_trigger_count[0][AI_PHASE2A_REASON_COMBO7], debug_metrics->phase2a_reason_changed_count[0][AI_PHASE2A_REASON_COMBO7],
           debug_metrics->phase2a_reason_trigger_count[0][AI_PHASE2A_REASON_HOT], debug_metrics->phase2a_reason_changed_count[0][AI_PHASE2A_REASON_HOT],
           debug_metrics->phase2a_reason_trigger_count[0][AI_PHASE2A_REASON_DEFENCE], debug_metrics->phase2a_reason_changed_count[0][AI_PHASE2A_REASON_DEFENCE],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_7PLUS], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_COMBO7], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_COMBO7],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_HOT], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_HOT],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_DEFENCE], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_DEFENCE],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_7PLUS], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_COMBO7], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_COMBO7],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_HOT], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_HOT],
           debug_metrics->phase2a_reason_trigger_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_DEFENCE], debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_DEFENCE],
           debug_metrics->phase2a_reason_changed_by_plan[0][AI_PLAN_PRESS][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->sevenline_trigger_count[0], debug_metrics->sevenline_changed_count[0],
           p1_sevenline_margin_avg,
           debug_metrics->sevenline_margin_count[0] ? debug_metrics->sevenline_margin_min[0] : 0,
           debug_metrics->sevenline_margin_count[0] ? debug_metrics->sevenline_margin_max[0] : 0,
           debug_metrics->sevenline_margin_hist[0][0], debug_metrics->sevenline_margin_hist[0][1], debug_metrics->sevenline_margin_hist[0][2],
           debug_metrics->sevenline_margin1_reach_hist[0][0], debug_metrics->sevenline_margin1_reach_hist[0][1], debug_metrics->sevenline_margin1_reach_hist[0][2],
           debug_metrics->sevenline_delay_hist[0][0], debug_metrics->sevenline_delay_hist[0][1], debug_metrics->sevenline_delay_hist[0][2], debug_metrics->sevenline_delay_hist[0][3],
           debug_metrics->sevenline_changed_delay_hist[0][0], debug_metrics->sevenline_changed_delay_hist[0][1], debug_metrics->sevenline_changed_delay_hist[0][2], debug_metrics->sevenline_changed_delay_hist[0][3],
           debug_metrics->sevenline_changed_margin_hist[0][0], debug_metrics->sevenline_changed_margin_hist[0][1], debug_metrics->sevenline_changed_margin_hist[0][2],
           p1_sevenline_bonus_avg,
           debug_metrics->sevenline_bonus_count[0] ? debug_metrics->sevenline_bonus_min[0] : 0,
           debug_metrics->sevenline_bonus_count[0] ? debug_metrics->sevenline_bonus_max[0] : 0,
           debug_metrics->sevenline_low_reach_count[0], debug_metrics->sevenline_press_applied_count[0],
           debug_metrics->sevenline_d1_loss_reason_count[0][AI_SEVENLINE_D1_LOSS_DEFENCE],
           debug_metrics->sevenline_d1_loss_reason_count[0][AI_SEVENLINE_D1_LOSS_COMBO7],
           debug_metrics->sevenline_d1_loss_reason_count[0][AI_SEVENLINE_D1_LOSS_DOMAIN_HOT],
           debug_metrics->sevenline_d1_loss_reason_count[0][AI_SEVENLINE_D1_LOSS_OTHER],
           debug_metrics->sevenline_d1_override_trigger_count[0], debug_metrics->sevenline_d1_override_applied_count[0],
           debug_metrics->sevenline_d1_override_changed_count[0],
           debug_metrics->sevenline_d1_override_blocked_count[0][AI_SEVENLINE_D1_OVERRIDE_BLOCK_KOIKOI_OPP],
           debug_metrics->sevenline_d1_override_blocked_count[0][AI_SEVENLINE_D1_OVERRIDE_BLOCK_INVALID],
           p1_d1_gap_before_avg,
           debug_metrics->sevenline_d1_gap_count[0] ? debug_metrics->sevenline_d1_gap_min_before[0] : 0,
           debug_metrics->sevenline_d1_gap_count[0] ? debug_metrics->sevenline_d1_gap_max_before[0] : 0,
           p1_d1_gap_after_avg,
           debug_metrics->sevenline_d1_gap_count[0] ? debug_metrics->sevenline_d1_gap_min_after[0] : 0,
           debug_metrics->sevenline_d1_gap_count[0] ? debug_metrics->sevenline_d1_gap_max_after[0] : 0,
           debug_metrics->sevenline_d1_gap_count[0]);
    printf("DEBUG CPU action_mc=%d/%d/%d/%d/%d qdiff=%.2f/%d/%d/%d/%d/%d/%d/%d action_mc_mat=%d,%d,%d,%d,%d,%d,%d/%d,%d,%d,%d,%d,%d,%d month_lock=%d/%d month_lock_hidden_key=%d/%d month_lock_hidden_deny=%d/%d overpay_delay=%d/%d certain_7plus=%d/%d mode=%d/%d/%d combo7=%d/%d g=%d/%d b=%d/%d d=%d/%d combo7_margin=%.2f/%d/%d/%d/%d plus1_rank=%d/%d koikoi_base6_push=%d/%d/%d keytarget_expose=%d/%d env_mc=%d/%d env_mc_E=%.2f/%d/%d env_mc_mode=%d/%d,%d/%d,%d/%d plan_count=%d/%d plan_reason=%d,%d,%d,%d,%d phase2a=%d/%d phase2a_plan=%d/%d phase2a_domain=%d,%d,%d,%d,%d,%d phase2a_reason=%d/%d,%d/%d,%d/%d,%d/%d phase2a_reason_press=%d/%d,%d/%d,%d/%d,%d/%d phase2a_reason_survive=%d/%d,%d/%d,%d/%d,%d/%d phase2a_press_reason_7plus_changed=%d sevenline=%d/%d sevenline_margin=%.2f/%d/%d sevenline_margin_hist=%d/%d/%d sevenline_margin1_reach_hist=%d/%d/%d sevenline_delay_hist=%d/%d/%d/%d sevenline_changed_delay_hist=%d/%d/%d/%d sevenline_changed_margin_hist=%d/%d/%d sevenline_bonus_stats=%.2f/%d/%d sevenline_low_reach_count=%d sevenline_press_applied_count=%d sevenline_d1_loss_reason=%d/%d/%d/%d sevenline_d1_override=%d/%d/%d sevenline_d1_override_blocked=%d/%d d1_gap_before=%.2f/%d/%d d1_gap_after=%.2f/%d/%d d1_gap_count=%d\n",
           debug_metrics->action_mc_trigger_count[1], debug_metrics->action_mc_penalty_applied_count[1],
           debug_metrics->action_mc_changed_choice_count[1], debug_metrics->action_mc_applied_best_count[1],
           debug_metrics->action_mc_total_candidates[1],
           cpu_action_mc_qdiff_avg, cpu_action_mc_qdiff_min, cpu_action_mc_qdiff_max, debug_metrics->action_mc_qdiff_lt0_count[1],
           debug_metrics->action_mc_qdiff_le_500_count[1], debug_metrics->action_mc_qdiff_le_1000_count[1],
           debug_metrics->action_mc_qdiff_le_2000_count[1], debug_metrics->action_mc_qdiff_le_4000_count[1],
           debug_metrics->action_mc_trigger_by_material[1][AI_ACTION_MC_MATERIAL_LIGHT],
           debug_metrics->action_mc_trigger_by_material[1][AI_ACTION_MC_MATERIAL_SAKE],
           debug_metrics->action_mc_trigger_by_material[1][AI_ACTION_MC_MATERIAL_AKATAN],
           debug_metrics->action_mc_trigger_by_material[1][AI_ACTION_MC_MATERIAL_AOTAN],
           debug_metrics->action_mc_trigger_by_material[1][AI_ACTION_MC_MATERIAL_ISC],
           debug_metrics->action_mc_trigger_by_material[1][AI_ACTION_MC_MATERIAL_AME],
           debug_metrics->action_mc_trigger_by_material[1][AI_ACTION_MC_MATERIAL_OTHER_HIGH],
           debug_metrics->action_mc_changed_by_material[1][AI_ACTION_MC_MATERIAL_LIGHT],
           debug_metrics->action_mc_changed_by_material[1][AI_ACTION_MC_MATERIAL_SAKE],
           debug_metrics->action_mc_changed_by_material[1][AI_ACTION_MC_MATERIAL_AKATAN],
           debug_metrics->action_mc_changed_by_material[1][AI_ACTION_MC_MATERIAL_AOTAN],
           debug_metrics->action_mc_changed_by_material[1][AI_ACTION_MC_MATERIAL_ISC],
           debug_metrics->action_mc_changed_by_material[1][AI_ACTION_MC_MATERIAL_AME],
           debug_metrics->action_mc_changed_by_material[1][AI_ACTION_MC_MATERIAL_OTHER_HIGH],
           debug_metrics->month_lock_trigger_count[1], debug_metrics->month_lock_changed_choice_count[1],
           debug_metrics->month_lock_hidden_key_trigger_count[1], debug_metrics->month_lock_hidden_key_changed_choice_count[1],
           debug_metrics->month_lock_hidden_deny_trigger_count[1], debug_metrics->month_lock_hidden_deny_changed_choice_count[1],
           debug_metrics->overpay_delay_penalty_applied_count[1], debug_metrics->overpay_delay_changed_choice_count[1],
           debug_metrics->certain_seven_plus_trigger_count[1], debug_metrics->certain_seven_plus_changed_choice_count[1],
           debug_metrics->mode_greedy_count[1], debug_metrics->mode_balanced_count[1], debug_metrics->mode_defensive_count[1],
           debug_metrics->combo7_trigger_count[1], debug_metrics->combo7_changed_choice_count[1],
           debug_metrics->combo7_greedy_trigger_count[1], debug_metrics->combo7_greedy_changed_choice_count[1],
           debug_metrics->combo7_balanced_trigger_count[1], debug_metrics->combo7_balanced_changed_choice_count[1],
           debug_metrics->combo7_defensive_trigger_count[1], debug_metrics->combo7_defensive_changed_choice_count[1],
           cpu_combo7_margin_avg, debug_metrics->combo7_margin_flip_count[1], debug_metrics->combo7_margin_lt_5k_count[1],
           debug_metrics->combo7_margin_lt_10k_count[1], debug_metrics->combo7_margin_lt_20k_count[1],
           debug_metrics->best_plus1_used_rank_count[1][0], debug_metrics->best_plus1_used_rank_count[1][1],
           debug_metrics->koikoi_base6_push_trigger_count[1], debug_metrics->koikoi_base6_push_applied_count[1],
           debug_metrics->koikoi_base6_push_flipped_count[1],
           debug_metrics->keytarget_expose_trigger_count[1], debug_metrics->keytarget_expose_changed_choice_count[1],
           debug_metrics->env_mc_trigger_count[1], debug_metrics->env_mc_changed_choice_count[1],
           cpu_env_mc_e_avg, cpu_env_mc_e_min, cpu_env_mc_e_max,
           debug_metrics->env_mc_mode_trigger_count[1][0], debug_metrics->env_mc_mode_changed_choice_count[1][0],
           debug_metrics->env_mc_mode_trigger_count[1][1], debug_metrics->env_mc_mode_changed_choice_count[1][1],
           debug_metrics->env_mc_mode_trigger_count[1][2], debug_metrics->env_mc_mode_changed_choice_count[1][2],
           debug_metrics->plan_count[1][AI_PLAN_PRESS], debug_metrics->plan_count[1][AI_PLAN_SURVIVE],
           debug_metrics->plan_reason_count[1][AI_PLAN_REASON_DIFF], debug_metrics->plan_reason_count[1][AI_PLAN_REASON_COMBO7],
           debug_metrics->plan_reason_count[1][AI_PLAN_REASON_SEVENLINE], debug_metrics->plan_reason_count[1][AI_PLAN_REASON_BLOCK_THREAT],
           debug_metrics->plan_reason_count[1][AI_PLAN_REASON_SURVIVE_DIFF],
           debug_metrics->phase2a_trigger_count[1], debug_metrics->phase2a_changed_choice_count[1],
           debug_metrics->phase2a_plan_count[1][AI_PLAN_PRESS], debug_metrics->phase2a_plan_count[1][AI_PLAN_SURVIVE],
           debug_metrics->phase2a_domain_count[1][AI_ENV_DOMAIN_NONE], debug_metrics->phase2a_domain_count[1][AI_ENV_DOMAIN_LIGHT],
           debug_metrics->phase2a_domain_count[1][AI_ENV_DOMAIN_SAKE], debug_metrics->phase2a_domain_count[1][AI_ENV_DOMAIN_AKATAN],
           debug_metrics->phase2a_domain_count[1][AI_ENV_DOMAIN_AOTAN], debug_metrics->phase2a_domain_count[1][AI_ENV_DOMAIN_ISC],
           debug_metrics->phase2a_reason_trigger_count[1][AI_PHASE2A_REASON_7PLUS], debug_metrics->phase2a_reason_changed_count[1][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->phase2a_reason_trigger_count[1][AI_PHASE2A_REASON_COMBO7], debug_metrics->phase2a_reason_changed_count[1][AI_PHASE2A_REASON_COMBO7],
           debug_metrics->phase2a_reason_trigger_count[1][AI_PHASE2A_REASON_HOT], debug_metrics->phase2a_reason_changed_count[1][AI_PHASE2A_REASON_HOT],
           debug_metrics->phase2a_reason_trigger_count[1][AI_PHASE2A_REASON_DEFENCE], debug_metrics->phase2a_reason_changed_count[1][AI_PHASE2A_REASON_DEFENCE],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_7PLUS], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_COMBO7], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_COMBO7],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_HOT], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_HOT],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_DEFENCE], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_DEFENCE],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_7PLUS], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_COMBO7], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_COMBO7],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_HOT], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_HOT],
           debug_metrics->phase2a_reason_trigger_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_DEFENCE], debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_SURVIVE][AI_PHASE2A_REASON_DEFENCE],
           debug_metrics->phase2a_reason_changed_by_plan[1][AI_PLAN_PRESS][AI_PHASE2A_REASON_7PLUS],
           debug_metrics->sevenline_trigger_count[1], debug_metrics->sevenline_changed_count[1],
           cpu_sevenline_margin_avg,
           debug_metrics->sevenline_margin_count[1] ? debug_metrics->sevenline_margin_min[1] : 0,
           debug_metrics->sevenline_margin_count[1] ? debug_metrics->sevenline_margin_max[1] : 0,
           debug_metrics->sevenline_margin_hist[1][0], debug_metrics->sevenline_margin_hist[1][1], debug_metrics->sevenline_margin_hist[1][2],
           debug_metrics->sevenline_margin1_reach_hist[1][0], debug_metrics->sevenline_margin1_reach_hist[1][1], debug_metrics->sevenline_margin1_reach_hist[1][2],
           debug_metrics->sevenline_delay_hist[1][0], debug_metrics->sevenline_delay_hist[1][1], debug_metrics->sevenline_delay_hist[1][2], debug_metrics->sevenline_delay_hist[1][3],
           debug_metrics->sevenline_changed_delay_hist[1][0], debug_metrics->sevenline_changed_delay_hist[1][1], debug_metrics->sevenline_changed_delay_hist[1][2], debug_metrics->sevenline_changed_delay_hist[1][3],
           debug_metrics->sevenline_changed_margin_hist[1][0], debug_metrics->sevenline_changed_margin_hist[1][1], debug_metrics->sevenline_changed_margin_hist[1][2],
           cpu_sevenline_bonus_avg,
           debug_metrics->sevenline_bonus_count[1] ? debug_metrics->sevenline_bonus_min[1] : 0,
           debug_metrics->sevenline_bonus_count[1] ? debug_metrics->sevenline_bonus_max[1] : 0,
           debug_metrics->sevenline_low_reach_count[1], debug_metrics->sevenline_press_applied_count[1],
           debug_metrics->sevenline_d1_loss_reason_count[1][AI_SEVENLINE_D1_LOSS_DEFENCE],
           debug_metrics->sevenline_d1_loss_reason_count[1][AI_SEVENLINE_D1_LOSS_COMBO7],
           debug_metrics->sevenline_d1_loss_reason_count[1][AI_SEVENLINE_D1_LOSS_DOMAIN_HOT],
           debug_metrics->sevenline_d1_loss_reason_count[1][AI_SEVENLINE_D1_LOSS_OTHER],
           debug_metrics->sevenline_d1_override_trigger_count[1], debug_metrics->sevenline_d1_override_applied_count[1],
           debug_metrics->sevenline_d1_override_changed_count[1],
           debug_metrics->sevenline_d1_override_blocked_count[1][AI_SEVENLINE_D1_OVERRIDE_BLOCK_KOIKOI_OPP],
           debug_metrics->sevenline_d1_override_blocked_count[1][AI_SEVENLINE_D1_OVERRIDE_BLOCK_INVALID],
           cpu_d1_gap_before_avg,
           debug_metrics->sevenline_d1_gap_count[1] ? debug_metrics->sevenline_d1_gap_min_before[1] : 0,
           debug_metrics->sevenline_d1_gap_count[1] ? debug_metrics->sevenline_d1_gap_max_before[1] : 0,
           cpu_d1_gap_after_avg,
           debug_metrics->sevenline_d1_gap_count[1] ? debug_metrics->sevenline_d1_gap_min_after[1] : 0,
           debug_metrics->sevenline_d1_gap_count[1] ? debug_metrics->sevenline_d1_gap_max_after[1] : 0,
           debug_metrics->sevenline_d1_gap_count[1]);
    printf("DEBUG P1 endgame=%d/%d endgame_k_hist=%d/%d/%d/%d endgame_changed_needed_hist=%d/%d/%d/%d\n",
           debug_metrics->endgame_trigger_count[0], debug_metrics->endgame_changed_choice_count[0],
           debug_metrics->endgame_k_hist[0][0], debug_metrics->endgame_k_hist[0][1], debug_metrics->endgame_k_hist[0][2], debug_metrics->endgame_k_hist[0][3],
           debug_metrics->endgame_changed_needed_hist[0][0], debug_metrics->endgame_changed_needed_hist[0][1],
           debug_metrics->endgame_changed_needed_hist[0][2], debug_metrics->endgame_changed_needed_hist[0][3]);
    printf("DEBUG CPU endgame=%d/%d endgame_k_hist=%d/%d/%d/%d endgame_changed_needed_hist=%d/%d/%d/%d\n",
           debug_metrics->endgame_trigger_count[1], debug_metrics->endgame_changed_choice_count[1],
           debug_metrics->endgame_k_hist[1][0], debug_metrics->endgame_k_hist[1][1], debug_metrics->endgame_k_hist[1][2], debug_metrics->endgame_k_hist[1][3],
           debug_metrics->endgame_changed_needed_hist[1][0], debug_metrics->endgame_changed_needed_hist[1][1],
           debug_metrics->endgame_changed_needed_hist[1][2], debug_metrics->endgame_changed_needed_hist[1][3]);
#endif
    if (loops >= 2) {
        printf("WIN_RATE P1=%.2f%%(%.2fpts) CPU=%.2f%%(%.2fpts)\n", p1_win_rate, p1_avg_win_score, cpu_win_rate, cpu_avg_win_score);
        for (int round = 0; round < round_max; round++) {
            double p1_round_rate = (metrics->round_wins[0][round] * 100.0) / loops;
            double cpu_round_rate = (metrics->round_wins[1][round] * 100.0) / loops;
            double p1_round_avg = metrics->round_wins[0][round] ? metrics->round_win_score_sum[0][round] / (double)metrics->round_wins[0][round] : 0.0;
            double cpu_round_avg = metrics->round_wins[1][round] ? metrics->round_win_score_sum[1][round] / (double)metrics->round_wins[1][round] : 0.0;
            printf("- Round %d: P1=%.2f%%(%.2fpts) CPU=%.2f%%(%.2fpts)\n", round + 1, p1_round_rate, p1_round_avg, cpu_round_rate, cpu_round_avg);
        }
        printf("Per Bias:\n");
        for (int mode_index = 0; mode_index < 3; mode_index++) {
            int mode;
            int win_count;
            int lose_count;
            int draw_count;
            int total;
            double win_pct;
            double lose_pct;
            double draw_pct;

            if (mode_index == 0) {
                mode = MODE_GREEDY;
            } else if (mode_index == 1) {
                mode = MODE_DEFENSIVE;
            } else {
                mode = MODE_BALANCED;
            }
            win_count = metrics->bias_round_result[mode][0];
            lose_count = metrics->bias_round_result[mode][1];
            draw_count = metrics->bias_round_result[mode][2];
            total = win_count + lose_count + draw_count;
            win_pct = total ? (win_count * 100.0) / total : 0.0;
            lose_pct = total ? (lose_count * 100.0) / total : 0.0;
            draw_pct = total ? (draw_count * 100.0) / total : 0.0;
            printf("- %s: win=%d (%.1f%%), lose=%d (%.1f%%), draw=%d (%.1f%%)\n", strategy_mode_name(mode), win_count, win_pct, lose_count, lose_pct,
                   draw_count, draw_pct);
        }
    }
    return 0;
}
