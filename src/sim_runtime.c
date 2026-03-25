#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "common.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static ObjectAttributeMemory g_oam[SP_SIZE + 64];
static uint32_t g_rng_state = 2463534242u;
static int g_log_enabled = ON;
static FILE* g_watch_log_fp = NULL;
static int g_watch_log_enabled = OFF;
static char g_watch_log_base_path[1024];
static char g_watch_log_active_path[1152];

static void watch_log_close_active(void)
{
    if (!g_watch_log_fp) {
        return;
    }
    fflush(g_watch_log_fp);
    fclose(g_watch_log_fp);
    g_watch_log_fp = NULL;
}

static void split_watch_log_path(const char* path, char* dir, size_t dir_size, char* stem, size_t stem_size, char* ext, size_t ext_size)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t dir_len;
    size_t stem_len;

    if (!path || !*path) {
        if (dir_size) {
            snprintf(dir, dir_size, ".");
        }
        if (stem_size) {
            snprintf(stem, stem_size, "watch");
        }
        if (ext_size) {
            ext[0] = '\0';
        }
        return;
    }

    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dir_len = slash ? (size_t)(slash - path) : 0;
    if (dir_size) {
        if (dir_len == 0) {
            snprintf(dir, dir_size, ".");
        } else {
            if (dir_len >= dir_size) {
                dir_len = dir_size - 1;
            }
            memcpy(dir, path, dir_len);
            dir[dir_len] = '\0';
        }
    }

    dot = strrchr(base, '.');
    if (!dot || dot == base) {
        dot = NULL;
    }
    stem_len = dot ? (size_t)(dot - base) : strlen(base);
    if (stem_size) {
        if (stem_len == 0) {
            snprintf(stem, stem_size, "watch");
        } else {
            if (stem_len >= stem_size) {
                stem_len = stem_size - 1;
            }
            memcpy(stem, base, stem_len);
            stem[stem_len] = '\0';
        }
    }
    if (ext_size) {
        snprintf(ext, ext_size, "%s", dot ? dot : "");
    }
}

static void watch_log_write_line(const char* line)
{
    if (!g_watch_log_fp || !line) {
        return;
    }
    fputs(line, g_watch_log_fp);
    fputc('\n', g_watch_log_fp);
}

void* vgs_memset(void* s, int c, size_t n) { return memset(s, c, n); }
void* vgs_memcpy(void* d, const void* s, size_t n) { return memcpy(d, s, n); }
int vgs_strlen(const char* s) { return (int)strlen(s ? s : ""); }
char* vgs_strcpy(char* d, const char* s) { return strcpy(d, s ? s : ""); }
char* vgs_strcat(char* d, const char* s) { return strcat(d, s ? s : ""); }

void vgs_u32str(char* out, uint32_t v) { sprintf(out, "%u", v); }
void vgs_d32str(char* out, int32_t v) { sprintf(out, "%d", v); }

void vgs_srand(uint32_t seed)
{
    g_rng_state = seed ? seed : 1u;
    srand(seed);
}

uint32_t vgs_rand(void)
{
    g_rng_state ^= g_rng_state << 13;
    g_rng_state ^= g_rng_state >> 17;
    g_rng_state ^= g_rng_state << 5;
    return g_rng_state;
}

int vgs_cos(int deg)
{
    double r = (double)deg * M_PI / 180.0;
    return (int)lrint(cos(r) * 256.0);
}

int vgs_sin(int deg)
{
    double r = (double)deg * M_PI / 180.0;
    return (int)lrint(sin(r) * 256.0);
}

int vgs_degree(int x1, int y1, int x2, int y2)
{
    double a = atan2((double)(y2 - y1), (double)(x2 - x1));
    int d = (int)lrint(a * 180.0 / M_PI);
    if (d < 0) {
        d += 360;
    }
    return d;
}

void vgs_putlog(const char* fmt, ...)
{
    char buf[4096];
    if (!g_log_enabled) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        watch_log_write_line(buf);
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    puts(buf);
    watch_log_write_line(buf);
}

void vgs_set_log_enabled(int enabled)
{
    g_log_enabled = enabled ? ON : OFF;
}

