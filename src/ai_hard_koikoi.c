#include "ai.h"
// KOIKOI 判断ログを出して continue/stop の理由を追えるようにする。

// live out を強めに見る最低得点ライン。
#define LIVE_OUT_SCORE_THRESHOLD 5
// GREEDY で KOIKOI を許す最低到達率。
#define KOIKOI_GREEDY_MIN_REACH 30
// GREEDY で KOIKOI を許す最長 delay。
#define KOIKOI_GREEDY_MAX_DELAY 2
// DEFENSIVE で stop 側へ寄せる基礎ボーナス。
#define KOIKOI_DEFENSIVE_STOP_BONUS 220
// BALANCED で相手脅威を continue 評価へ割り引いて反映する係数。
#define KOIKOI_BALANCED_THREAT_DIVISOR 2
// 小得点 KOIKOI とみなす base の上限。
#define KOIKOI_SMALL_BASE_MAX 2
// 小得点 KOIKOI を止める相手 7+ 脅威の閾値。
#define KOIKOI_SMALL_BASE_THREAT_THRESHOLD 60
// 小得点 KOIKOI を止める相手最短危険 delay の閾値。
#define KOIKOI_SMALL_BASE_RISK_DELAY_MAX 2
// 小得点 KOIKOI を止める相手危険役 score の閾値。
#define KOIKOI_SMALL_BASE_RISK_SCORE_MIN 5
// 小得点 KOIKOI を原則 stop に倒す強い補正。
#define KOIKOI_SMALL_BASE_STOP_BONUS 320
// リード時の小得点 KOIKOI をさらに止めやすくする補正。
#define KOIKOI_SMALL_BASE_LEAD_STOP_BONUS 220
// 7+ 狙い KOIKOI を許す最低到達率。
#define KOIKOI_SEVEN_PLUS_MIN_REACH 15
// 7+ 狙い KOIKOI を許す最長 delay。
#define KOIKOI_SEVEN_PLUS_MAX_DELAY 3
// 相手に勝てば x2 窓を渡す continue への減点。
#define KOIKOI_CONTINUE_RISK_X2_PENALTY 220
// 相手 7+ 脅威が高い時の continue 追加減点。
#define KOIKOI_CONTINUE_RISK_HIGH_THREAT_PENALTY 320
// 相手 7+ 脅威が中程度の時の continue 追加減点。
#define KOIKOI_CONTINUE_RISK_MID_THREAT_PENALTY 180
// 相手の危険役が近い時の continue 追加減点。
#define KOIKOI_CONTINUE_RISK_NEAR_PENALTY 260
// 小得点 KOIKOI そのものへ掛ける事故防止減点。
#define KOIKOI_CONTINUE_RISK_SMALL_BASE_PENALTY 220
// 危険 KOIKOI とみなす base の下限。
#define KOIKOI_DANGER_BASE_MIN 5
// 危険 KOIKOI を止める相手 7+ 脅威の閾値。
#define KOIKOI_DANGER_THREAT_THRESHOLD 60
// 危険 KOIKOI を通すのに必要な continue 側の余剰マージン。
#define KOIKOI_DANGER_WIN_MARGIN 650
// base=5 で live-out が薄い時に stop へ寄せる補正。
#define KOIKOI_BASE5_LOW_LIVE_OUT_STOP_BONUS 260
// base=6 で +1 が見えている時に continue へ寄せる補正。
#define KOIKOI_BASE6_LIVE_OUT_CONTINUE_BONUS 220
// 7+ 後は原則 stop に寄せる補正。
#define KOIKOI_SEVEN_PLUS_STOP_BONUS 270
// visible な 7+ 脅威が近い時は stop を強く押す。
#define KOIKOI_VISIBLE_SEVEN_THREAT_STOP_BONUS 3000
// 上記専用 stop 条件で使う exact 7+ threat 閾値。
#define KOIKOI_VISIBLE_SEVEN_THREAT_EXACT_MIN 25
// 上記専用 stop 条件で使う coarse threat 閾値。
#define KOIKOI_VISIBLE_SEVEN_THREAT_COARSE_MIN 70
// 上記専用 stop 条件で使う近接危険 delay 閾値。
#define KOIKOI_VISIBLE_SEVEN_THREAT_RISK_DELAY_MAX 2
// 上記専用 stop 条件で使う危険役 score 閾値。
#define KOIKOI_VISIBLE_SEVEN_THREAT_RISK_SCORE_MIN 7
// base=6 から +1 で 7 に届く live-out を強い push とみなす最低 reach。
#define KOIKOI_BASE6_SEVEN_PUSH_MIN_REACH 15
// base=6 から +1 で 7 に届く live-out を強い push とみなす最長 delay。
#define KOIKOI_BASE6_SEVEN_PUSH_MAX_DELAY 3
// base=6 から +1 が太い時の追加 continue ボーナス。
#define KOIKOI_BASE6_SEVEN_PUSH_BONUS 320
// base=6 から +1 が太い時に stop を少し削る補正。
#define KOIKOI_BASE6_SEVEN_PUSH_STOP_PENALTY 240
// 点差が拮抗し相手打点が低い中盤では、Hard の stop バイアスを少し緩める。
#define KOIKOI_CLOSE_MATCH_ATTACK_STOP_PENALTY 240
// 「多く残っている」とみなす残りラウンド下限。
#define KOIKOI_CLOSE_MATCH_LEFT_ROUNDS_MIN 3
// 「相手打点が低い」とみなす危険役 score 上限。
#define KOIKOI_CLOSE_MATCH_LOW_RISK_SCORE_MAX 4
// 終盤リード時の base=5 KOIKOI を止める専用補正。
#define KOIKOI_BASE5_ENDGAME_LEAD_STOP_BONUS 420
// Round 10 型の専用 stop 条件に使う相手脅威閾値。
#define KOIKOI_BASE5_ENDGAME_LEAD_THREAT_THRESHOLD 80
// Round 10 型の専用 stop 条件に使う近接危険 delay 閾値。
#define KOIKOI_BASE5_ENDGAME_LEAD_RISK_DELAY_MAX 2
// Round 10 型の専用 stop 条件に使う危険役 score 閾値。
#define KOIKOI_BASE5_ENDGAME_LEAD_RISK_SCORE_MIN 5
// base=5 で手札実態が薄いとみなす残り手札上限。
#define KOIKOI_BASE5_THIN_HAND_LEFT_OWN_MAX 4
// base=5 で手札実態が薄いとみなす最短 7+ delay 下限。
#define KOIKOI_BASE5_THIN_HAND_SEVEN_DELAY_MIN 4
// base=5 で手札実態が薄いとみなす 7+ 到達率上限。
#define KOIKOI_BASE5_THIN_HAND_SEVEN_REACH_MAX 25
// base=5 で手札実態が薄い時に stop へ倒す補正。
#define KOIKOI_BASE5_THIN_HAND_STOP_BONUS 420
// 遅延戦略を終えて終盤へ入った 5pt は stop を強める。
#define KOIKOI_BASE5_POST_DELAY_STOP_BONUS 2600
// 「遅延戦略を終えた」とみなす残り手札上限。
#define KOIKOI_BASE5_POST_DELAY_LEFT_OWN_MAX 2
// 上記専用 stop 条件で使う相手脅威閾値。
#define KOIKOI_BASE5_POST_DELAY_THREAT_MIN 50
// 上記専用 stop 条件で使う近接危険 delay 閾値。
#define KOIKOI_BASE5_POST_DELAY_RISK_DELAY_MAX 2
// 上記専用 stop 条件で使う危険役 score 閾値。
#define KOIKOI_BASE5_POST_DELAY_RISK_SCORE_MIN 5
// 上記専用 stop 条件で使う 7+ 到達率上限。
#define KOIKOI_BASE5_POST_DELAY_SEVEN_REACH_MAX 25
// base=5 の確定点を優先する近接高打点脅威の閾値。
#define KOIKOI_BASE5_NEAR_HIGH_RISK_THREAT_MIN 80
// base=5 の確定点を優先する近接高打点脅威の最短 delay。
#define KOIKOI_BASE5_NEAR_HIGH_RISK_DELAY_MAX 2
// base=5 の確定点を優先する近接高打点脅威の最低 risk score。
#define KOIKOI_BASE5_NEAR_HIGH_RISK_SCORE_MIN 5
// base=5 の確定点を優先する際、自分の 1pt 伸び筋が薄いとみなす到達率上限。
#define KOIKOI_BASE5_NEAR_HIGH_RISK_LIVE_OUT_REACH_MAX 25
// base=5 の確定点を優先する際、自分の 1pt 伸び筋が遅いとみなす最短 delay 下限。
#define KOIKOI_BASE5_NEAR_HIGH_RISK_LIVE_OUT_DELAY_MIN 4
// base=5 の近接高打点脅威 stop を許す最大ビハインド。
#define KOIKOI_BASE5_NEAR_HIGH_RISK_DIFF_MIN -10
// base=5・大差ビハインドでも、終盤で伸び筋が細い時は確定点を優先する。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_STOP_BONUS 520
// 上記専用 stop 条件で使う最大ビハインド閾値。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_DIFF_MAX -15
// 上記専用 stop 条件で使う残りラウンド閾値。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_LEFT_ROUNDS_MAX 5
// 上記専用 stop 条件で使う残り手札閾値。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_LEFT_OWN_MAX 2
// 上記専用 stop 条件で使う相手脅威閾値。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_THREAT_MIN 50
// 上記専用 stop 条件で使う live-out 到達率上限。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_LIVE_OUT_REACH_MAX 25
// 上記専用 stop 条件で使う live-out 最長 delay。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_LIVE_OUT_DELAY_MAX 2
// 上記専用 stop 条件で使う self/7+ 到達率上限。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_SELF_REACH_MAX 15
// 上記専用 stop 条件で使う危険役 score 上限。
#define KOIKOI_BASE5_BEHIND_THIN_PUSH_RISK_SCORE_MAX 2
// 酒役の片割れが相手に見えている時、follow-up 劣勢とみなす差分閾値。
#define KOIKOI_BASE5_BLOCKED_SAKE_FOLLOWUP_MARGIN 140
// 上記専用 stop 条件で使う残り手札上限。
#define KOIKOI_BASE5_BLOCKED_SAKE_LEFT_OWN_MAX 5
// 上記専用 stop 条件で使う live-out 到達率上限。
#define KOIKOI_BASE5_BLOCKED_SAKE_LIVE_OUT_REACH_MAX 30
// 上記専用 stop 条件で使う live-out 最短 delay 下限。
#define KOIKOI_BASE5_BLOCKED_SAKE_LIVE_OUT_DELAY_MIN 4
static int strategy_overpay_possible(const StrategyData* s);
static int count_known_month_cards_for_player(int player, int month);
static int count_owned_month_cards(int player, int month);
static int is_month_locked_for_player(int player, int month);
static int has_floor_same_month_card(int month);
static int month_lock_known_target(int month);
static int month_lock_hidden_floor_target(int month);
static int is_secured_threshold_followup_card(int player, Card* card);
static int secured_followup_single_card_gain(int player, Card* card, int* out_best_wid);
static int estimate_secured_followup_gain(int player, int* out_ready_cards, int* out_best_high_value_gain);
static int estimate_visible_threshold_followup_gain(int player, int* out_ready_cards, int* out_best_high_value_gain);
static int has_current_winning_hand(int player, int wid);
static int player_has_captured_month_type(int player, int month, int type);
static int should_stop_small_base_last_card_koikoi(const StrategyData* s, int round_score, int live_out_delay, int best_seven_plus_delay, int best_seven_plus_reach,
                                                   int visible_threshold_followup_gain, int secured_followup_cards, int visible_threshold_high_gain,
                                                   int secured_high_value_followup_gain);

