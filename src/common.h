#pragma once
#include <vgs.h>
#include <log.h>
#include "result_record.h"

extern const uint8_t rom_avatar_you[4096];
extern const uint8_t rom_avatar_cpu[4096];
extern const uint8_t rom_avatar_hard1[4096];
extern const uint8_t rom_avatar_stenv[4096];
extern const uint8_t rom_avatar_mcenv[4096];
extern const uint8_t rom_thunder1[32768];
extern const uint8_t rom_thunder2[32768];

#define RESULT_DRAW 0
#define RESULT_WIN 1
#define RESULT_LOSE 2

enum LOBBY_FILTER {
    LOBBY_FILTER_DEFAULT = 0,
    LOBBY_FILTER_CLOSE,
    LOBBY_FITLER_FAR,
    LOBBY_FILTER_WORLDWIDE,
};

enum LOBBY_TYPE {
    LOBBY_TYPE_PUBLIC = 0,
    LOBBY_TYPE_FRIENDS = 2,
};

enum AI_MODEL {
    AI_MODEL_NORMAL = 0,
    AI_MODEL_HARD,
    AI_MODEL_STENV,
    AI_MODEL_MCENV,
    AI_MODEL_MAX,
};

// ユーザー定義I/O
#define IN_STEAM_SETTINGS 0xE80000     // Steam の Settings を開く
#define IN_IS_WINDOW_MODE 0xE80004     // Window / FullScreen モード取得
#define OUT_IS_WINDOW_MODE 0xE80004    // Window / FullScreen モード切り替え
#define IN_ENABLE_WINDOW_MODE 0xE80008 // Windowモードが可能な環境かチェック
#define IN_IS_NEXT_FEST 0xE8000C       // Steam Next Fest 開催中かチェック
#define OUT_SESSION_ADDR 0xE80010      // セッションアドレス
#define OUT_SESSION_SAVE 0xE80014      // セッションセーブ
#define OUT_SESSION_LOAD 0xE80018      // セッションロード
#define OUT_OPEN_STORE 0xE8001C        // 製品版のストアページを開く
#define OUT_UNLOCK_TROPHY 0xE80020     // トロフィー（アチーブメント）をアンロック
#define IN_OPEN_SUPPORT 0xE80024       // サポート掲示板を開く
#define OUT_LB_BOARD_NAME 0xE80028     // リーダーボードのボード名を設定
#define OUT_LB_UGC_SIZE 0xE8002C       // リーダーボードの添付データサイズを設定
#define OUT_LB_UGC_DATA 0xE80030       // リーダーボードの添付データを設定
#define OUT_LB_SEND_SCORE 0xE80034     // リーダーボードへスコアを送信
#define OUT_LB_DATA_RAM 0xE80038       // リーダーボード構造体のポインタ (RAM)
#define IN_LB_DATA_READ 0xE8003C       // リーダーボード構造体を読み込む
#define OUT_LB_UGC_INDEX 0xE80040      // UGCデータの読み込みインデックス
#define IN_LB_UGC_READY 0xE80044       // UGCデータの読み込み完了チェック
#define IN_IS_NOT_STEAM_DECK 0xE80048  // SteamDeck以外の環境かチェック (SteamDeck=0, それ以外=1, 未定義=255)
#define OUT_WATCH_LOG_ADDR 0xE8004C    // watch.log 文字列アドレス
#define OUT_WATCH_LOG_CONTROL 0xE80050 // watch.log 制御/長さ
#define IN_LB_DATA_READY 0xE80054      // リーダーボードが読み込み済みかチェック (0: まだ読み込まれていない(reload中), 1: 読み込み済み)
#define OUT_PLAY_LOG_ADDR 0xE80058     // play.log 文字列アドレス
#define OUT_PLAY_LOG_CONTROL 0xE8005C  // play.log 制御/長さ

