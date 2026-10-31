#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

TOPDIR ?= $(CURDIR)

# Pulls in all the WUPS-aware build rules, RPXSPECS, WUPSSPECS,
# the powerpc-eabi toolchain paths and the elf -> wps conversion.
include $(DEVKITPRO)/wups/share/wups_rules

WUT_ROOT  := $(DEVKITPRO)/wut
WUPS_ROOT := $(DEVKITPRO)/wups

#-------------------------------------------------------------------------------
# Target / layout
#-------------------------------------------------------------------------------
TARGET   := SplatWeaponChanger
BUILD    := build
SOURCES  := src
DATA     :=
INCLUDES := src

#-------------------------------------------------------------------------------
# Compile options
#-------------------------------------------------------------------------------
OPTIMIZATION := -O2
DEBUG_DEFINES :=
ifneq ($(filter 1 VERBOSE,$(DEBUG)),)
OPTIMIZATION := -Og
DEBUG_DEFINES += -DDEBUG
endif
ifeq ($(DEBUG),VERBOSE)
DEBUG_DEFINES += -DVERBOSE
endif

CFLAGS := -g $(OPTIMIZATION) -fshort-wchar -mrvl -mcpu=750 -meabi -mhard-float \
          -ffast-math -fdata-sections -ffunction-sections -Wall -Wextra \
          -D_GNU_SOURCE -Wno-unused-parameter -Wno-strict-aliasing \
        $(DEBUG_DEFINES) $(MACHDEP)
CFLAGS += $(INCLUDE) -D__WIIU__ -D__WUT__ -D__WUPS__

CXXFLAGS := $(CFLAGS) -std=gnu++20 -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)

LDFLAGS  = -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map) $(WUPSSPECS)

# Plugin needs wups + wut + libnotifications (for on-screen debug toasts)
LIBS := -lwups -lwut -lnotifications

LIBDIRS := $(PORTLIBS) $(WUPS_ROOT) $(WUT_ROOT)

#-------------------------------------------------------------------------------
# Boilerplate pulled from wiiu-env/ftpiiu_plugin & TCPGecko-Plugin Makefiles.
# Do not edit below this point unless you really know what you're doing.
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
        $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
        $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
        @[ -d $@ ] || mkdir -p $@
        @$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
        @echo clean ...
        @rm -fr $(BUILD) $(TARGET).wps $(TARGET).elf $(TARGET).map

else

.PHONY: all
DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).wps

$(OUTPUT).wps: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)

-include $(DEPENDS)

endif
