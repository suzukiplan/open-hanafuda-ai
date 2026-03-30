from __future__ import annotations

import sys
from pathlib import Path


README_HEADING = "## Requirements"


def find_requirements_code_block(lines: list[str]) -> tuple[int, int]:
    heading_index = next(
        (index for index, line in enumerate(lines) if line.strip() == README_HEADING),
        None,
    )
    if heading_index is None:
        raise ValueError(f"Heading not found: {README_HEADING}")

    fence_start = next(
        (
            index
            for index in range(heading_index + 1, len(lines))
            if lines[index].startswith("```")
        ),
        None,
    )
    if fence_start is None:
        raise ValueError("Opening code fence not found in Requirements section")

    fence_end = next(
        (
            index
            for index in range(fence_start + 1, len(lines))
            if lines[index].startswith("```")
        ),
        None,
    )
    if fence_end is None:
        raise ValueError("Closing code fence not found in Requirements section")

    return fence_start, fence_end


def build_replacement_block(existing_block: list[str], log_lines: list[str]) -> list[str]:
    summary_index = next(
        (index for index, line in enumerate(existing_block) if line.startswith("SUMMARY ")),
        None,
    )
    if summary_index is None:
        return log_lines
    return existing_block[:summary_index] + log_lines


def update_readme(readme_path: Path, log_path: Path) -> None:
    readme_text = readme_path.read_text(encoding="utf-8")
    log_text = log_path.read_text(encoding="utf-8").rstrip("\n")

    readme_lines = readme_text.splitlines()
    log_lines = log_text.splitlines()

    fence_start, fence_end = find_requirements_code_block(readme_lines)
    existing_block = readme_lines[fence_start + 1 : fence_end]
    replacement_block = build_replacement_block(existing_block, log_lines)

    updated_lines = (
        readme_lines[: fence_start + 1]
        + replacement_block
        + readme_lines[fence_end:]
    )
    readme_path.write_text("\n".join(updated_lines) + "\n", encoding="utf-8")


def main() -> int:
    base_dir = Path(__file__).resolve().parent
    readme_path = Path(sys.argv[1]) if len(sys.argv) > 1 else base_dir / "README.md"
    log_path = Path(sys.argv[2]) if len(sys.argv) > 2 else base_dir / "run1k.log"

    update_readme(readme_path, log_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
