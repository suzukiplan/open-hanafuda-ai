#include "ai.h"

int ai_normal_koikoi(int player, int round_score)
{
    // 持ち札の残数による確率のベースライン
    static const int base_probabilities[] = {
        0,   // 未使用
        12,  // 1枚 -> 5%
        51,  // 2枚 -> 20%
        128, // 3枚 -> 50%
        179, // 4枚 -> 70%
        204, // 5枚 -> 80%
        230, // 6枚 -> 90%
        243  // 7枚 -> 95%
    };
    int probability = base_probabilities[g.own[player].num];

    int has_gokou = OFF;
    int has_tane = OFF;
    int has_tan = OFF;
    int has_kasu = OFF;
    int has_tsukimi_or_hanami_gokou = OFF;

    for (int i = 0; i < g.own[player].num; ++i) {
        Card* card = g.own[player].cards[i];
        if (!card) {
            continue;
        }
        switch (card->type) {
            case CARD_TYPE_GOKOU:
                has_gokou = ON;
                if (card->month == 7 || card->month == 2) {
                    has_tsukimi_or_hanami_gokou = ON;
                }
                break;
            case CARD_TYPE_TANE:
                has_tane = ON;
                break;
            case CARD_TYPE_TAN:
                has_tan = ON;
                break;
            case CARD_TYPE_KASU:
                has_kasu = ON;
                break;
        }
    }

    if (g.invent[player][CARD_TYPE_GOKOU].num >= 3 && has_gokou) {
        // 取り札に五光が3枚以上あり手札に五光札がある
        probability += 153; // +60%
    }
    if (g.invent[player][CARD_TYPE_TANE].num >= 4 && has_tane) {
        // 取り札にタネが4枚以上あり手札にタネがある
        probability += 89; // +35%
    }
    if (g.invent[player][CARD_TYPE_TAN].num >= 4 && has_tan) {
        // 取り札に短冊が4枚以上あり手札にタネがある
        probability += 102; // +40%
    }
    if (g.invent[player][CARD_TYPE_KASU].num >= 9 && has_kasu) {
        // 取り札にカスが9枚以上あり手札にカスがある
        probability += 115; // +45%
    }

    // 　取り札に杯があるかチェック
    CardSet* captured_tane = &g.invent[player][CARD_TYPE_TANE];
    int has_cup = OFF;
    for (int i = 0; i < captured_tane->num; ++i) {
        Card* card = captured_tane->cards[i];
        if (card && card->month == 8) {
            has_cup = ON;
            break;
        }
    }
    if (has_cup) {
        probability += has_tsukimi_or_hanami_gokou ? 204 : 89; // +80% or +35%
    }

    int score_diff = g.total_score[0] - (g.total_score[player] + round_score);
    if (15 <= score_diff) {
        probability += 128; // +50% (15点差以上)
    } else if (10 <= score_diff) {
        probability += 102; // +40% (10~15点差)
    } else if (5 <= score_diff) {
        probability += 89; // +35% (5〜9点差)
    }

    if (round_score < 7) {
        probability += 64; // +25% (倍付狙い)
    }

    ai_putlog("KOIKOI execution probability: %d / 256", probability);
    if (probability >= 256) {
        ai_putlog("KOIKOI: Yes");
        return ON;
    }
    int isYes = (int)(vgs_rand() & 0xFFu) < probability ? ON : OFF;
    ai_putlog("KOIKOI: %s", isYes ? "Yes" : "No");
    return isYes;
}
