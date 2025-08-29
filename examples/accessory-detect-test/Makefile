all: accessory-detect-test.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/accessory-detect-test.o

accessory-detect-test.z64: N64_ROM_TITLE = "Accessory DETECT"

$(BUILD_DIR)/accessory-detect-test.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
