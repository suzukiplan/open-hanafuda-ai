#include "ai.h"
#include <limits.h>

typedef struct {
    long long sum;
    int count;
    int max;
    int min;
} McenvForecastStats;

static void mcenv_backup_state(CardSet own_backup[2], CardSet* deck_backup, CardSet invent_backup[2][4])
{
    vgs_memcpy(own_backup, g.own, sizeof(g.own));
    vgs_memcpy(deck_backup, &g.deck, sizeof(CardSet));
    vgs_memcpy(invent_backup, g.invent, sizeof(g.invent));
}

static void mcenv_restore_state(const CardSet own_backup[2], const CardSet* deck_backup, const CardSet invent_backup[2][4])
{
    vgs_memcpy(g.own, own_backup, sizeof(g.own));
    vgs_memcpy(&g.deck, deck_backup, sizeof(CardSet));
    vgs_memcpy(g.invent, invent_backup, sizeof(g.invent));
}

static int mcenv_card_is_visible_to_player(int player, Card* card)
{
    int p;
    int t;
    int i;

    if (!card) {
        return OFF;
    }

    for (i = 0; i < g.own[player].num; i++) {
        if (g.own[player].cards[i] == card) {
            return ON;
        }
    }

    for (p = 0; p < 2; p++) {
        for (t = 0; t < 4; t++) {
            for (i = 0; i < g.invent[p][t].num; i++) {
                if (g.invent[p][t].cards[i] == card) {
                    return ON;
                }
            }
        }
    }

    for (i = 0; i < 48; i++) {
        if (g.deck.cards[i] == card) {
            return ON;
        }
    }

    return OFF;
}

static int mcenv_build_hidden_cards(int player, Card** hidden_cards, int cap)
{
    int count = 0;
    int i;

    if (!hidden_cards || cap <= 0) {
        return 0;
    }

    for (i = 0; i < 48 && count < cap; i++) {
        Card* card = &g.cards[i];
        if (!mcenv_card_is_visible_to_player(player, card)) {
            hidden_cards[count++] = card;
        }
    }
    return count;
}

static int mcenv_find_valid_target(int preferred_target, const int* targets, int target_count)
{
    int i;

    if (!targets || target_count <= 0) {
        return find_empty_deck_slot();
    }
    if (target_count == 1) {
        return targets[0];
    }
    if (target_count >= 3) {
        return targets[0];
    }
    for (i = 0; i < target_count; i++) {
        if (targets[i] == preferred_target) {
            return preferred_target;
        }
    }
    return targets[0];
}

static int mcenv_choose_target_deck(int player, Card* card)
{
    int targets[48];
    int target_count;
    int preferred_target;

    if (!card) {
        return find_empty_deck_slot();
    }

    target_count = collect_target_deck_indexes(card, targets);
    if (target_count <= 0) {
        return find_empty_deck_slot();
    }
    if (target_count == 1) {
        return targets[0];
    }
    if (target_count >= 3) {
        return targets[0];
    }

    preferred_target = ai_mcenv_select(player, card);
    return mcenv_find_valid_target(preferred_target, targets, target_count);
}

static void mcenv_capture_card(int player, Card* card)
{
    if (!card) {
        return;
    }
    g.invent[player][card->type].cards[g.invent[player][card->type].num++] = card;
}

static void mcenv_remove_own_card(int player, int hand_index)
{
    int i;

    if (hand_index < 0 || hand_index >= g.own[player].num) {
        return;
    }
    for (i = hand_index; i < 7; i++) {
        g.own[player].cards[i] = g.own[player].cards[i + 1];
    }
    g.own[player].cards[7] = NULL;
    if (g.own[player].num > 0) {
        g.own[player].num--;
    }
}

