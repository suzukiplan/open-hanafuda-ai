import os
import re
import csv
import sys

def extract_data_from_filename(filename):
    """
    Support:
    1. watch_<count>_win{0|1}_score0-score1.log
    2. watch_<count>_draw_score0-score1.log
    """

    # win pattern
    m_win = re.search(r'_(\d+)_win([01])_(\d+)-(\d+)', filename)
    if m_win:
        count = int(m_win.group(1))
        num = int(m_win.group(2))
        score0 = int(m_win.group(3))
        score1 = int(m_win.group(4))
        return count, num, score0, score1

    # draw pattern
    m_draw = re.search(r'_(\d+)_draw_(\d+)-(\d+)', filename)
    if m_draw:
        count = int(m_draw.group(1))
        num = "draw"
        score0 = int(m_draw.group(2))
        score1 = int(m_draw.group(3))
        return count, num, score0, score1

    return None


def process_directory(input_dir):
    writer = csv.writer(sys.stdout)
    writer.writerow(["count", "num", "score0", "score1"])

    count_rows = 0

    for fname in os.listdir(input_dir):
        if not fname.endswith(".log"):
            continue

        result = extract_data_from_filename(fname)
        if result:
            writer.writerow(result)
            count_rows += 1
        else:
            print(f"skip (no match): {fname}", file=sys.stderr)

    print(f"# rows={count_rows}", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python extract_watch_csv.py <input_dir>", file=sys.stderr)
        sys.exit(1)

    input_dir = sys.argv[1]
    process_directory(input_dir)