// ユーザー定義I/O (オンライン系)
#define OUT_MATCHING_START 0xE81000           // マッチングリクエスト
#define IN_MATCHING_RESULT 0xE81004           // 0: 切断, 1: マッチング中, 2 or 3: 成立 (2=親, 3=子)
#define OUT_ONLINE_DECK_ADDR 0xE81008         // デッキ情報の入出力RAMアドレスを設定
#define OUT_ONLINE_DECK_SEND 0xE8100C         // デッキ情報を送信
#define IN_ONLINE_DECK_RECV 0xE81010          // デッキ情報を受信 (0: 切断, 1: 受信待ち, 2: データ到着)
#define OUT_ONLINE_CARD_SEND 0xE81014         // カード送信
#define IN_ONLINE_CARD_RECV 0xE81018          // カード受信 (0~47: カードindex, 256: 受信待ち, 512: 切断)
#define OUT_ONLINE_TURNEND_SEND 0xE8101C      // ターン終了送信 (0: 継続, 1: こいこい, 2: あがり)
#define IN_ONLINE_TURNEND_RECV 0xE81020       // ターン終了受信 (0: 継続, 1: こいこい, 2: あがり, 256: 受信待ち, 512: 切断)
#define OUT_ONLINE_SET_LOBBY_ID_ADDR 0xE81024 // ロビーID入出力用の文字列ポインタを設定
#define IN_ONLINE_GET_LOBBY_ID 0xE81028       // ロビーIDを取得
#define IN_ONLINE_JOIN_LOBBY_ID 0xE8102C      // 特定のロビーへの参加リクエスト
#define OUT_AVATAR_ADDR 0xE81030              // アバター (32x32) を取得する RAM アドレスを設定
#define IN_AVATAR_READ_OWN 0xE81034           // 自分のアバターを読み込む (0: 読み込めなかった, 1: 読み込めた)
#define IN_AVATAR_READ_OPP 0xE81038           // 対戦相手のアバターを読み込む (0: 読み込めなかった, 1: 読み込めた)
#define OUT_STEAM_ID_ADDR 0xE8103C            // SteamIDの入出力アドレスを設定
#define IN_OPPONENT_STEAM_ID 0xE81040         // 対戦相手のSteamIDの読み込む
#define OUT_OPEN_STEAM_ID 0xE81044            // SteamIDのプロフィールを開く
#define OUT_STEAM_NAME_ADDR 0xE81048          // Steamアカウント名の入力アドレスを設定
#define IN_OPPONENT_STEAM_NAME 0xE8104C       // 対戦相手のSteamアカウント名を読み込む
#define OUT_ONLINE_FIN_SEND 0xE81050          // FIN メッセージ送信
#define IN_ONLINE_FIN_RECV 0xE81054           // FIN メッセージ受信 (1: 受信, 256: 未受信, 512: 切断)
#define OUT_ONLINE_QL_CHECK_REQ 0xE81058      // Quick Match ロビーがすぐに参加可能な状態かチェック（リクエスト）
#define IN_ONLINE_QL_CHECK_RES 0xE8105C       // Quick Match ロビーがすぐに参加可能な状態かチェック（結果確認・ポーリング）
#define OUT_ONLINE_QL_CHECK_RST 0xE81060      // Quick Match ロビーがすぐに参加可能な状態かチェックするのをキャンセル
#define OUT_ONLINE_FL_CHECK_REQ 0xE81064      // Friends Match ロビーがすぐに参加可能な状態かチェック（リクエスト）
#define IN_ONLINE_FL_CHECK_RES 0xE81068       // Friends Match ロビーがすぐに参加可能な状態かチェック（結果確認・ポーリング）
#define OUT_ONLINE_FL_CHECK_RST 0xE8106C      // Friends Match ロビーがすぐに参加可能な状態かチェックするのをキャンセル
#define OUT_ONLINE_REASON_ADDR 0xE81FEC       // 指定アドレス（64バイトのバッファ）に通信エラー要因を格納
#define IN_ONLINE_ROUND_NUM 0xE81FF4          // 対戦ラウンド数
#define IN_ONLINE_CHECK 0xE81FF8              // オンラインの状態確認
#define OUT_ONLINE_DISCONNECT 0xE81FFC        // オンライン切断

enum TROPHY {
    TROPHY_WH_KASU = 0,    // B: あがり役「カス」(カス札10枚+) であがる
    TROPHY_WH_TAN,         // B: あがり役「タン」(短冊札5枚+) であがる
    TROPHY_WH_TANE,        // B: あがり役「タネ」 (タネ札5枚+) であがる
    TROPHY_WH_AKATAN,      // S: あがり役「赤短」であがる
    TROPHY_WH_AOTAN,       // S: あがり役「青短」であがる
    TROPHY_WH_DBTAN,       // G: あがり役「赤青重複」であがる
    TROPHY_WH_ISC,         // S: あがり役「猪鹿蝶」であがる
    TROPHY_WH_TSUKIMI,     // B: あがり役「月見酒」であがる
    TROPHY_WH_HANAMI,      // B: あがり役「花見酒」であがる
    TROPHY_WH_SANKOU,      // B: あがり役「三光」であがる
    TROPHY_WH_SHIKOU7,     // S: あがり役「四光」（雨入り7点）であがる
    TROPHY_WH_SHIKOU8,     // S: あがり役「四光」（雨無し8点）であがる
    TROPHY_WH_GOKOU,       // G: あがり役「五光」であがる
    TROPHY_KOIKOI,         // B: 「こいこい」をした
    TROPHY_KOIKOI_SUCCESS, // B: 「こいこい」をしてあがった
    TROPHY_KOIKOI_FAILED,  // B: 「こいこい」をしてあがられた
    TROPHY_KOIKOI_ED,      // B: 「こいこい」された
    TROPHY_DOUBLE_UP,      // S: 「こいこい」されて 6点以下 であがり返した (x2)
    TROPHY_QUAD_UP,        // G: 「こいこい」されて 7点以上 であがり返した (x4)
    TROPHY_WIN_VERYSHORT,  // B: 2 rounds の CPU 戦 (0 == g.online, 0 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN_SHORT,      // B: 4 rounds の CPU 戦 (0 == g.online, 0 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN_HALF,       // B: 6 rounds の CPU 戦 (0 == g.online, 0 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN_FULL,       // S: 12 rounds の CPU 戦 (0 == g.online, 0 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN_ONLINE,     // S: オンラインモード (1 == g.online) で初勝利
    TROPHY_WIN_S50,        // B: 50点以上獲得して勝利
    TROPHY_WIN_S80,        // S: 80点以上獲得して勝利
    TROPHY_WIN_S100,       // S: 100点以上獲得して勝利
    TROPHY_WIN_S150,       // G: 150点以上獲得して勝利
    TROPHY_WIN2_VERYSHORT, // B: 2 rounds の CPU 戦 (0 == g.online, 1 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN2_SHORT,     // B: 4 rounds の CPU 戦 (0 == g.online, 1 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN2_HALF,      // S: 6 rounds の CPU 戦 (0 == g.online, 1 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN2_FULL,      // G: 12 rounds の CPU 戦 (0 == g.online, 1 == g.model, 0 == g.no_sake) で初勝利
    TROPHY_WIN3_VERYSHORT, // B: 2 rounds の CPU 戦 (0 == g.online, 0 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_WIN3_SHORT,     // B: 4 rounds の CPU 戦 (0 == g.online, 0 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_WIN3_HALF,      // B: 6 rounds の CPU 戦 (0 == g.online, 0 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_WIN3_FULL,      // S: 12 rounds の CPU 戦 (0 == g.online, 0 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_WIN4_VERYSHORT, // B: 2 rounds の CPU 戦 (0 == g.online, 1 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_WIN4_SHORT,     // B: 4 rounds の CPU 戦 (0 == g.online, 1 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_WIN4_HALF,      // S: 6 rounds の CPU 戦 (0 == g.online, 1 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_WIN4_FULL,      // G: 12 rounds の CPU 戦 (0 == g.online, 1 == g.model, 1 == g.no_sake) で初勝利
    TROPHY_SIZE
};

