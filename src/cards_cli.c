#include "common.h"
#include "cards.h"

static int hibana_requested;

void add_player_gekiatsu(Card* card)
{
    // 重複チェック
    for (int i = 0; i < g.player_gekiatsu.num; i++) {
        if (g.player_gekiatsu.cards[i] == card) {
            return;
        }
    }
    g.player_gekiatsu.cards[g.player_gekiatsu.num++] = card;
}

void reset_move_cards(void)
{
    vgs_memset(&g.moves, 0, sizeof(g.moves));
    g.move_count = 0;
}

int collect_target_deck_indexes(Card* card, int* targetDecks)
{
    int count = 0;
    for (int i = 0; i < 48; i++) {
        Card* deckCard = g.deck.cards[i];
        if (deckCard && deckCard->month == card->month) {
            targetDecks[count++] = i;
        }
    }
    return count;
}

int find_empty_deck_slot(void)
{
    for (int i = 0; i < 48; i++) {
        if (!g.deck.cards[i]) {
            return i;
        }
    }
    return -1;
}

static int show_deck_target_selector(const int* targetDecks, int targetDeckCount, int allowCancel, int* canceled)
{
    ObjectAttributeMemory* dc = vgs_oam(SP_DECK_CURSOR);
    int dcScale = 100;
    for (int i = 0; i < 2; i++) {
        dc[i].visible = ON;
        dc[i].pri = ON;
        dc[i].scale = dcScale;
    }
    int key = 0;
    int key_prev = -1;
    int key_enabled = OFF;
    int ptr = 0;
    int targetDeck = targetDecks[ptr];
    if (canceled) {
        *canceled = OFF;
    }
    while (ON) {
        key_prev = key;
        key = vgs_key_code();
        if (!key_enabled) {
            if (vgs_key_code_a(key)) {
                key = 0;
            } else {
                key_enabled = ON;
            }
        }
        if (timer_is_over() || (vgs_key_code_a(key) && !vgs_key_code_a(key_prev))) {
            vgs_sfx_play(SFX_CHOOSE);
            break;
        }
        if (allowCancel && vgs_key_code_b(key) && !vgs_key_code_b(key_prev)) {
            if (canceled) {
                vgs_sfx_play(SFX_CURSOR_LEAVE);
                *canceled = ON;
            }
            targetDeck = -1;
            break;
        }
        if ((vgs_key_code_left(key) && !vgs_key_code_left(key_prev)) ||
            (vgs_key_code_up(key) && !vgs_key_code_up(key_prev))) {
            vgs_sfx_play(SFX_MOVE_CURSOR2);
            ptr--;
            if (ptr < 0) {
                ptr = targetDeckCount - 1;
            }
            targetDeck = targetDecks[ptr];
            dcScale = 80;
        } else if ((vgs_key_code_right(key) && !vgs_key_code_right(key_prev)) ||
                   (vgs_key_code_down(key) && !vgs_key_code_down(key_prev))) {
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
        vsync();
        g.frame++;
    }
    if ((!allowCancel || !canceled || !*canceled) && targetDeck >= 0) {
        while (100 < dcScale) {
            dcScale -= 10;
            dc[0].scale = dcScale;
            dc[1].scale = dcScale;
            dc[0].attr = PTN_DECK_CURSOR + ((g.frame & 0b1100) >> 2) * CARD_SIZE;
            vsync();
            g.frame++;
        }
    }
    dc[0].visible = OFF;
    dc[1].visible = OFF;
    return targetDeck;
}

int resolve_target_deck(Card* card, int allowCancel, int* canceled)
{
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
            targetDeck = show_deck_target_selector(targetDecks, targetDeckCount, allowCancel, canceled);
            if (g.online_mode && !allowCancel) {
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
    return targetDeck;
}

static void add_zanzo(ObjectAttributeMemory* from)
{
    if (g.zanzo.alpha[g.zanzo.ptr]) {
        g.zanzo.ptr++;
        g.zanzo.ptr &= ZANZO_MAX - 1;
        return;
    }
    ObjectAttributeMemory* oam = vgs_oam(SP_ZANZO + g.zanzo.ptr);
    oam->attr = from->attr;
    oam->x = from->x;
    oam->y = from->y;
    oam->rotate = from->rotate;
    oam->scale = from->scale;
    oam->slx = from->slx;
    oam->sly = from->sly;
    g.zanzo.alpha[g.zanzo.ptr] = 255;
    g.zanzo.ptr++;
    g.zanzo.ptr &= ZANZO_MAX - 1;
}

static void draw_cards_zanzo(void)
{
    for (int i = 0; i < ZANZO_MAX; i++) {
        ObjectAttributeMemory* oam = vgs_oam(SP_ZANZO + i);
        if (g.zanzo.alpha[i]) {
            uint32_t a = g.zanzo.alpha[i];
            a <<= 8;
            a |= g.zanzo.alpha[i];
            a <<= 8;
            a |= g.zanzo.alpha[i];
            oam->visible = ON;
            oam->alpha = a;
            g.zanzo.alpha[i] -= 32;
            if (g.zanzo.alpha[i] < 0) {
                g.zanzo.alpha[i] = 0;
            }
        } else {
            oam->visible = OFF;
        }
    }
}

static void add_move_init(int i)
{
    vgs_memset(&g.moves.records[i], 0, sizeof(CardMoveRecord));
    ObjectAttributeMemory* o = vgs_oam(SP_MOVE + i);
    o->scale = 100;
    o->rotate = 0;
    o->slx = OFF;
    o->sly = OFF;
    o->mask = OFF;
    o->alpha = -1;
    o->pri = ON;
    g.moves.records[i].o = o;
}

void add_move_own(Card* card, int fx, int fy, int tx, int ty, int speed, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = MTYPE_OWN;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->tx = tx << 8;
    rec->ty = ty << 8;
    rec->speed = speed;
    rec->callback = callback;
    rec->angle = vgs_degree(fx, fy, tx, ty);
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
}

void add_move_own_open(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = MTYPE_OWN_OPEN;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->jump = -384;
    rec->callback = callback;
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
}

static void add_move_fopen_core(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y), int play_sfx)
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = MTYPE_FOPEN;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->jump = -384;
    rec->callback = callback;
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
    if (play_sfx) {
        vgs_sfx_play(SFX_CARD_OPEN);
    }
}

