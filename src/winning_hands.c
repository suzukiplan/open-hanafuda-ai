#include "common.h"
#include "cards.h"

WinningHand winning_hands[WINNING_HAND_MAX] = {
    {WID_GOKOU, "Five Brights", "All Five Light Cards United", "Gokou", 10, PTN_WH_GOKOU, 14, 11},
    {WID_SHIKOU, "Four Brights", "Four Lights Without the Rain", "Shikou", 8, PTN_WH_SHIKOU, 12, 5},
    {WID_AME_SHIKOU, "Rainy Four Brights", "Four Lights Beneath the Rain", "Ame-Shikou", 7, PTN_WH_SHIKOU, 12, 5},
    {WID_SANKOU, "Three Brights", "A Trio of Radiant Lights", "Sankou", 5, PTN_WH_SANKOU, 14, 14},
    {WID_HANAMI, "Hanami - A Cup Under the Blossoms", "Enjoying Sake Beneath the Cherry Trees", "Hanami-Sake", 5, PTN_WH_HANAMI, 20, 16},
    {WID_TSUKIMI, "Tsukimi - A Cup Under the Moon", "Enjoying Sake Beneath the Autumn Moon", "Tsukimi-Sake", 5, PTN_WH_TSUKIMI, 20, 16},
    {WID_ISC, "Ino-shika-cho - Boar, Deer, and Butterfly", "A Gathering of Nature's Grace", "Ino-shika-cho", 5, PTN_WH_ISC, 11, 8},
    {WID_DBTAN, "Crimson and Azure Verses", "Six Ribbons of Poetry and Grace", "Akatan+Aotan", 10, PTN_WH_DBTAN, 12, 12},
    {WID_AKATAN, "Red Poetry Ribbons", "Three Scarlet Verses of Elegance", "Akatan", 5, PTN_WH_AKATAN, 2, 8},
    {WID_AOTAN, "Blue Poetry Ribbons", "Three Azure Verses of Serenity", "Aotan", 5, PTN_WH_AOTAN, 2, 4},
    {WID_TANE, "Animal Set", "Five Cards of Flora and Fauna", "Tane", 1, PTN_WH_TANE, 12, 4},
    {WID_TAN, "Ribbon Set", "Five Elegant Verse Ribbons", "Tan", 1, PTN_WH_TAN, 12, 6},
    {WID_KASU, "Plain Set", "Ten Modest Flower Cards", "Kasu", 1, PTN_WH_KASU, 14, 12},
};

static WinningHand* page0[] = {
    &winning_hands[WID_GOKOU],
    &winning_hands[WID_SHIKOU],
    &winning_hands[WID_AME_SHIKOU],
    &winning_hands[WID_SANKOU],
    NULL,
};

static WinningHand* page1[] = {
    &winning_hands[WID_HANAMI],
    &winning_hands[WID_TSUKIMI],
    &winning_hands[WID_AKATAN],
    &winning_hands[WID_AOTAN],
    NULL,
};

static WinningHand* page2[] = {
    &winning_hands[WID_ISC],
    &winning_hands[WID_TANE],
    &winning_hands[WID_TAN],
    &winning_hands[WID_KASU],
    NULL,
};

static WinningHand** pages[] = {page0, page1, page2};

static int winning_hand_minimum[WINNING_HAND_MAX];
static int winning_hand_cards[WINNING_HAND_MAX][25];
static int winning_hand_must[WINNING_HAND_MAX][25];

