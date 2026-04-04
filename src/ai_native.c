#include "ai_native.h"
#include "cards.h"
#include "ai.h"

static NativeAiSnapshot native_ai_snapshot;
static int native_ai_enabled = -1;

static int is_snapshot_card_ptr_valid(Card* card)
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
    return &g.cards[card->id] == card ? ON : OFF;
}

static int32_t snapshot_card_id(Card* card)
{
    return is_snapshot_card_ptr_valid(card) ? card->id : -1;
}

static int32_t snapshot_winning_hand_id(WinningHand* hand)
{
    if (!hand) {
        return -1;
    }
    if (hand < &winning_hands[0] || hand >= &winning_hands[WINNING_HAND_MAX]) {
        return -1;
    }
    if (hand->id < 0 || hand->id >= WINNING_HAND_MAX) {
        return -1;
    }
    return hand->id;
}

static void snapshot_card_set48(NativeAiCardSet48Snapshot* dst, const CardSet* src)
{
    int i;
    int count = 0;
    for (i = 0; i < 48; i++) {
        dst->cards[i] = snapshot_card_id(src->cards[i]);
        if (dst->cards[i] >= 0) {
            count++;
        }
    }
    dst->start = src->start;
    dst->num = count;
}

static void snapshot_card_set8(NativeAiCardSet8Snapshot* dst, const CardSet* src)
{
    int i;
    int count = 0;
    for (i = 0; i < 8; i++) {
        dst->cards[i] = snapshot_card_id(src->cards[i]);
        if (src->cards[i]) {
            count++;
        }
    }
    dst->start = src->start;
    dst->num = count;
}

static void snapshot_strategy(NativeAiStrategySnapshot* dst, const StrategyData* src)
{
    int i;
    for (i = 0; i < WINNING_HAND_MAX; i++) {
        dst->reach[i] = src->reach[i];
        dst->delay[i] = src->delay[i];
        dst->score[i] = src->score[i];
        dst->risk_reach_estimate[i] = src->risk_reach_estimate[i];
        dst->risk_delay[i] = src->risk_delay[i];
        dst->risk_completion_score[i] = src->risk_completion_score[i];
        dst->risk_score[i] = src->risk_score[i];
        dst->priority_speed[i] = src->priority_speed[i];
        dst->priority_score[i] = src->priority_score[i];
    }
    dst->risk_7plus_distance = src->risk_7plus_distance;
    dst->opponent_win_x2 = src->opponent_win_x2;
    dst->left_own = src->left_own;
    dst->left_rounds = src->left_rounds;
    dst->round_max = src->round_max;
    dst->match_score_diff = src->match_score_diff;
    dst->koikoi_opp = src->koikoi_opp;
    dst->koikoi_mine = src->koikoi_mine;
    dst->opp_coarse_threat = src->opp_coarse_threat;
    dst->opp_exact_7plus_threat = src->opp_exact_7plus_threat;
    dst->risk_mult_pct = src->risk_mult_pct;
    dst->greedy_need = src->greedy_need;
    dst->defensive_need = src->defensive_need;
    dst->base_now = src->base_now;
    dst->best_additional_1pt_reach = src->best_additional_1pt_reach;
    dst->best_additional_1pt_delay = src->best_additional_1pt_delay;
    dst->can_overpay_akatan = src->can_overpay_akatan;
    dst->can_overpay_aotan = src->can_overpay_aotan;
    dst->can_overpay_ino = src->can_overpay_ino;
    dst->env_total = src->env_total;
    dst->env_diff = src->env_diff;
    for (i = 0; i < ENV_CAT_MAX; i++) {
        dst->env_cat_sum[i] = src->env_cat_sum[i];
    }
    dst->domain = src->domain;
    dst->plan = src->plan;
    dst->mode = src->mode;
    dst->bias_offence = src->bias.offence;
    dst->bias_speed = src->bias.speed;
    dst->bias_defence = src->bias.defence;
}

static void snapshot_stats(NativeAiInventStatsSnapshot* dst, const InventStats* src)
{
    int i;
    int wh_count = 0;
    for (i = 0; i < WINNING_HAND_MAX; i++) {
        dst->wh[i] = snapshot_winning_hand_id(src->wh[i]);
        if (dst->wh[i] >= 0) {
            wh_count++;
        }
        dst->base_up[i] = src->base_up[i];
        snapshot_card_set48(&dst->wcs[i], &src->wcs[i]);
    }
    dst->wh_count = wh_count;
    dst->score = src->score;
    snapshot_card_set48(&dst->want, &src->want);
}

static void restore_strategy(StrategyData* dst, const NativeAiStrategySnapshot* src)
{
    int i;
    for (i = 0; i < WINNING_HAND_MAX; i++) {
        dst->reach[i] = src->reach[i];
        dst->delay[i] = src->delay[i];
        dst->score[i] = src->score[i];
        dst->risk_reach_estimate[i] = src->risk_reach_estimate[i];
        dst->risk_delay[i] = src->risk_delay[i];
        dst->risk_completion_score[i] = src->risk_completion_score[i];
        dst->risk_score[i] = src->risk_score[i];
        dst->priority_speed[i] = src->priority_speed[i];
        dst->priority_score[i] = src->priority_score[i];
    }
    dst->risk_7plus_distance = src->risk_7plus_distance;
    dst->opponent_win_x2 = src->opponent_win_x2;
    dst->left_own = src->left_own;
    dst->left_rounds = src->left_rounds;
    dst->round_max = src->round_max;
    dst->match_score_diff = src->match_score_diff;
    dst->koikoi_opp = src->koikoi_opp;
    dst->koikoi_mine = src->koikoi_mine;
    dst->opp_coarse_threat = src->opp_coarse_threat;
    dst->opp_exact_7plus_threat = src->opp_exact_7plus_threat;
    dst->risk_mult_pct = src->risk_mult_pct;
    dst->greedy_need = src->greedy_need;
    dst->defensive_need = src->defensive_need;
    dst->base_now = src->base_now;
    dst->best_additional_1pt_reach = src->best_additional_1pt_reach;
    dst->best_additional_1pt_delay = src->best_additional_1pt_delay;
    dst->can_overpay_akatan = src->can_overpay_akatan;
    dst->can_overpay_aotan = src->can_overpay_aotan;
    dst->can_overpay_ino = src->can_overpay_ino;
    dst->env_total = src->env_total;
    dst->env_diff = src->env_diff;
    for (i = 0; i < ENV_CAT_MAX; i++) {
        dst->env_cat_sum[i] = src->env_cat_sum[i];
    }
    dst->domain = src->domain;
    dst->plan = src->plan;
    dst->mode = src->mode;
    dst->bias.offence = src->bias_offence;
    dst->bias.speed = src->bias_speed;
    dst->bias.defence = src->bias_defence;
}

