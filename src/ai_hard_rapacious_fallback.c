#include "ai.h"
#include <limits.h>

typedef struct {
    int env_gain;
    int immediate_gain;
    int immediate_base;
} RapaciousFallbackEval;

static int hard_rapacious_fallback_find_hand_index(int player, Card* card)
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

static void hard_rapacious_fallback_capture_card(int player, Card* card)
{
    if (!card) {
        return;
    }
    g.invent[player][card->type].cards[g.invent[player][card->type].num++] = card;
}

static int hard_rapacious_fallback_eval_target(int player, int hand_index, Card* hand_card, int deck_index, int remove_from_hand,
                                               RapaciousFallbackEval* out_eval)
{
    CardSet own_backup;
    CardSet deck_backup;
    CardSet invent_backup[4];
    int score;
    int current_score;
    int taken_card_no = -1;
    int immediate_gain = 0;
    int immediate_wid = -1;

    if (out_eval) {
        out_eval->env_gain = INT_MIN;
        out_eval->immediate_gain = 0;
        out_eval->immediate_base = 0;
    }

    vgs_memcpy(&own_backup, &g.own[player], sizeof(CardSet));
    vgs_memcpy(&deck_backup, &g.deck, sizeof(CardSet));
    vgs_memcpy(invent_backup, g.invent[player], sizeof(CardSet) * 4);
    current_score = ai_env_score(player);

    if (remove_from_hand && 0 <= hand_index && hand_index < g.own[player].num && g.own[player].cards[hand_index] == hand_card) {
        g.own[player].cards[hand_index] = NULL;
        if (g.own[player].num > 0) {
            g.own[player].num--;
        }
    }

    if (0 <= deck_index && deck_index < 48 && g.deck.cards[deck_index]) {
        Card* taken = g.deck.cards[deck_index];
        taken_card_no = taken->id;
        hard_rapacious_fallback_capture_card(player, hand_card);
        hard_rapacious_fallback_capture_card(player, taken);
        g.deck.cards[deck_index] = NULL;
        if (g.deck.num > 0) {
            g.deck.num--;
        }
    } else if (0 <= deck_index && deck_index < 48) {
        g.deck.cards[deck_index] = hand_card;
        g.deck.num++;
    }

    score = ai_env_score(player);
    if (taken_card_no >= 0) {
        int cards[2];
        cards[0] = hand_card ? hand_card->id : -1;
        cards[1] = taken_card_no;
        immediate_gain = analyze_score_gain_with_card_ids(player, cards, 2, &immediate_wid);
    }
    if (out_eval) {
        out_eval->env_gain = score - current_score;
        out_eval->immediate_gain = immediate_gain;
        out_eval->immediate_base = immediate_wid >= 0 ? winning_hands[immediate_wid].base_score : 0;
    }

    vgs_memcpy(&g.own[player], &own_backup, sizeof(CardSet));
    vgs_memcpy(&g.deck, &deck_backup, sizeof(CardSet));
    vgs_memcpy(g.invent[player], invent_backup, sizeof(CardSet) * 4);
    return score - current_score;
}

static int rapacious_fallback_eval_better(const RapaciousFallbackEval* lhs, const RapaciousFallbackEval* rhs)
{
    if (!lhs) {
        return OFF;
    }
    if (!rhs) {
        return ON;
    }
    if (lhs->immediate_gain != rhs->immediate_gain) {
        return lhs->immediate_gain > rhs->immediate_gain ? ON : OFF;
    }
    if (lhs->immediate_base != rhs->immediate_base) {
        return lhs->immediate_base > rhs->immediate_base ? ON : OFF;
    }
    if (lhs->env_gain != rhs->env_gain) {
        return lhs->env_gain > rhs->env_gain ? ON : OFF;
    }
    return OFF;
}

int ai_hard_rapacious_fallback_koikoi(int player, int round_score)
{
    return ai_normal_koikoi(player, round_score);
}

int ai_hard_rapacious_fallback_drop(int player)
{
    int best_hand_index = 0;
    RapaciousFallbackEval best_eval = {INT_MIN, 0, 0};
    int found = OFF;
    int i;

    for (i = 0; i < g.own[player].num; i++) {
        int targets[48];
        int target_count;
        Card* hand_card = g.own[player].cards[i];

        if (!hand_card) {
            continue;
        }

        target_count = collect_target_deck_indexes(hand_card, targets);
        if (target_count <= 0) {
            int deck_index = find_empty_deck_slot();
            RapaciousFallbackEval eval;

            if (deck_index < 0) {
                continue;
            }
            hard_rapacious_fallback_eval_target(player, i, hand_card, deck_index, ON, &eval);
            if (!found || rapacious_fallback_eval_better(&eval, &best_eval) ||
                (!rapacious_fallback_eval_better(&best_eval, &eval) && i < best_hand_index)) {
                best_eval = eval;
                best_hand_index = i;
                found = ON;
            }
            continue;
        }

        for (int ti = 0; ti < target_count; ti++) {
            RapaciousFallbackEval eval;
            hard_rapacious_fallback_eval_target(player, i, hand_card, targets[ti], ON, &eval);
            if (!found || rapacious_fallback_eval_better(&eval, &best_eval) ||
                (!rapacious_fallback_eval_better(&best_eval, &eval) && i < best_hand_index)) {
                best_eval = eval;
                best_hand_index = i;
                found = ON;
            }
        }
    }

    if (found) {
        return best_hand_index;
    }

    if (g.own[player].num > 0 && g.own[player].cards[0]) {
        return hard_rapacious_fallback_find_hand_index(player, g.own[player].cards[0]);
    }
    return 0;
}

int ai_hard_rapacious_fallback_select(int player, Card* card)
{
    int hand_index;
    int targets[48];
    int target_count;
    int best_deck;
    RapaciousFallbackEval best_eval = {INT_MIN, 0, 0};
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

    hand_index = hard_rapacious_fallback_find_hand_index(player, card);
    best_deck = targets[0];
    found = OFF;

    for (ti = 0; ti < target_count; ti++) {
        int deck_index = targets[ti];
        RapaciousFallbackEval eval;
        hard_rapacious_fallback_eval_target(player, hand_index, card, deck_index, ON, &eval);
        if (!found || rapacious_fallback_eval_better(&eval, &best_eval) ||
            (!rapacious_fallback_eval_better(&best_eval, &eval) && deck_index < best_deck)) {
            best_eval = eval;
            best_deck = deck_index;
            found = ON;
        }
    }

    return best_deck;
}