static void init(void)
{
    if (winning_hand_cards[0][0]) {
        return; // 初期化済み
    }
    vgs_memset(winning_hand_minimum, 0, sizeof(winning_hand_minimum));
    vgs_memset(winning_hand_cards, 0, sizeof(winning_hand_cards));
    vgs_memset(winning_hand_must, 0, sizeof(winning_hand_must));

    winning_hand_minimum[WID_GOKOU] = 5;
    winning_hand_cards[WID_GOKOU][0] = card_attr(0, 3);
    winning_hand_cards[WID_GOKOU][1] = card_attr(2, 3);
    winning_hand_cards[WID_GOKOU][2] = card_attr(7, 3);
    winning_hand_cards[WID_GOKOU][3] = card_attr(10, 3);
    winning_hand_cards[WID_GOKOU][4] = card_attr(11, 3);

    winning_hand_minimum[WID_SHIKOU] = 4;
    winning_hand_cards[WID_SHIKOU][0] = card_attr(0, 3);
    winning_hand_cards[WID_SHIKOU][1] = card_attr(2, 3);
    winning_hand_cards[WID_SHIKOU][2] = card_attr(7, 3);
    winning_hand_cards[WID_SHIKOU][3] = card_attr(11, 3);

    winning_hand_minimum[WID_AME_SHIKOU] = 4;
    winning_hand_cards[WID_AME_SHIKOU][0] = card_attr(10, 3);
    winning_hand_cards[WID_AME_SHIKOU][1] = card_attr(0, 3);
    winning_hand_cards[WID_AME_SHIKOU][2] = card_attr(2, 3);
    winning_hand_cards[WID_AME_SHIKOU][3] = card_attr(7, 3);
    winning_hand_cards[WID_AME_SHIKOU][4] = card_attr(11, 3);
    winning_hand_must[WID_AME_SHIKOU][0] = ON;

    winning_hand_minimum[WID_SANKOU] = 3;
    winning_hand_cards[WID_SANKOU][0] = card_attr(0, 3);
    winning_hand_cards[WID_SANKOU][1] = card_attr(2, 3);
    winning_hand_cards[WID_SANKOU][2] = card_attr(7, 3);
    winning_hand_cards[WID_SANKOU][3] = card_attr(11, 3);

    winning_hand_minimum[WID_HANAMI] = 2;
    winning_hand_cards[WID_HANAMI][0] = card_attr(2, 3);
    winning_hand_cards[WID_HANAMI][1] = card_attr(8, 3);

    winning_hand_minimum[WID_TSUKIMI] = 2;
    winning_hand_cards[WID_TSUKIMI][0] = card_attr(7, 3);
    winning_hand_cards[WID_TSUKIMI][1] = card_attr(8, 3);

    winning_hand_minimum[WID_AKATAN] = 3;
    winning_hand_cards[WID_AKATAN][0] = card_attr(0, 2);
    winning_hand_cards[WID_AKATAN][1] = card_attr(1, 2);
    winning_hand_cards[WID_AKATAN][2] = card_attr(2, 2);

    winning_hand_minimum[WID_AOTAN] = 3;
    winning_hand_cards[WID_AOTAN][0] = card_attr(5, 2);
    winning_hand_cards[WID_AOTAN][1] = card_attr(8, 2);
    winning_hand_cards[WID_AOTAN][2] = card_attr(9, 2);

    winning_hand_minimum[WID_ISC] = 3;
    winning_hand_cards[WID_ISC][0] = card_attr(5, 3);
    winning_hand_cards[WID_ISC][1] = card_attr(6, 3);
    winning_hand_cards[WID_ISC][2] = card_attr(9, 3);

    winning_hand_minimum[WID_DBTAN] = 6;
    winning_hand_cards[WID_DBTAN][0] = card_attr(0, 2);
    winning_hand_cards[WID_DBTAN][1] = card_attr(1, 2);
    winning_hand_cards[WID_DBTAN][2] = card_attr(2, 2);
    winning_hand_cards[WID_DBTAN][3] = card_attr(5, 2);
    winning_hand_cards[WID_DBTAN][4] = card_attr(8, 2);
    winning_hand_cards[WID_DBTAN][5] = card_attr(9, 2);

    winning_hand_minimum[WID_TANE] = 5;
    winning_hand_cards[WID_TANE][0] = card_attr(1, 3);
    winning_hand_cards[WID_TANE][1] = card_attr(3, 3);
    winning_hand_cards[WID_TANE][2] = card_attr(4, 3);
    winning_hand_cards[WID_TANE][3] = card_attr(5, 3);
    winning_hand_cards[WID_TANE][4] = card_attr(6, 3);
    winning_hand_cards[WID_TANE][5] = card_attr(7, 2);
    winning_hand_cards[WID_TANE][6] = card_attr(8, 3);
    winning_hand_cards[WID_TANE][7] = card_attr(9, 3);
    winning_hand_cards[WID_TANE][8] = card_attr(10, 2);

    winning_hand_minimum[WID_TAN] = 5;
    winning_hand_cards[WID_TAN][0] = card_attr(0, 2);
    winning_hand_cards[WID_TAN][1] = card_attr(1, 2);
    winning_hand_cards[WID_TAN][2] = card_attr(2, 2);
    winning_hand_cards[WID_TAN][3] = card_attr(3, 2);
    winning_hand_cards[WID_TAN][4] = card_attr(4, 2);
    winning_hand_cards[WID_TAN][5] = card_attr(5, 2);
    winning_hand_cards[WID_TAN][6] = card_attr(6, 2);
    winning_hand_cards[WID_TAN][7] = card_attr(8, 2);
    winning_hand_cards[WID_TAN][8] = card_attr(9, 2);
    winning_hand_cards[WID_TAN][9] = card_attr(10, 1);

    winning_hand_minimum[WID_KASU] = 10;
    winning_hand_cards[WID_KASU][0] = card_attr(0, 0);
    winning_hand_cards[WID_KASU][1] = card_attr(0, 1);
    winning_hand_cards[WID_KASU][2] = card_attr(1, 0);
    winning_hand_cards[WID_KASU][3] = card_attr(1, 1);
    winning_hand_cards[WID_KASU][4] = card_attr(2, 0);
    winning_hand_cards[WID_KASU][5] = card_attr(2, 1);
    winning_hand_cards[WID_KASU][6] = card_attr(3, 0);
    winning_hand_cards[WID_KASU][7] = card_attr(3, 1);
    winning_hand_cards[WID_KASU][8] = card_attr(4, 0);
    winning_hand_cards[WID_KASU][9] = card_attr(4, 1);
    winning_hand_cards[WID_KASU][10] = card_attr(5, 0);
    winning_hand_cards[WID_KASU][11] = card_attr(5, 1);
    winning_hand_cards[WID_KASU][12] = card_attr(6, 0);
    winning_hand_cards[WID_KASU][13] = card_attr(6, 1);
    winning_hand_cards[WID_KASU][14] = card_attr(7, 0);
    winning_hand_cards[WID_KASU][15] = card_attr(7, 1);
    winning_hand_cards[WID_KASU][16] = card_attr(8, 0);
    winning_hand_cards[WID_KASU][17] = card_attr(8, 1);
    winning_hand_cards[WID_KASU][18] = card_attr(8, 3);
    winning_hand_cards[WID_KASU][19] = card_attr(9, 0);
    winning_hand_cards[WID_KASU][20] = card_attr(9, 1);
    winning_hand_cards[WID_KASU][21] = card_attr(10, 0);
    winning_hand_cards[WID_KASU][22] = card_attr(11, 0);
    winning_hand_cards[WID_KASU][23] = card_attr(11, 1);
    winning_hand_cards[WID_KASU][24] = card_attr(11, 2);
}

