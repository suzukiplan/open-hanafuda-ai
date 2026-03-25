#include "common.h"

int is_exsit_same_month_in_deck(Card* card)
{
    for (int i = 0; i < 48; i++) {
        if (g.deck.cards[i]) {
            if (g.deck.cards[i]->month == card->month) {
                return 1;
            }
        }
    }
    return 0;
}

int is_gekiatsu_card(int p, Card* card)
{
    InventStats* stats = &g.stats[p];
    CardSet* i_kasu = &g.invent[p][CARD_TYPE_KASU];
    CardSet* i_tan = &g.invent[p][CARD_TYPE_TAN];
    CardSet* i_tane = &g.invent[p][CARD_TYPE_TANE];
    CardSet* i_gokou = &g.invent[p][CARD_TYPE_GOKOU];
    switch (card->type) {
        case CARD_TYPE_GOKOU:
            if (2 == stats->shikou && card->month != 10) {
                return 1; // 四光札が2枚以上ある状態で柳以外の五光札を取れば役が成立するので激アツ
            }
            if (3 <= stats->gokou) {
                return 1; // 光札が3枚以上あれば役が成立するので激アツ
            }
            if (2 == card->month || 7 == card->month) {
                for (int i = 0; i < i_tane->num; i++) {
                    if (i_tane->cards[i]->month == 8) {
                        return 1; // 盃を持っている状態での桜と月があれば役が成立するので激アツ
                    }
                }
            }
            break;
        case CARD_TYPE_TANE:
            if (4 <= stats->tane) {
                return 1; // タネ札が4枚以上あるので激アツ
            }
            if (2 <= stats->isc) {
                if (card->month == 5 || card->month == 6 || card->month == 9) {
                    return 1; // 猪鹿蝶が2枚以上ある状態での猪鹿蝶は激アツ
                }
            }
            if (card->month == 8) {
                for (int i = 0; i < i_gokou->num; i++) {
                    if (i_gokou->cards[i]->month == 2 || i_gokou->cards[i]->month == 7) {
                        return 1; // 桜か月がある時の盃は激アツ
                    }
                }
            }
            break;
        case CARD_TYPE_TAN:
            if (4 <= stats->tan) {
                return 1; // 短冊が4枚以上あるので激アツ
            }
            if (2 <= stats->akatan) {
                if (3 <= stats->akatan) {
                    return 1; // 赤短3枚以上なら全ての短冊札が激アツ
                }
                if (card->month == 0 || card->month == 1 || card->month == 2) {
                    return 1; // 赤短が2枚以上ある状態での赤短は激アツ
                }
            }
            if (2 <= stats->aotan) {
                if (3 <= stats->aotan) {
                    return 1; // 青短3枚以上なら全ての短冊札が激アツ
                }
                if (card->month == 5 || card->month == 8 || card->month == 9) {
                    return 1; // 青短が2枚以上ある状態での青短は激アツ
                }
            }
            break;
        case CARD_TYPE_KASU:
            if (9 <= stats->kasu) {
                return 1; // カスが9枚以上あるので激アツ
            }
            if (8 <= stats->kasu) {
                // 手札とデッキの両方に同じ月のカスがあれば8枚でも激アツ
                for (int i = 0; i < 8; i++) {
                    Card* own_card = g.own[p].cards[i];
                    if (!own_card) {
                        continue;
                    }
                    if (own_card->month == card->month && own_card->type == CARD_TYPE_KASU) {
                        for (int j = 0; j < 48; j++) {
                            if (g.deck.cards[j] && g.deck.cards[j]->month == card->month && g.deck.cards[j]->type == CARD_TYPE_KASU) {
                                return 1;
                            }
                        }
                    }
                }
            }
            break;
    }
    return 0;
}

static void reset_invent_score(InventStats* stats)
{
    stats->score = 0;
    stats->wh_count = 0;
    vgs_memset(stats->wh, 0, sizeof(stats->wh));
    vgs_memset(stats->base_up, 0, sizeof(stats->base_up));
}