int vgs_watch_log_enabled(void)
{
    return g_watch_log_enabled;
}

int vgs_watch_log_set_base_path(const char* path)
{
    watch_log_close_active();
    g_watch_log_enabled = (path && *path) ? ON : OFF;
    if (!g_watch_log_enabled) {
        g_watch_log_base_path[0] = '\0';
        g_watch_log_active_path[0] = '\0';
        return ON;
    }
    if (snprintf(g_watch_log_base_path, sizeof(g_watch_log_base_path), "%s", path) >= (int)sizeof(g_watch_log_base_path)) {
        fprintf(stderr, "watch log path too long: %s\n", path);
        g_watch_log_enabled = OFF;
        g_watch_log_base_path[0] = '\0';
        return OFF;
    }
    return ON;
}

int vgs_watch_log_begin_game(void)
{
    if (!g_watch_log_enabled) {
        return ON;
    }
    watch_log_close_active();
    if (snprintf(g_watch_log_active_path, sizeof(g_watch_log_active_path), "%s.tmp", g_watch_log_base_path) >= (int)sizeof(g_watch_log_active_path)) {
        fprintf(stderr, "watch log temp path too long: %s\n", g_watch_log_base_path);
        return OFF;
    }
    g_watch_log_fp = fopen(g_watch_log_active_path, "wb");
    if (!g_watch_log_fp) {
        fprintf(stderr, "failed to open watch log temp file %s: %s\n", g_watch_log_active_path, strerror(errno));
        g_watch_log_active_path[0] = '\0';
        return OFF;
    }
    return ON;
}

int vgs_watch_log_finalize_game(int game_index, int score0, int score1)
{
    char dir[1024];
    char stem[256];
    char ext[128];
    char final_path[1408];
    const char* result = "draw";

    if (!g_watch_log_enabled) {
        return ON;
    }
    watch_log_close_active();
    if (!g_watch_log_active_path[0]) {
        return OFF;
    }
    if (score0 > score1) {
        result = "win0";
    } else if (score1 > score0) {
        result = "win1";
    }
    split_watch_log_path(g_watch_log_base_path, dir, sizeof(dir), stem, sizeof(stem), ext, sizeof(ext));
    if (snprintf(final_path, sizeof(final_path), "%s/%s_%d_%s_%d-%d%s", dir, stem, game_index + 1, result, score0, score1, ext) >= (int)sizeof(final_path)) {
        fprintf(stderr, "watch log output path too long for game %d\n", game_index + 1);
        remove(g_watch_log_active_path);
        g_watch_log_active_path[0] = '\0';
        return OFF;
    }
    remove(final_path);
    if (rename(g_watch_log_active_path, final_path) != 0) {
        fprintf(stderr, "failed to rename watch log %s -> %s: %s\n", g_watch_log_active_path, final_path, strerror(errno));
        remove(g_watch_log_active_path);
        g_watch_log_active_path[0] = '\0';
        return OFF;
    }
    g_watch_log_active_path[0] = '\0';
    return ON;
}

void vgs_watch_log_shutdown(void)
{
    watch_log_close_active();
    g_watch_log_active_path[0] = '\0';
}

void vgs_draw_mode(int n, int on) { (void)n; (void)on; }
void vgs_cls_bg_all(int pal) { (void)pal; }
void vgs_cls_bg(int n, int pal) { (void)n; (void)pal; }
void vgs_draw_clear(int n, int x, int y, int w, int h) { (void)n; (void)x; (void)y; (void)w; (void)h; }
void vgs_draw_boxf(int n, int x, int y, int w, int h, int32_t pal) { (void)n; (void)x; (void)y; (void)w; (void)h; (void)pal; }
void vgs_draw_box(int n, int x, int y, int w, int h, int32_t pal) { (void)n; (void)x; (void)y; (void)w; (void)h; (void)pal; }
void vgs_draw_lineH(int n, int x, int y, int w, uint32_t pal) { (void)n; (void)x; (void)y; (void)w; (void)pal; }
void vgs_draw_lineV(int n, int x, int y, int h, uint32_t pal) { (void)n; (void)x; (void)y; (void)h; (void)pal; }
void vgs_draw_character(int n, int x, int y, int flip, int pal, uint32_t ptn) { (void)n; (void)x; (void)y; (void)flip; (void)pal; (void)ptn; }
void vgs_put_bg(int n, int x, int y, uint32_t attr) { (void)n; (void)x; (void)y; (void)attr; }
void vgs_print_bg(int n, int x, int y, int flip, const char* s) { (void)n; (void)x; (void)y; (void)flip; (void)s; }
void vgs_skip_bg(int n, int on) { (void)n; (void)on; }

