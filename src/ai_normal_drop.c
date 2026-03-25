#include "ai.h"

int ai_normal_drop(int player)
{
    enum { MAX_MATCHES_PER_CARD = 48 };
    int handIndexes[48];
    Card* handCards[48];
    int handCount = 0;
    for (int i = 0; i < g.own[player].num && i < 48; i++) {
        Card* card = g.own[player].cards[i];
        if (!card) {
            continue;
        }
        handIndexes[handCount] = i;
        handCards[handCount] = card;
        handCount++;
    }
    if (!handCount) {
        return 0;
    }

    typedef struct {
        int handIndex;
        int deckIndexes[MAX_MATCHES_PER_CARD];
        int deckCount;
        int bestPriority;
    } CaptureCandidate;

    CaptureCandidate candidates[48];
    int candidateCount = 0;
    for (int i = 0; i < handCount; i++) {
        int targets[48];
        int matchCount = collect_target_deck_indexes(handCards[i], targets);
        if (!matchCount || candidateCount >= (int)(sizeof(candidates) / sizeof(candidates[0]))) {
            continue;
        }
        CaptureCandidate* cand = &candidates[candidateCount++];
        cand->handIndex = handIndexes[i];
        cand->deckCount = matchCount < MAX_MATCHES_PER_CARD ? matchCount : MAX_MATCHES_PER_CARD;
        cand->bestPriority = -1;
        for (int j = 0; j < cand->deckCount; j++) {
            cand->deckIndexes[j] = targets[j];
        }
    }

    if (candidateCount) {
        for (int wi = 0; wi < g.stats[player].want.num; wi++) {
            Card* wantCard = g.stats[player].want.cards[wi];
            if (!wantCard) {
                continue;
            }
            for (int ci = 0; ci < candidateCount; ci++) {
                CaptureCandidate* cand = &candidates[ci];
                for (int dj = 0; dj < cand->deckCount; dj++) {
                    int deckIndex = cand->deckIndexes[dj];
                    if (g.deck.cards[deckIndex] == wantCard) {
                        return cand->handIndex;
                    }
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

        for (int ci = 0; ci < candidateCount; ci++) {
            CaptureCandidate* cand = &candidates[ci];
            for (int dj = 0; dj < cand->deckCount; dj++) {
                Card* deckCard = g.deck.cards[cand->deckIndexes[dj]];
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
                if (priority > cand->bestPriority) {
                    cand->bestPriority = priority;
                }
            }
        }

        int bestIndex = candidates[0].handIndex;
        int bestPriority = candidates[0].bestPriority;
        for (int ci = 1; ci < candidateCount; ci++) {
            if (candidates[ci].bestPriority > bestPriority) {
                bestPriority = candidates[ci].bestPriority;
                bestIndex = candidates[ci].handIndex;
            } else if (candidates[ci].bestPriority == bestPriority && candidates[ci].handIndex < bestIndex) {
                bestIndex = candidates[ci].handIndex;
            }
        }
        if (bestPriority >= 0) {
            return bestIndex;
        }
    }

    static const int discardPriorityMap[4] = {
        0, // CARD_TYPE_GOKOU
        1, // CARD_TYPE_TANE
        2, // CARD_TYPE_TAN
        3  // CARD_TYPE_KASU
    };

    int discardIndex = handIndexes[0];
    int discardPriority = discardPriorityMap[handCards[0]->type];
    for (int i = 1; i < handCount; i++) {
        int priority = discardPriorityMap[handCards[i]->type];
        if (priority > discardPriority) {
            discardPriority = priority;
            discardIndex = handIndexes[i];
        }
    }
    return discardIndex;
}