static void mcenv_apply_play(int player, Card* card, int hand_index)
{
    int targets[48];
    int target_count;
    int target_deck;
    int i;

    if (!card) {
        return;
    }

    if (hand_index >= 0) {
        mcenv_remove_own_card(player, hand_index);
    }

    target_count = collect_target_deck_indexes(card, targets);
    if (target_count <= 0) {
        target_deck = find_empty_deck_slot();
        if (0 <= target_deck && target_deck < 48) {
            g.deck.cards[target_deck] = card;
            g.deck.num++;
        }
        return;
    }

    mcenv_capture_card(player, card);
    if (target_count >= 3) {
        for (i = target_count - 1; i >= 0; i--) {
            Card* taken = g.deck.cards[targets[i]];
            if (!taken) {
                continue;
            }
            mcenv_capture_card(player, taken);
            g.deck.cards[targets[i]] = NULL;
            if (g.deck.num > 0) {
                g.deck.num--;
            }
        }
        return;
    }

    target_deck = mcenv_choose_target_deck(player, card);
    for (i = 0; i < target_count; i++) {
        if (targets[i] == target_deck) {
            Card* taken = g.deck.cards[target_deck];
            if (taken) {
                mcenv_capture_card(player, taken);
                g.deck.cards[target_deck] = NULL;
                if (g.deck.num > 0) {
                    g.deck.num--;
                }
            }
            return;
        }
    }
}

static void mcenv_record_score(McenvForecastStats* stats, int score)
{
    if (!stats) {
        return;
    }
    stats->sum += score;
    stats->count++;
    if (score > stats->max) {
        stats->max = score;
    }
    if (score < stats->min) {
        stats->min = score;
    }
}

static void mcenv_forecast_after_opp_open(int player, Card** hidden_cards, int hidden_count, int used_draw_index, int used_drop_index, McenvForecastStats* stats)
{
    int i;
    int emitted = OFF;

    for (i = 0; i < hidden_count; i++) {
        CardSet own_backup[2];
        CardSet deck_backup;
        CardSet invent_backup[2][4];

        if (i == used_draw_index || i == used_drop_index) {
            continue;
        }

        mcenv_backup_state(own_backup, &deck_backup, invent_backup);
        mcenv_apply_play(1 - player, hidden_cards[i], -1);
        mcenv_record_score(stats, ai_env_score(player));
        mcenv_restore_state(own_backup, &deck_backup, invent_backup);
        emitted = ON;
    }

    if (!emitted) {
        mcenv_record_score(stats, ai_env_score(player));
    }
}

static int mcenv_choose_opp_hidden_drop_index(int player, Card** hidden_cards, int hidden_count, int used_draw_index)
{
    int opp = 1 - player;
    CardSet own_backup[2];
    InventStats stats_backup[2];
    int hidden_index_by_hand[48];
    int hidden_hand_count = 0;
    int selected_hand_index;
    int i;

    vgs_memcpy(own_backup, g.own, sizeof(g.own));
    vgs_memcpy(stats_backup, g.stats, sizeof(g.stats));
    vgs_memset(&g.own[opp], 0, sizeof(CardSet));

    for (i = 0; i < hidden_count && hidden_hand_count < 48; i++) {
        if (i == used_draw_index || !hidden_cards[i]) {
            continue;
        }
        g.own[opp].cards[hidden_hand_count] = hidden_cards[i];
        hidden_index_by_hand[hidden_hand_count] = i;
        hidden_hand_count++;
    }
    g.own[opp].num = hidden_hand_count;

    if (hidden_hand_count <= 0) {
        vgs_memcpy(g.own, own_backup, sizeof(g.own));
        vgs_memcpy(g.stats, stats_backup, sizeof(g.stats));
        return -1;
    }

    analyze_invent(opp);
    selected_hand_index = ai_normal_drop(opp);
    if (selected_hand_index < 0 || selected_hand_index >= hidden_hand_count) {
        selected_hand_index = 0;
    }

    vgs_memcpy(g.own, own_backup, sizeof(g.own));
    vgs_memcpy(g.stats, stats_backup, sizeof(g.stats));
    return hidden_index_by_hand[selected_hand_index];
}

