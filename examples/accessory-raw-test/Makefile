all: accessory-raw-test.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/accessory-raw-test.o

accessory-raw-test.z64: N64_ROM_TITLE = "Accessory Raw"

$(BUILD_DIR)/accessory-raw-test.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