static void add_card_to_invent_stats(InventStats* stats, const Card* card)
{
    if (!card) {
        return;
    }
    int month = card->month;
    switch (card->type) {
        case CARD_TYPE_GOKOU:
            stats->gokou++;
            if (month != 10) {
                stats->shikou++;
            }
            if (month == 7) {
                stats->tsukimi++;
            }
            if (month == 2) {
                stats->hanami++;
            }
            break;
        case CARD_TYPE_TANE:
            stats->tane++;
            if (month == 8) {
                stats->tsukimi++;
                stats->hanami++;
                stats->kasu++;
            }
            if (month == 5 || month == 6 || month == 9) {
                stats->isc++;
            }
            break;
        case CARD_TYPE_TAN:
            stats->tan++;
            if (month == 0 || month == 1 || month == 2) {
                stats->akatan++;
            }
            if (month == 5 || month == 8 || month == 9) {
                stats->aotan++;
            }
            break;
        case CARD_TYPE_KASU:
            stats->kasu++;
            break;
        default:
            break;
    }
}

static void compute_invent_score(InventStats* stats)
{
    reset_invent_score(stats);

    if (5 <= stats->gokou) {
        stats->wh[stats->wh_count] = &winning_hands[WID_GOKOU];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->wh_count++;
    } else if (4 <= stats->shikou) {
        stats->wh[stats->wh_count] = &winning_hands[WID_SHIKOU];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->wh_count++;
    } else if (4 <= stats->gokou) {
        stats->wh[stats->wh_count] = &winning_hands[WID_AME_SHIKOU];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->wh_count++;
    } else if (3 <= stats->shikou) {
        stats->wh[stats->wh_count] = &winning_hands[WID_SANKOU];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->wh_count++;
    }

    if (2 <= stats->hanami) {
        stats->wh[stats->wh_count] = &winning_hands[WID_HANAMI];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->wh_count++;
    }
    if (2 <= stats->tsukimi) {
        stats->wh[stats->wh_count] = &winning_hands[WID_TSUKIMI];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->wh_count++;
    }

    if (3 <= stats->akatan && 3 <= stats->aotan) {
        stats->wh[stats->wh_count] = &winning_hands[WID_DBTAN];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->base_up[WID_DBTAN] = stats->tan - 6;
        stats->wh_count++;
    } else if (3 <= stats->akatan) {
        stats->wh[stats->wh_count] = &winning_hands[WID_AKATAN];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->base_up[WID_AKATAN] = stats->tan - 3;
        stats->wh_count++;
    } else if (3 <= stats->aotan) {
        stats->wh[stats->wh_count] = &winning_hands[WID_AOTAN];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->base_up[WID_AOTAN] = stats->tan - 3;
        stats->wh_count++;
    } else if (5 <= stats->tan) {
        stats->wh[stats->wh_count] = &winning_hands[WID_TAN];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->base_up[WID_TAN] = stats->tan - 5;
        stats->wh_count++;
    }

    if (3 <= stats->isc) {
        stats->wh[stats->wh_count] = &winning_hands[WID_ISC];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->base_up[WID_ISC] = stats->tane - 3;
        stats->wh_count++;
    } else if (5 <= stats->tane) {
        stats->wh[stats->wh_count] = &winning_hands[WID_TANE];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->base_up[WID_TANE] = stats->tane - 5;
        stats->wh_count++;
    }

    if (10 <= stats->kasu) {
        stats->wh[stats->wh_count] = &winning_hands[WID_KASU];
        stats->score += stats->wh[stats->wh_count]->base_score;
        stats->base_up[WID_KASU] = stats->kasu - 10;
        stats->wh_count++;
    }

    for (int i = 0; i < WINNING_HAND_MAX; i++) {
        stats->score += stats->base_up[i];
    }
}

static int calc_score_with_cards(const InventStats* base, Card* const* cards, int count)
{
    InventStats sim = *base;
    for (int i = 0; i < count; i++) {
        add_card_to_invent_stats(&sim, cards[i]);
    }
    compute_invent_score(&sim);
    return sim.score;
}