void vgs_sprite(int id, int visible, int x, int y, int size, int pri, int attr)
{
    if (id < 0 || id >= (int)(sizeof(g_oam) / sizeof(g_oam[0]))) {
        return;
    }
    g_oam[id].visible = visible;
    g_oam[id].x = x;
    g_oam[id].y = y;
    g_oam[id].size = size;
    g_oam[id].pri = pri;
    g_oam[id].attr = attr;
    if (!g_oam[id].scale) {
        g_oam[id].scale = 100;
    }
}

void vgs_sprite_hide_all(void)
{
    size_t n = sizeof(g_oam) / sizeof(g_oam[0]);
    for (size_t i = 0; i < n; i++) {
        g_oam[i].visible = OFF;
    }
}

void vgs_sprite_priority(int pri) { (void)pri; }

ObjectAttributeMemory* vgs_oam(int index)
{
    static ObjectAttributeMemory dummy;
    if (index < 0 || index >= (int)(sizeof(g_oam) / sizeof(g_oam[0]))) {
        return &dummy;
    }
    return &g_oam[index];
}

void vgs_sprite_alpha8(ObjectAttributeMemory* o, uint8_t a)
{
    if (!o) {
        return;
    }
    uint32_t v = a;
    o->alpha = (v << 24) | (v << 16) | (v << 8) | v;
}

void vgs_pal_set(int pal, int idx, uint32_t color) { (void)pal; (void)idx; (void)color; }
uint32_t vgs_pal_get(int pal, int idx) { (void)pal; (void)idx; return 0; }

void vgs_bgm_play(int id) { (void)id; }
void vgs_bgm_master_volume(int vol) { (void)vol; }
int vgs_bgm_master_volume_get(void) { return 0; }
void vgs_sfx_play(int id) { (void)id; }
void vgs_sfx_stop(int id) { (void)id; }
void vgs_sfx_master_volume(int vol) { (void)vol; }
int vgs_sfx_master_volume_get(void) { return 0; }

void vgs_pfont_init(int ptn) { (void)ptn; }
void vgs_pfont_print(int n, int x, int y, int flip, int ptn, const char* s) { (void)n; (void)x; (void)y; (void)flip; (void)ptn; (void)s; }
int vgs_pfont_strlen(const char* s) { return vgs_strlen(s); }

void vgs_ptn_transfer(int ptn, const uint8_t* src, size_t len) { (void)ptn; (void)src; (void)len; }

int vgs_key_code(void) { return 0; }
int vgs_key_code_a(int key) { (void)key; return 0; }
int vgs_key_code_b(int key) { (void)key; return 0; }
int vgs_key_code_x(int key) { (void)key; return 0; }
int vgs_key_code_y(int key) { (void)key; return 0; }
int vgs_key_code_up(int key) { (void)key; return 0; }
int vgs_key_code_down(int key) { (void)key; return 0; }
int vgs_key_code_left(int key) { (void)key; return 0; }
int vgs_key_code_right(int key) { (void)key; return 0; }

int vgs_key_a(void) { return 0; }
int vgs_key_b(void) { return 0; }
int vgs_key_x(void) { return 0; }
int vgs_key_y(void) { return 0; }