void add_move_fopen(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y))
{
    add_move_fopen_core(card, fx, fy, callback, ON);
}

void add_move_fopen_silent(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y))
{
    add_move_fopen_core(card, fx, fy, callback, OFF);
}

void add_move_fadeout(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = MTYPE_FADEOUT;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->jump = 0;
    rec->alpha = 255;
    rec->callback = callback;
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
}

void add_move_deck(Card* card, int fx, int fy, int tx, int ty, int speed, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = MTYPE_DECK;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->tx = tx << 8;
    rec->ty = ty << 8;
    rec->speed = speed;
    rec->callback = callback;
    rec->angle = vgs_degree(fx, fy, tx, ty);
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
}

void add_move_drop(Card* card, int fx, int fy, int tx, int ty, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = MTYPE_DROP;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->tx = tx << 8;
    rec->ty = ty << 8;
    rec->speed = 300;
    rec->callback = callback;
    rec->angle = vgs_degree(fx, fy, tx, ty);
    rec->gekiatsu = OFF;
    // vgs_putlog("check gekiatsu: %d", g.player_gekiatsu.num);
    for (int i = 0; i < g.player_gekiatsu.num; i++) {
        if (g.player_gekiatsu.cards[i] == card) {
            rec->gekiatsu = ON;
            break;
        }
    }
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
    vgs_sfx_play(SFX_CARD_THROW);
}

void add_move_discard(Card* card, int fx, int fy, int tx, int ty, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = MTYPE_DISCARD;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->tx = tx << 8;
    rec->ty = ty << 8;
    rec->speed = 500;
    rec->callback = callback;
    rec->angle = vgs_degree(fx, fy, tx, ty);
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
    vgs_sfx_play(SFX_CARD_DISCARD);
}