static int should_loosen_close_match_koikoi(const StrategyData* s, int max_risk_score)
{
    if (!s) {
        return OFF;
    }
    if (s->match_score_diff <= -15 || s->match_score_diff > 15) {
        return OFF;
    }
    if (max_risk_score > KOIKOI_CLOSE_MATCH_LOW_RISK_SCORE_MAX) {
        return OFF;
    }
    if (s->left_rounds < KOIKOI_CLOSE_MATCH_LEFT_ROUNDS_MIN) {
        return OFF;
    }
    if (s->left_own < 3) {
        return OFF;
    }
    if (s->koikoi_opp == ON || s->opponent_win_x2 == ON) {
        return OFF;
    }
    return ON;
}

static int should_stop_base5_endgame_lead_koikoi(const StrategyData* s, int round_score, int min_risk_delay, int max_risk_score, int best_seven_plus_delay)
{
    if (!s) {
        return OFF;
    }
    if (round_score != 5) {
        return OFF;
    }
    if (s->match_score_diff < 12) {
        return OFF;
    }
    if (s->left_rounds > 3) {
        return OFF;
    }
    if (s->opp_coarse_threat < KOIKOI_BASE5_ENDGAME_LEAD_THREAT_THRESHOLD) {
        return OFF;
    }
    if (min_risk_delay > KOIKOI_BASE5_ENDGAME_LEAD_RISK_DELAY_MAX || max_risk_score < KOIKOI_BASE5_ENDGAME_LEAD_RISK_SCORE_MIN) {
        return OFF;
    }
    if (best_seven_plus_delay > 1) {
        return OFF;
    }
    return ON;
}

static int should_stop_base5_thin_hand_koikoi(const StrategyData* s, int round_score, int live_out_est, int live_out_delay, int best_self_delay, int best_seven_plus_delay,
                                              int best_seven_plus_reach, int min_risk_delay, int max_risk_score)
{
    if (!s) {
        return OFF;
    }
    if (round_score != 5) {
        return OFF;
    }
    if (s->left_own > KOIKOI_BASE5_THIN_HAND_LEFT_OWN_MAX) {
        return OFF;
    }
    if (s->match_score_diff < 0 || s->match_score_diff > 10) {
        return OFF;
    }
    if (live_out_est > 0 || live_out_delay < 99) {
        return OFF;
    }
    if (best_self_delay < KOIKOI_BASE5_THIN_HAND_SEVEN_DELAY_MIN) {
        return OFF;
    }
    if (best_seven_plus_delay < KOIKOI_BASE5_THIN_HAND_SEVEN_DELAY_MIN) {
        return OFF;
    }
    if (best_seven_plus_reach > KOIKOI_BASE5_THIN_HAND_SEVEN_REACH_MAX) {
        return OFF;
    }
    if (s->opp_coarse_threat > 60 || max_risk_score > 4) {
        return OFF;
    }
    if (min_risk_delay <= 1) {
        return OFF;
    }
    return ON;
}

static int should_apply_base5_post_delay_stop_bonus(const StrategyData* s, int round_score, int best_self_delay, int best_self_reach, int best_seven_plus_delay,
                                                    int best_seven_plus_reach, int min_risk_delay, int max_risk_score)
{
    int near_danger;

    if (!s) {
        return OFF;
    }
    if (round_score != 5) {
        return OFF;
    }
    if (s->left_own > KOIKOI_BASE5_POST_DELAY_LEFT_OWN_MAX) {
        return OFF;
    }
    if (best_self_delay <= 1 && best_self_reach >= 30) {
        return OFF;
    }
    if (best_seven_plus_delay <= 1 && best_seven_plus_reach >= 30) {
        return OFF;
    }
    if (best_seven_plus_reach > KOIKOI_BASE5_POST_DELAY_SEVEN_REACH_MAX) {
        return OFF;
    }

    near_danger = (s->opp_coarse_threat >= KOIKOI_BASE5_POST_DELAY_THREAT_MIN) ? ON : OFF;
    if (min_risk_delay <= KOIKOI_BASE5_POST_DELAY_RISK_DELAY_MAX && max_risk_score >= KOIKOI_BASE5_POST_DELAY_RISK_SCORE_MIN) {
        near_danger = ON;
    }

    return near_danger;
}

static int should_stop_base5_behind_thin_push_koikoi(const StrategyData* s, int round_score, int live_out_est, int live_out_delay, int best_self_reach,
                                                     int best_seven_plus_reach, int min_risk_delay, int max_risk_score)
{
    if (!s) {
        return OFF;
    }
    if (round_score != 5) {
        return OFF;
    }
    if (s->match_score_diff > KOIKOI_BASE5_BEHIND_THIN_PUSH_DIFF_MAX) {
        return OFF;
    }
    if (s->left_rounds > KOIKOI_BASE5_BEHIND_THIN_PUSH_LEFT_ROUNDS_MAX) {
        return OFF;
    }
    if (s->left_own > KOIKOI_BASE5_BEHIND_THIN_PUSH_LEFT_OWN_MAX) {
        return OFF;
    }
    if (s->opp_coarse_threat < KOIKOI_BASE5_BEHIND_THIN_PUSH_THREAT_MIN) {
        return OFF;
    }
    if (s->opponent_win_x2 == ON || s->koikoi_opp == ON) {
        return OFF;
    }
    if (live_out_est > KOIKOI_BASE5_BEHIND_THIN_PUSH_LIVE_OUT_REACH_MAX || live_out_delay > KOIKOI_BASE5_BEHIND_THIN_PUSH_LIVE_OUT_DELAY_MAX) {
        return OFF;
    }
    if (best_self_reach > KOIKOI_BASE5_BEHIND_THIN_PUSH_SELF_REACH_MAX || best_seven_plus_reach > KOIKOI_BASE5_BEHIND_THIN_PUSH_SELF_REACH_MAX) {
        return OFF;
    }
    if (min_risk_delay < 2 || max_risk_score > KOIKOI_BASE5_BEHIND_THIN_PUSH_RISK_SCORE_MAX) {
        return OFF;
    }
    return ON;
}

