all: accessorytest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/accessorytest.o

accessorytest.z64: N64_ROM_TITLE = "Accessory Test"

$(BUILD_DIR)/accessorytest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