void add_move_pick_own(Card* pick1, Card* pick2, int fx, int fy, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = pick1;
    rec->mtype = MTYPE_PICK;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->tx = (fx - 20) << 8;
    rec->ty = fy << 8;
    rec->speed = 150;
    rec->callback = callback;
    rec->angle = vgs_degree(fx, fy, fx - 20, fy);
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;

    rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    rec->exist = ON;
    rec->card = pick2;
    rec->mtype = MTYPE_PICK;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->tx = (fx + 20) << 8;
    rec->ty = fy << 8;
    rec->speed = 150;
    rec->callback = callback;
    rec->angle = vgs_degree(fx, fy, fx + 20, fy);
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
}

void add_move_invent(Card* card, int fx, int fy, int tx, int ty, void (*callback)(Card* card, int x, int y))
{
    CardMoveRecord* rec = &g.moves.records[g.moves.ptr];
    add_move_init(g.moves.ptr);
    int hot = is_gekiatsu_card(g.current_player, card);
    rec->exist = ON;
    rec->card = card;
    rec->mtype = hot ? MTYPE_INVENT_HOT : MTYPE_INVENT;
    rec->fx = fx << 8;
    rec->fy = fy << 8;
    rec->tx = tx << 8;
    rec->ty = ty << 8;
    rec->speed = 600;
    rec->callback = callback;
    rec->angle = vgs_degree(fx, fy, tx, ty);
    g.moves.ptr++;
    g.moves.ptr &= MOVE_MAX - 1;
    g.move_count++;
    vgs_sfx_play(hot ? SFX_GET_HOT : SFX_GET);
}

static inline int move_cards_step_coord(int* coord, int target, int delta)
{
    if (0 == delta) {
        *coord = target;
        return OFF;
    }
    if ((*coord & 0xFFFF00) == (target & 0xFFFF00)) {
        return OFF;
    }
    const int dir = *coord < target;
    *coord += delta;
    if (dir != (*coord < target)) {
        *coord = target;
    }
    return ON;
}

static inline int move_cards_step_xy(CardMoveRecord* rec)
{
    const int dx = (vgs_cos(rec->angle) * rec->speed) / 100;
    const int dy = (vgs_sin(rec->angle) * rec->speed) / 100;
    int moved = move_cards_step_coord(&rec->fx, rec->tx, dx);
    if (move_cards_step_coord(&rec->fy, rec->ty, dy)) {
        moved = ON;
    }
    return moved;
}

static inline int move_cards_step_x(CardMoveRecord* rec)
{
    const int dx = (vgs_cos(rec->angle) * rec->speed) / 100;
    return move_cards_step_coord(&rec->fx, rec->tx, dx);
}

static inline void move_cards_decay_speed(CardMoveRecord* rec, int step)
{
    if (rec->speed > 100 && (rec->frame & 1)) {
        rec->speed -= step;
    }
}

static inline void move_cards_add_zanzo(CardMoveRecord* rec, ObjectAttributeMemory* o, int mask)
{
    if ((rec->frame & mask) == 0) {
        add_zanzo(o);
    }
}

static inline void move_cards_finish(CardMoveRecord* rec)
{
    g.move_count--;
    rec->exist = OFF;
    if (rec->callback) {
        rec->callback(rec->card, rec->fx >> 8, rec->fy >> 8);
    }
    rec->o->scale = 0;
    rec->o->visible = OFF;
}

static inline int move_cards_open_flip(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    o->sly = ON;
    if (rec->frame < 12) {
        rec->card->reversed = ON;
        o->scale = 100 - (95 / 12) * rec->frame;
        if (rec->jump != 384) {
            rec->fy += rec->jump;
            rec->jump += 32;
        }
        return OFF;
    }
    if (rec->frame < 24) {
        rec->card->reversed = OFF;
        o->scale = 5 + (95 / 12) * (rec->frame % 12);
        if (rec->jump != 384) {
            rec->fy += rec->jump;
            rec->jump += 32;
        }
        return OFF;
    }
    o->scale = 100;
    if (rec->jump < 384) {
        rec->fy += rec->jump;
        rec->jump += 32;
        return OFF;
    }
    return ON;
}

