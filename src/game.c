#include "common.h"
#include "cards.h"
#include "ai.h"

static void log_card_ids(const char* label, Card* const* cards, int count)
{
    char buf[256];
    char numbuf[16];
    vgs_strcpy(buf, label);
    vgs_strcat(buf, ": ");
    int has_card = OFF;
    for (int i = 0; i < count; i++) {
        Card* card = cards[i];
        if (!card) {
            continue;
        }
        if (has_card) {
            vgs_strcat(buf, ",");
        }
        vgs_d32str(numbuf, card->id);
        vgs_strcat(buf, numbuf);
        has_card = ON;
    }
    if (!has_card) {
        vgs_strcat(buf, "n/a");
    }
    ai_putlog("%s", buf);
}

static int has_card_id(Card* const* cards, int count, int card_id)
{
    for (int i = 0; i < count; i++) {
        Card* card = cards[i];
        if (card && card->id == card_id) {
            return ON;
        }
    }
    return OFF;
}

static void log_own_card_ids(const char* label, int player, Card* const* cards, int count)
{
    char buf[256];
    char numbuf[16];
    int has_card = OFF;
    int has_sake = has_card_id(cards, count, 35);

    ai_debug_set_initial_sake(player, has_sake);

    vgs_strcpy(buf, label);
    vgs_strcat(buf, ": ");
    for (int i = 0; i < count; i++) {
        Card* card = cards[i];
        if (!card) {
            continue;
        }
        if (has_card) {
            vgs_strcat(buf, ",");
        }
        vgs_d32str(numbuf, card->id);
        vgs_strcat(buf, numbuf);
        has_card = ON;
    }
    if (!has_card) {
        vgs_strcat(buf, "n/a");
    }
    if (has_sake) {
        vgs_strcat(buf, " (Sake)");
    }
    ai_putlog("%s", buf);
}

static void log_card_ids_env(const char* label, Card* const* cards, int count)
{
    char buf[256];
    char numbuf[16];
    vgs_strcpy(buf, label);
    vgs_strcat(buf, ": ");
    int has_card = OFF;
    for (int i = 0; i < count; i++) {
        Card* card = cards[i];
        if (!card) {
            continue;
        }
        if (has_card) {
            vgs_strcat(buf, ",");
        }
        vgs_d32str(numbuf, card->id);
        vgs_strcat(buf, numbuf);
        has_card = ON;
    }
    if (!has_card) {
        vgs_strcat(buf, "n/a");
    }
    ai_putlog_env("%s", buf);
}

static void log_initial_cards(void)
{
    if (g.auto_play == OFF) {
        return;
    }
    ai_putlog("[Initial] Round %d", g.round + 1);
    log_card_ids("Floor", g.floor.cards, 48);
    log_own_card_ids("P1 (DOWN-SIDE) own", 0, g.own[0].cards, 8);
    log_own_card_ids("CPU (UP-SIDE) own", 1, g.own[1].cards, 8);
}

static inline void unlock_win_trophies(void)
{
    if (g.total_score[0] <= g.total_score[1]) {
        return;
    }

    if (g.online_mode) {
        trophy_unlock(TROPHY_WIN_ONLINE);
    } else {
        static const int tids[] = {
            // Normal / Standard
            TROPHY_WIN_VERYSHORT,
            TROPHY_WIN_SHORT,
            TROPHY_WIN_HALF,
            TROPHY_WIN_FULL,
            // Hard / Standard
            TROPHY_WIN2_VERYSHORT,
            TROPHY_WIN2_SHORT,
            TROPHY_WIN2_HALF,
            TROPHY_WIN2_FULL,
            // Normal / No-Sake
            TROPHY_WIN3_VERYSHORT,
            TROPHY_WIN3_SHORT,
            TROPHY_WIN3_HALF,
            TROPHY_WIN3_FULL,
            // Hard / No-Sake
            TROPHY_WIN4_VERYSHORT,
            TROPHY_WIN4_SHORT,
            TROPHY_WIN4_HALF,
            TROPHY_WIN4_FULL,
        };
        int tid = 0;
        switch (g.round_max) {
            case 2: tid = 0; break;
            case 4: tid = 1; break;
            case 6: tid = 2; break;
            case 12: tid = 3; break;
            default: tid = -1;
        }
        if (0 <= tid) {
            tid <<= g.no_sake ? 8 : 0;
            tid <<= g.ai_model[1] == AI_MODEL_HARD ? 4 : 0;
            trophy_unlock(tids[tid]);
        }
    }

    if (50 <= g.total_score[0]) {
        trophy_unlock(TROPHY_WIN_S50);
    }
    if (80 <= g.total_score[0]) {
        trophy_unlock(TROPHY_WIN_S80);
    }
    if (100 <= g.total_score[0]) {
        trophy_unlock(TROPHY_WIN_S100);
    }
    if (150 <= g.total_score[0]) {
        trophy_unlock(TROPHY_WIN_S150);
    }
}

