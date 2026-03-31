#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


WATCH_START_RE = re.compile(r"^Watch Start: P1=(?P<p1>\S+) CPU=(?P<cpu>\S+) rounds=(?P<rounds>\d+) seed=(?P<seed>\d+)$")
ROUND_WINNER_RE = re.compile(
    r"^\[Round (?P<round>\d+)\] Winner=(?P<winner>P1|CPU) base=(?P<base>\d+) x(?P<mul>\d+) => \+(?P<score>\d+) "
    r"\(reason=(?P<reason>[^,]+), Total P1=(?P<p1>\d+) CPU=(?P<cpu>\d+)\)$"
)
ROUND_DRAW_RE = re.compile(r"^\[Round (?P<round>\d+)\] Draw$")
HAND_LINE_RE = re.compile(r"^  - (?P<name>.+?) \((?P<desc>.+)\): (?P<score>\d+)$")
WINNING_HAND_RE = re.compile(r'\{WID_(?P<wid>[A-Z_]+), "(?P<name>[^"]+)",')


def load_winning_hand_map(source_path: Path) -> dict[str, str]:
    text = source_path.read_text(encoding="utf-8")
    mapping: dict[str, str] = {}
    for match in WINNING_HAND_RE.finditer(text):
        mapping[match.group("name")] = match.group("wid")
    if not mapping:
        raise ValueError(f"failed to load winning hand definitions from {source_path}")
    return mapping


def format_round(round_no: int, winner: str, score: int, hand_ids: list[str], reason: str, total_diff: int, models: dict[str, str]) -> str:
    hand_part = ", ".join(hand_ids) if hand_ids else "none"
    return f"Round {round_no}: {winner}:{models[winner]}={score} <{hand_part}> reason={reason} ({total_diff})"


def format_draw(round_no: int, total_diff: int) -> str:
    return f"Round {round_no}: Draw ({total_diff})"


def analyze_watch_log(log_path: Path, hand_name_to_id: dict[str, str]) -> list[str]:
    lines = log_path.read_text(encoding="utf-8").splitlines()
    outputs: list[str] = []
    total_p1 = 0
    total_cpu = 0
    models = {"P1": "P1", "CPU": "CPU"}

    for idx, line in enumerate(lines):
        if idx == 0:
            match = WATCH_START_RE.match(line)
            if not match:
                raise ValueError("watch log header not found")
            models["P1"] = match.group("p1")
            models["CPU"] = match.group("cpu")
            continue

        draw_match = ROUND_DRAW_RE.match(line)
        if draw_match:
            round_no = int(draw_match.group("round"))
            outputs.append(format_draw(round_no, total_p1 - total_cpu))
            continue

        winner_match = ROUND_WINNER_RE.match(line)
        if not winner_match:
            continue

        round_no = int(winner_match.group("round"))
        winner = winner_match.group("winner")
        round_score = int(winner_match.group("score"))
        reason = winner_match.group("reason")
        total_p1 = int(winner_match.group("p1"))
        total_cpu = int(winner_match.group("cpu"))

        hand_ids: list[str] = []
        next_idx = idx + 1
        while next_idx < len(lines):
            hand_match = HAND_LINE_RE.match(lines[next_idx])
            if not hand_match:
                break

            hand_name = hand_match.group("name")
            hand_id = hand_name_to_id.get(hand_name)
            if hand_id is None:
                raise ValueError(f"unknown winning hand name in log: {hand_name}")
            hand_ids.append(hand_id)
            next_idx += 1

        outputs.append(format_round(round_no, winner, round_score, hand_ids, reason, total_p1 - total_cpu, models))

    final_diff = total_p1 - total_cpu
    if total_p1 > total_cpu:
        outputs.append(f"Winner: P1:{models['P1']} ({final_diff})")
    elif total_cpu > total_p1:
        outputs.append(f"Winner: CPU:{models['CPU']} ({final_diff})")
    else:
        outputs.append(f"Winner: Draw ({final_diff})")
    return outputs


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze round results from a watch.log file.")
    parser.add_argument("watch_log", type=Path, help="Path to watch.log")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    winning_hands_path = repo_root / "src" / "winning_hands.c"

    try:
        hand_name_to_id = load_winning_hand_map(winning_hands_path)
        results = analyze_watch_log(args.watch_log, hand_name_to_id)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    for line in results:
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