static inline void move_cards_own(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    o->scale = 100;
    move_cards_add_zanzo(rec, o, 0x3);
    const int moved = move_cards_step_xy(rec);
    o->rotate += 15;
    move_cards_decay_speed(rec, 1);
    if (!moved) {
        move_cards_finish(rec);
    }
}

static inline void move_cards_own_open(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    o->rotate = 0;
    o->slx = OFF;
    if (move_cards_open_flip(rec, o)) {
        move_cards_finish(rec);
    }
}

static inline void move_cards_fopen(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    o->rotate = 0;
    o->slx = OFF;
    if (move_cards_open_flip(rec, o) && rec->frame >= 60) {
        move_cards_finish(rec);
    }
}

static inline void move_cards_fadeout(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    rec->fy += rec->jump;
    rec->jump += 32;
    rec->alpha -= 8;
    if (rec->alpha < 1) {
        move_cards_finish(rec);
    } else {
        uint32_t a = rec->alpha;
        a <<= 8;
        a |= rec->alpha;
        a <<= 8;
        a |= rec->alpha;
        o->alpha = a;
    }
}

static inline void move_cards_deck(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    if (rec->frame < 12) {
        o->slx = OFF;
        o->sly = ON;
        rec->card->reversed = ON;
        o->scale = 100 - (95 / 12) * rec->frame;
    } else if (rec->frame < 24) {
        rec->card->reversed = OFF;
        o->scale = 5 + (95 / 12) * (rec->frame % 12);
    } else if (rec->frame == 24) {
        o->scale = 100;
        o->sly = OFF;
    }
    const int moved = move_cards_step_xy(rec);
    move_cards_decay_speed(rec, 1);
    if (!moved) {
        if (o->scale != 75) {
            o->scale--;
        } else {
            move_cards_finish(rec);
        }
    } else {
        move_cards_add_zanzo(rec, o, 0x3);
    }
}

static inline void move_cards_drop(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    if (rec->frame < 2) {
        o->slx = OFF;
        o->sly = OFF;
        rec->scale = 100;
    }
    const int moved = move_cards_step_xy(rec);
    move_cards_decay_speed(rec, 1);
    o->scale = rec->scale;
    if (!moved || 180 < rec->frame) {
        season_add(rec->card->month, o->x + 12, o->y + 12);
        if (rec->gekiatsu) {
            dot_add_range(o->x, o->y, DotColorYellow, 40, 40, 4);
            dot_add_range(o->x, o->y, DotColorRed, 40, 40, 8);
            dot_add_range(o->x, o->y, DotColorWhite, 40, 40, 8);
        } else {
            dot_add_range(o->x, o->y, DotColorWhite, 40, 40, 8);
        }
        if (rec->i[0]) {
            rec->i[0] = 0;
            vgs_sfx_stop(SFX_CARD_THROW);
            vgs_sfx_play(SFX_DROP_DOWN);
        }
        if (rec->scale > 75) {
            rec->scale -= 15;
            if (rec->scale < 75) {
                rec->scale = 75;
            }
        } else {
            if (rec->gekiatsu) {
                dot_add_range(o->x, o->y, DotColorRed, 40, 40, 24);
                dot_add_range(o->x, o->y, DotColorYellow, 40, 40, 12);
                dot_add_range(o->x, o->y, DotColorWhite, 40, 40, 32);
            } else {
                dot_add_range(o->x, o->y, DotColorWhite, 40, 40, 32);
            }
            vgs_sfx_stop(SFX_DROP_DOWN);
            vgs_sfx_play(SFX_ATTACK);
            move_cards_finish(rec);
        }
    } else {
        rec->i[0] = 1;
        if (rec->scale < 350) {
            rec->scale += 7;
        }
        o->rotate = rec->frame << 4;
        move_cards_add_zanzo(rec, o, 0x1);
    }
}