void game(int round_max)
{
    g_no_sake_latched = g.no_sake ? ON : OFF;
#ifdef LOGGING
    ai_putlog("=================");
    ai_putlog("=  Round Start  =");
    ai_putlog("=================");
#endif
    g.round_max = round_max;
    g.total_score[0] = 0;
    g.total_score[1] = 0;
    g.round = 0;
    vgs_memset(g.round_result, 0, sizeof(g.round_result));
    ai_watch_log_begin();

    if (g.online_mode) {
        g.oya = online_matching_result() == 2 ? 0 : 1;
        uint8_t* id = online_opponent_steam_id();
        const char* name = online_opponent_steam_name();
        if (id && name) {
            savedata_online_set(id, name);
            g.online_log_need_commit = ON;
        } else {
            vgs_putlog("Cannot get steamId or persona name");
            g.online_log_need_commit = OFF;
        }
    } else {
        // かならず月見+花見であがれる乱数（ただしSteamだとうまくいかない）
        // vgs_srand(53495);
        g.oya = vgs_rand() & 1;
        g.online_log_need_commit = OFF;
    }

    do {
        // グラフィックス初期化
        reset_all_sprites();
        vgs_draw_mode(0, OFF);
        vgs_draw_mode(1, ON);
        vgs_draw_mode(2, ON);
        vgs_draw_mode(3, OFF);
        vgs_cls_bg_all(0);
        draw_fade2(15);
        vgs_sprite_hide_all();
        vgs_sprite_priority(0);
        vgs_sprite(SP_FF, OFF, 0, -2, 1, 0, PTN_FF);
        vgs_oam(SP_FF)->pri = ON;
        vgs_pal_set(0, 0, g.bg_color);

        // BG に 親 or 子 を描画
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                vgs_put_bg(0, x, 6 + y, PTN_OYA + (y * 4 + x) + (g.oya ? 0 : 16));
                vgs_put_bg(0, x, 15 + y, PTN_OYA + (y * 4 + x) + (g.oya ? 16 : 0));
            }
        }

        // BG にラウンドを描画
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 5; x++) {
                vgs_put_bg(0, x + 35, y + 11, PTN_ROUND + y * 5 + x);
            }
        }
        vgs_put_bg(0, 36, 12, PTN_ROUND + 15 + g.round);
        vgs_put_bg(0, 38, 12, PTN_ROUND + 14 + g.round_max);

        // スコア表示
        vgs_put_bg(0, 38, 9, PTN_ROUND + 27);
        vgs_put_bg(0, 39, 9, PTN_ROUND + 28);
        char sc[8];
        vgs_u32str(sc, g.total_score[1]);
        vgs_print_bg(0, 38 - vgs_strlen(sc), 9, 0, sc);

        vgs_put_bg(0, 38, 15, PTN_ROUND + 27);
        vgs_put_bg(0, 39, 15, PTN_ROUND + 28);
        vgs_u32str(sc, g.total_score[0]);
        vgs_print_bg(0, 38 - vgs_strlen(sc), 15, 0, sc);

        // 禁酒マーク
        if (g.no_sake) {
            for (int i = 0; i < 4; i++) {
                vgs_put_bg(0, i, 24, 0x10 + i);
            }
        }

        vgs_bgm_play(BGM_MAIN_THEME);

        // ラウンド開始のための初期設定
        g.avatar[0] = OFF;            // アバター再表示リクエスト (1P)
        g.avatar[1] = OFF;            // アバター再表示リクエスト (COM)
        g.no_sake = g_no_sake_latched;
        reset_cards();                // シャッフルして配る
        if (g.online_error) break;    // オンラインエラーによる中断
        ai_putlog("[Round Start]");   // ラウンド開始
        log_initial_cards();          // ログ出力
        analyze_invent(0);            // プレイヤーの欲しいカードを分析
        analyze_invent(1);            // CPUの欲しいカードを分析
        ai_debug_reset_round_state(); // ラウンド内の最終戦略モード記録を初期化
        g.koikoi[0] = 0;              // こいこい状態をクリア
        g.koikoi[1] = 0;              // こいこい状態をクリア
        g.koikoi_score[0] = 0;        // こいこい状態をクリア
        g.koikoi_score[1] = 0;        // こいこい状態をクリア
        g.cursor = 0;                 // カーソルを初期位置に戻す
        g.current_player = 1 - g.oya; // カレントプレイヤを設定

