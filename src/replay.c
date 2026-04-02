#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "ai.h"

typedef struct {
    int first_actor;
    int floor_ids[48];
    int floor_count;
} ReplayRoundSpec;

typedef struct {
    int ai_model[2];
    int round_max;
    int no_sake;
    int seed;
    int has_seed;
    int native_ai_seed;
    int has_native_ai_seed;
    ReplayRoundSpec rounds[12];
} ReplaySpec;

static int parse_ai_model(const char* name)
{
    if (!name) {
        return AI_MODEL_HARD;
    }
    if (strcmp(name, "Normal") == 0) {
        return AI_MODEL_NORMAL;
    }
    if (strcmp(name, "Hard") == 0) {
        return AI_MODEL_HARD;
    }
    if (strcmp(name, "Stenv") == 0) {
        return AI_MODEL_STENV;
    }
    if (strcmp(name, "Mcenv") == 0) {
        return AI_MODEL_MCENV;
    }
    return AI_MODEL_HARD;
}

static const char* ai_name_local(int model)
{
    switch (model) {
        case AI_MODEL_NORMAL: return "Normal";
        case AI_MODEL_HARD: return "Hard";
        case AI_MODEL_STENV: return "Stenv";
        case AI_MODEL_MCENV: return "Mcenv";
        default: return "Unknown";
    }
}

static void configure_game(int ai0, int ai1, int no_sake)
{
    vgs_memset(&g, 0, sizeof(g));
    g.online_mode = OFF;
    g.auto_play = ON;
    g.no_sake = no_sake ? ON : OFF;
    g_no_sake_latched = g.no_sake;
    g.ai_model[0] = ai0;
    g.ai_model[1] = ai1;
    g.bg_color = 0;
}

static int parse_floor_ids(const char* line, int* out_ids, int cap)
{
    const char* p = line;
    int count = 0;

    if (!line || !out_ids || cap < 48) {
        return -1;
    }
    if (strncmp(line, "Floor: ", 7) != 0) {
        return -1;
    }
    p += 7;
    while (*p && count < cap) {
        char* end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) {
            break;
        }
        if (v < 0 || v >= 48) {
            return -1;
        }
        out_ids[count++] = (int)v;
        p = end;
        if (*p == ',') {
            p++;
        }
    }
    return count == 48 ? count : -1;
}

static int parse_watch_log(const char* path, ReplaySpec* out)
{
    FILE* fp;
    char line[2048];
    int current_round = -1;

    if (!path || !out) {
        return OFF;
    }

    fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return OFF;
    }

    vgs_memset(out, 0, sizeof(*out));
    for (int i = 0; i < 12; i++) {
        out->rounds[i].first_actor = -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        char p1_model[32];
        char cpu_model[32];
        int round_no = 0;

        int seed = 0;
        int no_sake = 0;

        if (sscanf(line, "Watch Start: P1=%31s CPU=%31s rounds=%d no_sake=%d", p1_model, cpu_model, &out->round_max, &no_sake) == 4) {
            out->ai_model[0] = parse_ai_model(p1_model);
            out->ai_model[1] = parse_ai_model(cpu_model);
            out->no_sake = no_sake ? ON : OFF;
            continue;
        }

        if (sscanf(line, "Watch Start: P1=%31s CPU=%31s rounds=%d seed=%d", p1_model, cpu_model, &out->round_max, &seed) == 4) {
            out->ai_model[0] = parse_ai_model(p1_model);
            out->ai_model[1] = parse_ai_model(cpu_model);
            out->seed = seed;
            out->has_seed = ON;
            continue;
        }

        if (sscanf(line, "Watch Start: P1=%31s CPU=%31s rounds=%d", p1_model, cpu_model, &out->round_max) == 3) {
            out->ai_model[0] = parse_ai_model(p1_model);
            out->ai_model[1] = parse_ai_model(cpu_model);
            continue;
        }

        if (sscanf(line, "Watch Seed: %d", &seed) == 1) {
            out->seed = seed;
            out->has_seed = ON;
            continue;
        }

        if (!out->has_native_ai_seed) {
            if (sscanf(line, "[NATIVE-AI-RNG] seed=%d", &seed) == 1) {
                out->native_ai_seed = seed;
                out->has_native_ai_seed = ON;
                continue;
            }
        }

        if (sscanf(line, "[Initial] Round %d", &round_no) == 1) {
            if (round_no < 1 || round_no > 12) {
                fclose(fp);
                return OFF;
            }
            current_round = round_no - 1;
            continue;
        }

        if (current_round < 0 || current_round >= 12) {
            continue;
        }

        if (out->rounds[current_round].floor_count == 0 && strncmp(line, "Floor: ", 7) == 0) {
            out->rounds[current_round].floor_count = parse_floor_ids(line, out->rounds[current_round].floor_ids, 48);
            continue;
        }

        if (out->rounds[current_round].first_actor < 0) {
            if (strncmp(line, "[P1]", 4) == 0) {
                out->rounds[current_round].first_actor = 0;
                continue;
            }
            if (strncmp(line, "[CPU]", 5) == 0) {
                out->rounds[current_round].first_actor = 1;
                continue;
            }
        }
    }

    fclose(fp);

    if (out->round_max <= 0 || out->round_max > 12) {
        return OFF;
    }
    for (int i = 0; i < out->round_max; i++) {
        if (out->rounds[i].floor_count != 48 || out->rounds[i].first_actor < 0) {
            return OFF;
        }
    }
    return ON;
}

