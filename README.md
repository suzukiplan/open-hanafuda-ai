# Open Hanafuda AI [![CircleCI](https://dl.circleci.com/status-badge/img/gh/suzukiplan/open-hanafuda-ai/tree/master.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/suzukiplan/open-hanafuda-ai/tree/master)

## Overview

This project is motivated by a research-oriented question:

**Can a strong Hanafuda (Koi-Koi) AI be achieved purely through statistical reasoning and simulation, without relying on hidden information or artificial bias?**

In many existing implementations, perceived difficulty is often influenced by non-transparent mechanisms such as biased dealing or access to hidden game state. While these approaches are practical for tuning player experience, they obscure the relationship between decision-making quality and actual AI strength.

From a technical perspective, simulation-based decision-making has historically been constrained by hardware limitations. Techniques such as Monte Carlo evaluation require substantial computational resources, making them impractical for older consoles and arcade systems.

However, with modern CPU performance, it has become feasible to perform large numbers of forward simulations within real-time constraints. This enables a different design approach:

- Treat Hanafuda as a stochastic, imperfect-information game
- Estimate action values through repeated simulation
- Infer hidden state only from observable information
- Combine statistical evaluation with deterministic heuristics

Under this framework, AI strength emerges from the accuracy and efficiency of its inference process, rather than from external adjustments to randomness.

Hanafuda inherently includes a significant degree of chance, making it an interesting domain for studying the balance between stochasticity and decision quality. This project explores how far simulation-based reasoning alone can push competitive performance in such an environment.

By providing this implementation as open source under a permissive license, the project aims to:

- Enable reproducible experiments and comparative evaluation
- Encourage transparent AI design in Hanafuda games
- Serve as a reference implementation for simulation-based decision systems in small decision-space games

