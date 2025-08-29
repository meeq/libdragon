all: accessory-short-cmds-test.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/accessory-short-cmds-test.o

accessory-short-cmds-test.z64: N64_ROM_TITLE = "Accessory Short CMDs"

$(BUILD_DIR)/accessory-short-cmds-test.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