#define SFX_CURSOR_MOVE 0
#define SFX_CURSOR_ENTER 1
#define SFX_CARD_OPEN 2
#define SFX_CARD_THROW 3
#define SFX_CARD_DISCARD 4
#define SFX_DROP_DOWN 5
#define SFX_ATTACK 6
#define SFX_GET 7
#define SFX_MOVE_CURSOR2 8
#define SFX_CURSOR_LEAVE 9
#define SFX_CHOOSE 10
#define SFX_CARD_DRAW 11
#define SFX_PAUSE 12
#define SFX_KOIKOI_IN 13
#define SFX_KOIKOI_OUT 14
#define SFX_GET_HOT 15
#define SFX_AGARI 16
#define SFX_THUNDER 17

#define BGM_TITLE 0
#define BGM_MAIN_THEME 1
#define BGM_WINNER 2
#define BGM_LOSER 3
#define BGM_KOIKOI 4

#define WINDOW_COLOR 0x00007F

// 上限値
#define FONT_SIZE 128
#define CARD_SIZE 25
#define CARD_TOP 128
#define CARD_REV (0x10000 | (CARD_TOP + 4 * 12 * CARD_SIZE))
#define WH_SIZE (30 * 30)
#define MOVE_MAX 64
#define ZANZO_MAX 64
#define AGARI_MAX 48
#define GEKIATSU_MAX 48
#define DOT_MAX 1024
#define SEASON_MAX 64
#define SP_ECARDS_MAX 64
#define HIBANA_MAX 128

// パターン
#define PTN_FONT 0
#define PTN_CARD FONT_SIZE
#define PTN_CARD_REV (PTN_CARD + 4 * 12 * CARD_SIZE)
#define PTN_CURSOR (PTN_CARD_REV + CARD_SIZE)
#define PTN_DROP (PTN_CURSOR + 4 * 4)
#define PTN_DECK_CURSOR (PTN_DROP + CARD_SIZE)
#define PTN_WH_GOKOU (PTN_DECK_CURSOR + CARD_SIZE * 4)
#define PTN_WH_SHIKOU (PTN_WH_GOKOU + WH_SIZE)
#define PTN_WH_SANKOU (PTN_WH_SHIKOU + WH_SIZE)
#define PTN_WH_HANAMI (PTN_WH_SANKOU + WH_SIZE)
#define PTN_WH_TSUKIMI (PTN_WH_HANAMI + WH_SIZE)
#define PTN_WH_ISC (PTN_WH_TSUKIMI + WH_SIZE)
#define PTN_WH_DBTAN (PTN_WH_ISC + WH_SIZE)
#define PTN_WH_AKATAN (PTN_WH_DBTAN + WH_SIZE)
#define PTN_WH_AOTAN (PTN_WH_AKATAN + WH_SIZE)
#define PTN_WH_TANE (PTN_WH_AOTAN + WH_SIZE)
#define PTN_WH_TAN (PTN_WH_TANE + WH_SIZE)
#define PTN_WH_KASU (PTN_WH_TAN + WH_SIZE)
#define PTN_FADE (PTN_WH_KASU + WH_SIZE)
#define PTN_FF (PTN_FADE + 121)
#define PTN_CURSOR2 (PTN_FF + 16)
#define PTN_FONT_YELLOW (PTN_CURSOR2 + 4)
#define PTN_KOIKOI (PTN_FONT_YELLOW + 128)
#define PTN_OYA (PTN_KOIKOI + 900)
#define PTN_ROUND (PTN_OYA + 32)
#define PTN_LOGO_BATTLE (PTN_ROUND + 30)
#define PTN_LOGO_HANAFUDA (PTN_LOGO_BATTLE + 400)
#define PTN_LOGO_JP (PTN_LOGO_HANAFUDA + 400)
#define PTN_FADE2 (PTN_LOGO_JP + 256)
#define PTN_GEKIATSU (PTN_FADE2 + 16)
#define PTN_PAUSE (PTN_GEKIATSU + CARD_SIZE)
#define PTN_FONT_ERROR (PTN_PAUSE + 18 * 9)
#define PTN_FF_GUIDE (PTN_FONT_ERROR + FONT_SIZE)
#define PTN_GEKIATSU2 (PTN_FF_GUIDE + 16)
#define PTN_WH_GUIDE (PTN_GEKIATSU2 + CARD_SIZE)
#define PTN_SEASON_EFFECT (PTN_WH_GUIDE + 16)
#define PTN_TIMER (PTN_SEASON_EFFECT + 4 * 12 * 2)
#define PTN_PRESS (PTN_TIMER + 4 * 10)
#define PTN_THUNDER (PTN_PRESS + 16 * 3)
#define PTN_FLASH (PTN_THUNDER + 32 * 32)
#define PTN_X2 (PTN_FLASH + 121)
#define PTN_X4 (PTN_X2 + 36)
#define PTN_LOGO_HIBANA (PTN_X4 + 36)
#define PTN_MOUSE (PTN_LOGO_HIBANA + 8)
#define PTN_ONLINE_WAIT (PTN_MOUSE + 4)
#define PTN_ONLINE_READY (PTN_ONLINE_WAIT + 8)
#define PTN_SIZE (PTN_ONLINE_READY + 3)

