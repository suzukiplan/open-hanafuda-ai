#include "ai.h"

int ai_normal_select(int player, Card* card)
{
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

    for (int wi = 0; wi < g.stats[player].want.num; wi++) {
        Card* wantCard = g.stats[player].want.cards[wi];
        if (!wantCard) {
            continue;
        }
        for (int ti = 0; ti < targetDeckCount; ti++) {
            int deckIndex = targetDecks[ti];
            if (g.deck.cards[deckIndex] == wantCard) {
                return deckIndex;
            }
        }
    }

    int capturedCounts[4];
    for (int t = 0; t < 4; t++) {
        capturedCounts[t] = g.invent[player][t].num;
    }
    static const int baseCapturePriority[4] = {
        400, // CARD_TYPE_GOKOU
        300, // CARD_TYPE_TANE
        200, // CARD_TYPE_TAN
        100  // CARD_TYPE_KASU
    };

    int bestDeck = targetDecks[0];
    int bestPriority = -1;
    for (int ti = 0; ti < targetDeckCount; ti++) {
        int deckIndex = targetDecks[ti];
        Card* deckCard = g.deck.cards[deckIndex];
        if (!deckCard) {
            continue;
        }
        int type = deckCard->type;
        int priority = baseCapturePriority[type] + capturedCounts[type] * 32;
        if (type == CARD_TYPE_TANE && capturedCounts[type] >= 2) {
            priority += 1000;
        }
        if (type == CARD_TYPE_TAN && capturedCounts[type] >= 2) {
            priority += 900;
        }
        if (type == CARD_TYPE_KASU && capturedCounts[type] >= 5) {
            priority += 800;
        }
        if (priority > bestPriority || (priority == bestPriority && deckIndex < bestDeck)) {
            bestPriority = priority;
            bestDeck = deckIndex;
        }
    }
    return bestDeck;
}
