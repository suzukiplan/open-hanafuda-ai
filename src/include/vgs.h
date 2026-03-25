#pragma once

#include <stdint.h>
#include <stddef.h>

#define VGS_HOST_SIM 1

#ifndef ON
#define ON 1
#endif
#ifndef OFF
#define OFF 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef struct ObjectAttributeMemory {
    int visible;
    int x;
    int y;
    int size;
    int pri;
    int attr;
    int scale;
    int rotate;
    int slx;
    int sly;
    int mask;
    uint32_t alpha;
} ObjectAttributeMemory;

void* vgs_memset(void* s, int c, size_t n);
void* vgs_memcpy(void* d, const void* s, size_t n);
int vgs_strlen(const char* s);
char* vgs_strcpy(char* d, const char* s);
char* vgs_strcat(char* d, const char* s);

void vgs_u32str(char* out, uint32_t v);
void vgs_d32str(char* out, int32_t v);

void vgs_srand(uint32_t seed);
uint32_t vgs_rand(void);

int vgs_cos(int deg);
int vgs_sin(int deg);
int vgs_degree(int x1, int y1, int x2, int y2);

void vgs_draw_mode(int n, int on);
void vgs_cls_bg_all(int pal);
void vgs_cls_bg(int n, int pal);
void vgs_draw_clear(int n, int x, int y, int w, int h);
void vgs_draw_boxf(int n, int x, int y, int w, int h, int32_t pal);
void vgs_draw_box(int n, int x, int y, int w, int h, int32_t pal);
void vgs_draw_lineH(int n, int x, int y, int w, uint32_t pal);
void vgs_draw_lineV(int n, int x, int y, int h, uint32_t pal);
void vgs_draw_character(int n, int x, int y, int flip, int pal, uint32_t ptn);
void vgs_put_bg(int n, int x, int y, uint32_t attr);
void vgs_print_bg(int n, int x, int y, int flip, const char* s);
void vgs_skip_bg(int n, int on);

void vgs_sprite(int id, int visible, int x, int y, int size, int pri, int attr);
void vgs_sprite_hide_all(void);
void vgs_sprite_priority(int pri);
ObjectAttributeMemory* vgs_oam(int index);
void vgs_sprite_alpha8(ObjectAttributeMemory* o, uint8_t a);

void vgs_pal_set(int pal, int idx, uint32_t color);
uint32_t vgs_pal_get(int pal, int idx);

void vgs_bgm_play(int id);
void vgs_bgm_master_volume(int vol);
int vgs_bgm_master_volume_get(void);
void vgs_sfx_play(int id);
void vgs_sfx_stop(int id);
void vgs_sfx_master_volume(int vol);
int vgs_sfx_master_volume_get(void);

void vgs_pfont_init(int ptn);
void vgs_pfont_print(int n, int x, int y, int flip, int ptn, const char* s);
int vgs_pfont_strlen(const char* s);

void vgs_ptn_transfer(int ptn, const uint8_t* src, size_t len);

int vgs_key_code(void);
int vgs_key_code_a(int key);
int vgs_key_code_b(int key);
int vgs_key_code_x(int key);
int vgs_key_code_y(int key);
int vgs_key_code_up(int key);
int vgs_key_code_down(int key);
int vgs_key_code_left(int key);
int vgs_key_code_right(int key);

int vgs_key_a(void);
int vgs_key_b(void);
int vgs_key_x(void);
int vgs_key_y(void);

int vgs_mouse_enabled(void);
void vgs_mouse_setup(uint16_t ptn, uint8_t pal);
void vgs_mouse_hidden(int on);
int vgs_mouse_moving(void);
int vgs_mouse_x(void);
int vgs_mouse_y(void);
int vgs_mouse_left(void);
int vgs_mouse_right(void);
int vgs_mouse_left_clicked(int* x, int* y);
int vgs_mouse_right_clicked(int* x, int* y);
int vgs_mouse_scroll_vertical(void);
int vgs_mouse_scroll_horizontal(void);

int vgs_button_id_a(void);
int vgs_button_id_b(void);
int vgs_button_id_x(void);
int vgs_button_id_y(void);

uint32_t vgs_utc_year(void);
uint32_t vgs_utc_month(void);
uint32_t vgs_utc_mday(void);
uint32_t vgs_utc_hour(void);
uint32_t vgs_utc_minute(void);
uint32_t vgs_utc_second(void);

uint32_t vgs_local_year(void);
uint32_t vgs_local_month(void);
uint32_t vgs_local_mday(void);
uint32_t vgs_local_hour(void);
uint32_t vgs_local_minute(void);
uint32_t vgs_local_second(void);

uint32_t vgs_user_in(uint32_t addr);
void vgs_exit(int code);
void vgs_vsync(void);