int vgs_mouse_enabled(void) { return OFF; }
void vgs_mouse_setup(uint16_t ptn, uint8_t pal) { (void)ptn; (void)pal; }
void vgs_mouse_hidden(int on) { (void)on; }
int vgs_mouse_moving(void) { return OFF; }
int vgs_mouse_x(void) { return 0; }
int vgs_mouse_y(void) { return 0; }
int vgs_mouse_left(void) { return OFF; }
int vgs_mouse_right(void) { return OFF; }
int vgs_mouse_left_clicked(int* x, int* y)
{
    (void)x;
    (void)y;
    return OFF;
}
int vgs_mouse_right_clicked(int* x, int* y)
{
    (void)x;
    (void)y;
    return OFF;
}
int vgs_mouse_scroll_vertical(void) { return 0; }
int vgs_mouse_scroll_horizontal(void) { return 0; }

int vgs_button_id_a(void) { return 'A'; }
int vgs_button_id_b(void) { return 'B'; }
int vgs_button_id_x(void) { return 'X'; }
int vgs_button_id_y(void) { return 'Y'; }

int position_in_rect(int pos_x, int pos_y, int x, int y, int width, int height)
{
    return x <= pos_x && y <= pos_y && pos_x < x + width && pos_y < y + height;
}

int mouse_shown(void) { return OFF; }
int mouse_in_rect(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return OFF;
}

int push_any(void) { return OFF; }
int push_a(void) { return OFF; }
int push_b(void) { return OFF; }
int push_x(void) { return OFF; }
int push_y(void) { return OFF; }
int push_up(void) { return OFF; }
int push_down(void) { return OFF; }
int push_left(void) { return OFF; }
int push_right(void) { return OFF; }
int push_start(void) { return OFF; }
int pushing_ff(void) { return savedata_get_anime_speed(); }

static void now_local(struct tm* out)
{
    time_t t = time(NULL);
    struct tm* p = localtime(&t);
    if (p) {
        *out = *p;
    } else {
        memset(out, 0, sizeof(*out));
    }
}

uint32_t vgs_utc_year(void) { struct tm tmv; now_local(&tmv); return (uint32_t)(tmv.tm_year + 1900); }
uint32_t vgs_utc_month(void) { struct tm tmv; now_local(&tmv); return (uint32_t)(tmv.tm_mon + 1); }
uint32_t vgs_utc_mday(void) { struct tm tmv; now_local(&tmv); return (uint32_t)tmv.tm_mday; }
uint32_t vgs_utc_hour(void) { struct tm tmv; now_local(&tmv); return (uint32_t)tmv.tm_hour; }
uint32_t vgs_utc_minute(void) { struct tm tmv; now_local(&tmv); return (uint32_t)tmv.tm_min; }
uint32_t vgs_utc_second(void) { struct tm tmv; now_local(&tmv); return (uint32_t)tmv.tm_sec; }

uint32_t vgs_local_year(void) { return vgs_utc_year(); }
uint32_t vgs_local_month(void) { return vgs_utc_month(); }
uint32_t vgs_local_mday(void) { return vgs_utc_mday(); }
uint32_t vgs_local_hour(void) { return vgs_utc_hour(); }
uint32_t vgs_local_minute(void) { return vgs_utc_minute(); }
uint32_t vgs_local_second(void) { return vgs_utc_second(); }

uint32_t vgs_user_in(uint32_t addr) { (void)addr; return 0; }
void vgs_exit(int code) { exit(code); }
void vgs_vsync(void) {}
void system_vsync(void) { vgs_vsync(); }
void vsync(void)
{
    // Real implementation does not advance g.frame here.
}

void reset_all_sprites(void)
{
    vgs_sprite_hide_all();
}

void set_fade(uint8_t progress) { (void)progress; }
void set_flash(uint8_t progress) { (void)progress; }
int draw_fade2(int progress) { (void)progress; return 0; }

void dot_add(int x, int y, int color) { (void)x; (void)y; (void)color; }
void dot_add_range(int x, int y, int color, int w, int h, int n) { (void)x; (void)y; (void)color; (void)w; (void)h; (void)n; }
void dot_draw(void) {}
void season_add(int month, int x, int y) { (void)month; (void)x; (void)y; }
void season_draw(void) {}

void hibana_clear(int sprite_base) { (void)sprite_base; }
void hibana_rect_clear(void) {}
void hibana_rect_add(int x, int y, int width, int height) { (void)x; (void)y; (void)width; (void)height; }
void hibana_frame(int interval, int limit) { (void)interval; (void)limit; }

