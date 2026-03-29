CC := cc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Isrc/include -Isrc -D_USE_MATH_DEFINES
CPPFLAGS := -MMD -MP
LDFLAGS := -lm

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

COMMON_SRC := \
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

AI_SIM_SRC := src/main.c
REPLAY_SRC := src/replay.c

COMMON_OBJ := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
AI_SIM_OBJ := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(AI_SIM_SRC))
REPLAY_OBJ := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(REPLAY_SRC))
DEPS := $(COMMON_OBJ:.o=.d) $(AI_SIM_OBJ:.o=.d) $(REPLAY_OBJ:.o=.d)

BINARIES := ai_sim replay

all: $(BINARIES)

ai_sim: $(COMMON_OBJ) $(AI_SIM_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

replay: $(COMMON_OBJ) $(REPLAY_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BINARIES)

run1k: ai_sim
	./ai_sim -0 0 -1 1 -r 12 -l 1000 --seed=1772851247 >run1k.log
	@cat run1k.log

run100: ai_sim
	rm -rf run100.log
	mkdir run100.log
	./ai_sim -0 0 -1 1 -r 12 -l 100 --seed=1772851247 --log run100.log/watch.log > run100.txt
	cat run100.txt
	python3 extract_watch_csv.py run100.log >run100.csv

test:
	make all
	make run100
	make run1k

.PHONY: all clean run1k

-include $(DEPS)
