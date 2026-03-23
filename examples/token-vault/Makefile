PROGRAM   := token_vault
SRC_DIR   := src
BUILD_DIR := build
INCLUDE   := ../../include

CLANG     ?= $(HOME)/.cache/solana/v1.51/platform-tools/llvm/bin/clang
LLD       ?= $(HOME)/.cache/solana/v1.51/platform-tools/llvm/bin/ld.lld

CFLAGS  := --target=sbf -fPIC -O2 -fno-builtin -I$(INCLUDE)
LDFLAGS := -z notext -shared --Bdynamic $(INCLUDE)/bpf.ld --entry entrypoint

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
SO   := $(BUILD_DIR)/program.so

.PHONY: all clean

all: $(SO)

$(SO): $(OBJS)
	$(LLD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
