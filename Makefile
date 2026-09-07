CC      = g++
CFLAGS  = -Wall -Wextra -D_GNU_SOURCE -Iinclude
TARGET  = cache
BUILD_DIR = build
SRCS    = src/main.cpp src/config.cpp src/test.cpp src/cache.cpp
OBJS    = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

run: all
	@./$(TARGET)

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all run clean