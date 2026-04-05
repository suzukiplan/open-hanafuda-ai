#include "common.h"
#include "cards.h"
#include "ai.h"

static void player_move_card_to_invent_open(Card* card, int x, int y);
static void player_on_floor_open(Card* card, int x, int y);
static Card* g_player_floor_log_card1;
static Card* g_player_floor_log_card2;

static const char* player_log_prefix(void)
{
    return "[P1]";
}

static void set_player_cursor(int next_cursor, int play_se)
{
    if (next_cursor < 0 || g.own[0].num <= next_cursor || g.cursor == next_cursor) {
        return;
    }
    g.cursor = next_cursor;
    vgs_oam(SP_CURSOR)->x = get_own_x(0, g.cursor) + (40 - 16) / 2;
    if (play_se) {
        vgs_sfx_play(SFX_MOVE_CURSOR2);
    }
}

static int find_hover_own_cursor(void)
{
    for (int i = 0; i < g.own[0].num; i++) {
        ObjectAttributeMemory* own = vgs_oam(SP_OWN + i);
        if (mouse_in_rect(own->x - 4, own->y - 4, 48, 56)) {
            return i;
        }
    }
    return -1;
}

static int show_player_target_selector(const int* targetDecks, int targetDeckCount, int allowCancel, int* canceled)
{
    ObjectAttributeMemory* dc = vgs_oam(SP_DECK_CURSOR);
    int dcScale = 100;
    for (int i = 0; i < 2; i++) {
        dc[i].visible = ON;
        dc[i].pri = ON;
        dc[i].scale = dcScale;
    }
    int ptr = 0;
    int targetDeck = targetDecks[ptr];
    int loop_count = -1;
    if (canceled) {
        *canceled = OFF;
    }
    while (ON) {
        loop_count++;
        if (timer_is_over() || push_a()) {
            vgs_sfx_play(SFX_CHOOSE);
            break;
        }
        if (allowCancel && (push_b() || vgs_mouse_right_clicked(NULL, NULL))) {
            if (canceled) {
                vgs_sfx_play(SFX_CURSOR_LEAVE);
                *canceled = ON;
            }
            targetDeck = -1;
            break;
        }
        if (mouse_shown()) {
            for (int i = 0; i < targetDeckCount; i++) {
                int deck = targetDecks[i];
                if (mouse_in_rect(get_deck_x(deck) - 6, get_deck_y(deck) - 6, 52, 68)) {
                    if (ptr != i) {
                        ptr = i;
                        targetDeck = targetDecks[ptr];
                        dcScale = 80;
                        if (0 != loop_count) {
                            vgs_sfx_play(SFX_MOVE_CURSOR2);
                        }
                    }
                    break;
                }
            }
        }
        int clk_x, clk_y;
        if (vgs_mouse_left_clicked(&clk_x, &clk_y)) {
            for (int i = 0; i < targetDeckCount; i++) {
                int deck = targetDecks[i];
                if (position_in_rect(clk_x, clk_y, get_deck_x(deck) - 6, get_deck_y(deck) - 6, 52, 68)) {
                    if (ptr != i) {
                        ptr = i;
                        targetDeck = targetDecks[ptr];
                        dcScale = 80;
                    } else {
                        vgs_sfx_play(SFX_CHOOSE);
                        goto choose_done;
                    }
                    break;
                }
            }
        }
        if (push_left() || push_up()) {
            vgs_sfx_play(SFX_MOVE_CURSOR2);
            ptr--;
            if (ptr < 0) {
                ptr = targetDeckCount - 1;
            }
            targetDeck = targetDecks[ptr];
            dcScale = 80;
        } else if (push_right() || push_down()) {
            vgs_sfx_play(SFX_MOVE_CURSOR2);
            ptr++;
            if (targetDeckCount <= ptr) {
                ptr = 0;
            }
            targetDeck = targetDecks[ptr];
            dcScale = 80;
        }
        if (targetDeck >= 0) {
            if (dcScale < 200) {
                dcScale += 20;
            }
            int dx = get_deck_x(targetDeck);
            int dy = get_deck_y(targetDeck);
            dc[0].scale = dcScale;
            dc[1].scale = dcScale;
            dc[0].x = dx;
            dc[0].y = dy;
            dc[1].x = dx;
            dc[1].y = dy;
            dc[0].attr = PTN_DECK_CURSOR + ((g.frame & 0b1100) >> 2) * CARD_SIZE;
            Card* deckCard = g.deck.cards[targetDeck];
            if (deckCard) {
                dc[1].attr = card_attr(deckCard->month, deckCard->index);
            }
        }
        system_vsync();
        g.frame++;
    }
choose_done:
    if ((!allowCancel || !canceled || !*canceled) && targetDeck >= 0) {
        while (100 < dcScale) {
            dcScale -= 10;
            dc[0].scale = dcScale;
            dc[1].scale = dcScale;
            dc[0].attr = PTN_DECK_CURSOR + ((g.frame & 0b1100) >> 2) * CARD_SIZE;
            system_vsync();
            g.frame++;
        }
    }
    dc[0].visible = OFF;
    dc[1].visible = OFF;
    return targetDeck;
}

