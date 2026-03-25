#pragma once
#include "common.h"

int validate_floor_cards(CardSet* floor);
void reset_cards(void);
void add_move_own(Card* card, int fx, int fy, int tx, int ty, int speed, void (*callback)(Card* card, int x, int y));
void add_move_own_open(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y));
void add_move_fopen(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y));
void add_move_fopen_silent(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y));
void add_move_fadeout(Card* card, int fx, int fy, void (*callback)(Card* card, int x, int y));
void add_move_deck(Card* card, int fx, int fy, int tx, int ty, int speed, void (*callback)(Card* card, int x, int y));
void add_move_drop(Card* card, int fx, int fy, int tx, int ty, void (*callback)(Card* card, int x, int y));
void add_move_discard(Card* card, int fx, int fy, int tx, int ty, void (*callback)(Card* card, int x, int y));
void add_move_pick_own(Card* pick1, Card* pick2, int fx, int fy, void (*callback)(Card* card, int x, int y));
void add_move_invent(Card* card, int fx, int fy, int tx, int ty, void (*callback)(Card* card, int x, int y));
int resolve_target_deck(Card* card, int allowCancel, int* canceled);
int collect_target_deck_indexes(Card* card, int* targetDecks);
int find_empty_deck_slot(void);
void draw_cards(void);
void reset_move_cards(void);
void add_player_gekiatsu(Card* card);

static inline int card_ptn(int month, int index)
{
    month <<= 2;
    month |= index & 3;
    month *= CARD_SIZE;
    month += CARD_TOP;
    month &= 0xFFFF;
    return month;
}

static inline int card_attr(int month, int index)
{
    return card_ptn(month, index) | 0x10000;
}

static inline int card_global_index(Card* card)
{
    if (!card || card < g.cards || card >= g.cards + 48) {
        return -1;
    }
    return (int)(card - g.cards);
}

static inline int get_deck_x(int index)
{
    index /= 2;
    index *= 26;
    index += 36;
    return index;
}

static inline int get_deck_y(int index)
{
    return (index & 1) ? 80 + 16 : 80 - 16;
}

static inline int get_own_x(int p, int index)
{
    return index * 36 + (320 - (g.own[p].num * 36 + 4)) / 2 + 13;
}

static inline int get_own_y(int p)
{
    return p ? 4 : 156;
}

static inline int get_invent_x(Card* card, int index)
{
    int x = 18;
    switch (card->type) {
        case CARD_TYPE_GOKOU:
            x += index * 12;
            break;
        case CARD_TYPE_TANE:
            x += 64;
            x += index * 10;
            break;
        case CARD_TYPE_TAN:
            x += 140;
            x += index * 8;
            break;
        case CARD_TYPE_KASU:
            x += 220;
            x += index * 6;
            break;
    }
    return x;
}

static inline int get_invent_y(int p)
{
    return p ? 36 : 124;
}
