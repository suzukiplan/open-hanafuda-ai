#include "ai.h"
#include <limits.h>

static int stenv_find_hand_index(int player, Card* card)
{
    int i;

    if (!card) {
        return -1;
    }
    for (i = 0; i < g.own[player].num; i++) {
        if (g.own[player].cards[i] == card) {
            return i;
        }
    }
    return -1;
}

static void stenv_capture_card(int player, Card* card)
{
    if (!card) {
        return;
    }
    g.invent[player][card->type].cards[g.invent[player][card->type].num++] = card;
}

static int stenv_eval_select_target(int player, int hand_index, Card* hand_card, int deck_index)
{
    CardSet own_backup;
    CardSet deck_backup;
    CardSet invent_backup[4];
    int score;

    vgs_memcpy(&own_backup, &g.own[player], sizeof(CardSet));
    vgs_memcpy(&deck_backup, &g.deck, sizeof(CardSet));
    vgs_memcpy(invent_backup, g.invent[player], sizeof(CardSet) * 4);

    if (0 <= hand_index && hand_index < g.own[player].num && g.own[player].cards[hand_index] == hand_card) {
        g.own[player].cards[hand_index] = NULL;
        if (g.own[player].num > 0) {
            g.own[player].num--;
        }
    }

    if (0 <= deck_index && deck_index < 48 && g.deck.cards[deck_index]) {
        Card* taken = g.deck.cards[deck_index];
        stenv_capture_card(player, hand_card);
        stenv_capture_card(player, taken);
        g.deck.cards[deck_index] = NULL;
        if (g.deck.num > 0) {
            g.deck.num--;
        }
    } else if (0 <= deck_index && deck_index < 48) {
        g.deck.cards[deck_index] = hand_card;
        g.deck.num++;
    }

    score = ai_env_score(player);

    vgs_memcpy(&g.own[player], &own_backup, sizeof(CardSet));
    vgs_memcpy(&g.deck, &deck_backup, sizeof(CardSet));
    vgs_memcpy(g.invent[player], invent_backup, sizeof(CardSet) * 4);
    return score;
}

int ai_stenv_select(int player, Card* card)
{
    int current_score;
    int hand_index;
    int targets[48];
    int target_count;
    int best_deck;
    int best_gain;
    int found;
    int ti;

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

    hand_index = stenv_find_hand_index(player, card);
    current_score = ai_env_score(player);
    best_deck = targets[0];
    best_gain = INT_MIN;
    found = OFF;

    for (ti = 0; ti < target_count; ti++) {
        int deck_index = targets[ti];
        int gain = stenv_eval_select_target(player, hand_index, card, deck_index) - current_score;
        if (!found || gain > best_gain || (gain == best_gain && deck_index < best_deck)) {
            best_gain = gain;
            best_deck = deck_index;
            found = ON;
        }
    }

    return best_deck;
}