static void init_card_table(void)
{
    for (int i = 0; i < 48; i++) {
        g.cards[i].type = card_types[i];
        g.cards[i].reversed = OFF;
        g.cards[i].month = i / 4;
        g.cards[i].index = i & 3;
        g.cards[i].id = i;
    }
}

static int hand_has_card_id(Card* const* cards, int count, int card_id)
{
    for (int i = 0; i < count; i++) {
        if (cards[i] && cards[i]->id == card_id) {
            return ON;
        }
    }
    return OFF;
}

static void log_card_ids_local(const char* label, Card* const* cards, int count)
{
    char buf[256];
    char numbuf[16];
    int has_card = OFF;

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
    vgs_putlog("%s", buf);
}

static void log_own_cards_local(const char* label, int player, Card* const* cards, int count)
{
    char buf[256];
    char numbuf[16];
    int has_card = OFF;
    int has_sake = hand_has_card_id(cards, count, 35);

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
    vgs_putlog("%s", buf);
}

static void log_initial_cards_local(void)
{
    vgs_putlog("[Initial] Round %d", g.round + 1);
    log_card_ids_local("Floor", g.floor.cards, 48);
    log_own_cards_local("P1 (DOWN-SIDE) own", 0, g.own[0].cards, g.own[0].num);
    log_own_cards_local("CPU (UP-SIDE) own", 1, g.own[1].cards, g.own[1].num);
}

static void setup_round_from_floor_ids(const int* floor_ids)
{
    vgs_memset(&g.cards, 0, sizeof(g.cards));
    vgs_memset(&g.deck, 0, sizeof(g.deck));
    vgs_memset(&g.floor, 0, sizeof(g.floor));
    vgs_memset(&g.invent, 0, sizeof(g.invent));
    vgs_memset(&g.moves, 0, sizeof(g.moves));
    vgs_memset(&g.own, 0, sizeof(g.own));
    vgs_memset(&g.stats, 0, sizeof(g.stats));
    vgs_memset(&g.zanzo, 0, sizeof(g.zanzo));
    vgs_memset(&g.player_gekiatsu, 0, sizeof(g.player_gekiatsu));
    g.move_count = 0;
    g.frame = 0;
    g.cursor = 0;
    g.koikoi[0] = 0;
    g.koikoi[1] = 0;
    g.koikoi_score[0] = 0;
    g.koikoi_score[1] = 0;

    init_card_table();

    for (int i = 0; i < 48; i++) {
        g.floor.cards[i] = &g.cards[floor_ids[i]];
    }
    g.floor.num = 24;
    g.floor.start = 24;
    g.own[0].num = 8;
    g.own[1].num = 8;
    for (int i = 0; i < 8; i++) {
        g.own[0].cards[i] = &g.cards[floor_ids[i]];
        g.own[1].cards[i] = &g.cards[floor_ids[8 + i]];
        g.deck.cards[i] = &g.cards[floor_ids[16 + i]];
    }
    g.deck.num = 8;
}

