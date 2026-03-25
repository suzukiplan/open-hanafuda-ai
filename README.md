# Open Hanafuda AI

## Overview

This directory contains an open-source snapshot of the Hanafuda AI simulator used in [Battle Hanafuda](https://store.steampowered.com/app/4161340/Battle_Hanafuda/).

The OSS package focuses on the CPU decision logic and the minimum game-side code required to run AI-vs-AI simulations on macOS. It is intended for AI behavior inspection, balance tuning, and reproducible local experiments.

## Coding Rules

As a fundamental principle, the game must be implemented in **full compliance** with the [Nintendo rules for Hanafuda Koi-Koi](https://www.nintendo.com/jp/others/hanafuda_kabufuda/howtoplay/koikoi/index.html) only, and no other local rule variations shall be supported.

AI hidden-info rule for `ai*.{c,h}`:

- **Never read the opponent hand directly via `own[opp]`.**
- **Never read unopened draw-pile contents via `floor`.**
- **Visible cards may be read directly: `own[player]`, `deck`, `invent`.**
- **Estimation is allowed only by inferring unseen cards from visible cards.**

## Requirements

As a general rule, this repository only accepts changes that improve the win rate produced by `make run1k` (for example, `57.60%` in the case below), and does not allow changes that reduce it.

```
% make run1k
./ai_sim -0 0 -1 1 -r 12 -l 1000 --seed=1772851247
SUMMARY games=1000 seed=1772851247 rounds=12
P1 model=Normal wins=413 avg_diff=-5.373 reason_7plus=1551 reason_opponent_koikoi=878 koikoi=4166 koikoi_success=2660 koikoi_success_rate=63.85%
CPU model=Hard wins=576 avg_diff=5.373 reason_7plus=1589 reason_opponent_koikoi=1308 koikoi=2870 koikoi_success=1872 koikoi_success_rate=65.23%
DRAW draws=11
Sake round summary: 1P=2011, CPU=2005
 - 1P detail: win=1030 (51.22%), average=9.80pts koikoi-cnt=1006 koikoi-win=661 koikoi-up=8.14pts
 - CPU detail: win=1187 (59.20%), average=10.15pts koikoi-cnt=912 koikoi-win=608 koikoi-up=9.84pts
KOIKOI_BASE6_PUSH P1=0/0/0 CPU=132/132/8
KOIKOI_LOCKED_THRESHOLD P1=0/0 CPU=12/1
WIN_RATE P1=41.30%(61.12pts) CPU=57.60%(60.88pts)   <-- MOST IMPORTANT: CPU WIN_RATE
- Round 1: P1=44.00%(7.80pts) CPU=52.20%(7.81pts)
- Round 2: P1=42.60%(8.14pts) CPU=53.50%(7.36pts)
- Round 3: P1=42.30%(9.31pts) CPU=53.40%(7.23pts)
- Round 4: P1=43.70%(8.96pts) CPU=52.30%(7.91pts)
- Round 5: P1=44.30%(7.43pts) CPU=50.20%(7.72pts)
- Round 6: P1=44.70%(7.83pts) CPU=50.90%(8.46pts)
- Round 7: P1=42.50%(8.61pts) CPU=53.70%(7.44pts)
- Round 8: P1=42.50%(7.83pts) CPU=52.80%(7.44pts)
- Round 9: P1=47.50%(7.58pts) CPU=48.90%(8.25pts)
- Round 10: P1=43.60%(7.53pts) CPU=51.00%(7.68pts)
- Round 11: P1=45.80%(8.24pts) CPU=50.00%(8.05pts)
- Round 12: P1=44.70%(6.56pts) CPU=50.90%(6.67pts)
Per Bias:
- GREEDY: win=1808 (60.3%), lose=1097 (36.6%), draw=95 (3.2%)
- DEFENSIVE: win=3791 (46.5%), lose=3959 (48.6%), draw=401 (4.9%)
- BALANCED: win=599 (70.6%), lose=226 (26.6%), draw=24 (2.8%)
```

## Specification Summary

- Target platform: macOS only
- Output binary: `ai_sym`
- Entry point: `src/main.c`
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
make -C oss
```

This produces the executable below:

```bash
./ai_sym
```

## Run

Example:

```bash
./ai_sym -0 0 -1 1 -r 12 -l 100
```

Main options:

- `-0 <ai_model>`: player 1 AI model
- `-1 <ai_model>`: player 2 AI model
- `-r <rounds>`: number of rounds per game
- `-l <loops>`: number of games to simulate
- `--seed=<seed>`: fixed random seed for reproducible runs
- `--log=<path>`: write watch logs under the specified base path

AI model values:

- `0`: Normal
- `1`: Hard
- `2`: Stenv
- `3`: Mcenv

## License

[MIT](./LICENSE.txt)
