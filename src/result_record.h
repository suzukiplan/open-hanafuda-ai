#pragma once

typedef struct {
    uint32_t sec;     // 0 ~ 86399
    uint16_t wh[2];   // あがり役 (0: プレイヤー, 1: CPU)
    uint8_t round;    // ラウンド番号
    uint8_t stat;     // 0: draw, 1: player, 2: opponent, MSB: 0=not koikoi, koikoi, MSB-1: 親
    uint8_t score;    // スコア
    uint8_t reserved; // 予約領域
    uint16_t year;    // 年
    uint8_t month;    // 月 (1~12)
    uint8_t mday;     // 日 (1~31)
} ResultRecord;