int analyze_score_gain_with_card_ids(int p, const int* card_ids, int count, int* out_best_wid)
{
    InventStats sim;
    const InventStats* base;

    if (p < 0 || p > 1) {
        if (out_best_wid) {
            *out_best_wid = -1;
        }
        return 0;
    }

    base = &g.stats[p];
    sim = *base;
    for (int i = 0; i < count; i++) {
        int card_id = card_ids ? card_ids[i] : -1;
        if (card_id < 0 || card_id >= 48) {
            continue;
        }
        add_card_to_invent_stats(&sim, &g.cards[card_id]);
    }
    compute_invent_score(&sim);
    if (out_best_wid) {
        *out_best_wid = (sim.wh_count > 0 && sim.wh[0]) ? sim.wh[0]->id : -1;
    }
    return sim.score - base->score;
}

static void append_unique_card(Card** list, int* count, Card* card)
{
    if (!card) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (list[i] == card) {
            return;
        }
    }
    if (*count < 48) {
        list[*count] = card;
        (*count)++;
    }
}

int analyze_invent(int p)
{
    InventStats* stats = &g.stats[p];
    vgs_memset(stats, 0, sizeof(InventStats));
    for (int t = 0; t < 4; t++) {
        Card** cards = g.invent[p][t].cards;
        for (int i = 0; i < 48; i++) {
            Card* card = cards[i];
            if (!card) {
                continue;
            }
            int month = card->month;
            switch (card->type) {
                case CARD_TYPE_GOKOU:
                    stats->gokou++;
                    stats->wcs[WID_GOKOU].cards[stats->wcs[WID_GOKOU].num++] = card;
                    stats->wcs[WID_SANKOU].cards[stats->wcs[WID_SANKOU].num++] = card;
                    stats->wcs[WID_AME_SHIKOU].cards[stats->wcs[WID_AME_SHIKOU].num++] = card;
                    if (month != 10) {
                        stats->shikou++;
                        stats->wcs[WID_SHIKOU].cards[stats->wcs[WID_SHIKOU].num++] = card;
                    }
                    if (month == 7) {
                        stats->tsukimi++;
                        stats->wcs[WID_TSUKIMI].cards[stats->wcs[WID_TSUKIMI].num++] = card;
                    }
                    if (month == 2) {
                        stats->hanami++;
                        stats->wcs[WID_HANAMI].cards[stats->wcs[WID_HANAMI].num++] = card;
                    }
                    break;
                case CARD_TYPE_TANE:
                    stats->tane++;
                    stats->wcs[WID_TANE].cards[stats->wcs[WID_TANE].num++] = card;
                    if (month == 8) {
                        stats->tsukimi++;
                        stats->hanami++;
                        stats->kasu++;
                        stats->wcs[WID_TSUKIMI].cards[stats->wcs[WID_TSUKIMI].num++] = card;
                        stats->wcs[WID_HANAMI].cards[stats->wcs[WID_HANAMI].num++] = card;
                        stats->wcs[WID_KASU].cards[stats->wcs[WID_KASU].num++] = card;
                    }
                    if (month == 5 || month == 6 || month == 9) {
                        stats->isc++;
                    }
                    stats->wcs[WID_ISC].cards[stats->wcs[WID_ISC].num++] = card;
                    break;
                case CARD_TYPE_TAN:
                    stats->tan++;
                    if (month == 0 || month == 1 || month == 2) {
                        stats->akatan++;
                    }
                    if (month == 5 || month == 8 || month == 9) {
                        stats->aotan++;
                    }
                    stats->wcs[WID_TAN].cards[stats->wcs[WID_TAN].num++] = card;
                    stats->wcs[WID_AKATAN].cards[stats->wcs[WID_AKATAN].num++] = card;
                    stats->wcs[WID_AOTAN].cards[stats->wcs[WID_AOTAN].num++] = card;
                    stats->wcs[WID_DBTAN].cards[stats->wcs[WID_DBTAN].num++] = card;
                    break;
                case CARD_TYPE_KASU:
                    stats->kasu++;
                    stats->wcs[WID_KASU].cards[stats->wcs[WID_KASU].num++] = card;
                    break;
                default:
                    break;
            }
        }
    }

    compute_invent_score(stats);
    vsync();

    //-------------------
    //  欲しいカードを分析
    //-------------------
    vgs_memset(stats->want.cards, 0, sizeof(stats->want.cards));
    stats->want.start = 0;
    stats->want.num = 0;

    // 欲しいカードの優先:
    // 0. gekiatsu
    // 1. month = 8, index = 3 (盃)
    // 2. month = 2, index = 3 (桜の光札)
    // 3. month = 7, index = 3 (月の光札)
    // 4. month = 0, index = 3 (光札)
    // 5. month = 11, index = 3 (光札)
    // 6. month = 10, index = 3 (光札 ※雨)
    // 7. 相手の取り札 (g.invent[1-p]) に猪鹿蝶が含まれない場合、猪鹿蝶
    //    ※猪鹿蝶: month=5, index=3 および month=6, index=3 および month=9, index=3
    // 8. 相手の取り札 (g.invent[1-p]) に赤短が含まれない場合、赤短
    //    ※赤短: month=0, index=2 および month=1, index=2 および month=2, index=2
    // 9. 相手の取り札 (g.invent[1-p]) に青短が含まれない場合、青短
    //    ※青短: month=5, index=2 および month=8, index=2 および month=9, index=2
    // 10. stats->tane が 3 以上の場合、type = CARD_TYPE_TANE
    // 11. stats->tan が 3 以上の場合、type = CARD_TYPE_TAN
    // 12. stats->kasu が 7 以上の場合、type = CARD_TYPE_KASU

    CardSet* hand = &g.own[p];
    for (int i = 0; i < 8; i++) {
        Card* card = hand->cards[i];
        if (card && is_gekiatsu_card(p, card)) {
            append_unique_card(stats->want.cards, &stats->want.num, card);
        }
    }
    for (int i = 0; i < 48; i++) {
        Card* card = g.deck.cards[i];
        if (card && is_gekiatsu_card(p, card)) {
            append_unique_card(stats->want.cards, &stats->want.num, card);
        }
    }

    const int opponent = 1 - p;
    unsigned char captured_flags[48] = {0};
    unsigned char captured_by_player[2][48] = {{0}};
    for (int owner = 0; owner < 2; owner++) {
        for (int t = 0; t < 4; t++) {
            Card** cards = g.invent[owner][t].cards;
            for (int i = 0; i < 48; i++) {
                Card* card = cards[i];
                if (!card) {
                    continue;
                }
                int idx = (int)(card - g.cards);
                if (idx < 0 || idx >= 48) {
                    continue;
                }
                captured_by_player[owner][idx] = 1;
                captured_flags[idx] = 1;
            }
        }
    }

    Card* card_lookup[48] = {0};
    for (int i = 0; i < 48; i++) {
        Card* card = &g.cards[i];
        int key = (card->month << 2) | (card->index & 3);
        card_lookup[key] = card;
    }

#define CARD_KEY(month, index) (((month) << 2) | ((index) & 3))
#define ADD_CARD_PTR(cardPtr)                                                    \
    do {                                                                         \
        Card* __card = (cardPtr);                                                \
        if (__card) {                                                            \
            int __idx = (int)(__card - g.cards);                                 \
            if (0 <= __idx && __idx < 48 && !captured_flags[__idx]) {            \
                append_unique_card(stats->want.cards, &stats->want.num, __card); \
            }                                                                    \
        }                                                                        \
    } while (0)
#define ADD_MONTH_INDEX(month, index) ADD_CARD_PTR(card_lookup[CARD_KEY((month), (index))])

    ADD_MONTH_INDEX(8, 3);
    ADD_MONTH_INDEX(2, 3);
    ADD_MONTH_INDEX(7, 3);
    ADD_MONTH_INDEX(0, 3);
    ADD_MONTH_INDEX(11, 3);
    ADD_MONTH_INDEX(10, 3);

    const int isc_keys[] = {CARD_KEY(5, 3), CARD_KEY(6, 3), CARD_KEY(9, 3)};
    int opp_has_isc = 1;
    for (int i = 0; i < 3; i++) {
        Card* card = card_lookup[isc_keys[i]];
        int idx = card ? (int)(card - g.cards) : -1;
        if (idx < 0 || !captured_by_player[opponent][idx]) {
            opp_has_isc = 0;
            break;
        }
    }
    if (!opp_has_isc) {
        for (int i = 0; i < 3; i++) {
            ADD_CARD_PTR(card_lookup[isc_keys[i]]);
        }
    }

    const int red_keys[] = {CARD_KEY(0, 2), CARD_KEY(1, 2), CARD_KEY(2, 2)};
    int opp_has_red = 1;
    for (int i = 0; i < 3; i++) {
        Card* card = card_lookup[red_keys[i]];
        int idx = card ? (int)(card - g.cards) : -1;
        if (idx < 0 || !captured_by_player[opponent][idx]) {
            opp_has_red = 0;
            break;
        }
    }
    if (!opp_has_red) {
        for (int i = 0; i < 3; i++) {
            ADD_CARD_PTR(card_lookup[red_keys[i]]);
        }
    }

    const int blue_keys[] = {CARD_KEY(5, 2), CARD_KEY(8, 2), CARD_KEY(9, 2)};
    int opp_has_blue = 1;
    for (int i = 0; i < 3; i++) {
        Card* card = card_lookup[blue_keys[i]];
        int idx = card ? (int)(card - g.cards) : -1;
        if (idx < 0 || !captured_by_player[opponent][idx]) {
            opp_has_blue = 0;
            break;
        }
    }
    if (!opp_has_blue) {
        for (int i = 0; i < 3; i++) {
            ADD_CARD_PTR(card_lookup[blue_keys[i]]);
        }
    }

    if (stats->tane >= 3) {
        for (int i = 0; i < 48; i++) {
            if (!captured_flags[i] && g.cards[i].type == CARD_TYPE_TANE) {
                append_unique_card(stats->want.cards, &stats->want.num, &g.cards[i]);
            }
        }
    }
    if (stats->tan >= 3) {
        for (int i = 0; i < 48; i++) {
            if (!captured_flags[i] && g.cards[i].type == CARD_TYPE_TAN) {
                append_unique_card(stats->want.cards, &stats->want.num, &g.cards[i]);
            }
        }
    }
    if (stats->kasu >= 7) {
        for (int i = 0; i < 48; i++) {
            if (!captured_flags[i] && g.cards[i].type == CARD_TYPE_KASU) {
                append_unique_card(stats->want.cards, &stats->want.num, &g.cards[i]);
            }
        }
    }

#undef ADD_MONTH_INDEX
#undef ADD_CARD_PTR
#undef CARD_KEY

    //----------------
    //  結果をログ出力
    //----------------
#if 1
    int ret = stats->wh_count ? ON : OFF;
#else
    int ret = OFF;
    if (stats->wh_count) {
        vgs_putlog("%s Winning Hand(s) Detected!", p ? "CPU's" : "Your");
        for (int i = 0; i < stats->wh_count; i++) {
            vgs_putlog("- %s - %s (%d pts)", stats->wh[i]->nameJ, stats->wh[i]->nameE, stats->wh[i]->base_score + stats->base_up[stats->wh[i]->id]);
        }
        ret = ON;
        vgs_putlog("Total score: %dpt", stats->score);
    }
    if (stats->want.num) {
        vgs_putlog("%s candidate cards: ", p ? "CPU's" : "Your");
        for (int i = 0; i < 10 && i < stats->want.num; i++) {
            vgs_putlog("  %d. month=%d, index=%d, type=%d", i + 1, stats->want.cards[i]->month, stats->want.cards[i]->index, stats->want.cards[i]->type);
        }
    } else {
        vgs_putlog("%s candidate cards: n/a", p ? "CPU's" : "Your");
    }
    vgs_putlog("%s score: %d", p ? "CPU's" : "Player's", stats->score);
#endif

    vsync();

    if (ret && g.koikoi[p]) {
        // こいこい実行中の場合、こいこい実行時点のスコアより上昇しなければ勝負継続
        ret = g.koikoi_score[p] < stats->score;
    }
    return ret;
}