static uint32_t wh_card_attr(int wid, int index)
{
    if (index < winning_hand_minimum[wid]) {
        return winning_hand_cards[wid][index];
    }
    return 0;
}

static int wh_contains_attr(int wid, uint32_t attr)
{
    for (int i = 0; i < 25; i++) {
        if ((uint32_t)winning_hand_cards[wid][i] == attr) {
            return ON;
        }
    }
    return OFF;
}

static void append_attr(uint32_t* attrs, int* count, int limit, uint32_t attr)
{
    if (!attr || *count >= limit) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (attrs[i] == attr) {
            return;
        }
    }
    attrs[(*count)++] = attr;
}

static int invent_has_attr(uint32_t attr)
{
    for (int t = 0; t < 4; t++) {
        CardSet* set = &g.invent[0][t];
        for (int i = 0; i < set->num; i++) {
            Card* card = set->cards[i];
            if (card && (uint32_t)card_attr(card->month, card->index) == attr) {
                return ON;
            }
        }
    }
    return OFF;
}

static void draw_wh(int* sprite_index, int bx, int by, WinningHand* wh)
{
    const int limit = winning_hand_minimum[wh->id];
    if (!limit) {
        return;
    }

    uint32_t attrs[25];
    int attr_count = 0;

    for (int i = 0; i < 25 && attr_count < limit; i++) {
        if (!winning_hand_must[wh->id][i]) {
            continue;
        }
        append_attr(attrs, &attr_count, limit, winning_hand_cards[wh->id][i]);
    }

    if (attr_count < limit) {
        for (int t = 0; t < 4 && attr_count < limit; t++) {
            CardSet* set = &g.invent[0][t];
            for (int i = 0; i < set->num && attr_count < limit; i++) {
                Card* card = set->cards[i];
                if (!card) {
                    continue;
                }
                uint32_t attr = card_attr(card->month, card->index);
                if (!wh_contains_attr(wh->id, attr)) {
                    continue;
                }
                append_attr(attrs, &attr_count, limit, attr);
            }
        }
    }

    int index = 0;
    while (attr_count < limit) {
        uint32_t attr = wh_card_attr(wh->id, index++);
        if (!attr) {
            break;
        }
        append_attr(attrs, &attr_count, limit, attr);
    }

    for (int i = 0; i < attr_count; i++) {
        ObjectAttributeMemory* o = vgs_oam(SP_AGARI + (*sprite_index));
        o->visible = ON;
        o->x = bx;
        o->y = by;
        o->attr = attrs[i];
        if (wh->id == WID_KASU) {
            // カス札は所持 or 非所持に関係なく濃く表示
            o->alpha = 0xFFFFFF;
        } else {
            if (invent_has_attr(attrs[i])) {
                // 持っていないカードは濃く表示
                o->alpha = 0xFFFFFF;
                vgs_draw_box(2, bx + 6, by + 3, 28, 35, 0xFFFF00);
            } else {
                // 持っていないカードは少し薄く表示
                o->alpha = 0xC0C0C0;
            }
        }
        bx += wh->id == WID_KASU ? 14 : 30;
        (*sprite_index)++;
    }
}