// エイリアス
#define PTN_MENU_CURSOR (PTN_FONT + 0x0F)

// スプライト
#define DEFINE_SP(name, prev, offset) enum { name = (prev) + (offset) }
enum { SP_AGARI = 32 };
DEFINE_SP(SP_KOIKOI, SP_AGARI, AGARI_MAX);
DEFINE_SP(SP_THUNDER, SP_KOIKOI, 1);
DEFINE_SP(SP_FLASH, SP_THUNDER, 1);
DEFINE_SP(SP_X, SP_FLASH, 1);
DEFINE_SP(SP_WH_FLASH, SP_X, 1);
DEFINE_SP(SP_WH, SP_WH_FLASH, 1);
DEFINE_SP(SP_DECK_CURSOR, SP_WH, 1);
DEFINE_SP(SP_CURSOR, SP_DECK_CURSOR, 2);
DEFINE_SP(SP_SEASON, SP_CURSOR, 1);
DEFINE_SP(SP_MOVE, SP_SEASON, SEASON_MAX);
DEFINE_SP(SP_FF, SP_MOVE, MOVE_MAX);
DEFINE_SP(SP_TIMER, SP_FF, 1);
DEFINE_SP(SP_FADE, SP_TIMER, 1);
DEFINE_SP(SP_ECARDS, SP_FADE, 1);
DEFINE_SP(SP_ZANZO, SP_ECARDS, SP_ECARDS_MAX);
DEFINE_SP(SP_FLOOR, SP_ZANZO, ZANZO_MAX);
DEFINE_SP(SP_GEKIATSU, SP_FLOOR, 2);
DEFINE_SP(SP_DECK, SP_GEKIATSU, GEKIATSU_MAX);
DEFINE_SP(SP_OWN, SP_DECK, 48);
DEFINE_SP(SP_INVENT, SP_OWN, 48 * 2);
DEFINE_SP(SP_DROP, SP_INVENT, 24 * 4 * 2);
DEFINE_SP(SP_AVATAR, SP_DROP, 1);
DEFINE_SP(SP_HIBANA, SP_AVATAR, 2);
DEFINE_SP(SP_SIZE, SP_HIBANA, HIBANA_MAX);

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rect;

// カード
enum CARD_TYPE {
    CARD_TYPE_GOKOU = 0,
    CARD_TYPE_TANE = 1,
    CARD_TYPE_TAN = 2,
    CARD_TYPE_KASU = 3,
};

static int card_types[48] = {
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_GOKOU,  // Jan
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE,   // Feb
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_GOKOU,  // Mar
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE,   // Apr
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE,   // May
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE,   // Jun
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE,   // Jul
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TANE, CARD_TYPE_GOKOU, // Aug
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE,   // Spt
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE,   // Oct
    CARD_TYPE_KASU, CARD_TYPE_TAN, CARD_TYPE_TANE, CARD_TYPE_GOKOU,  // Nov
    CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_KASU, CARD_TYPE_GOKOU  // Dec
};

typedef struct {
    int type;     // 種別
    int reversed; // 裏面表示フラグ
    int month;    // 月 (0 ~ 11)
    int index;    // インデックス (0 ~ 3) *ポイントが小さいカードが0
    int id;       // ID: 0 ~ 47
} Card;

typedef struct {
    Card* cards[48]; // カード実体へのポインタ
    int start;       // 探索起点インデックス
    int num;         // 存在する枚数
} CardSet;

// あがり役
enum WID {
    WID_GOKOU,      // 五光: CARD_TYPE_GOKOU が5枚 (すべて) <10pts> [WID_SHIKOU, WID_AME_SHIKOU, WID_SANKOU を無効]
    WID_SHIKOU,     // 四光: 11月の CARD_TYPE_GOKOU を除く CARD_TYPE_GOKOU が4枚 <8pts> [WID_SANKOU を無効]
    WID_AME_SHIKOU, // 雨四光: 11月の CARD_TYPE_GOKOU を含む CARD_TYPE_GOKOU が4枚 <7pts>
    WID_SANKOU,     // 三光: 11月の CARD_TYPE_GOKOU を除く CARD_TYPE_GOKOU が3枚 <5pts>
    WID_HANAMI,     // 花見酒: 9月の CARD_TYPE_TANE + 3月の CARD_TYPE_GOKOU <5pts>
    WID_TSUKIMI,    // 月見酒: 9月の CARD_TYPE_TANE + 8月の CARD_TYPE_GOKOU <5pts>
    WID_ISC,        // 猪鹿蝶: 6月, 7月, 10月 の CARD_TYPE_TANE <5pts+> ※その他の CARD_TYPE_TANE 1枚につき 1pts 加算 [WID_TANE を無効]
    WID_DBTAN,      // 赤青重複: 1月, 2月, 3月 並びに 6月, 9月, 10月 の CARD_TYPE_TAN <10pts+> ※その他の CARD_TYPE_TANE 1枚につき 1pts 加算 [WID_TAN, WID_AKATAN, WID_AOTAN を無効]
    WID_AKATAN,     // 赤短: 1月, 2月, 3月 の CARD_TYPE_TAN <5pts+> ※その他の CARD_TYPE_TAN 1枚につき 1pts 加算 [WID_TAN を無効]
    WID_AOTAN,      // 青短: 6月, 9月, 10月 の CARD_TYPE_TAN <5pts+> ※その他の CARD_TYPE_TAN 1枚につき 1pts 加算 [WID_TAN を無効]
    WID_TANE,       // タネ: CARD_TYPE_TANE が 5枚 <1pts+> ※その他の CARD_TYPE_TANE 1枚につき 1pts 加算
    WID_TAN,        // タン: CARD_TYPE_TAN が 5枚 <1pts+> ※その他の CARD_TYPE_TAN 1枚につき 1pts 加算
    WID_KASU,       // カス: CARD_TYPE_KASU または 9月の CARD_TYPE_TANE が 10枚 <1pts+> ※その他の CARD_TYPE_KASU 1枚につき 1pts 加算
    WINNING_HAND_MAX
};