static int should_stop_base5_near_high_risk_koikoi(const StrategyData* s, int round_score, int live_out_est, int live_out_delay, int min_risk_delay,
                                                   int max_risk_score, int visible_threshold_high_gain, int secured_high_value_followup_gain)
{
    int best_visible_high_gain = visible_threshold_high_gain;

    if (!s) {
        return OFF;
    }
    if (round_score != 5) {
        return OFF;
    }
    if (s->match_score_diff < KOIKOI_BASE5_NEAR_HIGH_RISK_DIFF_MIN) {
        return OFF;
    }
    if (s->opp_coarse_threat < KOIKOI_BASE5_NEAR_HIGH_RISK_THREAT_MIN) {
        return OFF;
    }
    if (min_risk_delay > KOIKOI_BASE5_NEAR_HIGH_RISK_DELAY_MAX || max_risk_score < KOIKOI_BASE5_NEAR_HIGH_RISK_SCORE_MIN) {
        return OFF;
    }
    if (live_out_est > KOIKOI_BASE5_NEAR_HIGH_RISK_LIVE_OUT_REACH_MAX || live_out_delay < KOIKOI_BASE5_NEAR_HIGH_RISK_LIVE_OUT_DELAY_MIN) {
        return OFF;
    }
    if (secured_high_value_followup_gain > best_visible_high_gain) {
        best_visible_high_gain = secured_high_value_followup_gain;
    }
    if (best_visible_high_gain >= 5) {
        return OFF;
    }
    return ON;
}

static int should_stop_small_base_last_card_koikoi(const StrategyData* s, int round_score, int live_out_delay, int best_seven_plus_delay, int best_seven_plus_reach,
                                                   int visible_threshold_followup_gain, int secured_followup_cards, int visible_threshold_high_gain,
                                                   int secured_high_value_followup_gain)
{
    int best_high_gain = visible_threshold_high_gain;

    if (!s) {
        return OFF;
    }
    if (round_score > KOIKOI_SMALL_BASE_MAX) {
        return OFF;
    }
    if (s->left_own > 1) {
        return OFF;
    }
    if (best_seven_plus_delay <= KOIKOI_SEVEN_PLUS_MAX_DELAY && best_seven_plus_reach >= KOIKOI_SEVEN_PLUS_MIN_REACH) {
        return OFF;
    }
    if (secured_followup_cards >= 2) {
        return OFF;
    }
    if (visible_threshold_followup_gain >= 2) {
        return OFF;
    }
    if (secured_high_value_followup_gain > best_high_gain) {
        best_high_gain = secured_high_value_followup_gain;
    }
    if (best_high_gain >= 2) {
        return OFF;
    }
    if (live_out_delay > 1) {
        return OFF;
    }
    return ON;
}

static int calc_followup_pressure_value(const int* reach, const int* delay)
{
    static const int target_wids[] = {WID_ISC, WID_AKATAN, WID_AOTAN, WID_TANE, WID_TAN, WID_KASU};
    int top_values[3] = {0, 0, 0};

    if (!reach || !delay) {
        return 0;
    }

    for (int i = 0; i < (int)(sizeof(target_wids) / sizeof(target_wids[0])); i++) {
        int wid = target_wids[i];
        int base = winning_hands[wid].base_score;
        int value;

        if (reach[wid] <= 0 || delay[wid] >= 99) {
            continue;
        }

        value = (reach[wid] * base * 10) / (delay[wid] + 1);
        if (wid == WID_ISC || wid == WID_AKATAN || wid == WID_AOTAN) {
            value += 20;
        }
        if (value > top_values[0]) {
            top_values[2] = top_values[1];
            top_values[1] = top_values[0];
            top_values[0] = value;
        } else if (value > top_values[1]) {
            top_values[2] = top_values[1];
            top_values[1] = value;
        } else if (value > top_values[2]) {
            top_values[2] = value;
        }
    }

    return top_values[0] + top_values[1] + top_values[2];
}

static int find_blocked_sake_wid(int player)
{
    int opp = 1 - player;

    if (player < 0 || player > 1) {
        return -1;
    }
    if (has_current_winning_hand(player, WID_HANAMI) && player_has_captured_month_type(opp, 7, CARD_TYPE_GOKOU)) {
        return WID_HANAMI;
    }
    if (has_current_winning_hand(player, WID_TSUKIMI) && player_has_captured_month_type(opp, 2, CARD_TYPE_GOKOU)) {
        return WID_TSUKIMI;
    }
    return -1;
}

static int should_stop_base5_blocked_sake_koikoi(int player, const StrategyData* s, int round_score, int live_out_est, int live_out_delay,
                                                 int best_seven_plus_delay, int best_seven_plus_reach)
{
    int self_followup_pressure;
    int opp_followup_pressure;

    if (player < 0 || player > 1 || !s) {
        return OFF;
    }
    if (round_score != 5) {
        return OFF;
    }
    if (find_blocked_sake_wid(player) < 0) {
        return OFF;
    }
    if (s->left_own > KOIKOI_BASE5_BLOCKED_SAKE_LEFT_OWN_MAX) {
        return OFF;
    }
    if (s->match_score_diff <= -35) {
        return OFF;
    }
    if (live_out_est > KOIKOI_BASE5_BLOCKED_SAKE_LIVE_OUT_REACH_MAX || live_out_delay < KOIKOI_BASE5_BLOCKED_SAKE_LIVE_OUT_DELAY_MIN) {
        return OFF;
    }
    if (best_seven_plus_delay <= 3 && best_seven_plus_reach >= KOIKOI_SEVEN_PLUS_MIN_REACH) {
        return OFF;
    }

    self_followup_pressure = calc_followup_pressure_value(s->reach, s->delay);
    opp_followup_pressure = calc_followup_pressure_value(s->risk_reach_estimate, s->risk_delay);
    return (opp_followup_pressure >= self_followup_pressure + KOIKOI_BASE5_BLOCKED_SAKE_FOLLOWUP_MARGIN) ? ON : OFF;
}

static int should_apply_visible_seven_threat_stop_bonus(const StrategyData* s, int round_score, int best_self_delay, int best_self_reach, int best_seven_plus_delay,
                                                        int best_seven_plus_reach, int min_risk_delay, int max_risk_score, int base6_seven_push)
{
    int self_can_race;

    if (!s) {
        return OFF;
    }
    if (round_score >= 7) {
        return OFF;
    }
    if (s->opp_exact_7plus_threat < KOIKOI_VISIBLE_SEVEN_THREAT_EXACT_MIN) {
        return OFF;
    }
    if (min_risk_delay > KOIKOI_VISIBLE_SEVEN_THREAT_RISK_DELAY_MAX) {
        return OFF;
    }
    if (max_risk_score < KOIKOI_VISIBLE_SEVEN_THREAT_RISK_SCORE_MIN) {
        return OFF;
    }
    if (s->opp_coarse_threat < KOIKOI_VISIBLE_SEVEN_THREAT_COARSE_MIN) {
        return OFF;
    }

    self_can_race = (best_self_delay <= 1 && best_self_reach >= 35) ? ON : OFF;
    if (best_seven_plus_delay <= 2 && best_seven_plus_reach >= 35) {
        self_can_race = ON;
    }
    if (round_score == 6 && base6_seven_push) {
        self_can_race = ON;
    }

    return self_can_race ? OFF : ON;
}

static const char* strategy_mode_name(int mode)
{
    switch (mode) {
        case MODE_GREEDY: return "GREEDY";
        case MODE_DEFENSIVE: return "DEFENSIVE";
        default: return "BALANCED";
    }
}

static int has_realistic_seven_plus_line(int round_score, int best_delay, int best_reach)
{
    if (round_score >= 7) {
        return ON;
    }
    return (best_reach >= KOIKOI_SEVEN_PLUS_MIN_REACH && best_delay <= KOIKOI_SEVEN_PLUS_MAX_DELAY) ? ON : OFF;
}

static int count_known_month_cards_for_player(int player, int month)
{
    int count = 0;

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 4; t++) {
            CardSet* inv = &g.invent[p][t];
            for (int i = 0; i < inv->num; i++) {
                Card* card = inv->cards[i];
                if (card && card->month == month) {
                    count++;
                }
            }
        }
    }
    for (int i = 0; i < 48; i++) {
        Card* card = g.deck.cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    return count;
}

static int count_owned_month_cards(int player, int month)
{
    int count = 0;

    if (player < 0 || player > 1 || month < 0 || month >= 12) {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    for (int t = 0; t < 4; t++) {
        CardSet* inv = &g.invent[player][t];
        for (int i = 0; i < inv->num; i++) {
            Card* card = inv->cards[i];
            if (card && card->month == month) {
                count++;
            }
        }
    }
    return count;
}

static int is_month_locked_for_player(int player, int month)
{
    return (count_known_month_cards_for_player(player, month) == month_lock_known_target(month) && count_owned_month_cards(player, month) >= 2) ? ON : OFF;
}

static int month_lock_known_target(int month)
{
    (void)month;
    return 3;
}

static int month_lock_hidden_floor_target(int month)
{
    (void)month;
    return 2;
}

static int count_floor_month_cards(int month)
{
    int count = 0;

    if (month < 0 || month >= 12) {
        return 0;
    }
    for (int i = 0; i < 48; i++) {
        Card* card = g.deck.cards[i];
        if (card && card->month == month) {
            count++;
        }
    }
    return count;
}

static int player_has_hand_card(int player, int card_no)
{
    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->id == card_no) {
            return ON;
        }
    }
    return OFF;
}