static void log_take_cards(const char* phase, Card* card1, Card* card2)
{
    if (!ai_log_enabled()) {
        return;
    }
    if (!card1 || !card2) {
        ai_putlog_env("%s %s take: n/a", player_log_prefix(), phase);
        return;
    }
    ai_putlog_env("%s %s take: %d,%d", player_log_prefix(), phase, card1->id, card2->id);
}

static int player_think_resolve_target_deck(Card* card, int allowCancel, int* canceled)
{
    if (!g.auto_play) {
        if (!card) {
            if (canceled) {
                *canceled = OFF;
            }
            return find_empty_deck_slot();
        }
        int targetDecks[48];
        int targetDeckCount = collect_target_deck_indexes(card, targetDecks);
        int targetDeck = -1;
        if (targetDeckCount) {
            targetDeck = targetDecks[0];
            if (1 < targetDeckCount) {
                targetDeck = show_player_target_selector(targetDecks, targetDeckCount, allowCancel, canceled);
                if (g.online_mode && !allowCancel && 0 <= targetDeck) {
                    online_card_send2(g.deck.cards[targetDeck]);
                }
            } else if (canceled) {
                *canceled = OFF;
            }
        } else {
            if (canceled) {
                *canceled = OFF;
            }
            targetDeck = find_empty_deck_slot();
        }
        if (allowCancel && canceled && *canceled) {
            return -1;
        }
        if (ai_log_enabled() && 0 <= targetDeck) {
            ai_putlog_env("%s think select: %d", player_log_prefix(), targetDeck);
        }
        return targetDeck;
    }
    if (canceled) {
        *canceled = OFF;
    }
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

    int targetDeck = ai_select(0, card);
    if (ai_log_enabled()) {
        ai_putlog_env("%s think select: %d%s", player_log_prefix(), targetDeck, ai_get_last_think_extra(0));
    }
    int valid = OFF;
    for (int i = 0; i < targetDeckCount; i++) {
        if (targetDecks[i] == targetDeck) {
            valid = ON;
            break;
        }
    }
    if (!valid) {
        targetDeck = targetDecks[0];
    }
    if (g.online_mode && !allowCancel) {
        online_card_send2(g.deck.cards[targetDeck]);
    }
    return targetDeck;
}

static void clear_gekiatsu(void)
{
    for (int i = 0; i < GEKIATSU_MAX; i++) {
        vgs_oam(SP_GEKIATSU + i)->visible = OFF;
        vgs_oam(SP_GEKIATSU + i)->attr = PTN_GEKIATSU | 0x10000;
        vgs_oam(SP_GEKIATSU + i)->size = 4;
    }
}