typedef struct {
    int id;            // ID
    const char* nameJ; // 和名
    const char* nameE; // 英語名
    const char* name;  // 短縮名
    int base_score;    // 基礎点
    int ptn;           // キャラクタパターン
    int top_y_plus;    // 上の文字を基点から下げるピクセル数
    int btm_y_minus;   // 下の文字を基点から上げるピクセル数
} WinningHand;

extern WinningHand winning_hands[WINNING_HAND_MAX];
extern const char* BASE28;

// 移動種別
enum MTYPE {
    MTYPE_OWN,        // 座標移動 (手札)
    MTYPE_OWN_OPEN,   // 手札をめくる
    MTYPE_DECK,       // 座標移動（場札）
    MTYPE_DROP,       // 手札を場札へ移動
    MTYPE_DISCARD,    // 手札を場札へ捨てる
    MTYPE_PICK,       // 場札→取り札 のピックアップ
    MTYPE_INVENT,     // 場札を取り札の場所へ移動
    MTYPE_INVENT_HOT, // 激アツの場札を取り札の場所へ移動
    MTYPE_FOPEN,      // 場札を開く
    MTYPE_FADEOUT,    // 下に落ちながらフェードアウト
};

enum StrategyMode {
    MODE_BALANCED = 0,
    MODE_GREEDY = 1,
    MODE_DEFENSIVE = 2,
};

enum {
    ENV_CAT_1 = 0, // 光札（桜・月） 10pts ... 花見酒(5pts), 月見酒(5pts), 三光(5pts), 四光(8pts), 五光(10pts)の構成札 ※相手取札に盃があれば ENV_CAT_3 へフォールバック
    ENV_CAT_2,     // 盃(1) ... 花見酒(5pts), 月見酒(5pts), タネ(1pt), カス(1pt) ※相手取札の月 or 桜の取得状況によって ENV_CAT_11 or ENV_CAT_12 へフォールバック
    ENV_CAT_3,     // 光札（1月・12月) 9pts ... 三光(5pts), 四光(8pts), 五光(10pts)の構成札
    ENV_CAT_4,     // 赤短 ... 赤青重複 (10pts), 赤短 (5pts), タン (1pt) の構成札 ※相手取札に赤短が1枚でもあれば ENV_CAT_8 へフォールバック
    ENV_CAT_5,     // 青短 ... 赤青重複 (10pts), 青短 (5pts), タン (1pt) の構成札 ※相手取札に青短が1枚でもあれば ENV_CAT_8 へフォールバック
    ENV_CAT_6,     // 光札 (柳) 8pts ... 雨四光(7pts), 五光(10pts)の構成札
    ENV_CAT_7,     // 猪鹿蝶 ... 猪鹿蝶 (5pts), タネ (1pt) の構成札 ※相手取札に猪鹿蝶が1枚でもあれば ENV_CAT_9 へフォールバック
    ENV_CAT_8,     // 短冊 ...  赤青重複 (+1), 赤短 (+1), 青短(+1), タン (1pt) の構成札
    ENV_CAT_9,     // タネ ... 猪鹿蝶 (+1), タネ (1pt) の構成札
    ENV_CAT_10,    // カス ... カス(1pt)の構成札
    ENV_CAT_11,    // 盃(2) ... 花見酒(5pts) or 月見酒(5pts) | タネ(1pt), カス(1pt)
    ENV_CAT_12,    // 盃(3) ... タネ(1pt), カス(1pt)
    ENV_CAT_NA,    // 無価値 (相手が桜・月・1月・12月の光札を2枚以上持っている時の光札: 桜・月・1月・11月・12月)
    ENV_CAT_MAX,
};

#define ENV_CAT1_BASE 99  // 桜・月 ... 素早く 5pts/20pts を作れるので加点高め
#define ENV_CAT2_BASE 100 // 盃 ... 素早く 5pts/20pts を作れるので加点高め
#define ENV_CAT3_BASE 70  // 三光 ... 赤短・青短・猪鹿蝶より構成札の分母が1枚多い
#define ENV_CAT4_BASE 50  // 赤短 ... 3枚で 5pts
#define ENV_CAT5_BASE 50  // 青短 ... 3枚で 5pts
#define ENV_CAT6_BASE 25  // 柳 ... 4光, 5光 など難度が高い構成札 なので加点低め
#define ENV_CAT7_BASE 50  // 猪鹿蝶 ... 3枚で 5pts
#define ENV_CAT8_BASE 2   // 短冊 (5枚目以降は1枚1点)
#define ENV_CAT9_BASE 2   // タネ (5枚目以降は1枚1点)
#define ENV_CAT10_BASE 1  // カス (10枚目以降は1枚1点)
#define ENV_CAT11_BASE 80 // 盃(2) ... 素早く 5pts を作れるので加点高め
#define ENV_CAT12_BASE 3  // 盃(3) ... カスにもタネにもなる
#define ENV_CATNA_BASE 0  // 無価値

