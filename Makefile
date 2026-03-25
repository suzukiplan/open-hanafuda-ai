CC := cc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Isrc/include -Isrc
LDFLAGS := -lm
BIN := ai_sym

SRC := \
	src/main.c \
	src/sim_runtime.c \
	src/sim_result.c \
	src/cards_cli.c \
	src/analyze_cli.c \
	src/game.c \
	src/global.c \
	src/turn_player.c \
	src/turn_enemy.c \
	src/winning_hands.c \
	src/ai.c \
	src/ai_native.c \
	src/ai_normal_drop.c \
	src/ai_normal_select.c \
	src/ai_normal_koikoi.c \
	src/ai_hard_drop.c \
	src/ai_hard_select.c \
	src/ai_hard_koikoi.c \
	src/ai_hard_rapacious_fallback.c \
	src/ai_stenv_drop.c \
	src/ai_stenv_select.c \
	src/ai_stenv_koikoi.c \
	src/ai_mcenv_drop.c \
	src/ai_mcenv_select.c \
	src/ai_mcenv_koikoi.c \
	src/ai_strategy.c

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)

run1k: $(BIN)
	./$(BIN) -0 0 -1 1 -r 12 -l 1000 --seed=1772851247

.PHONY: all clean