static inline void move_cards_discard(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    if (rec->frame < 2) {
        o->slx = OFF;
        o->sly = OFF;
        o->rotate = 0;
        rec->scale = 100;
    }
    const int moved = move_cards_step_xy(rec);
    move_cards_decay_speed(rec, 1);
    if (rec->scale > 75) {
        rec->scale--;
    }
    o->scale = rec->scale;
    if (!moved) {
        move_cards_finish(rec);
    } else {
        move_cards_add_zanzo(rec, o, 0x1);
    }
}

static inline void move_cards_pick(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    if (rec->frame < 2) {
        o->slx = OFF;
        o->sly = OFF;
        o->rotate = 0;
        rec->scale = 75;
    }
    const int moved = move_cards_step_x(rec);
    move_cards_decay_speed(rec, 2);
    if (rec->scale < 120) {
        rec->scale += 5;
    }
    o->scale = rec->scale;
    if (!moved) {
        move_cards_finish(rec);
    } else {
        move_cards_add_zanzo(rec, o, 0x1);
    }
}

static inline void move_cards_invent(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    if (rec->frame < 30) {
        o->slx = OFF;
        o->sly = OFF;
        o->rotate = 0;
        o->scale = 120;
        rec->scale = 120;
        return;
    }
    const int moved = move_cards_step_xy(rec);
    move_cards_decay_speed(rec, 1);
    if (rec->scale > 50) {
        rec->scale -= 2;
    }
    o->scale = rec->scale;
    if (!moved) {
        move_cards_finish(rec);
    } else {
        move_cards_add_zanzo(rec, o, 0x1);
    }
}

static inline void move_cards_invent_hot(CardMoveRecord* rec, ObjectAttributeMemory* o)
{
    if (rec->frame < 30) {
        o->slx = OFF;
        o->sly = OFF;
        o->rotate = 0;
        o->scale = 120;
        rec->scale = 120;
        return;
    }
    const int hx = rec->fx >> 8;
    const int hy = rec->fy >> 8;
    hibana_requested = ON;
    hibana_rect_add(hx, hy, 40, 40);
    hibana_rect_add(hx + 4, hy + 4, 40, 40);
    dot_add_range(hx, hy, DotColorRed, 40, 40, 32);
    const int moved = move_cards_step_xy(rec);
    move_cards_decay_speed(rec, 1);
    if (rec->scale > 50) {
        rec->scale -= 2;
    }
    o->scale = rec->scale;
    if (!moved) {
        move_cards_finish(rec);
    } else {
        move_cards_add_zanzo(rec, o, 0x1);
    }
}

static void move_cards(void)
{
    for (int i = 0; i < MOVE_MAX; i++) {
        CardMoveRecord* rec = &g.moves.records[i];
        ObjectAttributeMemory* o = vgs_oam(SP_MOVE + i);
        if (rec->exist) {
            rec->frame++;
            o->visible = ON;
            switch (rec->mtype) {
                case MTYPE_OWN: move_cards_own(rec, o); break;
                case MTYPE_OWN_OPEN: move_cards_own_open(rec, o); break;
                case MTYPE_FOPEN: move_cards_fopen(rec, o); break;
                case MTYPE_DECK: move_cards_deck(rec, o); break;
                case MTYPE_DROP: move_cards_drop(rec, o); break;
                case MTYPE_DISCARD: move_cards_discard(rec, o); break;
                case MTYPE_PICK: move_cards_pick(rec, o); break;
                case MTYPE_INVENT: move_cards_invent(rec, o); break;
                case MTYPE_INVENT_HOT: move_cards_invent_hot(rec, o); break;
                case MTYPE_FADEOUT: move_cards_fadeout(rec, o); break;
            }
            if (rec->card->reversed) {
                o->attr = CARD_REV;
            } else {
                o->attr = card_attr(rec->card->month, rec->card->index);
            }
            o->x = rec->fx >> 8;
            o->y = rec->fy >> 8;
        } else {
            o->visible = OFF;
        }
    }
}