#if 0
        // カス テスト
        for (int i = 28; i < 48; i++) {
            if (g.floor.cards[i]->type == CARD_TYPE_KASU) {
                g.invent[0][CARD_TYPE_KASU].cards[g.invent[0][CARD_TYPE_KASU].num] = g.floor.cards[i];
                g.invent[0][CARD_TYPE_KASU].num++;
            }
        }
#endif

#if 0
        // 青短 テスト
        g.invent[0][CARD_TYPE_KASU].cards[g.invent[0][CARD_TYPE_KASU].num++] = &g.cards[22];
        g.invent[0][CARD_TYPE_KASU].cards[g.invent[0][CARD_TYPE_KASU].num++] = &g.cards[34];
        g.invent[0][CARD_TYPE_KASU].cards[g.invent[0][CARD_TYPE_KASU].num++] = &g.cards[38];
#endif

        // ゲームループ (設定されたラウンド数までラウンドを繰り返す)
        vgs_memset(&g.player_gekiatsu, 0, sizeof(CardSet));
        do {
            // watch.log にターン開始時点の場札を出す
            log_card_ids_env("Deck", g.deck.cards, 48);
            // Player or CPU のいずれかの役ができるか両方の持ち札が無くなるまでターンを回す
            while (0 < g.own[0].num || 0 < g.own[1].num) {
                g.current_player = 1 - g.current_player; // toggle
                if (g.current_player) {
                    // CPU's turn
                    enemy_turn();
                    if (analyze_invent(g.current_player)) {
                        break;
                    } else if (g.online_mode) {
                        online_sync_turnend(ON, OFF, OFF);
                    }
                    ai_watch_note_after_opp_none(g.current_player);
                } else {
                    // Player's turn
                    player_turn();
                    if (analyze_invent(g.current_player)) {
                        break;
                    } else if (g.online_mode) {
                        online_turnend_send(0);
                    }
                    ai_watch_note_after_opp_none(g.current_player);
                    vgs_memset(&g.player_gekiatsu, 0, sizeof(CardSet));
                }
            }
        } while (show_result(g.current_player));

        // 次ラウンドへ進む
        const int finished_round = g.round;
        g.round++;

        // 親交代
        const uint32_t p0 = g.round_score[0][finished_round];
        const uint32_t p1 = g.round_score[1][finished_round];
        if ((p0 | p1) == 0) {
            g.oya ^= 1; // 交代 (no game)
        } else {
            g.oya = p1 > p0 ? 1 : 0; // 勝者を親
        }
    } while (g.round < g.round_max);

    ai_watch_log_end();
    unlock_win_trophies();

    // オンライン切断
    if (g.online_mode) {
        vgs_putlog("Sync FIN message...");
        // 相互に FIN 送信
        online_fin_send();

        // 相互に FIN 同期 (この時点以降の切断検出は正常とみなす)
        g.ignore_online_error = ON;
        g.com_wait_frames = 0;
        while (0 == online_fin_recv()) {
            g.com_wait_frames++;
            vsync();
        }

        // 切断
        if (g.online_mode) {
            g.online_mode = OFF;
            online_disconnect();
        }

        // waiting for oppponent が残る場合があるので消しておく
        vgs_cls_bg(3, 0);
        vgs_draw_mode(3, OFF);
    }

    show_game_result();
    g.ignore_online_error = OFF;
}