typedef enum {
    AI_ENV_DOMAIN_NONE = 0,
    AI_ENV_DOMAIN_LIGHT,
    AI_ENV_DOMAIN_SAKE,
    AI_ENV_DOMAIN_AKATAN,
    AI_ENV_DOMAIN_AOTAN,
    AI_ENV_DOMAIN_ISC,
    AI_ENV_DOMAIN_MAX,
} AiEnvDomain;

typedef enum {
    AI_PLAN_SURVIVE = 0,
    AI_PLAN_PRESS,
    AI_PLAN_MAX,
} AiPlan;

/// @brief AI 戦略分析テーブル
typedef struct {
    int reach[WINNING_HAND_MAX];               // 役毎の新規成立確率 [%] (決定化MC近似)
    int delay[WINNING_HAND_MAX];               // 役 (WID) 毎のあがりまで速度（小さいほど早くあがれる）
    int score[WINNING_HAND_MAX];               // 役 (WID) 毎の期待スコア
    int risk_reach_estimate[WINNING_HAND_MAX]; // 相手の役の危険度 [%]（スタティック推定）
    int risk_delay[WINNING_HAND_MAX];          // 相手の役の最短成立手数（小さいほど危険）
    int risk_completion_score[WINNING_HAND_MAX]; // 相手がその役を初回成立した時の公開情報ベース見込み点
    int risk_score[WINNING_HAND_MAX];          // 相手の役の期待損失（Expected Loss）
    int priority_speed[WINNING_HAND_MAX];      // 狙うべき役のプライオリティ <速度重視>
    int priority_score[WINNING_HAND_MAX];      // 狙うべき役のプライオリティ <得点重視>
    int risk_7plus_distance;                   // 相手が 7点以上へ到達するまでの推定距離
    int opponent_win_x2;                       // 相手が倍化窓（7点到達見込み or 攻勢局面）にいるなら ON
    int left_own;                              // 戦況: 自分の残り手札数
    int left_rounds;                           // 戦況: 残りラウンド数
    int round_max;                             // 戦況: 対局の総ラウンド数
    int match_score_diff;                      // 戦況: 総合点差 (自分 total_score - 相手 total_score)
    int koikoi_opp;                            // 戦況: 相手が「こいこい」中なら ON
    int koikoi_mine;                           // 戦況: 自分が「こいこい」中なら ON
    int opp_coarse_threat;                     // 相手脅威の粗い集約値 0..100
    int opp_exact_7plus_threat;                // 相手の厳密な7点到達脅威 0..100
    int risk_mult_pct;                         // risk_score へ掛けた倍率[%]
    int greedy_need;                           // 貪欲モード要求度 0..100
    int defensive_need;                        // 防御モード要求度 0..100
    int base_now;                              // 現在の成立済み基礎点
    int best_additional_1pt_reach;             // 次の +1 成立筋の最大到達率
    int best_additional_1pt_delay;             // 次の +1 成立筋の最短 delay
    int can_overpay_akatan;                    // 赤短を 6点以上で成立させやすいなら ON
    int can_overpay_aotan;                     // 青短を 6点以上で成立させやすいなら ON
    int can_overpay_ino;                       // 猪鹿蝶を 6点以上で成立させやすいなら ON
    int env_total;                             // 自分視点 Env total
    int env_diff;                              // 自分 Env - 相手 Env
    int env_cat_sum[ENV_CAT_MAX];              // 自分視点カテゴリ別 Env 寄与合計
    AiEnvDomain domain;                        // Env が高い理由の主ドメイン
    AiPlan plan;                               // Env から見た暫定戦型
    int mode;                                  // enum StrategyMode

    // 戦略判断変数
    struct Bias {
        int offence; // 高得点・上振れ・逆転狙いをどれだけ重視するか
        int speed;   // 早期成立・短手数・即上がりをどれだけ重視するか
        int defence; // 相手妨害・危険回避をどれだけ重視するか
    } bias;
} StrategyData;

// カード移動アニメーション・レコード
typedef struct {
    Card* card;                                 // 対象カード
    int mtype;                                  // 移動種別
    int exist;                                  // 存在フラグ
    int fx;                                     // 現在の座標 (X * 256)
    int fy;                                     // 現在の座標 (Y * 256)
    int tx;                                     // 目標の座標 (X * 256)
    int ty;                                     // 目標の座標 (Y * 256)
    int speed;                                  // 移動速度
    int angle;                                  // 目標角度
    int frame;                                  // フレームカウンタ
    int jump;                                   // ジャンプ
    int scale;                                  // 拡大
    int alpha;                                  // アルファブレンド
    int gekiatsu;                               // 激アツフラグ
    int i[4];                                   // 汎用カウンタ
    ObjectAttributeMemory* o;                   // OAM
    void (*callback)(Card* card, int x, int y); // 終了時コールバック
} CardMoveRecord;

// カード移動アニメーション用カードセット
typedef struct {
    CardMoveRecord records[MOVE_MAX]; // カード移動アニメーション・レコード
    int ptr;                          // 追加位置
} CardMoveSet;

// 残像アニメーションレコードセット
typedef struct {
    int alpha[ZANZO_MAX]; // 残像アニメーションレコード
    int ptr;              // 追加位置
} CardZanzoSet;

