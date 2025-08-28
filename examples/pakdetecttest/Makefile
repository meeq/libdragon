all: pakdetecttest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/pakdetecttest.o

pakdetecttest.z64: N64_ROM_TITLE = "Pak DETECT Test"

$(BUILD_DIR)/pakdetecttest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