static int is_hidden_month_lock_key_card_for_player(int player, int card_no)
{
    int month;

    if (player < 0 || player > 1 || card_no < 0 || card_no >= 48) {
        return OFF;
    }
    if (!player_has_hand_card(player, card_no)) {
        return OFF;
    }
    month = g.cards[card_no].month;
    if (count_known_month_cards_for_player(player, month) != month_lock_known_target(month)) {
        return OFF;
    }
    if (count_owned_month_cards(player, month) != 1) {
        return OFF;
    }
    if (count_floor_month_cards(month) != month_lock_hidden_floor_target(month)) {
        return OFF;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (ai_is_card_critical_for_wid(card_no, wid)) {
            return ON;
        }
    }
    return OFF;
}

static int has_floor_same_month_card(int month)
{
    if (month < 0 || month >= 12) {
        return OFF;
    }
    for (int i = 0; i < 48; i++) {
        Card* card = g.deck.cards[i];
        if (card && card->month == month) {
            return ON;
        }
    }
    return OFF;
}

static int is_secured_threshold_followup_card(int player, Card* card)
{
    if (player < 0 || player > 1 || !card) {
        return OFF;
    }
    if (card->type != CARD_TYPE_KASU && card->type != CARD_TYPE_TAN && card->type != CARD_TYPE_TANE) {
        return OFF;
    }
    if (has_floor_same_month_card(card->month)) {
        return ON;
    }
    return is_month_locked_for_player(player, card->month) || is_hidden_month_lock_key_card_for_player(player, card->id);
}

static int secured_followup_single_card_gain(int player, Card* card, int* out_best_wid)
{
    int card_id;

    if (out_best_wid) {
        *out_best_wid = -1;
    }
    if (player < 0 || player > 1 || !card) {
        return 0;
    }
    card_id = card->id;
    return analyze_score_gain_with_card_ids(player, &card_id, 1, out_best_wid);
}

static int estimate_secured_followup_gain(int player, int* out_ready_cards, int* out_best_high_value_gain)
{
    int card_ids[8];
    int card_count = 0;

    if (out_ready_cards) {
        *out_ready_cards = 0;
    }
    if (out_best_high_value_gain) {
        *out_best_high_value_gain = 0;
    }
    if (player < 0 || player > 1) {
        return 0;
    }

    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (!card) {
            continue;
        }
        if (!is_secured_threshold_followup_card(player, card)) {
            continue;
        }
        card_ids[card_count++] = card->id;
        if (out_best_high_value_gain) {
            int best_wid = -1;
            int gain = secured_followup_single_card_gain(player, card, &best_wid);
            if ((best_wid == WID_AKATAN || best_wid == WID_AOTAN || best_wid == WID_ISC) && *out_best_high_value_gain < gain) {
                *out_best_high_value_gain = gain;
            }
        }
    }

    if (out_ready_cards) {
        *out_ready_cards = card_count;
    }
    if (card_count <= 0) {
        return 0;
    }
    return analyze_score_gain_with_card_ids(player, card_ids, card_count, NULL);
}

static int estimate_visible_threshold_followup_gain(int player, int* out_ready_cards, int* out_best_high_value_gain)
{
    int card_ids[8];
    int card_count = 0;

    if (out_ready_cards) {
        *out_ready_cards = 0;
    }
    if (out_best_high_value_gain) {
        *out_best_high_value_gain = 0;
    }
    if (player < 0 || player > 1) {
        return 0;
    }

    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (!card) {
            continue;
        }
        if (card->type != CARD_TYPE_KASU && card->type != CARD_TYPE_TAN && card->type != CARD_TYPE_TANE) {
            continue;
        }
        if (!has_floor_same_month_card(card->month)) {
            continue;
        }
        card_ids[card_count++] = card->id;
        if (out_best_high_value_gain) {
            int best_wid = -1;
            int gain = secured_followup_single_card_gain(player, card, &best_wid);
            if (*out_best_high_value_gain < gain) {
                *out_best_high_value_gain = gain;
            }
        }
    }

    if (out_ready_cards) {
        *out_ready_cards = card_count;
    }
    if (card_count <= 0) {
        return 0;
    }
    return analyze_score_gain_with_card_ids(player, card_ids, card_count, NULL);
}

static int strategy_overpay_possible(const StrategyData* s)
{
    if (!s) {
        return OFF;
    }
    return (s->can_overpay_akatan || s->can_overpay_aotan || s->can_overpay_ino) ? ON : OFF;
}

static int has_current_winning_hand(int player, int wid)
{
    if (player < 0 || player > 1 || wid < 0 || wid >= WINNING_HAND_MAX) {
        return OFF;
    }
    for (int i = 0; i < g.stats[player].wh_count; i++) {
        if (g.stats[player].wh[i] && g.stats[player].wh[i]->id == wid) {
            return ON;
        }
    }
    return OFF;
}

static int has_visible_light_out(int player)
{
    if (player < 0 || player > 1) {
        return OFF;
    }

    for (int i = 0; i < g.own[player].num; i++) {
        Card* card = g.own[player].cards[i];
        if (card && card->type == CARD_TYPE_GOKOU) {
            return ON;
        }
    }
    for (int i = 0; i < 48; i++) {
        Card* card = g.deck.cards[i];
        if (card && card->type == CARD_TYPE_GOKOU) {
            return ON;
        }
    }
    return OFF;
}

static const char* forced_big_hand_stop_reason(int player)
{
    if (player < 0 || player > 1) {
        return NULL;
    }
    if (has_current_winning_hand(player, WID_GOKOU)) {
        return "GOKOU";
    }
    if (has_current_winning_hand(player, WID_DBTAN)) {
        return "DBTAN";
    }
    if ((has_current_winning_hand(player, WID_SHIKOU) || has_current_winning_hand(player, WID_AME_SHIKOU)) && !has_visible_light_out(player)) {
        return has_current_winning_hand(player, WID_SHIKOU) ? "SHIKOU_NO_LAST_LIGHT" : "AME_SHIKOU_NO_LAST_LIGHT";
    }
    return NULL;
}

static int has_live_out_to_seven_push(const StrategyData* s, int round_score)
{
    if (!s || round_score != 6) {
        return OFF;
    }
    if (s->best_additional_1pt_reach >= KOIKOI_BASE6_SEVEN_PUSH_MIN_REACH && s->best_additional_1pt_delay <= KOIKOI_BASE6_SEVEN_PUSH_MAX_DELAY) {
        return ON;
    }
    if (s->best_additional_1pt_reach >= 35 && s->best_additional_1pt_delay <= 3) {
        return ON;
    }
    return OFF;
}

static int has_captured_sake_cup(int player)
{
    if (player < 0 || player > 1) {
        return OFF;
    }

    for (int i = 0; i < g.invent[player][CARD_TYPE_TANE].num; i++) {
        Card* card = g.invent[player][CARD_TYPE_TANE].cards[i];
        if (card && card->month == 8) {
            return ON;
        }
    }
    return OFF;
}

static int player_has_captured_month_type(int player, int month, int type)
{
    if (player < 0 || player > 1) {
        return OFF;
    }

    for (int i = 0; i < g.invent[player][type].num; i++) {
        Card* card = g.invent[player][type].cards[i];
        if (card && card->month == month) {
            return ON;
        }
    }
    return OFF;
}

static int count_captured_non_rain_lights(int player)
{
    int count = 0;

    if (player < 0 || player > 1) {
        return 0;
    }

    for (int i = 0; i < g.invent[player][CARD_TYPE_GOKOU].num; i++) {
        Card* card = g.invent[player][CARD_TYPE_GOKOU].cards[i];
        if (card && card->month != 10) {
            count++;
        }
    }
    return count;
}

static int has_same_month_hand_followup(int player, Card* target)
{
    if (player < 0 || player > 1) {
        return OFF;
    }

    for (int i = 0; i < g.own[player].num; i++) {
        Card* hand = g.own[player].cards[i];
        if (hand && hand->id != target->id && hand->month == target->month) {
            return ON;
        }
    }
    return OFF;
}

static const char* find_visible_immediate_five_point_followup(int player, const StrategyData* s)
{
    static const int red_months[] = {0, 1, 2};
    static const int blue_months[] = {5, 8, 9};

    if (player < 0 || player > 1 || !s) {
        return NULL;
    }

    for (int i = 0; i < 48; i++) {
        Card* card = g.deck.cards[i];
        if (!card || !has_same_month_hand_followup(player, card)) {
            continue;
        }

        if (card->type == CARD_TYPE_GOKOU && card->month == 2 && has_captured_sake_cup(player) && s->reach[WID_HANAMI] >= 20 && s->delay[WID_HANAMI] <= 2) {
            return "VISIBLE_HANAMI_FOLLOWUP";
        }
        if (card->type == CARD_TYPE_GOKOU && card->month == 7 && has_captured_sake_cup(player) && s->reach[WID_TSUKIMI] >= 20 && s->delay[WID_TSUKIMI] <= 2) {
            return "VISIBLE_TSUKIMI_FOLLOWUP";
        }
        if (card->type == CARD_TYPE_GOKOU && card->month != 10 && count_captured_non_rain_lights(player) >= 2 && s->reach[WID_SANKOU] >= 20 &&
            s->delay[WID_SANKOU] <= 2) {
            return "VISIBLE_SANKOU_FOLLOWUP";
        }
        if (card->type == CARD_TYPE_TAN && s->reach[WID_AKATAN] >= 20 && s->delay[WID_AKATAN] <= 2) {
            for (int j = 0; j < 3; j++) {
                if (card->month == red_months[j] && !player_has_captured_month_type(player, card->month, CARD_TYPE_TAN)) {
                    int ready_count = 0;
                    for (int k = 0; k < 3; k++) {
                        if (red_months[k] != card->month && player_has_captured_month_type(player, red_months[k], CARD_TYPE_TAN)) {
                            ready_count++;
                        }
                    }
                    if (ready_count >= 2) {
                        return "VISIBLE_AKATAN_FOLLOWUP";
                    }
                }
            }
        }
        if (card->type == CARD_TYPE_TAN && s->reach[WID_AOTAN] >= 20 && s->delay[WID_AOTAN] <= 2) {
            for (int j = 0; j < 3; j++) {
                if (card->month == blue_months[j] && !player_has_captured_month_type(player, card->month, CARD_TYPE_TAN)) {
                    int ready_count = 0;
                    for (int k = 0; k < 3; k++) {
                        if (blue_months[k] != card->month && player_has_captured_month_type(player, blue_months[k], CARD_TYPE_TAN)) {
                            ready_count++;
                        }
                    }
                    if (ready_count >= 2) {
                        return "VISIBLE_AOTAN_FOLLOWUP";
                    }
                }
            }
        }
    }

    return NULL;
}