static void start_preset_round(const ReplayRoundSpec* round)
{
    setup_round_from_floor_ids(round->floor_ids);
    g.oya = round->first_actor;
    g.current_player = 1 - g.oya;
    vgs_putlog("[Round Start]");
    log_initial_cards_local();
    analyze_invent(0);
    analyze_invent(1);
    ai_debug_reset_round_state();
}

static void run_round_loop(void)
{
    do {
        log_card_ids_local("Deck", g.deck.cards, 48);
        while (0 < g.own[0].num || 0 < g.own[1].num) {
            g.current_player = 1 - g.current_player;
            if (g.current_player) {
                enemy_turn();
                if (analyze_invent(g.current_player)) {
                    break;
                }
                ai_watch_note_after_opp_none(g.current_player);
            } else {
                player_turn();
                if (analyze_invent(g.current_player)) {
                    break;
                }
                ai_watch_note_after_opp_none(g.current_player);
                vgs_memset(&g.player_gekiatsu, 0, sizeof(CardSet));
            }
        }
    } while (show_result(g.current_player));
}

static int replay_game(const ReplaySpec* spec, int force_preset_rounds)
{
    if (!spec) {
        return 1;
    }

    if (spec->has_seed && !force_preset_rounds) {
        configure_game(spec->ai_model[0], spec->ai_model[1], spec->no_sake);
        ai_debug_set_run_context(spec->seed, 0);
        vgs_srand((uint32_t)spec->seed);
        game(spec->round_max);
        return 0;
    }

    configure_game(spec->ai_model[0], spec->ai_model[1], spec->no_sake);
    g.round_max = spec->round_max;
    g.round = 0;
    g.total_score[0] = 0;
    g.total_score[1] = 0;

    if (spec->has_native_ai_seed) {
        ai_debug_set_run_context(spec->native_ai_seed, 0);
        vgs_srand((uint32_t)spec->native_ai_seed);
    }

    ai_watch_log_begin();
    vgs_putlog("[Replay] rounds=%d models=P1:%s CPU:%s source=watch.log",
               spec->round_max, ai_name_local(g.ai_model[0]), ai_name_local(g.ai_model[1]));

    for (int round_index = 0; round_index < spec->round_max; round_index++) {
        g.round = round_index;
        start_preset_round(&spec->rounds[round_index]);
        run_round_loop();
    }

    ai_watch_log_end();
    return 0;
}

static void print_usage(const char* argv0)
{
    fprintf(stderr, "usage: %s [--force_sake] <watch.log>\n", argv0 ? argv0 : "replay");
}

int main(int argc, char** argv)
{
    ReplaySpec spec;
    const char* log_path = NULL;
    int force_sake = OFF;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--force_sake") == 0) {
            force_sake = ON;
            continue;
        }
        if (!log_path) {
            log_path = argv[i];
            continue;
        }
        print_usage(argv[0]);
        return 1;
    }

    if (!log_path) {
        print_usage(argv[0]);
        return 1;
    }

    if (!parse_watch_log(log_path, &spec)) {
        fprintf(stderr, "failed to parse replay inputs from %s\n", log_path);
        return 1;
    }

    if (force_sake) {
        spec.no_sake = OFF;
    }

    return replay_game(&spec, force_sake);
}
