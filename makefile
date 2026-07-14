CC=arm-none-eabi-gcc -mcpu=cortex-m0plus -march=armv6-m
AS=arm-none-eabi-as -mcpu=cortex-m0plus -march=armv6-m
OBJDUMP=arm-none-eabi-objdump
CCFLAGS=--std=c23 -Wall -Wextra -pedantic -Werror -fmerge-all-constants -fipa-cp-clone
ASFLAGS=
LDFLAGS=-Wl,--nostdlib -nolibc -nostartfiles -ffreestanding
BIN=image.elf
SRC_DIR=src
BUILD_DIR=build
DEBUG?=0

ifeq ($(DEBUG), 1)
	BUILD_DIR:=$(BUILD_DIR)/debug
	CCFLAGS+=-Og -g3 -DDEBUG
	ASFLAGS+=-g
else
	BUILD_DIR:=$(BUILD_DIR)/release
	CCFLAGS+=-O3
endif

GENERATED_C_FILES=$(SRC_DIR)/board_usb_descriptors.c $(SRC_DIR)/board_usb_endpoints.c
GENERATED_H_FILES=$(SRC_DIR)/board_usb_descriptors.h $(SRC_DIR)/board_usb_endpoints.h
C_FILES=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/drivers/*.c) $(wildcard $(SRC_DIR)/util/*.c) $(GENERATED_C_FILES)
S_FILES=$(wildcard $(SRC_DIR)/*.s)
O_FILES=$(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/s_%.o,$(S_FILES)) $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_FILES))

$(BUILD_DIR)/s_%.o: $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(GENERATED_H_FILES)
	$(CC) $(CCFLAGS) -o $@ -c $<

$(BUILD_DIR)/$(BIN): $(O_FILES)
	$(CC) $(LDFLAGS) -o $@ $^ -T link.ld
	$(OBJDUMP) -xhD $@ > $(BUILD_DIR)/disas

$(SRC_DIR)/board_usb_descriptors.c $(SRC_DIR)/board_usb_descriptors.h: tools/usb_desc_gen.py
	python3 $< $(basename $@)

$(SRC_DIR)/board_usb_endpoints.c $(SRC_DIR)/board_usb_endpoints.h: tools/usb_malloc.py
	python3 $< $(basename $@)

.PHONY: build
.PHONY: clean
build:
	mkdir -p $@ $@/debug $@/debug/drivers $@/debug/util $@/release $@/release/drivers $@/release/util

clean:
	rm -f $(BUILD_DIR)/$(BIN) $(BUILD_DIR)/disas $(O_FILES) $(GENERATED_C_FILES) $(GENERATED_H_FILES)