// 山札を描画
static void draw_cards_floor(void)
{
    ObjectAttributeMemory* o[2];
    o[0] = vgs_oam(SP_FLOOR);
    o[1] = vgs_oam(SP_FLOOR + 1);
    o[0]->visible = OFF;
    o[1]->visible = OFF;
    if (1 <= g.floor.num) {
        o[0]->visible = ON;
        o[0]->x = 0;
        o[0]->y = 80;
        o[0]->attr = CARD_REV;
    }
    if (2 <= g.floor.num) {
        o[1]->visible = ON;
        o[1]->x = 3;
        o[1]->y = 83;
        o[1]->attr = CARD_REV;
    }
}

// 場札を描画
static void draw_cards_deck(void)
{
    for (int i = 0; i < 48; i++) {
        ObjectAttributeMemory* o = vgs_oam(SP_DECK + i);
        if (g.deck.cards[i]) {
            o->visible = ON;
            o->x = get_deck_x(i);
            o->y = get_deck_y(i);
            o->attr = card_attr(g.deck.cards[i]->month, g.deck.cards[i]->index);
            o->scale = 75;
        } else {
            o->visible = OFF;
        }
    }
}

static void draw_cards_own(void)
{
    for (int p = 0; p < 2; p++) {
        const int force_open = (p == 1 && g.open_mode && !g.online_mode);
        for (int i = 0; i < 8; i++) {
            ObjectAttributeMemory* o = vgs_oam(SP_OWN + p * 48 + i);
            if (g.own[p].cards[i]) {
                int move_x = 0;
                if (g.own_move_flag[p]) {
                    int n = 0;
                    for (n = 0; n < 8; n++) {
                        if (!g.own[p].cards[n]) {
                            break;
                        }
                    }
                    move_x = i < n ? g.own_move_x[p] : -g.own_move_x[p];
                }
                o->visible = ON;
                o->x = get_own_x(p, i) + move_x;
                o->y = get_own_y(p);
                if (!force_open && g.own[p].cards[i]->reversed) {
                    o->attr = CARD_REV;
                } else {
                    o->attr = card_attr(g.own[p].cards[i]->month, g.own[p].cards[i]->index);
                }
            } else {
                o->visible = OFF;
            }
        }
        if (g.own_move_flag[p] && g.own_move_x[p] < 18) {
            g.own_move_x[p]++;
        }
    }
}

static void draw_cards_invent(void)
{
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 4; t++) {
            for (int i = 0; i < 24; i++) {
                ObjectAttributeMemory* o = vgs_oam(SP_INVENT + p * 24 * 4 + t * 24 + 23 - i);
                Card* card = g.invent[p][t].cards[i];
                if (card) {
                    o->visible = ON;
                    o->x = get_invent_x(card, i);
                    o->y = get_invent_y(p);
                    o->attr = card_attr(card->month, card->index);
                    o->scale = 50;
                } else {
                    o->visible = OFF;
                }
            }
        }
    }
}

static int has_active_hibana(void)
{
    for (int i = 0; i < HIBANA_MAX; i++) {
        if (vgs_oam(SP_HIBANA + i)->visible) {
            return ON;
        }
    }
    return OFF;
}

// カードを描画
void draw_cards(void)
{
    vsync();
    for (int i = 0; i < MOVE_MAX; i++) {
        CardMoveRecord* rec = &g.moves.records[i];
        if (!rec->exist) {
            continue;
        }
        if (rec->card) {
            if (rec->mtype == MTYPE_OWN_OPEN || rec->mtype == MTYPE_FOPEN) {
                rec->card->reversed = OFF;
            }
            if (rec->tx || rec->ty) {
                rec->fx = rec->tx;
                rec->fy = rec->ty;
            }
        }
        rec->exist = OFF;
        if (g.move_count > 0) {
            g.move_count--;
        }
        if (rec->callback) {
            rec->callback(rec->card, rec->fx >> 8, rec->fy >> 8);
        }
    }
}

static inline int floor_index(Card* card)
{
    if (!card) {
        vgs_putlog("Invalid argument");
        return -1;
    }
    for (int i = 0; i < 48; i++) {
        if (g.floor.cards[i] == card) {
            return i;
        }
    }
    vgs_putlog("Not found!");
    return -1;
}

