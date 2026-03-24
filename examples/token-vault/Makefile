PROGRAM   := token_vault
SRC_DIR   := src
BUILD_DIR := build
INCLUDE   := ../../include

SOLANA_VER := $(shell ls -d $(HOME)/.cache/solana/v*/platform-tools 2>/dev/null | sort -t/ -k7 -V | tail -1 | xargs dirname)
CLANG      ?= $(SOLANA_VER)/platform-tools/llvm/bin/clang
LLD        ?= $(SOLANA_VER)/platform-tools/llvm/bin/ld.lld

CFLAGS  := --target=sbf -fPIC -Oz -fno-builtin -fdata-sections -I$(INCLUDE)
LDFLAGS := -z notext -shared --Bdynamic --gc-sections $(INCLUDE)/bpf.ld --entry entrypoint

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
SO   := $(BUILD_DIR)/program.so

.PHONY: all clean

all: $(SO)

$(SO): $(OBJS)
	$(LLD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CLANG) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