static void restore_native_ai_results(void)
{
    restore_strategy(&g.strategy[0], &native_ai_snapshot.strategy[0]);
    restore_strategy(&g.strategy[1], &native_ai_snapshot.strategy[1]);
    ai_watch_min_import_action(0, (const char*)native_ai_snapshot.watch_min_action[0]);
    ai_watch_min_import_action(1, (const char*)native_ai_snapshot.watch_min_action[1]);
}

static void build_native_ai_snapshot(void)
{
    int i;
    int p;
    int t;

    for (i = 0; i < 48; i++) {
        native_ai_snapshot.cards[i].type = g.cards[i].type;
        native_ai_snapshot.cards[i].reversed = g.cards[i].reversed;
        native_ai_snapshot.cards[i].month = g.cards[i].month;
        native_ai_snapshot.cards[i].index = g.cards[i].index;
        native_ai_snapshot.cards[i].id = g.cards[i].id;
    }

    native_ai_snapshot.frame = g.frame;
    snapshot_card_set48(&native_ai_snapshot.floor, &g.floor);
    snapshot_card_set48(&native_ai_snapshot.deck, &g.deck);
    for (p = 0; p < 2; p++) {
        snapshot_card_set8(&native_ai_snapshot.own[p], &g.own[p]);
        for (t = 0; t < 4; t++) {
            snapshot_card_set48(&native_ai_snapshot.invent[p][t], &g.invent[p][t]);
        }
        snapshot_stats(&native_ai_snapshot.stats[p], &g.stats[p]);
        native_ai_snapshot.total_score[p] = g.total_score[p];
        native_ai_snapshot.koikoi[p] = g.koikoi[p];
        native_ai_snapshot.koikoi_score[p] = g.koikoi_score[p];
        snapshot_strategy(&native_ai_snapshot.strategy[p], &g.strategy[p]);
    }

    native_ai_snapshot.round_max = g.round_max;
    native_ai_snapshot.round = g.round;
    native_ai_snapshot.oya = g.oya;
    native_ai_snapshot.current_player = g.current_player;
    native_ai_snapshot.auto_play = g.auto_play;
    native_ai_snapshot.no_sake = g.no_sake;
    native_ai_snapshot.ai_model[0] = g.ai_model[0];
    native_ai_snapshot.ai_model[1] = g.ai_model[1];
    native_ai_snapshot.watch_min_action[0][0] = '\0';
    native_ai_snapshot.watch_min_action[1][0] = '\0';
}

int native_ai_available(void)
{
#ifdef VGS_HOST_SIM
    return OFF;
#else
    if (native_ai_enabled < 0) {
        native_ai_enabled = *((volatile uint32_t*)IN_NATIVE_AI_AVAILABLE) == ON ? ON : OFF;
    }
    return native_ai_enabled;
#endif
}

int native_ai_execute(int command, int player, int arg, int* result)
{
#ifdef VGS_HOST_SIM
    (void)command;
    (void)player;
    (void)arg;
    (void)result;
    return OFF;
#else
    int status;
    uint32_t watchdog = 0;

    if (!native_ai_available()) {
        return OFF;
    }

    build_native_ai_snapshot();
    *((volatile uint32_t*)OUT_NATIVE_AI_SNAPSHOT_ADDR) = (uint32_t)&native_ai_snapshot;
    *((volatile uint32_t*)OUT_NATIVE_AI_ARG0) = (uint32_t)player;
    *((volatile uint32_t*)OUT_NATIVE_AI_ARG1) = (uint32_t)arg;

    ai_think_start();
    *((volatile uint32_t*)OUT_NATIVE_AI_COMMAND) = (uint32_t)command;
    while (ON) {
        status = *((volatile uint32_t*)IN_NATIVE_AI_STATUS);
        if (status == NATIVE_AI_STATUS_DONE) {
            if (result) {
                *result = *((volatile int32_t*)IN_NATIVE_AI_RESULT);
            }
            restore_native_ai_results();
            ai_think_end();
            return ON;
        }
        if (status == NATIVE_AI_STATUS_ERROR || status == NATIVE_AI_STATUS_UNAVAILABLE) {
            native_ai_enabled = OFF;
            ai_think_end();
            return OFF;
        }
        if (status != NATIVE_AI_STATUS_IDLE && status != NATIVE_AI_STATUS_BUSY) {
            native_ai_enabled = OFF;
            ai_think_end();
            return OFF;
        }
        ai_vsync();
        watchdog++;
        if (watchdog > 1800) {
            native_ai_enabled = OFF;
            ai_think_end();
            return OFF;
        }
    }
#endif
}