static int calc_continue_risk_penalty(const StrategyData* s, int round_score, int min_risk_delay, int max_risk_score, int best_seven_plus_delay,
                                      int best_seven_plus_reach)
{
    int penalty = 0;
    int realistic_seven_plus = has_realistic_seven_plus_line(round_score, best_seven_plus_delay, best_seven_plus_reach);

    if (s->opponent_win_x2) {
        penalty += KOIKOI_CONTINUE_RISK_X2_PENALTY;
    }
    if (s->opp_coarse_threat >= 80) {
        penalty += KOIKOI_CONTINUE_RISK_HIGH_THREAT_PENALTY;
    } else if (s->opp_coarse_threat >= KOIKOI_SMALL_BASE_THREAT_THRESHOLD) {
        penalty += KOIKOI_CONTINUE_RISK_MID_THREAT_PENALTY;
    }
    if (min_risk_delay <= KOIKOI_SMALL_BASE_RISK_DELAY_MAX && max_risk_score >= KOIKOI_SMALL_BASE_RISK_SCORE_MIN) {
        penalty += KOIKOI_CONTINUE_RISK_NEAR_PENALTY;
    }
    if (round_score <= KOIKOI_SMALL_BASE_MAX) {
        penalty += KOIKOI_CONTINUE_RISK_SMALL_BASE_PENALTY;
        if (!realistic_seven_plus) {
            penalty += KOIKOI_SMALL_BASE_STOP_BONUS;
        }
    }
    if (s->match_score_diff >= 0 && round_score <= KOIKOI_SMALL_BASE_MAX) {
        penalty += KOIKOI_SMALL_BASE_LEAD_STOP_BONUS;
    }
    return penalty;
}

static int strategy_best_seven_plus_line(const StrategyData* s, int round_score, int* best_delay, int* best_reach)
{
    int best = 0;
    int out_delay = 99;
    int out_reach = 0;

    if (!s) {
        return 0;
    }
    if (round_score >= 7) {
        if (best_delay) {
            *best_delay = 99;
        }
        if (best_reach) {
            *best_reach = 0;
        }
        return 0;
    }

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int reach = s->reach[wid];
        int delay = s->delay[wid];
        int score = s->score[wid];
        int value;

        if (reach <= 0 || score <= 0 || delay >= 99) {
            continue;
        }
        if (round_score + score < 7) {
            continue;
        }

        value = (reach * (round_score + score) * 100) / (delay + 1);
        if (best < value) {
            best = value;
            out_delay = delay;
            out_reach = reach;
        }
    }

    if (best_delay) {
        *best_delay = out_delay;
    }
    if (best_reach) {
        *best_reach = out_reach;
    }
    return best;
}

static int strategy_best_continue_value(const StrategyData* s, int* best_delay, int* best_reach)
{
    int best = 0;
    int out_delay = 99;
    int out_reach = 0;

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int reach = s->reach[wid];
        int delay = s->delay[wid];
        int score = s->score[wid];
        int value;

        if (reach <= 0 || score <= 0 || delay >= 99) {
            continue;
        }

        // こいこい継続で得られる上積み期待値（新規役期待）。
        value = (reach * score * 100) / (delay + 1);
        if (best < value) {
            best = value;
            out_delay = delay;
            out_reach = reach;
        }
    }

    if (best_delay) {
        *best_delay = out_delay;
    }
    if (best_reach) {
        *best_reach = out_reach;
    }
    return best;
}

static int strategy_max_risk_value(const StrategyData* s, int* min_risk_delay, int* max_risk_score)
{
    int worst = 0;
    int out_min_delay = 99;
    int out_max_score = 0;

    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int risk_reach = s->risk_reach_estimate[wid];
        int risk_delay = s->risk_delay[wid];
        int risk_score = s->risk_score[wid];
        int delay_bonus = 0;
        int value;

        if (risk_reach <= 0 && risk_score <= 0) {
            continue;
        }
        if (risk_delay < out_min_delay) {
            out_min_delay = risk_delay;
        }
        if (out_max_score < risk_score) {
            out_max_score = risk_score;
        }

        if (risk_delay <= 1) {
            delay_bonus = 200;
        } else if (risk_delay <= 2) {
            delay_bonus = 120;
        } else if (risk_delay <= 4) {
            delay_bonus = 60;
        }

        value = risk_score * 100 + risk_reach * 2 + delay_bonus;
        if (worst < value) {
            worst = value;
        }
    }

    if (min_risk_delay) {
        *min_risk_delay = out_min_delay;
    }
    if (max_risk_score) {
        *max_risk_score = out_max_score;
    }
    return worst;
}

static int calc_continue_base(const StrategyData* s, int round_score)
{
    int diff = s->match_score_diff;
    int continue_base = round_score * 60;

    if (diff < 0) {
        int bonus = (-diff) * 6;
        if (bonus > 120) {
            bonus = 120;
        }
        continue_base += bonus;
    } else if (diff > 0) {
        int penalty = diff * 4;
        if (penalty > 80) {
            penalty = 80;
        }
        continue_base -= penalty;
    }
    if (s->koikoi_mine == ON) {
        continue_base += round_score * 20;
    }
    if (continue_base < 0) {
        continue_base = 0;
    }
    return continue_base;
}

static int calc_continue_live_out_bonus(const StrategyData* s, int round_score)
{
    int bonus = 0;
    int seen[WINNING_HAND_MAX];
    vgs_memset(seen, 0, sizeof(seen));

    for (int lane = 0; lane < 2; lane++) {
        const int* priorities = lane == 0 ? s->priority_score : s->priority_speed;
        for (int i = 0; i < 4; i++) {
            int wid = priorities[i];
            if (wid < 0 || wid >= WINNING_HAND_MAX || seen[wid]) {
                continue;
            }
            seen[wid] = ON;
            if (s->reach[wid] <= 0 || s->delay[wid] >= 99 || s->delay[wid] > 4) {
                continue;
            }

            if (s->score[wid] >= LIVE_OUT_SCORE_THRESHOLD && s->delay[wid] <= 3 && s->reach[wid] >= 10) {
                bonus += 80; // (C) 5点級以上のライブな伸び筋を重視
                if (s->delay[wid] <= 2) {
                    bonus += 80;
                }
            } else if ((wid == WID_KASU || wid == WID_TAN || wid == WID_TANE) && s->reach[wid] >= 50) {
                bonus += 20;
            }
        }
    }
    if (round_score >= 5 && bonus > 0) {
        bonus += 20;
    }
    return bonus;
}

static int strategy_calc_koikoi_continue_value(const StrategyData* s, int round_score)
{
    int best_self_delay = 99;
    int best_self_reach = 0;
    int best_seven_plus_delay = 99;
    int best_seven_plus_reach = 0;
    int continue_risk = strategy_max_risk_value(s, NULL, NULL);
    int continue_gain = strategy_best_continue_value(s, &best_self_delay, &best_self_reach);
    int seven_plus_gain = strategy_best_seven_plus_line(s, round_score, &best_seven_plus_delay, &best_seven_plus_reach);
    int continue_base = calc_continue_base(s, round_score);
    int continue_live_out_bonus = calc_continue_live_out_bonus(s, round_score);
    int continue_value;

    continue_gain = continue_gain > 6000 ? 6000 : continue_gain;
    if (s->match_score_diff < 0) {
        continue_risk = (continue_risk * 80) / 100; // (C) ビハインド時は risk を少し軽く
    } else if (s->match_score_diff > 0) {
        continue_risk = (continue_risk * 120) / 100; // (C) リード時は risk を重く
    }

    continue_value = continue_base + continue_gain + continue_live_out_bonus;
    continue_value += s->bias.offence * 5;
    if (s->koikoi_mine == ON) {
        continue_value += 80;
    }
    if (best_self_delay <= 2) {
        continue_value += 120;
    } else if (best_self_delay <= 3) {
        continue_value += 60;
    }
    if (best_seven_plus_delay <= 1) {
        continue_value += 260;
    } else if (best_seven_plus_delay <= 2) {
        continue_value += 180;
    } else if (best_seven_plus_delay <= 3) {
        continue_value += 90;
    } else if (round_score < 7) {
        continue_value -= 220;
    }
    continue_value += seven_plus_gain / 10;
    continue_value += s->bias.speed * 3;
    continue_value -= s->bias.defence * 5;
    if (s->opponent_win_x2) {
        continue_value -= 180;
    }
    continue_value -= continue_risk;
    return continue_value;
}