static void initial_own_open(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    int fi = floor_index(card);
    if (fi < 0 || fi >= 8) {
        return;
    }
    g.own[0].cards[fi] = card;
    g.own[0].cards[fi]->reversed = OFF;
}

static void initial_enemy_open(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    int fi = floor_index(card);
    if (fi < 8 || fi >= 16) {
        return;
    }
    g.own[1].cards[fi - 8] = card;
    g.own[1].cards[fi - 8]->reversed = OFF;
}

static void initial_own_move(Card* card, int x, int y)
{
    int fi = floor_index(card);
    if (fi < 0) {
        return;
    }
    if (fi < 8) {
        add_move_own_open(card, x, y, initial_own_open);
    } else if (fi < 16) {
        if (g.open_mode && !g.online_mode) {
            add_move_fopen_silent(card, x, y, initial_enemy_open);
        } else {
            g.own[1].cards[fi - 8] = card;
        }
    }
}

static void initial_deck_move(Card* card, int x, int y)
{
    (void)x;
    (void)y;
    int fi = floor_index(card);
    if (fi >= 16) {
        int deckIndex = fi - 16;
        if (deckIndex < 48) {
            g.deck.cards[deckIndex] = card;
            g.deck.num++;
        }
    }
}

void initial_drawing_cards(void)
{
    set_fade(255);
    while (g.frame < 32) {
        if (g.frame < 16) {
            draw_fade2(15 - g.frame);
        } else if (g.frame == 16) {
            vgs_cls_bg(3, 0);
        }
        int i = g.frame >> 2;
        if (0 == (i & 0x03)) {
            vgs_sfx_play(SFX_CARD_DISCARD);
        }
        switch (g.frame & 0x03) {
            case 0:
                add_move_own(g.floor.cards[i], 0, 80, get_own_x(0, i), get_own_y(0), 200 + i * 46, initial_own_move);
                break;
            case 2:
                add_move_own(g.floor.cards[8 + i], 0, 80, get_own_x(1, i), get_own_y(1), 200 + i * 37, initial_own_move);
                break;
        }
        set_fade(255 - (g.frame << 3));
        draw_cards();
        g.frame++;
    }
    set_fade(OFF);

    while (g.frame < 32 + 8 * 8) {
        if (7 == (g.frame & 0x07)) {
            int i = (g.frame - 32) >> 3;
            i = 7 - i;
            vgs_sfx_play(SFX_CARD_DRAW);
            add_move_deck(g.floor.cards[16 + i], 0, 80, get_deck_x(i), get_deck_y(i), 100 + i * 45, initial_deck_move);
        }
        draw_cards();
        g.frame++;
    }

    while (g.move_count) {
        draw_cards();
    }
}

int validate_floor_cards(CardSet* floor)
{
    static const unsigned char thresholds[3] = {4, 4, 3};

    if (floor->num != 48) {
        return OFF; // need 48 cards
    }

    int detect[48];
    vgs_memset(detect, 0, sizeof(detect));
    for (int i = 0; i < 48; i++) {
        if (detect[floor->cards[i]->id]) {
            return OFF; // dup card detected
        }
        detect[floor->cards[i]->id] = ON;
    }

    for (int block = 0; block < 3; block++) {
        unsigned char month_counts[12] = {0};
        int start = block * 8;
        for (int i = 0; i < 8; i++) {
            Card* card = floor->cards[start + i];
            if (!card) {
                continue;
            }
            if (++month_counts[card->month] >= thresholds[block]) {
                return OFF;
            }
        }
    }

    for (int block = 0; block < 3; block++) {
        unsigned char month_counts[12] = {0};
        int pairs = 0;
        int start = block * 8;
        for (int i = 0; i < 8; i++) {
            Card* card = floor->cards[start + i];
            if (!card) {
                continue;
            }
            unsigned char count = ++month_counts[card->month];
            if (!(count & 1)) {
                pairs++;
            }
        }
        if (pairs == 4) {
            return OFF;
        }
    }

    return ON;
}