Finally, this repository contains an open-source snapshot of the Hanafuda AI simulator used in [Battle Hanafuda](https://store.steampowered.com/app/4161340/Battle_Hanafuda/), bridging experimental design and real-world deployment.

## Coding Rules

### KoiKoi Rules

As a fundamental principle, the game must be implemented in **full compliance** with the [Nintendo rules for Hanafuda Koi-Koi](https://www.nintendo.com/jp/others/hanafuda_kabufuda/howtoplay/koikoi/index.html) only, and no other local rule variations shall be supported.

### Hidden Info

AI hidden-info rule for `ai*.{c,h}`:

- **Never read the opponent hand directly via `own[opp]`.**
- **Never read unopened draw-pile contents via `floor`.**
- **Visible cards may be read directly: `own[player]`, `deck`, `invent`.**
- **Estimation is allowed only by inferring unseen cards from visible cards.**

### Standard Library Usage

Use of the C standard library is prohibited, except in the following files, which contain host-simulator-specific implementations (a constraint of VGS-X):

- `main.c`: Entry point for the host simulator only
- `sim_*.c`: Submodules for the host simulator only

## Requirements

As a general rule, this repository only accepts changes that improve the win rate produced by `make run1k` (for example, `57.60%` in the case below), and does not allow changes that reduce it.

```
% make run1k
./ai_sim -0 0 -1 1 -r 12 -l 1000 --seed=1772851247
SUMMARY games=1000 seed=1772851247 rounds=12
P1 model=Normal wins=397 avg_diff=-7.342 reason_7plus=1509 reason_opponent_koikoi=861 koikoi=4165 koikoi_success=2633 koikoi_success_rate=63.22%
CPU model=Hard wins=597 avg_diff=7.342 reason_7plus=1628 reason_opponent_koikoi=1339 koikoi=2806 koikoi_success=1843 koikoi_success_rate=65.68%
DRAW draws=6
Sake round summary: 1P=2020, CPU=2005
 - 1P detail: win=1015 (50.25%), average=9.70pts koikoi-cnt=1002 koikoi-win=650 koikoi-up=8.07pts
 - CPU detail: win=1229 (61.30%), average=10.24pts koikoi-cnt=912 koikoi-win=628 koikoi-up=9.87pts
KOIKOI_BASE6_PUSH P1=0/0/0 CPU=124/124/9
KOIKOI_LOCKED_THRESHOLD P1=0/0 CPU=12/1
WIN_RATE P1=39.70%(60.75pts) CPU=59.70%(61.22pts)
- Round 1: P1=43.70%(7.80pts) CPU=52.60%(7.78pts)
- Round 2: P1=42.30%(8.11pts) CPU=53.50%(7.29pts)
- Round 3: P1=41.90%(8.43pts) CPU=54.00%(7.47pts)
- Round 4: P1=44.10%(8.86pts) CPU=51.70%(7.73pts)
- Round 5: P1=44.60%(7.32pts) CPU=50.50%(8.01pts)
- Round 6: P1=42.80%(7.64pts) CPU=53.20%(8.16pts)
- Round 7: P1=41.40%(8.08pts) CPU=55.20%(7.83pts)
- Round 8: P1=41.70%(8.21pts) CPU=54.00%(7.23pts)
- Round 9: P1=46.20%(7.24pts) CPU=50.40%(8.06pts)
- Round 10: P1=42.30%(7.99pts) CPU=53.00%(7.58pts)
- Round 11: P1=45.90%(8.09pts) CPU=50.40%(8.62pts)
- Round 12: P1=45.80%(6.86pts) CPU=50.40%(6.83pts)
Per Bias:
- GREEDY: win=1789 (61.2%), lose=1054 (36.1%), draw=80 (2.7%)
- DEFENSIVE: win=3861 (47.3%), lose=3926 (48.1%), draw=383 (4.7%)
- BALANCED: win=639 (70.5%), lose=246 (27.2%), draw=21 (2.3%)
```

If you find room for improvement in Battle Hanafuda's Watch Mode match results, you may be able to improve the win rate by cross-referencing `watch.log` with the source code in this repository and having a coding agent (such as Codex or Claude) analyze it.

If you come up with a fix that improves the win rate, please submit a **Pull Request**.

Of course, implementations produced with the help of a coding agent are also welcome.

## Specification Summary

- Target platform: macOS / Linux
- Output binaries:
  - `ai_sim`: AI-vs-AI simulator
  - `replay`: watch-log replay tool
- Entry points:
  - `src/main.c`
  - `src/replay.c`
- Included scope:
  - AI decision logic
  - Round/game progression logic required by the simulator
  - CLI helpers for card handling and result analysis
  - Host-side simulation runtime for logging and deterministic random seeds
- Excluded scope:
  - Full game application assets and platform-specific runtime outside the simulator

## Build

After exporting the sources, build the OSS simulator with:

```bash
make
```

This produces the executables below:

```bash
./ai_sim
./replay
```

## Run: ai_sim

Example:

```bash
./ai_sim -0 0 -1 1 -r 12 -l 100
```

Main options:

- `-0 <ai_model>`: player 1 (P1) AI model
- `-1 <ai_model>`: player 2 (CPU) AI model
- `-r <rounds>`: number of rounds per game
- `-l <loops>`: number of games to simulate
- `--seed=<seed>`: fixed random seed for reproducible runs
- `--log=<path>`: write watch logs under the specified base path

AI model values:

- `0`: Normal
- `1`: Hard
- `2`: Stenv
- `3`: Mcenv

## Run: replay

The `replay` tool reconstructs a game from a `watch.log` file generated by `ai_sim` or [Battle Hanafuda](https://store.steampowered.com/app/4161340/Battle_Hanafuda/)'s **Watch Mode**.

```bash
./replay /path/to/watch.log
```

Behavior summary:

- If the log contains `Watch Start: ... seed=<seed>`, `replay` reruns the original seeded simulation.
- Otherwise it parses each round's `Floor:` card order and first actor (`[P1]` / `[CPU]`) and replays the recorded deal deterministically.
- Supported input is a single `watch.log` path. Parsing fails if required round metadata is missing or incomplete.

## License

[MIT](./LICENSE.txt)
