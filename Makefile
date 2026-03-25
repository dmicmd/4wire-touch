PROJECT      := touchpad_ps2_mouse
BUILD_DIR    := build
TOOLCHAIN    ?= arm-none-eabi
CC           := $(TOOLCHAIN)-gcc
OBJCOPY      := $(TOOLCHAIN)-objcopy
SIZE         := $(TOOLCHAIN)-size

SPL_ROOT     := third_party/emdr1986x-std-per-lib
CMSIS_ROOT   := $(SPL_ROOT)/CMSIS/CM3
DEVICE_ROOT  := $(CMSIS_ROOT)/DeviceSupport/MDR32F9Qx
SPL_INC_ROOT := $(SPL_ROOT)/MDR32F9Qx_StdPeriph_Driver/inc
SPL_SRC_ROOT := $(SPL_ROOT)/MDR32F9Qx_StdPeriph_Driver/src

CPUFLAGS     := -mcpu=cortex-m3 -mthumb
OPTFLAGS     ?= -O2
WARNFLAGS    := -Wall -Wextra -Wshadow -Wredundant-decls -Wno-missing-field-initializers
CSTD         := -std=c99
DEFS         := -DUSE_MDR1986VE9x -D__STARTUP_CLEAR_BSS -D__START=main

INCLUDES := \
	-Iinclude \
	-Iexamples \
	-I$(SPL_INC_ROOT) \
	-I$(SPL_ROOT)/Config \
	-I$(CMSIS_ROOT)/CoreSupport \
	-I$(DEVICE_ROOT)/inc \
	-I$(DEVICE_ROOT)/startup

CFLAGS := $(CPUFLAGS) $(OPTFLAGS) $(WARNFLAGS) $(CSTD) $(DEFS) $(INCLUDES) -ffunction-sections -fdata-sections
ASFLAGS := $(CPUFLAGS) $(OPTFLAGS) $(DEFS) $(INCLUDES) -x assembler-with-cpp

LDSCRIPT := $(DEVICE_ROOT)/startup/gcc/MDR32F9Qx.ld
LDFLAGS  := $(CPUFLAGS) -T$(LDSCRIPT) --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map -nostartfiles

APP_SRCS := \
	firmware/main.c \
	src/rtouch.c \
	src/ps2_mouse.c \
	examples/mdr1986ve92qi_touch_ps2_mouse.c

SPL_SRCS := \
	$(DEVICE_ROOT)/startup/system_MDR32F9Qx.c \
	$(SPL_SRC_ROOT)/MDR32F9Qx_rst_clk.c \
	$(SPL_SRC_ROOT)/MDR32F9Qx_port.c \
	$(SPL_SRC_ROOT)/MDR32F9Qx_adc.c \
	$(SPL_SRC_ROOT)/MDR32F9Qx_timer.c

STARTUP_SRC := $(DEVICE_ROOT)/startup/gcc/startup_MDR32F9Qx.S

SRCS := $(APP_SRCS) $(SPL_SRCS)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS)) $(BUILD_DIR)/$(STARTUP_SRC:.S=.o)

.PHONY: all clean size

all: $(BUILD_DIR)/$(PROJECT).hex

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROJECT).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@
	$(OBJCOPY) -O binary $< $(BUILD_DIR)/$(PROJECT).bin
	$(SIZE) $<

clean:
	rm -rf $(BUILD_DIR)

size: $(BUILD_DIR)/$(PROJECT).elf
	$(SIZE) $<