// 所有カード状態
typedef struct {
    int gokou;                         // 五光 (CARD_TYPE_GOKOU の数)
    int shikou;                        // 四光 (11月の CARD_TYPE_GOKOU を除く CARD_TYPE_GOKOU の数)
    int tan;                           // 短尺 (CARD_TYPE_TAN の数)
    int isc;                           // 猪鹿蝶 (6月, 7月, 10月 の CARD_TYPE_TAN の数)
    int tsukimi;                       // 月見酒 (8月の CARD_TYPE_GOKOU + 9月の CARD_TYPE_TANE)
    int hanami;                        // 花見酒 (3月の CARD_TYPE_GOKOU + 9月の CARD_TYPE_TANE)
    int akatan;                        // 赤短 (1月, 2月, 3月の CARD_TYPE_TAN の数)
    int aotan;                         // 青短 (6月, 9月, 10月の CARD_TYPE_TAN の数)
    int tane;                          // タネ (CARD_TYPE_TANE の数)
    int kasu;                          // カス (CARD_TYPE_KASU の数)
    WinningHand* wh[WINNING_HAND_MAX]; // 有効なあがり役
    int base_up[WINNING_HAND_MAX];     // 基礎点の加算値
    CardSet wcs[WINNING_HAND_MAX];     // あがり役構成カード
    int wh_count;                      // 有効なあがり役数
    int score;                         // 得点
    CardSet want;                      // 欲しいカード
} InventStats;

enum DotColor {
    DotColorBlue = 1,
    DotColorRed,
    DotColorPurpule,
    DotColorGreen,
    DotColorCyan,
    DotColorYellow,
    DotColorWhite
};

typedef struct {
    int flag;
    int fx;
    int fy;
    int degree;
    int speed;
    int color;
} DotRecord;

typedef struct {
    DotRecord data[DOT_MAX];
    int index;
    int count;
} DotTable;

typedef struct {
    int flag;
    int fx;
    int fy;
    int vt;
    int rotate;
    int rotate_add;
    int scale;
    int scale_add;
    int speed;
    int alpha;
} SeasonRecord;

typedef struct {
    SeasonRecord data[SEASON_MAX];
    int index;
    int count;
} SeasonTable;

typedef struct {
    uint32_t frame;                  // ゲームフレーム
    Card cards[48];                  // カード実体
    CardSet floor;                   // 山札
    CardSet deck;                    // 場札
    CardSet own[2];                  // 手札
    CardSet invent[2][4];            // 取り札 (0: 20pt, 1: 10pts, 2: 5pts, 3: 1pt)
    InventStats stats[2];            // invent (取り札) の保有状態
    CardMoveSet moves;               // 移動アニメーション
    CardZanzoSet zanzo;              // 残像アニメーション
    CardSet player_gekiatsu;         // プレイヤーにとっての激アツカードリスト
    int32_t move_count;              // 移動アニメーションのアクティブ数
    int32_t cursor;                  // カーソル
    int32_t total_score[2];          // 前ラウンドからの累計スコア
    int32_t koikoi[2];               // こいこい実行フラグ
    int32_t koikoi_score[2];         // こいこい実行時スコア
    int32_t round_max;               // ラウンド上限 (公式準拠: 12, ハーフ: 6, ショート: 2)
    int32_t round;                   // ラウンド番号
    int32_t round_score[2][12];      // 各ラウンドのスコア
    int32_t oya;                     // 親 (0: Playerが親, 1: CPUが親)
    int32_t current_player;          // 誰が操作中か (0: Player, 1: CPU)
    int32_t online_mode;             // オンラインモード
    int32_t online_log_need_commit;  // オンラインログの書き込み要否
    int32_t online_error;            // オンラインエラー
    int32_t ignore_online_error;     // オンラインエラーチェックの抑止
    int32_t game_mode;               // ゲームモード
    int32_t player_own_select;       // プレイヤーが手札を選択する場面で ON
    uint32_t bg_color;               // 背景色
    DotTable dots;                   // ドット・エフェクト
    SeasonTable season;              // 季節エフェクト
    int32_t own_move_flag[2];        // 手札移動フラグ
    int32_t own_move_x[2];           // 手札移動座標
    uint32_t com_wait_frames;        // 通信待ちフレーム数
    int timer_enabled;               // タイマー有効
    uint32_t timer_left;             // 残り時間
    int com_wait_msg;                // 通信待ちメッセージ表示フラグ
    int avatar[2];                   // アバター読み込み完了フラグ
    uint32_t avatar_buf[2][32 * 32]; // アバター読み込みバッファ
    ResultRecord round_result[12];   // ラウンド・リザルト
    int ai_model[2];                 // AI モデル
    int ai_thinking;                 // AI思考フレーム数
    int ai_thinking_render;          // thinking... 描画フラグ
    int ai_think_end;                // AI思考終了フラグ
    int auto_play;                   // Player操作をAIに委譲する (debug)
    int open_mode;                   // 相手の手札をオープン状態にする (debug)
    int no_sake;                     // 盃OFFモード
    StrategyData strategy[2];        // AI思考結果（前回）
} GlobalVariables;

extern GlobalVariables g;
extern int g_no_sake_latched;
extern int card_target_deck_map[48];

#define ONLINE_RESULT_DRAW 1
#define ONLINE_RESULT_WIN 2
#define ONLINE_RESULT_LOSE 3
#define ONLINE_RESULT_DISCONNECTED 4

typedef struct {
    uint8_t id[8];       // User ID (raw)
    uint8_t name[20];    // ユーザ名 (ASCII以外?) の先頭19文字 + \0
    uint16_t year;       // 年
    uint8_t month;       // 月 (1~12)
    uint8_t mday;        // 日 (1~31)
    uint8_t hour;        // 時 (0~23)
    uint8_t minute;      // 分 (0~59)
    uint8_t second;      // 秒 (0~59)
    uint8_t result;      // 0: 未commit, 1: Draw, 2: Win, 3: Lose, 4: 相手が切断
    uint8_t reserved[4]; // 予約領域
} OnlineRecord;