void reset_cards()
{
    vgs_memset(&g.cards, 0, sizeof(g.cards));
    vgs_memset(&g.deck, 0, sizeof(g.deck));
    vgs_memset(&g.floor, 0, sizeof(g.floor));
    vgs_memset(&g.invent, 0, sizeof(g.invent));
    vgs_memset(&g.moves, 0, sizeof(g.moves));
    vgs_memset(&g.own, 0, sizeof(g.own));
    vgs_memset(&g.stats, 0, sizeof(g.stats));
    vgs_memset(&g.zanzo, 0, sizeof(g.zanzo));
    g.move_count = 0;
    g.frame = 0;

    vgs_memset(card_target_deck_map, 0xFF, sizeof(card_target_deck_map));

    hibana_clear(SP_HIBANA);

    for (int i = 0; i < MOVE_MAX; i++) {
        vgs_sprite(SP_MOVE + i, OFF, 0, 0, 4, 1, 128);
    }
    for (int i = 0; i < ZANZO_MAX; i++) {
        vgs_sprite(SP_ZANZO + i, OFF, 0, 0, 4, 1, 128);
    }
    for (int i = 0; i < 48; i++) {
        vgs_sprite(SP_DECK + i, OFF, 0, 0, 4, 1, 128);
        vgs_sprite(SP_OWN + i, OFF, 0, 0, 4, 1, 128);
        vgs_sprite(SP_OWN + 48 + i, OFF, 0, 0, 4, 1, 128);
    }
    for (int i = 0; i < 24 * 4 * 2; i++) {
        vgs_sprite(SP_INVENT + i, OFF, 0, 0, 4, 1, 128);
    }
    vgs_sprite(SP_FLOOR, OFF, 0, 0, 4, 1, 128);
    vgs_sprite(SP_FLOOR + 1, OFF, 0, 0, 4, 1, 128);
    vgs_sprite(SP_DROP, OFF, 0, 0, 4, 0, PTN_DROP);
    vgs_sprite(SP_DECK_CURSOR, OFF, 0, 0, 4, 0, PTN_DECK_CURSOR);
    vgs_sprite(SP_DECK_CURSOR + 1, OFF, 0, 0, 4, 0, PTN_DECK_CURSOR);

    // カード実体の初期化
    for (int i = 0; i < 48; i++) {
        g.cards[i].type = card_types[i];
        g.cards[i].reversed = ON; // 裏面にしておく
        g.cards[i].month = i / 4; // 月
        g.cards[i].index = i & 3; // インデックス
        g.cards[i].id = i;        // ID
    }

    // Shuffle: 全てのカードを山札にランダムで並べる
    if (!g.online_mode || g.oya) {
        while (TRUE) {
            vgs_memset(g.floor.cards, 0, sizeof(g.floor.cards));
            g.floor.num = 0;

            // とりあえずランダムに並べる
            for (int i = 0; i < 48; i++) {
                int r = vgs_rand() % 48;
                while (g.floor.cards[r]) {
                    r = vgs_rand() % 48;
                }
                g.floor.cards[r] = &g.cards[i];
                g.floor.num++;
            }

            if (validate_floor_cards(&g.floor)) {
                break;
            }
        }
        if (g.online_mode) {
            online_deck_send(&g.floor);
        }
    } else {
        // オンラインモードの子の場合は親から送信されたものを使用する
        vgs_draw_mode(3, ON);
        vgs_draw_boxf(3, 0, 0, 324, 204, 1);
        int recv_result = online_deck_recv(&g.floor);
        g.com_wait_frames = 0;
        while (0 == recv_result) {
            vsync();
            recv_result = online_deck_recv(&g.floor);
            g.com_wait_frames++;
        }
        g.com_wait_frames = 0;
        if (recv_result < 0) {
            g.online_error = ON;
            return;
        }
        vgs_draw_mode(3, OFF);
        draw_fade2(15);
    }

    // カード数調整
    g.own[0].num = 8;
    g.own[1].num = 8;
    g.floor.num -= 24;
    g.floor.start = 24;

    // ドロー
    initial_drawing_cards();
}
