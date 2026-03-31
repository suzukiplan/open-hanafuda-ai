import os
import re
import csv
import sys

WATCH_START_RE = re.compile(r'^Watch Start: P1=(\S+) CPU=(\S+) rounds=\d+ seed=\d+$')

def extract_data_from_filename(filename):
    """
    Support:
    1. watch_<count>_win{0|1}_score0-score1.log
    2. watch_<count>_draw_score0-score1.log
    3. watch_<count>_score0-score1.log
    """

    # win pattern
    m_win = re.search(r'_(\d+)_win([01])_(\d+)-(\d+)', filename)
    if m_win:
        count = int(m_win.group(1))
        score0 = int(m_win.group(3))
        score1 = int(m_win.group(4))
        winner = "P1" if m_win.group(2) == "0" else "CPU"
        return count, score0, score1, winner

    # draw pattern
    m_draw = re.search(r'_(\d+)_draw_(\d+)-(\d+)', filename)
    if m_draw:
        count = int(m_draw.group(1))
        score0 = int(m_draw.group(2))
        score1 = int(m_draw.group(3))
        return count, score0, score1, "draw"

    # score-only pattern
    m_score = re.search(r'_(\d+)_(\d+)-(\d+)(?:_|\.|$)', filename)
    if m_score:
        count = int(m_score.group(1))
        score0 = int(m_score.group(2))
        score1 = int(m_score.group(3))
        winner = "draw"
        if score0 > score1:
            winner = "P1"
        elif score1 > score0:
            winner = "CPU"
        return count, score0, score1, winner

    return None


ROUND_RESULT_RE = re.compile(
    r'^\[Round (\d+)\] Winner=.*Total P1=(\d+) CPU=(\d+)\)$'
)
ROUND_DRAW_RE = re.compile(r'^\[Round (\d+)\] Draw$')


def extract_round_diffs(log_path):
    round_diffs = {}
    current_diff = 0

    with open(log_path, encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.rstrip("\n")

            m_result = ROUND_RESULT_RE.match(line)
            if m_result:
                round_no = int(m_result.group(1))
                score0 = int(m_result.group(2))
                score1 = int(m_result.group(3))
                current_diff = score0 - score1
                round_diffs[round_no] = current_diff
                continue

            m_draw = ROUND_DRAW_RE.match(line)
            if m_draw:
                round_no = int(m_draw.group(1))
                round_diffs[round_no] = current_diff

    return round_diffs


def extract_models(log_path):
    with open(log_path, encoding="utf-8") as f:
        first_line = f.readline().rstrip("\n")

    match = WATCH_START_RE.match(first_line)
    if not match:
        raise ValueError(f"watch start header not found: {log_path}")
    return match.group(1), match.group(2)


def process_directory(input_dir):
    rows = []
    max_round = 0
    count_rows = 0

    for fname in sorted(os.listdir(input_dir)):
        if not fname.endswith(".log"):
            continue

        result = extract_data_from_filename(fname)
        if result:
            count, score0, score1, winner = result
            diff = score0 - score1
            log_path = os.path.join(input_dir, fname)
            p1_model, cpu_model = extract_models(log_path)
            round_diffs = extract_round_diffs(log_path)
            if round_diffs:
                max_round = max(max_round, max(round_diffs))
            winner_label = "draw"
            if winner == "P1":
                winner_label = f"P1:{p1_model}"
            elif winner == "CPU":
                winner_label = f"CPU:{cpu_model}"
            rows.append(
                {
                    "count": count,
                    "score0": score0,
                    "score1": score1,
                    "p1_model": p1_model,
                    "cpu_model": cpu_model,
                    "diff": diff,
                    "winner": winner_label,
                    "round_diffs": round_diffs,
                    "filename": fname,
                }
            )
            count_rows += 1
        else:
            print(f"skip (no match): {fname}", file=sys.stderr)

    rows.sort(key=lambda row: (row["count"], row["filename"]))

    writer = csv.writer(sys.stdout)
    header = ["count", "P1:Model", "CPU:Model", "diff", "winner"]
    header.extend(f"round{i}" for i in range(1, max_round + 1))
    writer.writerow(header)

    for row in rows:
        csv_row = [row["count"], row["score0"], row["score1"], row["diff"], row["winner"]]
        csv_row.extend(row["round_diffs"].get(i, "") for i in range(1, max_round + 1))
        writer.writerow(csv_row)

    print(f"# rows={count_rows}", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python extract_watch_csv.py <input_dir>", file=sys.stderr)
        sys.exit(1)

    input_dir = sys.argv[1]
    process_directory(input_dir)
