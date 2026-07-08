all: flashramtest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/flashramtest.o

flashramtest.z64: N64_ROM_TITLE = "FlashRAM WRV Test"
flashramtest.z64: N64_ROM_SAVETYPE = flashram

$(BUILD_DIR)/flashramtest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64 *.v64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
