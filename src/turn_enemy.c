#include "common.h"
#include "cards.h"
#include "ai.h"

static Card* g_enemy_floor_log_card1;
static Card* g_enemy_floor_log_card2;

static void log_take_cards(const char* phase, Card* card1, Card* card2)
{
    if (g.auto_play == OFF) {
        return;
    }
    if (!card1 || !card2) {
        ai_putlog_env("[CPU] %s take: n/a", phase);
        return;
    }
    ai_putlog_env("[CPU] %s take: %d,%d", phase, card1->id, card2->id);
}

int enemy_think_resolve_target_deck(Card* card)
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
    if (g.online_mode) {
        Card* deck = NULL;
        int error;
        vgs_putlog("Receive deck card number...");
        g.com_wait_frames = 0;
        while (NULL == deck) {
            deck = online_card_recv(&error);
            if (error) {
                g.online_error = ON;
                return 0;
            } else {
                if (deck) {
                    for (int i = 0; i < 48; i++) {
                        if (g.deck.cards[i] && g.deck.cards[i]->id == deck->id) {
                            g.com_wait_frames = 0;
                            return i;
                        }
                    }
                    g.online_error = ON;
                    return 0;
                }
            }
            g.com_wait_frames++;
            vsync();
        }
        g.com_wait_frames = 0;
    }

    {
        int targetDeck = ai_select(1, card);
        if (g.auto_play != OFF) {
            ai_putlog_env("[CPU] think select: %d%s", targetDeck, ai_get_last_think_extra(1));
        }
        return targetDeck;
    }
}

static void remember_card_target(Card* card, int targetDeck)
{
    int idx = card_global_index(card);
    if (idx >= 0) {
        card_target_deck_map[idx] = targetDeck;
    }
}

static int consume_card_target(Card* card)
{
    int idx = card_global_index(card);
    if (idx < 0) {
        return -1;
    }
    int targetDeck = card_target_deck_map[idx];
    card_target_deck_map[idx] = -1;
    return targetDeck;
}

static void enemy_store_captured_card(Card* captured, int x, int y)
{
    (void)x;
    (void)y;
    g.invent[1][captured->type].cards[g.invent[1][captured->type].num++] = captured;
}

static void enemy_move_card_to_invent(Card* card, int x, int y)
{
    int tx = get_invent_x(card, g.invent[1][card->type].num);
    int ty = get_invent_y(1);
    add_move_invent(card, x, y, tx, ty, enemy_store_captured_card);
}

void enemy_pick_invent(Card* card, int targetDeck)
{
    Card* deckCard = g.deck.cards[targetDeck];
    add_move_pick_own(deckCard, card, get_deck_x(targetDeck), get_deck_y(targetDeck), enemy_move_card_to_invent);
    g.deck.cards[targetDeck] = NULL;
    g.deck.num--;
    while (g.move_count) {
        draw_cards();
    }
}

static void enemy_finish_discard(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    int targetDeck = consume_card_target(card);
    if (targetDeck < 0) {
        return;
    }
    g.deck.cards[targetDeck] = card;
    g.deck.num++;
}

static void enemy_finish_drop(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    int targetDeck = consume_card_target(card);
    if (targetDeck < 0) {
        return;
    }
    enemy_pick_invent(card, targetDeck);
}

static void enemy_handle_open_floor(Card* card, int x, int y)
{
    int targetDeck = enemy_think_resolve_target_deck(card);
    if (targetDeck < 0) {
        return;
    }
    remember_card_target(card, targetDeck);
    int tx = get_deck_x(targetDeck);
    int ty = get_deck_y(targetDeck);
    g_enemy_floor_log_card1 = card;
    g_enemy_floor_log_card2 = g.deck.cards[targetDeck];
    if (!g.deck.cards[targetDeck]) {
        add_move_discard(card, x, y, tx, ty, enemy_finish_discard);
    } else {
        add_move_drop(card, x, y, tx, ty, enemy_finish_drop);
    }
}

