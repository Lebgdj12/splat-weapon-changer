# Makefile for Splat Weapon Changer Aroma plugin
# Compatible with:
#   - Local devkitPro install (DEVKITPRO env var)
#   - wiiuenv/cmlwut Docker image (used by .github/workflows/build.yml)
#     -> image already has devkitPPC, wut, WUPS, libwupsbackend preinstalled

# ----------------------------------------------------------------------------
# Toolchain paths (the Docker image sets these; falls back to /opt/devkitpro)
# ----------------------------------------------------------------------------
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
WUPS_ROOT ?= $(DEVKITPRO)/wups

PATH := $(DEVKITPPC)/bin:$(PATH)

# ----------------------------------------------------------------------------
# Tools
# ----------------------------------------------------------------------------
CC      := powerpc-eabi-gcc
CXX     := powerpc-eabi-g++
STRIP   := powerpc-eabi-strip
ELF2RPL := $(WUT_ROOT)/tools/elf2rpl

# ----------------------------------------------------------------------------
# Project
# ----------------------------------------------------------------------------
TARGET   := SplatWeaponChanger
SOURCES  := src
INCLUDES := src $(WUPS_ROOT)/include $(WUT_ROOT)/include
BUILD     = build

# ----------------------------------------------------------------------------
# Flags
# ----------------------------------------------------------------------------
CFLAGS   := -std=gnu11 -O2 -Wall -Wextra -fno-builtin -fpermissive \
            -D__WIIU__ -D__WUT__ -fno-strict-aliasing
CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra -fno-builtin -fpermissive \
            -fno-exceptions -fno-rtti -ffreestanding \
            -D__WIIU__ -D__WUT__ -fno-strict-aliasing

CFLAGS   += $(addprefix -I,$(INCLUDES))
CXXFLAGS += $(addprefix -I,$(INCLUDES))

# Linker
LDFLAGS  := -T$(WUT_ROOT)/rpl/rpl.ld -nostartfiles -Wl,--gc-sections
LIBS     := -lwupsbackend -lwut -lcoreinit -lproc_ui -lngn -lpthread -lm -lc

# ----------------------------------------------------------------------------
# Sources
# ----------------------------------------------------------------------------
CFILES   := $(foreach d,$(SOURCES),$(wildcard $(d)/*.c))
CPPFILES := $(foreach d,$(SOURCES),$(wildcard $(d)/*.cpp))
OFILES   := $(patsubst %.c,$(BUILD)/%.o,$(CFILES)) \
            $(patsubst %.cpp,$(BUILD)/%.o,$(CPPFILES))

# ----------------------------------------------------------------------------
# Rules
# ----------------------------------------------------------------------------
.PHONY: all clean install

all: $(TARGET).wps

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

$(TARGET).rpx: $(TARGET).elf
	$(ELF2RPL) $< $@

$(TARGET).wps: $(TARGET).rpx
	$(STRIP) --strip-unneeded $< -o $@
	@echo ""
	@echo "✓ Built: $(TARGET).wps"
	@echo "  Install to: sd:/wiiu/environments/aroma/plugins/$(TARGET).wps"

install: $(TARGET).wps
	@echo "Copy $(TARGET).wps to: sd:/wiiu/environments/aroma/plugins/"

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).rpx $(TARGET).wps
