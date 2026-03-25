#pragma once

#include <stdarg.h>

void vgs_putlog(const char* fmt, ...);
int vgs_watch_log_enabled(void);
int vgs_watch_log_set_base_path(const char* path);
int vgs_watch_log_begin_game(void);
int vgs_watch_log_finalize_game(int game_index, int score0, int score1);
void vgs_watch_log_shutdown(void);