static void mcenv_forecast_after_opp_drop(int player, Card** hidden_cards, int hidden_count, int used_draw_index, McenvForecastStats* stats)
{
    int used_drop_index = mcenv_choose_opp_hidden_drop_index(player, hidden_cards, hidden_count, used_draw_index);

    if (used_drop_index < 0 || used_drop_index >= hidden_count || !hidden_cards[used_drop_index]) {
        mcenv_record_score(stats, ai_env_score(player));
        return;
    }

    mcenv_apply_play(1 - player, hidden_cards[used_drop_index], -1);
    mcenv_forecast_after_opp_open(player, hidden_cards, hidden_count, used_draw_index, used_drop_index, stats);
}

static void mcenv_forecast_for_card(int player, int hand_index, Card** hidden_cards, int hidden_count, McenvForecastStats* stats)
{
    CardSet own_backup[2];
    CardSet deck_backup;
    CardSet invent_backup[2][4];
    Card* hand_card;
    int i;
    int emitted = OFF;

    if (!stats || hand_index < 0 || hand_index >= g.own[player].num) {
        return;
    }

    hand_card = g.own[player].cards[hand_index];
    if (!hand_card) {
        return;
    }

    mcenv_backup_state(own_backup, &deck_backup, invent_backup);
    mcenv_apply_play(player, hand_card, hand_index);

    for (i = 0; i < hidden_count; i++) {
        CardSet own_backup2[2];
        CardSet deck_backup2;
        CardSet invent_backup2[2][4];

        mcenv_backup_state(own_backup2, &deck_backup2, invent_backup2);
        mcenv_apply_play(player, hidden_cards[i], -1);
        mcenv_forecast_after_opp_drop(player, hidden_cards, hidden_count, i, stats);
        mcenv_restore_state(own_backup2, &deck_backup2, invent_backup2);
        emitted = ON;
    }

    if (!emitted) {
        mcenv_record_score(stats, ai_env_score(player));
    }

    mcenv_restore_state(own_backup, &deck_backup, invent_backup);
}

int ai_mcenv_drop(int player)
{
    Card* hidden_cards[48];
    McenvForecastStats stats[8];
    int hidden_count;
    int best_hand_index = 0;
    int best_ave = INT_MIN;
    int best_max = INT_MIN;
    int best_min = INT_MIN;
    int i;

    if (player < 0 || player > 1 || g.own[player].num <= 0) {
        return 0;
    }

    hidden_count = mcenv_build_hidden_cards(player, hidden_cards, 48);
    ai_putlog("EnvScore forecast:");

    for (i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        int ave;

        stats[i].sum = 0;
        stats[i].count = 0;
        stats[i].max = INT_MIN;
        stats[i].min = INT_MAX;

        if (!card) {
            continue;
        }

        mcenv_forecast_for_card(player, i, hidden_cards, hidden_count, &stats[i]);
        if (stats[i].count <= 0) {
            stats[i].count = 1;
            stats[i].sum = ai_env_score(player);
            stats[i].max = (int)stats[i].sum;
            stats[i].min = (int)stats[i].sum;
        }

        ave = (int)(stats[i].sum / stats[i].count);
        ai_putlog("%d. card=%d, max=%d, ave=%d, min=%d", i + 1, card->id, stats[i].max, ave, stats[i].min);

        if (ave > best_ave ||
            (ave == best_ave && stats[i].max > best_max) ||
            (ave == best_ave && stats[i].max == best_max && stats[i].min > best_min) ||
            (ave == best_ave && stats[i].max == best_max && stats[i].min == best_min && i < best_hand_index)) {
            best_ave = ave;
            best_max = stats[i].max;
            best_min = stats[i].min;
            best_hand_index = i;
        }
    }

    return best_hand_index;
}