void timer_start(uint32_t time_left, int right, int bottom) { (void)time_left; (void)right; (void)bottom; }
void timer_stop(void) {}
void timer_tick(void) {}
int timer_is_over(void) { return 0; }

int center_print(int bg, int y, const char* text) { (void)bg; (void)y; (void)text; return 0; }
int center_print_bx(int bg, int y, const char* text, int bx) { (void)bg; (void)y; (void)text; (void)bx; return 0; }
int center_print_y(int bg, int y, const char* text) { (void)bg; (void)y; (void)text; return 0; }
int center_print_y_bx(int bg, int y, const char* text, int bx) { (void)bg; (void)y; (void)text; (void)bx; return 0; }
const char* key_name_lower(int id) { (void)id; return "a"; }

void trophy_unlock(uint32_t id) { (void)id; }

void savedata_load(void) {}
ResultRecord* savedata_add_round_result(int player, int round_score) { (void)player; (void)round_score; return NULL; }
void savedata_game_result(int result) { (void)result; }
int savedata_get_window_mode(void) { return 0; }
int savedata_get_bgm_volume(void) { return 0; }
int savedata_get_sfx_volume(void) { return 0; }
int savedata_get_anime_speed(void) { return 1; }
void savedata_set_config(int window_mode, int bgm_volume, int sfx_volume, int anime_speed)
{
    (void)window_mode;
    (void)bgm_volume;
    (void)sfx_volume;
    (void)anime_speed;
}
void savedata_set_cleared(void) {}
int savedata_get_cleared(int game_mode) { (void)game_mode; return 0; }
void savedata_online_set(uint8_t* id, const char* name) { (void)id; (void)name; }
void savedata_online_commit(uint8_t result) { (void)result; }
OnlineRecord* savedata_online_get(int index) { (void)index; return NULL; }
const char* savedata_room_id_create_get(void) { return ""; }
void savedata_room_id_create_set(const char* roomId) { (void)roomId; }
int savedata_online_understand_get(void) { return 1; }
void savedata_online_understand(int on) { (void)on; }

uint32_t online_matching_result(void) { return 2; }
uint32_t online_game_mode(void) { return 0; }
void online_deck_send(CardSet* deck) { (void)deck; }
int online_deck_recv(CardSet* deck) { (void)deck; return -1; }
int online_check(void) { return 0; }
void online_disconnect(void) {}
void online_card_send(Card* own, Card* deck) { (void)own; (void)deck; }
void online_card_send2(Card* deck) { (void)deck; }
Card* online_card_recv(int* error)
{
    if (error) {
        *error = 1;
    }
    return NULL;
}
void online_turnend_send(int type) { (void)type; }
int online_sync_turnend(int allowCnt, int allowKoi, int allowAga)
{
    (void)allowCnt;
    (void)allowKoi;
    (void)allowAga;
    return 0;
}
void online_fin_send(void) {}
int online_fin_recv(void) { return 1; }
const char* online_get_lobby_id() { return ""; }
int online_join_lobby(const char* lobbyIdBase36) { (void)lobbyIdBase36; return 0; }
uint8_t* online_opponent_steam_id(void) { return NULL; }
const char* online_opponent_steam_name(void) { return NULL; }
const char* online_error_reason(void) { return ""; }
void open_steam_profile(uint8_t* steamId) { (void)steamId; }

int online_notice(void (*frame_proc)(void), int force)
{
    (void)frame_proc;
    (void)force;
    return 0;
}

int session_online_get(void) { return 0; }
void session_online_set(int online_mode) { (void)online_mode; }
int session_local_game_mode_get(void) { return 0; }
void session_local_game_mode_set(int local_game_mode) { (void)local_game_mode; }
int session_online_game_mode_get(void) { return 0; }
void session_online_game_mode_set(int online_game_mode) { (void)online_game_mode; }
int session_private_round_get(void) { return 0; }
void session_private_round_set(int private_round) { (void)private_round; }

void window(int bg, int x, int y, int width, int height)
{
    (void)bg;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}