static void clear_cards(void)
{
    for (int i = 0; i < AGARI_MAX; i++) {
        ObjectAttributeMemory* o = vgs_oam(SP_AGARI + i);
        o->scale = 80;
        o->slx = OFF;
        o->sly = OFF;
        o->rotate = 0;
        o->pri = ON;
        o->size = 4;
        o->visible = OFF;
    }
}

static void draw(int page)
{
    vgs_putlog("draw page: %d", page);
    vgs_cls_bg(2, 0);
    center_print(2, 8, "Winning Hands");

    char tbuf[32];
    vgs_strcpy(tbuf, "- Page ");
    vgs_d32str(&tbuf[vgs_strlen(tbuf)], page + 1);
    vgs_strcat(tbuf, "/3 -");
    center_print(2, 190, tbuf);

    clear_cards();

    int sprite_index = 0;
    for (int i = 0; pages[page][i]; i++) {
        vgs_pfont_print(2, 100 - vgs_pfont_strlen(pages[page][i]->name), 36 + i * 38, 0, PTN_FONT, pages[page][i]->name);
        draw_wh(&sprite_index, 100, 28 + i * 38, pages[page][i]);
    }
}

void show_winning_hands(void)
{
    vgs_putlog("Show winning hands");
    static int current_page = 0;
    init();
    for (int i = 0; i < 224; i += 16) {
        set_fade(i);
        if (128 == i) {
            draw(current_page);
            vgs_oam(SP_CURSOR)->visible = OFF;
        }
        vsync();
    }
    while (vgs_key_y()) {
        vsync();
    }
    int key = 0;
    int key_prev;
    while (ON) {
        vsync();
        key_prev = key;
        key = vgs_key_code();
        if (vgs_key_code_left(key) && !vgs_key_code_left(key_prev)) {
            current_page--;
            if (current_page < 0) {
                current_page = 2;
            }
            draw(current_page);
            vgs_sfx_play(SFX_CURSOR_MOVE);
        }

        if (vgs_key_code_right(key) && !vgs_key_code_right(key_prev)) {
            current_page++;
            if (2 < current_page) {
                current_page = 0;
            }
            draw(current_page);
            vgs_sfx_play(SFX_CURSOR_MOVE);
        }

        if (vgs_key_code_a(key) && !vgs_key_code_a(key_prev)) {
            break;
        }
        if (vgs_key_code_b(key) && !vgs_key_code_b(key_prev)) {
            break;
        }
        if (vgs_key_code_x(key) && !vgs_key_code_x(key_prev)) {
            break;
        }
        if (vgs_key_code_y(key) && !vgs_key_code_y(key_prev)) {
            break;
        }
    }
    vgs_sfx_play(SFX_CURSOR_LEAVE);

    while (vgs_key_a() || vgs_key_b() || vgs_key_x() || vgs_key_y()) {
        vsync();
    }
    vgs_oam(SP_CURSOR)->visible = ON;

    for (int i = 224; 0 <= i; i -= 16) {
        set_fade(i);
        if (80 == i) {
            vgs_cls_bg(2, 0);
            clear_cards();
            vgs_oam(SP_CURSOR)->visible = ON;
        }
        vsync();
    }
    set_fade(0);
}