void system_vsync(void);
void vsync(void);
void reset_all_sprites(void);
void set_fade(uint8_t progress);
void set_flash(uint8_t progress);
int draw_fade2(int progress);
int title(void);
void game(int round_max);
void init_random_seed(void);
int analyze_invent(int p);
int analyze_score_gain_with_card_ids(int p, const int* card_ids, int count, int* out_best_wid);
void player_turn(void);
void enemy_turn(void);
int show_result(int currentPlayer);
void show_game_result(void);
void savedata_load(void);
ResultRecord* savedata_add_round_result(int player, int round_score);
void savedata_game_result(int result);
int savedata_get_window_mode();
int savedata_get_bgm_volume();
int savedata_get_sfx_volume();
int savedata_get_anime_speed();
void savedata_set_config(int window_mode, int bgm_volume, int sfx_volume, int anime_speed);
void savedata_set_cleared(void);
int savedata_get_cleared(int game_mode);
void savedata_online_set(uint8_t* id, const char* name);
void savedata_online_commit(uint8_t result);
OnlineRecord* savedata_online_get(int index);
// const char* savedata_room_id_join_get(void);
// void savedata_room_id_join_set(const char* roomId);
const char* savedata_room_id_create_get(void);
void savedata_room_id_create_set(const char* roomId);
int savedata_online_understand_get(void);
void savedata_online_understand(int on);
void show_winning_hands(void);
int center_print(int bg, int y, const char* text);
int center_print_bx(int bg, int y, const char* text, int bx);
int center_print_y(int bg, int y, const char* text);
int center_print_y_bx(int bg, int y, const char* text, int bx);
int is_gekiatsu_card(int p, Card* card);
int is_exsit_same_month_in_deck(Card* card);
void dot_add(int x, int y, int color);
void dot_add_range(int x, int y, int color, int w, int h, int n);
void dot_draw(void);
void season_add(int month, int x, int y);
void season_draw(void);
void config_init(void);
void config(void (*frame_proc)(void));
int select_game_mode(void (*frame_proc)(void));
void draw_select_game_mode(int bx);
void draw_select_online(int bx);
int select_game_mode_online(void (*frame_proc)(void));
void draw_private_create(int bx, int by);
int private_create(void (*frame_proc)(void));
void draw_private_create_mode(int bx);
int private_create_mode(void (*frame_proc)(void));
void draw_private_join(int bx, int by);
int private_join(void (*frame_proc)(void));
int start_matching(void (*frame_proc)(void), int lobbyType, const char* roomId, int game_mode);
void draw_wait_opponent_dialog(int bx);
void draw_create_private_room_dialog(int bx);
void online_matching_start(int lobbyFilter, int lobbyType, int game_mode, const char* roomId);
uint32_t online_matching_result(void);
uint32_t online_game_mode(void);
void online_deck_send(CardSet* deck);
int online_deck_recv(CardSet* deck);
int online_check(void);
void online_disconnect(void);
void online_card_send(Card* own, Card* deck);
void online_card_send2(Card* deck);
Card* online_card_recv(int* error);
void online_turnend_send(int type);
int online_sync_turnend(int allowCnt, int allowKoi, int allowAga);
void online_fin_send(void);
int online_fin_recv(void);
const char* online_get_lobby_id();
int online_join_lobby(const char* lobbyIdBase36);
uint8_t* online_opponent_steam_id(void);
const char* online_opponent_steam_name(void);
const char* online_error_reason(void);
void online_ql_check_request(int lobbyFilter, int lobbyType, int game_mode, const char* roomId);
uint32_t online_ql_check_result(void);
void online_ql_check_cancel(void);
void online_fl_check_request(int lobbyFilter, int lobbyType, int game_mode, const char* roomId);
uint32_t online_fl_check_result(void);
void online_fl_check_cancel(void);
void open_steam_profile(uint8_t* steamId);
void avatar_show(void);
void draw_playlog(int bx);
void playlog_show(void (*frame_proc)(void));
void timer_start(uint32_t time_left, int right, int bottom);
void timer_stop(void);
void timer_tick(void);
int timer_is_over(void);
int online_notice(void (*frame_proc)(void), int force);
void trophy_unlock(uint32_t id);
int session_online_get(void);
void session_online_set(int online_mode);
int session_local_game_mode_get(void);
void session_local_game_mode_set(int local_game_mode);
int session_online_game_mode_get(void);
void session_online_game_mode_set(int online_game_mode);
int session_private_round_get(void);
void session_private_round_set(int private_round);
void window(int bg, int x, int y, int width, int height);
void leaderboard_render(int bx);
void leaderboard(void (*frame_proc)(void));
void leaderboard_send(void);
const char* key_name_lower(int id);
void ecards_clear(void);
void ecards_frame(void);
void ecards_alpha(int alpha);
void result_log_render(int line, int bx);
void result_log_show(void (*frame_proc)(void));
void hibana_clear(int sprite_base);
void hibana_rect_clear(void);
void hibana_rect_add(int x, int y, int width, int height);
void hibana_frame(int interval, int limit);
int push_any(void);
int push_a(void);
int push_b(void);
int push_x(void);
int push_y(void);
int push_up(void);
int push_down(void);
int push_left(void);
int push_right(void);
int push_start(void);
int pushing_ff(void);
int mouse_shown(void);
int mouse_in_rect(int x, int y, int width, int height);
int position_in_rect(int pos_x, int pos_y, int x, int y, int width, int height);