static int strategy_calc_koikoi_stop_value(const StrategyData* s, int round_score)
{
    int diff = s->match_score_diff;
    int stop_value = round_score * 100;

    if (s->left_rounds <= 1) {
        stop_value += 260;
    }
    if (s->left_own <= 2) {
        stop_value += 160;
    } else if (s->left_own <= 3) {
        stop_value += 80;
    }
    stop_value += s->bias.defence * 3;
    if (s->koikoi_opp == ON) {
        stop_value += 140;
    }
    if (s->opponent_win_x2) {
        stop_value += 180;
    }
    if (round_score >= 7) {
        stop_value += 200;
    } else if (round_score >= 5) {
        stop_value += 100;
    } else if (round_score <= 1) {
        stop_value -= 60;
    }
    if (s->bias.offence >= 80) {
        stop_value -= 80;
    } else if (s->bias.offence >= 60) {
        stop_value -= 40;
    }
    if (diff < 0) {
        int penalty = (-diff) * 40;
        if (penalty > 200) {
            penalty = 200;
        }
        stop_value -= penalty;
        if (round_score == 1 && s->left_rounds <= 2) {
            stop_value -= 120;
        }
        if (s->left_rounds <= 2 && round_score <= 2) {
            stop_value -= 160; // (C) ビハインド時の小得点 stop を抑制
        }
    } else if (diff > 0) {
        int bonus = diff * 30;
        if (bonus > 180) {
            bonus = 180;
        }
        stop_value += bonus;
        if (round_score >= 5) {
            stop_value += 120; // (C) リード時は保守寄り
        }
    }
    if (s->risk_7plus_distance <= 2) {
        stop_value += 180;
    } else if (s->risk_7plus_distance <= 4) {
        stop_value += 80;
    }

    return stop_value;
}

static int trace_min_risk_delay(const StrategyData* s)
{
    int min_delay = 99;

    if (!s) {
        return 99;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (s->risk_reach_estimate[wid] <= 0 && s->risk_score[wid] <= 0) {
            continue;
        }
        if (s->risk_delay[wid] < min_delay) {
            min_delay = s->risk_delay[wid];
        }
    }
    return min_delay;
}

static int trace_max_risk_score(const StrategyData* s)
{
    int max_score = 0;

    if (!s) {
        return 0;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        if (s->risk_score[wid] > max_score) {
            max_score = s->risk_score[wid];
        }
    }
    return max_score;
}

static int trace_best_live7_wid(const StrategyData* s, int round_score, int* out_delay, int* out_reach)
{
    int best_wid = -1;
    int best_delay = 99;
    int best_reach = 0;
    int best_margin = -99;

    if (out_delay) {
        *out_delay = 99;
    }
    if (out_reach) {
        *out_reach = 0;
    }
    if (!s) {
        return -1;
    }
    for (int wid = 0; wid < WINNING_HAND_MAX; wid++) {
        int total_score;
        int margin;

        if (s->reach[wid] <= 0 || s->score[wid] <= 0 || s->delay[wid] >= 99) {
            continue;
        }
        total_score = round_score + s->score[wid];
        if (total_score < 7) {
            continue;
        }
        margin = total_score - 7;
        if (best_wid < 0 || s->delay[wid] < best_delay || (s->delay[wid] == best_delay && margin > best_margin) ||
            (s->delay[wid] == best_delay && margin == best_margin && s->reach[wid] > best_reach)) {
            best_wid = wid;
            best_delay = s->delay[wid];
            best_reach = s->reach[wid];
            best_margin = margin;
        }
    }
    if (out_delay) {
        *out_delay = best_delay;
    }
    if (out_reach) {
        *out_reach = best_reach;
    }
    return best_wid;
}

/**
 * @brief こいこいを する or しない の判断
 * @param player 0: 自分, 1: CPU
 * @param koikoi_enabled ON: こいこい可能な状態, OFF: こいこい不可の状態
 * @param round_score 現在のラウンドスコア
 * @return ON: する, OFF: しない
 */