void player_turn(void)
{
    if (g.own[0].num < 1) {
        return;
    }

    g.cursor = 0;
    vgs_sprite(SP_CURSOR, ON, get_own_x(0, g.cursor) + (40 - 16) / 2, 200, 1, 0, PTN_CURSOR);
    int cancel = ON;
    int targetDeck = -1;
    int discardIndex = 0;
    int dropScale = 10;

    // 捨てる位置を探索
    for (int i = 0; i < 48; i++) {
        if (!g.deck.cards[i]) {
            discardIndex = i;
            break;
        }
    }
    ObjectAttributeMemory* o = vgs_oam(SP_DROP);
    o->x = get_deck_x(discardIndex);
    o->y = get_deck_y(discardIndex);

    // 捨て札を決定するループ
    if (g.auto_play) {
        g.cursor = ai_drop(0);
        if (g.cursor < 0 || g.own[0].num <= g.cursor) {
            g.cursor = 0;
        }
        targetDeck = player_think_resolve_target_deck(g.own[0].cards[g.cursor], ON, NULL);
        if (targetDeck < 0) {
            targetDeck = find_empty_deck_slot();
        }
        if (targetDeck < 0) {
            vgs_oam(SP_CURSOR)->visible = OFF;
            return;
        }
        if (g.online_mode) {
            online_card_send(g.own[0].cards[g.cursor], g.deck.cards[targetDeck]);
        }
        vgs_oam(SP_CURSOR)->visible = OFF;
        clear_gekiatsu();
        hibana_clear(SP_HIBANA);
    } else {
        timer_start(900, OFF, ON);
        while (cancel) {
            int ry[8];
            int ty[8];
            int rvy[8];
            for (int i = 0; i < 8; i++) {
                ry[i] = get_own_y(0) << 8;
                ty[i] = ry[i];
                rvy[i] = 0;
            }
            int loop_count = -1;
            int deckScale[48];
            for (int i = 0; i < 48; i++) {
                deckScale[i] = 75;
            }

            dropScale = 10;
            int animating = OFF;
            int choose = OFF;
            while (!choose || animating) {
                loop_count++;
                g.player_own_select = ON;
                animating = OFF;
                if (mouse_shown()) {
                    int hover_cursor = find_hover_own_cursor();
                    if (0 <= hover_cursor) {
                        set_player_cursor(hover_cursor, 0 != loop_count);
                    }
                }
                if (push_left()) {
                    set_player_cursor(0 < g.cursor ? g.cursor - 1 : g.own[0].num - 1, ON);
                } else if (push_right()) {
                    set_player_cursor(g.own[0].num <= g.cursor + 1 ? 0 : g.cursor + 1, ON);
                }
                if (!choose && push_y()) {
                    if (!g.online_mode) {
                        vgs_sfx_play(SFX_CURSOR_ENTER);
                        show_winning_hands();
                    }
                }
                int clicked_current = OFF;
                int clk_x, clk_y;
                if (!choose && vgs_mouse_left_clicked(&clk_x, &clk_y)) {
                    int clicked_cursor = -1;
                    for (int i = 0; i < g.own[0].num; i++) {
                        ObjectAttributeMemory* own = vgs_oam(SP_OWN + i);
                        if (position_in_rect(clk_x, clk_y, own->x - 4, own->y - 4, 48, 56)) {
                            clicked_cursor = i;
                            break;
                        }
                    }
                    if (0 <= clicked_cursor) {
                        if (clicked_cursor != g.cursor) {
                            set_player_cursor(clicked_cursor, OFF);
                        } else {
                            clicked_current = ON;
                        }
                    }
                }
                if (!choose && (timer_is_over() || push_a() || clicked_current)) {
                    if (!vgs_key_x()) {
                        vgs_sfx_play(SFX_CHOOSE);
                        choose = ON;
                    }
                }
                vgs_oam(SP_CURSOR)->attr = PTN_CURSOR + ((g.frame & 0b11000) >> 1);
                for (int i = 0; i < 8; i++) {
                    if (g.cursor == i) {
                        ty[i] = (get_own_y(0) - 8) << 8;
                    } else {
                        ty[i] = get_own_y(0) << 8;
                    }

                    if (ty[i] < ry[i]) {
                        if (0 <= rvy[i]) {
                            rvy[i] = -256;
                        }
                        ry[i] += rvy[i];
                        if (-128 < rvy[i]) {
                            rvy[i] += 16;
                        }
                        if (ty[i] >= ry[i]) {
                            rvy[i] = 0;
                            ry[i] = ty[i];
                        }
                    } else if (ry[i] < ty[i]) {
                        if (rvy[i] <= 0) {
                            rvy[i] = 256;
                        }
                        ry[i] += rvy[i];
                        if (128 < rvy[i]) {
                            rvy[i] -= 16;
                        }
                        if (ty[i] >= ry[i]) {
                            rvy[i] = 0;
                            ry[i] = ty[i];
                        }
                    }
                    vgs_oam(SP_OWN + i)->y = ry[i] >> 8;
                }
                for (int i = 0; i < 8; i++) {
                    if (ry[i] != ty[i]) {
                        animating = ON;
                        break;
                    }
                }

                // 現在選択中のカードと一致する月のデッキカードを拡大する
                Card* currentCard = g.own[0].cards[g.cursor];
                int found = OFF;
                int dec_index[4]; // 候補は理論上必ず2以下になる（3になるケースはシャッフルで弾かれている）が念の為4確保
                int found_count = 0;
                for (int i = 0; i < 48; i++) {
                    Card* deckCard = g.deck.cards[i];
                    if (!deckCard) {
                        continue;
                    }
                    if (currentCard->month == deckCard->month) {
                        found = ON;
                        dec_index[found_count++] = i;
                        vgs_oam(SP_DECK + i)->pri = ON;
                        if (deckScale[i] < 100) {
                            deckScale[i] += 5;
                            animating = ON;
                        }
                    } else {
                        vgs_oam(SP_DECK + i)->pri = OFF;
                        if (75 < deckScale[i]) {
                            deckScale[i] -= 5;
                            animating = ON;
                        }
                    }
                    vgs_oam(SP_DECK + i)->scale = deckScale[i];
                    vgs_oam(SP_DECK + i)->x = get_deck_x(i);
                    vgs_oam(SP_DECK + i)->y = get_deck_y(i);
                }
                vgs_oam(SP_DROP)->scale = dropScale;
                if (!found) {
                    vgs_oam(SP_DROP)->visible = ON;
                    if (dropScale < 100) {
                        dropScale += 10;
                        animating = ON;
                    }
                }
                if (found) {
                    if (10 < dropScale) {
                        vgs_oam(SP_DROP)->visible = ON;
                        dropScale -= 10;
                    } else {
                        vgs_oam(SP_DROP)->visible = OFF;
                    }
                    // 2枚以上が重なり合っている場合は位置を調整（3枚目というケースはない）
                    if (2 <= found_count) {
                        if ((dec_index[0] | 1) == (dec_index[1] | 1)) {
                            vgs_oam(SP_DECK + dec_index[0])->y -= 5;
                            vgs_oam(SP_DECK + dec_index[1])->y += 5;
                        }
                        if (dec_index[0] + 2 == dec_index[1]) {
                            vgs_oam(SP_DECK + dec_index[0])->x -= 5;
                            vgs_oam(SP_DECK + dec_index[1])->x += 5;
                        }
                    }
                }
                if (191 < vgs_oam(SP_CURSOR)->y) {
                    vgs_oam(SP_CURSOR)->y -= 1;
                }

                // 取得可能なカードを光らせる
                clear_gekiatsu(); // 一旦表示をOFF
                hibana_rect_clear();
                uint32_t gekiatsu_alpha;
                if ((g.frame & 0x7F) < 0x40) {
                    uint8_t a = (g.frame & 0x3F) << 1;
                    gekiatsu_alpha = a;
                    gekiatsu_alpha <<= 8;
                    gekiatsu_alpha |= a;
                    gekiatsu_alpha <<= 8;
                    gekiatsu_alpha |= a;
                    gekiatsu_alpha |= 1;
                } else {
                    uint8_t a = (0x40 - (g.frame & 0x3F)) << 1;
                    gekiatsu_alpha = a;
                    gekiatsu_alpha <<= 8;
                    gekiatsu_alpha |= a;
                    gekiatsu_alpha <<= 8;
                    gekiatsu_alpha |= a;
                    gekiatsu_alpha |= 1;
                }
                int ga_index = 0;
                for (int i = 0; i < g.own->num; i++) {
                    if (g.own->cards[i]) {
                        if (is_gekiatsu_card(0, g.own->cards[i]) && is_exsit_same_month_in_deck(g.own->cards[i])) {
                            add_player_gekiatsu(g.own->cards[i]);
                            ObjectAttributeMemory* src = vgs_oam(SP_OWN + i);
                            ObjectAttributeMemory* dst = vgs_oam(SP_GEKIATSU + ga_index);
                            dst->x = src->x;
                            dst->y = src->y;
                            dst->scale = src->scale;
                            dst->pri = ON;
                            dst->visible = ON;
                            dst->alpha = gekiatsu_alpha;
                            dst->attr = 0x10000 | PTN_GEKIATSU2;
                            hibana_rect_add(dst->x, dst->y, 48, 48);
                            ga_index++;
                        } else {
                            int found_normal = 0;
                            int found_gekiatsu = 0;
                            for (int j = 0; j < 48; j++) {
                                if (g.deck.cards[j] && g.deck.cards[j]->month == g.own->cards[i]->month) {
                                    if (is_gekiatsu_card(0, g.deck.cards[j])) {
                                        // 激アツカードを取得できるノーマルカードは激アツ扱いにする
                                        found_gekiatsu++;
                                        add_player_gekiatsu(g.own->cards[i]);
                                        break;
                                    } else {
                                        // 普通に取得できるカードを発見したフラグ
                                        found_normal++;
                                    }
                                }
                            }
                            if (found_gekiatsu) {
                                ObjectAttributeMemory* src = vgs_oam(SP_OWN + i);
                                ObjectAttributeMemory* dst = vgs_oam(SP_GEKIATSU + ga_index);
                                dst->x = src->x;
                                dst->y = src->y;
                                dst->scale = src->scale;
                                dst->pri = ON;
                                dst->visible = ON;
                                dst->alpha = gekiatsu_alpha;
                                dst->attr = 0x10000 | PTN_GEKIATSU2;
                                hibana_rect_add(dst->x, dst->y, 48, 48);
                                ga_index++;
                            } else if (found_normal) {
                                ObjectAttributeMemory* src = vgs_oam(SP_OWN + i);
                                ObjectAttributeMemory* dst = vgs_oam(SP_GEKIATSU + ga_index);
                                dst->x = src->x;
                                dst->y = src->y;
                                dst->scale = src->scale;
                                dst->pri = ON;
                                dst->visible = ON;
                                dst->alpha = gekiatsu_alpha;
                                dst->attr = 0x10000 | PTN_GEKIATSU;
                                ga_index++;
                            }
                        }
                    }
                }
                for (int i = 0; i < 48; i++) {
                    if (g.deck.cards[i] && is_gekiatsu_card(0, g.deck.cards[i])) {
                        ObjectAttributeMemory* src = vgs_oam(SP_DECK + i);
                        ObjectAttributeMemory* dst = vgs_oam(SP_GEKIATSU + ga_index);
                        dst->x = src->x;
                        dst->y = src->y;
                        dst->scale = src->scale;
                        dst->pri = ON;
                        dst->visible = ON;
                        dst->alpha = gekiatsu_alpha;
                        dst->attr = 0x10000 | PTN_GEKIATSU2;
                        hibana_rect_add(dst->x - 6, dst->y - 6, 52, 68);
                        ga_index++;
                    }
                }
                hibana_frame(0x01, 32);
                vsync();
                g.frame++;
            }
            clear_gekiatsu();
            hibana_clear(SP_HIBANA);

            // ターゲットデッキ位置を特定
            int selectionCanceled = OFF;
            g.player_own_select = OFF;
            targetDeck = player_think_resolve_target_deck(g.own[0].cards[g.cursor], ON, &selectionCanceled);
            if (selectionCanceled) {
                cancel = ON;
                continue;
            }
            if (targetDeck < 0) {
                cancel = ON;
                continue;
            }
            if (g.online_mode) {
                online_card_send(g.own[0].cards[g.cursor], g.deck.cards[targetDeck]);
            }
            cancel = OFF;
        }
        timer_stop();
    }

    Card* drop = g.own[0].cards[g.cursor];
    if (ai_log_enabled()) {
        ai_putlog_env("%s think drop: %d%s", player_log_prefix(), drop ? drop->id : -1, ai_get_last_think_extra(0));
        ai_watch_begin_after_opp_window(0);
    }
    Card* takenDrop = (0 <= targetDeck && targetDeck < 48) ? g.deck.cards[targetDeck] : NULL;
    g.own[0].cards[g.cursor] = NULL;
    g.own_move_flag[0] = ON;
    g.own_move_x[0] = 0;
    if (g.deck.cards[targetDeck]) {
        add_move_drop(drop, get_own_x(0, g.cursor), get_own_y(0), get_deck_x(targetDeck), get_deck_y(targetDeck), NULL);
    } else {
        add_move_discard(drop, get_own_x(0, g.cursor), get_own_y(0), get_deck_x(targetDeck), get_deck_y(targetDeck), NULL);
    }
    for (int i = 0; i < 48; i++) {
        vgs_oam(SP_DECK + i)->pri = OFF;
    }
    while (g.move_count) {
        vgs_oam(SP_CURSOR)->y++;
        draw_cards();
        if (10 < dropScale) {
            vgs_oam(SP_DROP)->scale = dropScale;
            vgs_oam(SP_DROP)->visible = ON;
            dropScale -= 10;
        } else {
            vgs_oam(SP_DROP)->visible = OFF;
        }
        g.frame++;
    }
    if (!g.deck.cards[targetDeck]) {
        g.deck.num++;
        g.deck.cards[targetDeck] = drop;
    } else {
        Card* deck = g.deck.cards[targetDeck];
        add_move_pick_own(deck, drop, get_deck_x(targetDeck), get_deck_y(targetDeck), player_move_card_to_invent_open);
        g.deck.cards[targetDeck] = NULL;
        g.deck.num--;
        while (g.move_count) {
            draw_cards();
        }
    }
    log_take_cards("drop", drop, takenDrop);
    // 手札数を減らす
    for (int i = g.cursor; i < 7; i++) {
        g.own[0].cards[i] = g.own[0].cards[i + 1];
    }
    g.own[0].cards[7] = NULL;
    g.own[0].num--;
    g.own_move_flag[0] = OFF;

    // 山札をオープン
    analyze_invent(0);
    Card* c = g.floor.cards[g.floor.start];
    add_move_fopen(c, 0, 80, player_on_floor_open);
    g.floor.start++;
    g.floor.num--;
    if (ai_log_enabled()) {
        ai_putlog_env("%s floor open: %d", player_log_prefix(), c ? c->id : -1);
    }
    // vgs_putlog("Closed floor cards left: %d", g.floor.num);
    while (g.move_count) {
        draw_cards();
    }
    log_take_cards("floor", g_player_floor_log_card1, g_player_floor_log_card2);
    g_player_floor_log_card1 = NULL;
    g_player_floor_log_card2 = NULL;

    for (int i = 0; i < 20; i++) {
        draw_cards();
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

static void player_store_invent_open(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    // vgs_putlog("P1 invent+(O): month=%d, index=%d, type=%d", card->month, card->index, card->type);
    g.invent[0][card->type].cards[g.invent[0][card->type].num++] = card;
}

static void player_store_invent_blind(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    static int seq;
    // vgs_putlog("%d: P1 invent+(B): month=%d, index=%d, type=%d", ++seq, card->month, card->index, card->type);
    g.invent[0][card->type].cards[g.invent[0][card->type].num++] = card;
}

static void player_move_card_to_invent_open(Card* card, int x, int y)
{
    int tx = get_invent_x(card, g.invent[0][card->type].num);
    int ty = get_invent_y(0);
    add_move_invent(card, x, y, tx, ty, player_store_invent_open);
}

static void player_move_card_to_invent_blind(Card* card, int x, int y)
{
    int ix = get_invent_x(card, g.invent[0][card->type].num);
    int iy = get_invent_y(0);
    add_move_invent(card, x, y, ix, iy, player_store_invent_blind);
}

static void player_finish_floor_discard(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    int targetDeck = consume_card_target(card);
    if (targetDeck < 0 || targetDeck >= 48) {
        return;
    }
    if (!g.deck.cards[targetDeck]) {
        g.deck.cards[targetDeck] = card;
        g.deck.num++;
    }
}

static void player_finish_floor_drop(Card* dropCard, int x, int y)
{
    (void)x;
    (void)y;
    int dropIdx = card_global_index(dropCard);
    if (dropIdx < 0) {
        return;
    }
    int targetDeck = card_target_deck_map[dropIdx];
    card_target_deck_map[dropIdx] = -1;
    if (targetDeck < 0 || targetDeck >= 48) {
        return;
    }
    Card* deckCard = g.deck.cards[targetDeck];
    if (!deckCard) {
        return;
    }
    add_move_pick_own(deckCard, dropCard, get_deck_x(targetDeck), get_deck_y(targetDeck), player_move_card_to_invent_blind);
    g.deck.cards[targetDeck] = NULL;
    if (g.deck.num > 0) {
        g.deck.num--;
    }
}

static void player_on_floor_open(Card* card, int x, int y)
{
    int targetDeck;
    if (g.auto_play) {
        targetDeck = player_think_resolve_target_deck(card, OFF, NULL);
    } else {
        timer_start(480, OFF, ON);
        targetDeck = player_think_resolve_target_deck(card, OFF, NULL);
        timer_stop();
    }
    if (targetDeck < 0) {
        return;
    }
    int cardIdx = card_global_index(card);
    if (cardIdx < 0) {
        return;
    }
    card_target_deck_map[cardIdx] = targetDeck;
    int tx = get_deck_x(targetDeck);
    int ty = get_deck_y(targetDeck);
    g_player_floor_log_card1 = card;
    g_player_floor_log_card2 = g.deck.cards[targetDeck];
    if (g.deck.cards[targetDeck]) {
        if (is_gekiatsu_card(0, card) || is_gekiatsu_card(0, g.deck.cards[targetDeck])) {
            add_player_gekiatsu(card);
        }
        add_move_drop(card, x, y, tx, ty, player_finish_floor_drop);
    } else {
        add_move_discard(card, x, y, tx, ty, player_finish_floor_discard);
    }
}
