#pragma once
#include "common.h"

#define IN_NATIVE_AI_AVAILABLE 0xE82000
#define OUT_NATIVE_AI_SNAPSHOT_ADDR 0xE82004
#define OUT_NATIVE_AI_ARG0 0xE82008
#define OUT_NATIVE_AI_ARG1 0xE8200C
#define OUT_NATIVE_AI_COMMAND 0xE82010
#define IN_NATIVE_AI_STATUS 0xE82014
#define IN_NATIVE_AI_RESULT 0xE82018
#define OUT_WATCH_LOG_ADDR 0xE8004C
#define OUT_WATCH_LOG_CONTROL 0xE80050

enum NATIVE_AI_COMMAND {
    NATIVE_AI_COMMAND_NONE = 0,
    NATIVE_AI_COMMAND_KOIKOI = 1,
    NATIVE_AI_COMMAND_DROP = 2,
    NATIVE_AI_COMMAND_SELECT = 3,
};

enum NATIVE_AI_STATUS {
    NATIVE_AI_STATUS_UNAVAILABLE = 0,
    NATIVE_AI_STATUS_IDLE = 1,
    NATIVE_AI_STATUS_BUSY = 2,
    NATIVE_AI_STATUS_DONE = 3,
    NATIVE_AI_STATUS_ERROR = 4,
};

enum WATCH_LOG_COMMAND {
    WATCH_LOG_COMMAND_WRITE = 1,
    WATCH_LOG_COMMAND_BEGIN = 2,
    WATCH_LOG_COMMAND_END = 3,
};

typedef struct {
    int32_t type;
    int32_t reversed;
    int32_t month;
    int32_t index;
    int32_t id;
} NativeAiCardSnapshot;

typedef struct {
    int32_t cards[48];
    int32_t start;
    int32_t num;
} NativeAiCardSet48Snapshot;

typedef struct {
    int32_t cards[8];
    int32_t start;
    int32_t num;
} NativeAiCardSet8Snapshot;

typedef struct {
    int32_t reach[WINNING_HAND_MAX];
    int32_t delay[WINNING_HAND_MAX];
    int32_t score[WINNING_HAND_MAX];
    int32_t risk_reach_estimate[WINNING_HAND_MAX];
    int32_t risk_delay[WINNING_HAND_MAX];
    int32_t risk_completion_score[WINNING_HAND_MAX];
    int32_t risk_score[WINNING_HAND_MAX];
    int32_t priority_speed[WINNING_HAND_MAX];
    int32_t priority_score[WINNING_HAND_MAX];
    int32_t risk_7plus_distance;
    int32_t opponent_win_x2;
    int32_t left_own;
    int32_t left_rounds;
    int32_t round_max;
    int32_t match_score_diff;
    int32_t koikoi_opp;
    int32_t koikoi_mine;
    int32_t opp_coarse_threat;
    int32_t opp_exact_7plus_threat;
    int32_t risk_mult_pct;
    int32_t greedy_need;
    int32_t defensive_need;
    int32_t base_now;
    int32_t best_additional_1pt_reach;
    int32_t best_additional_1pt_delay;
    int32_t can_overpay_akatan;
    int32_t can_overpay_aotan;
    int32_t can_overpay_ino;
    int32_t env_total;
    int32_t env_diff;
    int32_t env_cat_sum[ENV_CAT_MAX];
    int32_t domain;
    int32_t plan;
    int32_t mode;
    int32_t bias_offence;
    int32_t bias_speed;
    int32_t bias_defence;
} NativeAiStrategySnapshot;

typedef struct {
    int32_t wh[WINNING_HAND_MAX];
    int32_t base_up[WINNING_HAND_MAX];
    NativeAiCardSet48Snapshot wcs[WINNING_HAND_MAX];
    int32_t wh_count;
    int32_t score;
    NativeAiCardSet48Snapshot want;
} NativeAiInventStatsSnapshot;

typedef struct {
    uint32_t frame;
    NativeAiCardSnapshot cards[48];
    NativeAiCardSet48Snapshot floor;
    NativeAiCardSet48Snapshot deck;
    NativeAiCardSet8Snapshot own[2];
    NativeAiCardSet48Snapshot invent[2][4];
    NativeAiInventStatsSnapshot stats[2];
    int32_t total_score[2];
    int32_t koikoi[2];
    int32_t koikoi_score[2];
    int32_t round_max;
    int32_t round;
    int32_t oya;
    int32_t current_player;
    int32_t auto_play;
    int32_t no_sake;
    int32_t ai_model[2];
    NativeAiStrategySnapshot strategy[2];
    int8_t watch_min_action[2][256];
} NativeAiSnapshot;

int native_ai_available(void);
int native_ai_execute(int command, int player, int arg, int* result);
void native_ai_watch_log_write(const char* s);