int ai_hard_koikoi(int player, int round_score)
{
    StrategyData strategy_work;
    int best_self_delay = 99;
    int best_self_reach = 0;
    int best_seven_plus_delay = 99;
    int best_seven_plus_reach = 0;
    int min_risk_delay = 99;
    int max_risk_score = 0;
    int continue_risk_penalty = 0;
    int realistic_seven_plus = OFF;
    int live_out_est;
    int live_out_delay;
    int base6_seven_push;
    int overpay_possible;
    int secured_followup_gain = 0;
    int secured_followup_cards = 0;
    int secured_high_value_followup_gain = 0;
    int visible_threshold_followup_gain = 0;
    int visible_threshold_followup_cards = 0;
    int visible_threshold_high_gain = 0;
    int continue_live_out_bonus;
    int continue_value;
    int continue_value_without_push;
    int stop_value;
    int stop_value_without_push;
    int base6_push_applied = OFF;
    const char* forced_stop_reason = NULL;
    const char* forced_continue_reason = NULL;
    const char* visible_five_point_followup_reason = NULL;
    int forced_result = -1;
    int forced_result_without_push = -1;
    int result_without_push = OFF;
    int loosen_close_match_push = OFF;
    int result;
    const StrategyData* s;

    ai_think_strategy(player, &strategy_work);
    vgs_memcpy(&g.strategy[player], &strategy_work, sizeof(StrategyData));
    s = &g.strategy[player];
    forced_stop_reason = forced_big_hand_stop_reason(player);

    live_out_est = s->best_additional_1pt_reach;
    live_out_delay = s->best_additional_1pt_delay;
    base6_seven_push = has_live_out_to_seven_push(s, round_score);
    overpay_possible = strategy_overpay_possible(s);
    secured_followup_gain = estimate_secured_followup_gain(player, &secured_followup_cards, &secured_high_value_followup_gain);
    visible_threshold_followup_gain = estimate_visible_threshold_followup_gain(player, &visible_threshold_followup_cards, &visible_threshold_high_gain);
    continue_live_out_bonus = calc_continue_live_out_bonus(s, round_score);
    continue_value = strategy_calc_koikoi_continue_value(s, round_score);
    continue_value_without_push = continue_value;
    stop_value = strategy_calc_koikoi_stop_value(s, round_score);
    stop_value_without_push = stop_value;

    (void)strategy_best_continue_value(s, &best_self_delay, &best_self_reach);
    (void)strategy_best_seven_plus_line(s, round_score, &best_seven_plus_delay, &best_seven_plus_reach);
    (void)strategy_max_risk_value(s, &min_risk_delay, &max_risk_score);
    realistic_seven_plus = has_realistic_seven_plus_line(round_score, best_seven_plus_delay, best_seven_plus_reach);
    continue_risk_penalty = calc_continue_risk_penalty(s, round_score, min_risk_delay, max_risk_score, best_seven_plus_delay, best_seven_plus_reach);
    loosen_close_match_push = should_loosen_close_match_koikoi(s, max_risk_score);
    visible_five_point_followup_reason = find_visible_immediate_five_point_followup(player, s);
    continue_value -= continue_risk_penalty;
    continue_value_without_push -= continue_risk_penalty;
    stop_value += continue_risk_penalty / 2;
    stop_value_without_push += continue_risk_penalty / 2;

    if (s->mode == MODE_GREEDY) {
        int greedy_push_ready = (best_seven_plus_delay <= KOIKOI_GREEDY_MAX_DELAY && best_seven_plus_reach >= KOIKOI_GREEDY_MIN_REACH) ||
                                continue_live_out_bonus >= 160;
        continue_value += continue_live_out_bonus;
        continue_value_without_push += continue_live_out_bonus;
        if (!greedy_push_ready && round_score < 7) {
            stop_value += 260;
            stop_value_without_push += 260;
        } else if (greedy_push_ready) {
            continue_value += 140;
            continue_value_without_push += 140;
        }
    } else if (s->mode == MODE_DEFENSIVE) {
        stop_value += KOIKOI_DEFENSIVE_STOP_BONUS;
        stop_value_without_push += KOIKOI_DEFENSIVE_STOP_BONUS;
        if (round_score >= 5) {
            stop_value += 120;
            stop_value_without_push += 120;
        }
        if (s->koikoi_opp == ON || s->opp_coarse_threat >= 60) {
            stop_value += 120;
            stop_value_without_push += 120;
        }
    } else {
        continue_value -= (s->opp_coarse_threat * KOIKOI_BALANCED_THREAT_DIVISOR);
        continue_value_without_push -= (s->opp_coarse_threat * KOIKOI_BALANCED_THREAT_DIVISOR);
        if (s->match_score_diff < 0) {
            continue_value += (-s->match_score_diff) * 20;
            continue_value_without_push += (-s->match_score_diff) * 20;
        } else if (s->match_score_diff > 0) {
            stop_value += s->match_score_diff * 15;
            stop_value_without_push += s->match_score_diff * 15;
        }
    }

    if (loosen_close_match_push) {
        stop_value -= KOIKOI_CLOSE_MATCH_ATTACK_STOP_PENALTY;
        stop_value_without_push -= KOIKOI_CLOSE_MATCH_ATTACK_STOP_PENALTY;
        if (stop_value < 0) {
            stop_value = 0;
        }
        if (stop_value_without_push < 0) {
            stop_value_without_push = 0;
        }
    }

    if (round_score == 5) {
        if (live_out_est < 35 || live_out_delay > 3) {
            stop_value += KOIKOI_BASE5_LOW_LIVE_OUT_STOP_BONUS;
            stop_value_without_push += KOIKOI_BASE5_LOW_LIVE_OUT_STOP_BONUS;
        } else {
            continue_value += continue_live_out_bonus / 2;
            continue_value_without_push += continue_live_out_bonus / 2;
        }
    } else if (round_score == 6) {
        if (live_out_est >= 35 && live_out_delay <= 3) {
            continue_value += KOIKOI_BASE6_LIVE_OUT_CONTINUE_BONUS;
            continue_value_without_push += KOIKOI_BASE6_LIVE_OUT_CONTINUE_BONUS;
        } else {
            stop_value += KOIKOI_BASE5_LOW_LIVE_OUT_STOP_BONUS / 2;
            stop_value_without_push += KOIKOI_BASE5_LOW_LIVE_OUT_STOP_BONUS / 2;
        }
        if (base6_seven_push) {
            continue_value += KOIKOI_BASE6_SEVEN_PUSH_BONUS;
            stop_value -= KOIKOI_BASE6_SEVEN_PUSH_STOP_PENALTY;
            base6_push_applied = ON;
            if (stop_value < 0) {
                stop_value = 0;
            }
        }
    } else if (round_score >= 7) {
        int allow_exception = (best_self_delay <= 2 && best_self_reach >= 35 && continue_live_out_bonus >= 160) ? ON : OFF;
        if (!allow_exception) {
            stop_value += KOIKOI_SEVEN_PLUS_STOP_BONUS;
            stop_value_without_push += KOIKOI_SEVEN_PLUS_STOP_BONUS;
        }
    }

    if (visible_five_point_followup_reason && s->opponent_win_x2 == OFF && s->opp_coarse_threat < KOIKOI_SMALL_BASE_THREAT_THRESHOLD &&
        !(min_risk_delay <= 1 && max_risk_score >= 2)) {
        forced_continue_reason = visible_five_point_followup_reason;
        forced_result = ON;
        forced_result_without_push = ON;
        goto finalize;
    }

    if (round_score <= KOIKOI_SMALL_BASE_MAX && visible_threshold_followup_cards > 0 && visible_threshold_followup_gain >= 2 && s->opponent_win_x2 == OFF &&
        s->opp_coarse_threat < 40 && s->opp_exact_7plus_threat <= 0 && min_risk_delay >= 3 && max_risk_score <= 1) {
        forced_continue_reason = "VISIBLE_THRESHOLD_PLUS2";
        forced_result = ON;
        forced_result_without_push = ON;
        goto finalize;
    }

    if (round_score == 6 &&
        ((secured_followup_cards >= 2 && secured_followup_gain >= 2) || secured_high_value_followup_gain >= 5)) {
        ai_debug_note_koikoi_locked_threshold(player, OFF);
    }

    if (round_score == 6 &&
        ((secured_followup_cards >= 2 && secured_followup_gain >= 2) || secured_high_value_followup_gain >= 5) && s->opponent_win_x2 == OFF &&
        s->opp_coarse_threat <= 20 && s->opp_exact_7plus_threat <= 0 && min_risk_delay >= 4 && max_risk_score <= 1) {
        ai_debug_note_koikoi_locked_threshold(player, ON);
        forced_continue_reason = secured_high_value_followup_gain >= 5 ? "SECURED_OVERPAY_FOLLOWUP" : "LOCKED_THRESHOLD_PLUS2";
        forced_result = ON;
        forced_result_without_push = ON;
        goto finalize;
    }

    if (round_score <= KOIKOI_SMALL_BASE_MAX) {
        int small_base_forbidden = OFF;
        int small_base_escape = OFF;

        /* 小得点でも、ビハインド時に次巡の伸び筋が極めて太い場合は
         * 後段の危険判定へ流して比較評価させる。 */
        if (best_self_reach >= 30 && best_self_delay <= s->left_own * 2 &&
            continue_value > stop_value + 120 &&
            (continue_live_out_bonus >= 160 || (live_out_est >= 80 && live_out_delay <= 1))) {
            small_base_escape = ON;
        }

        if (s->match_score_diff >= 0) {
            small_base_forbidden = ON;
        }
        if (s->opp_coarse_threat >= KOIKOI_SMALL_BASE_THREAT_THRESHOLD) {
            small_base_forbidden = ON;
        }
        if (min_risk_delay <= KOIKOI_SMALL_BASE_RISK_DELAY_MAX && max_risk_score >= KOIKOI_SMALL_BASE_RISK_SCORE_MIN) {
            small_base_forbidden = ON;
        }
        if (!realistic_seven_plus) {
            small_base_forbidden = ON;
        }
        if (small_base_forbidden && !small_base_escape) {
            forced_result = OFF;
            forced_result_without_push = OFF;
            goto finalize;
        }
    }

    if (round_score >= KOIKOI_DANGER_BASE_MIN && s->opp_coarse_threat >= KOIKOI_DANGER_THREAT_THRESHOLD && s->opponent_win_x2) {
        int opp_can_blow_up_soon = (min_risk_delay <= 1 && max_risk_score >= 7) ? ON : OFF;
        int can_close_before_blowup =
            ((realistic_seven_plus && best_self_delay <= 2 && best_self_reach >= 35) || base6_seven_push) &&
            continue_value > stop_value + KOIKOI_DANGER_WIN_MARGIN;
        int can_close_before_blowup_without_push =
            (realistic_seven_plus && best_self_delay <= 2 && best_self_reach >= 35) &&
            continue_value_without_push > stop_value_without_push + KOIKOI_DANGER_WIN_MARGIN;
        if (opp_can_blow_up_soon) {
            forced_result = OFF;
            forced_result_without_push = OFF;
            goto finalize;
        }
        if (!can_close_before_blowup) {
            forced_result = OFF;
            forced_result_without_push = OFF;
            goto finalize;
        }
        if (!can_close_before_blowup_without_push) {
            forced_result_without_push = OFF;
        }
    }

    // 安全策1: 相手の即上がり危険が高い時は原則ストップ。
    if (min_risk_delay <= 1 && max_risk_score >= 2) {
        int can_push = (continue_value > stop_value + 120 &&
                        ((round_score <= 1 && best_self_reach >= 35 && best_self_delay <= 2) ||
                         continue_live_out_bonus >= 160 || round_score >= 5 || base6_seven_push)); // (C) 太い伸び筋がある時は押し返す
        int can_push_without = (continue_value_without_push > stop_value_without_push + 120 &&
                                ((round_score <= 1 && best_self_reach >= 35 && best_self_delay <= 2) ||
                                 continue_live_out_bonus >= 160 || round_score >= 5));
        if (!can_push) {
            forced_result = OFF;
            forced_result_without_push = OFF;
            goto finalize;
        }
        if (!can_push_without) {
            forced_result_without_push = OFF;
        }
    }

    // 安全策3: 追加見込みが薄いならストップ寄りに補正。
    if (best_self_reach < 15 && best_self_delay > 3) {
        stop_value += 220;
        stop_value_without_push += 220;
    }
    if (round_score < 7 && (best_seven_plus_delay > 3 || best_seven_plus_reach < 20)) {
        stop_value += 200;
        stop_value_without_push += 200;
    }
    if (base6_seven_push) {
        stop_value -= 180;
        if (stop_value < 0) {
            stop_value = 0;
        }
    }
    if (s->opponent_win_x2 && round_score < 7 && best_seven_plus_delay > 2) {
        stop_value += 220;
        stop_value_without_push += 220;
    }
    if (should_apply_visible_seven_threat_stop_bonus(s, round_score, best_self_delay, best_self_reach, best_seven_plus_delay, best_seven_plus_reach,
                                                     min_risk_delay, max_risk_score, base6_seven_push)) {
        stop_value += KOIKOI_VISIBLE_SEVEN_THREAT_STOP_BONUS;
        stop_value_without_push += KOIKOI_VISIBLE_SEVEN_THREAT_STOP_BONUS;
        continue_value -= KOIKOI_VISIBLE_SEVEN_THREAT_STOP_BONUS / 3;
        continue_value_without_push -= KOIKOI_VISIBLE_SEVEN_THREAT_STOP_BONUS / 3;
    }
    if (should_stop_base5_endgame_lead_koikoi(s, round_score, min_risk_delay, max_risk_score, best_seven_plus_delay)) {
        stop_value += KOIKOI_BASE5_ENDGAME_LEAD_STOP_BONUS;
        stop_value_without_push += KOIKOI_BASE5_ENDGAME_LEAD_STOP_BONUS;
        continue_value -= KOIKOI_BASE5_ENDGAME_LEAD_STOP_BONUS / 3;
        continue_value_without_push -= KOIKOI_BASE5_ENDGAME_LEAD_STOP_BONUS / 3;
        if (!forced_stop_reason) {
            forced_stop_reason = "BASE5_ENDGAME_LEAD_RISK";
        }
    }
    if (should_stop_base5_thin_hand_koikoi(s, round_score, live_out_est, live_out_delay, best_self_delay, best_seven_plus_delay, best_seven_plus_reach,
                                           min_risk_delay, max_risk_score)) {
        stop_value += KOIKOI_BASE5_THIN_HAND_STOP_BONUS;
        stop_value_without_push += KOIKOI_BASE5_THIN_HAND_STOP_BONUS;
        continue_value -= KOIKOI_BASE5_THIN_HAND_STOP_BONUS / 3;
        continue_value_without_push -= KOIKOI_BASE5_THIN_HAND_STOP_BONUS / 3;
        if (!forced_stop_reason) {
            forced_stop_reason = "BASE5_THIN_HAND";
        }
    }
    if (should_apply_base5_post_delay_stop_bonus(s, round_score, best_self_delay, best_self_reach, best_seven_plus_delay, best_seven_plus_reach,
                                                 min_risk_delay, max_risk_score)) {
        stop_value += KOIKOI_BASE5_POST_DELAY_STOP_BONUS;
        stop_value_without_push += KOIKOI_BASE5_POST_DELAY_STOP_BONUS;
    }
    if (should_stop_base5_near_high_risk_koikoi(s, round_score, live_out_est, live_out_delay, min_risk_delay, max_risk_score, visible_threshold_high_gain,
                                                secured_high_value_followup_gain)) {
        if (!forced_stop_reason) {
            forced_stop_reason = "BASE5_NEAR_HIGH_RISK";
        }
    }
    if (should_stop_base5_behind_thin_push_koikoi(s, round_score, live_out_est, live_out_delay, best_self_reach, best_seven_plus_reach, min_risk_delay,
                                                  max_risk_score)) {
        stop_value += KOIKOI_BASE5_BEHIND_THIN_PUSH_STOP_BONUS;
        stop_value_without_push += KOIKOI_BASE5_BEHIND_THIN_PUSH_STOP_BONUS;
        continue_value -= KOIKOI_BASE5_BEHIND_THIN_PUSH_STOP_BONUS / 3;
        continue_value_without_push -= KOIKOI_BASE5_BEHIND_THIN_PUSH_STOP_BONUS / 3;
        if (!forced_stop_reason) {
            forced_stop_reason = "BASE5_BEHIND_THIN_PUSH";
        }
    }
    if (should_stop_base5_blocked_sake_koikoi(player, s, round_score, live_out_est, live_out_delay, best_seven_plus_delay, best_seven_plus_reach)) {
        if (!forced_stop_reason) {
            forced_stop_reason = "BASE5_BLOCKED_SAKE";
        }
    }
    if (should_stop_small_base_last_card_koikoi(s, round_score, live_out_delay, best_seven_plus_delay, best_seven_plus_reach,
                                                visible_threshold_followup_gain, secured_followup_cards, visible_threshold_high_gain,
                                                secured_high_value_followup_gain)) {
        if (!forced_stop_reason) {
            forced_stop_reason = "SMALL_BASE_LAST_CARD";
        }
    }

    if (round_score >= 7 && s->left_own <= 1 && s->match_score_diff >= -10 && !visible_five_point_followup_reason && visible_threshold_followup_gain < 2 &&
        secured_high_value_followup_gain < 5) {
        if (!forced_stop_reason) {
            forced_stop_reason = "SEVEN_PLUS_LAST_CARD";
        }
    }

    if (forced_stop_reason) {
        forced_result = OFF;
        forced_result_without_push = OFF;
        goto finalize;
    }

    // 安全策4: 最終盤の終盤は安全優先。ただし高攻撃性かつ高見込みなら例外。
    if (s->left_rounds <= 1 && s->left_own <= 3) {
        int allow_exception = (s->bias.offence >= 85 && best_self_reach >= 35 && best_self_delay <= 2);
        if (!allow_exception) {
            stop_value += 260;
            stop_value_without_push += 260;
        }
    }

finalize:
    // continue_value と stop_value を同一スケールで比較し、勝率寄りの意思決定を行う。
    result = (forced_result >= 0) ? forced_result : ((continue_value > stop_value) ? ON : OFF);
    result_without_push =
        (forced_result_without_push >= 0) ? forced_result_without_push : ((continue_value_without_push > stop_value_without_push) ? ON : OFF);
    if (base6_seven_push) {
        ai_debug_note_koikoi_base6_push(player, base6_push_applied, result != result_without_push);
    }

    {
        int opp_next_base = (min_risk_delay <= 1) ? max_risk_score : 0;
        int opp_next_win = (min_risk_delay <= 1 && max_risk_score > 0) ? ON : OFF;
        int live7_delay = 99;
        int live7_reach = 0;
        int live7_wid = trace_best_live7_wid(s, round_score, &live7_delay, &live7_reach);

        ai_watch_min_set_koikoi(player, "[KOIKOI-MIN] k_base=%d need7=%d live7{d=%d r=%d wid=%s} opp_next{win=%d base=%d x2=%d}", round_score,
                                round_score < 7 ? 7 - round_score : 0, round_score < 7 ? live7_delay : 0, round_score < 7 ? live7_reach : 0,
                                (round_score < 7 && live7_wid >= 0) ? winning_hands[live7_wid].name : "NA", opp_next_win ? 1 : 0, opp_next_base,
                                s->koikoi_mine ? 1 : 0);
    }

    ai_putlog("[koikoi] base=%d own_win_requires=%d live_out_est=%d live_out_delay=%d base6_push=%d overpay_possible=%d decision=%s", round_score,
              s->koikoi_mine == ON ? g.koikoi_score[player] : round_score, live_out_est, live_out_delay, base6_seven_push,
              overpay_possible, result == ON ? "ON" : "OFF");
    ai_putlog("KoiKoi Decision:");
    ai_putlog("- round_score=%d", round_score);
    ai_putlog("- mode=%s", strategy_mode_name(s->mode));
    ai_putlog("- match_score_diff=%d", s->match_score_diff);
    ai_putlog("- left_rounds=%d left_own=%d", s->left_rounds, s->left_own);
    ai_putlog("- opp_coarse_threat=%d opp_exact_7plus_threat=%d risk_mult=%d", s->opp_coarse_threat, s->opp_exact_7plus_threat, s->risk_mult_pct);
    ai_putlog("- greedy_need=%d defensive_need=%d", s->greedy_need, s->defensive_need);
    ai_putlog("- bias(off/speed/def)=%d/%d/%d", s->bias.offence, s->bias.speed, s->bias.defence);
    ai_putlog("- continue_value=%d", continue_value);
    ai_putlog("- stop_value=%d", stop_value);
    ai_putlog("- continue_risk_penalty=%d", continue_risk_penalty);
    ai_putlog("- continue_base=%d live_out=%d", calc_continue_base(s, round_score), continue_live_out_bonus);
    ai_putlog("- base_now=%d best_additional_1pt=%d/%d base6_push=%d overpay_possible=%d", s->base_now, s->best_additional_1pt_reach,
              s->best_additional_1pt_delay, base6_seven_push, overpay_possible);
    ai_putlog("- visible_threshold_followup=%d/%d secured_followup=%d/%d high=%d", visible_threshold_followup_cards, visible_threshold_followup_gain,
              secured_followup_cards, secured_followup_gain, secured_high_value_followup_gain > visible_threshold_high_gain ? secured_high_value_followup_gain : visible_threshold_high_gain);
    ai_putlog("- best_self_delay=%d best_self_reach=%d", best_self_delay, best_self_reach);
    ai_putlog("- best_7plus_delay=%d best_7plus_reach=%d", best_seven_plus_delay, best_seven_plus_reach);
    ai_putlog("- min_risk_delay=%d max_risk_score=%d", min_risk_delay, max_risk_score);
    ai_putlog("- base6_push_effect applied=%d flipped=%d decision_without_push=%s", base6_push_applied ? 1 : 0,
              (result != result_without_push) ? 1 : 0, result_without_push == ON ? "ON" : "OFF");
    ai_putlog("- close_match_push=%d", loosen_close_match_push);
    ai_putlog("- own_win_requires>%d opponent_win_x2=%s", s->koikoi_mine == ON ? g.koikoi_score[player] : round_score,
              s->opponent_win_x2 ? "ON" : "OFF");
    ai_putlog("- risk_7plus_distance=%d opp_win_x2=%d", s->risk_7plus_distance, s->opponent_win_x2);
    ai_putlog("- forced_stop_reason=%s", forced_stop_reason ? forced_stop_reason : "none");
    ai_putlog("- forced_continue_reason=%s", forced_continue_reason ? forced_continue_reason : "none");
    ai_putlog("- decision=%s", result == ON ? "ON" : "OFF");
    return result;
}