void enemy_turn(void)
{
    if (g.own[1].num < 1) {
        return;
    }

    // 場に出す手札を選択
    int dropIndex;
    if (g.online_mode) {
        Card* own = NULL;
        int error;
        // 手札から捨てるカードを受信
        vgs_putlog("Receive hand card number...");
        g.com_wait_frames = 0;
        while (NULL == own) {
            own = online_card_recv(&error);
            if (own) {
                break;
            } else {
                if (!online_check()) {
                    g.online_error = ON;
                    return;
                }
            }
            g.com_wait_frames++;
            draw_cards();
        }
        g.com_wait_frames = 0;
        if (!own) {
            // これが NULL になることは無い（論理不整合）
            vgs_putlog("Unexpected NULL card received.");
            g.online_error = ON;
            return;
        }
        for (dropIndex = 0; dropIndex < g.own[1].num; dropIndex++) {
            if (g.own[1].cards[dropIndex]->id == own->id) {
                break;
            }
        }
        if (g.own[1].num <= dropIndex) {
            // 手札に持っていないカードを受信（論理不整合）
            vgs_putlog("Unexpected hand card(%d) received.", own->id);
            g.online_error = ON;
            return;
        }
    } else {
        dropIndex = ai_drop(1);
    }

    // 場に出す手札をオープン
    Card* dropCard = g.own[1].cards[dropIndex];
    if (g.auto_play != OFF) {
        ai_putlog_env("[CPU] think drop: %d%s", dropCard ? dropCard->id : -1, ai_get_last_think_extra(1));
        ai_watch_begin_after_opp_window(1);
    }
    g.own[1].cards[dropIndex] = NULL;
    if (!g.open_mode || g.online_mode) {
        add_move_fopen(dropCard, get_own_x(1, dropIndex), get_own_y(1), NULL);
        while (g.move_count) {
            draw_cards();
        }
    }

    // 場札をチェック
    int targetDeck;
    if (g.online_mode) {
        Card* deck = NULL;
        int error;
        // 手札から捨てるカードを受信
        vgs_putlog("Receive deck card number...");
        g.com_wait_frames = 0;
        while (NULL == deck) {
            deck = online_card_recv(&error);
            if (!error) {
                break;
            } else {
                if (!online_check()) {
                    g.online_error = ON;
                    return;
                }
            }
            g.com_wait_frames++;
            draw_cards();
        }
        g.com_wait_frames = 0;
        if (deck) {
            for (targetDeck = 0; targetDeck < 48; targetDeck++) {
                if (g.deck.cards[targetDeck]) {
                    if (g.deck.cards[targetDeck]->id == deck->id) {
                        break;
                    }
                }
            }
            if (48 == targetDeck) {
                // 場札にないカード受信（論理不整合）
                vgs_putlog("Unexpected deck card(%d) received.", deck->id);
                g.online_error = ON;
                return;
            }
        } else {
            targetDeck = find_empty_deck_slot();
        }
    } else {
        targetDeck = enemy_think_resolve_target_deck(dropCard);
    }
    Card* takenDrop = (0 <= targetDeck && targetDeck < 48) ? g.deck.cards[targetDeck] : NULL;

    remember_card_target(dropCard, targetDeck);
    if (!g.deck.cards[targetDeck]) {
        add_move_discard(dropCard, get_own_x(1, dropIndex), get_own_y(1), get_deck_x(targetDeck), get_deck_y(targetDeck), enemy_finish_discard);
    } else {
        add_move_drop(dropCard, get_own_x(1, dropIndex), get_own_y(1), get_deck_x(targetDeck), get_deck_y(targetDeck), enemy_finish_drop);
    }

    // 手札スライドを開始
    g.own_move_flag[1] = ON;
    g.own_move_x[1] = 0;

    while (g.move_count) {
        draw_cards();
    }
    log_take_cards("drop", dropCard, takenDrop);

    // 手札数を減らす
    for (int i = dropIndex; i < 7; i++) {
        g.own[1].cards[i] = g.own[1].cards[i + 1];
    }
    g.own[1].cards[7] = NULL;
    g.own[1].num--;
    g.own_move_flag[1] = OFF;

    // 場札をオープン
    analyze_invent(1);
    Card* floorCard = g.floor.cards[g.floor.start];
    g.floor.start++;
    g.floor.num--;
    if (g.auto_play != OFF) {
        ai_putlog_env("[CPU] floor open: %d", floorCard ? floorCard->id : -1);
    }
    // vgs_putlog("Closed floor cards left: %d", g.floor.num);
    add_move_fopen(floorCard, 0, 80, enemy_handle_open_floor);
    while (g.move_count) {
        draw_cards();
    }
    log_take_cards("floor", g_enemy_floor_log_card1, g_enemy_floor_log_card2);
    g_enemy_floor_log_card1 = NULL;
    g_enemy_floor_log_card2 = NULL;
    for (int i = 0; i < 20; i++) {
        draw_cards();
    }
}
