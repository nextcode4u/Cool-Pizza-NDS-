# ARM9 C build using devkitPro's DS linker, ROM tools and bundled ARM7 runtime.
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error Please set DEVKITARM to your devkitARM installation)
endif

GAME_TITLE     := Cool Pizza DS
GAME_SUBTITLE1 := Reconstruction prototype
GAME_SUBTITLE2 := Original by Secret Library
include $(DEVKITARM)/ds_rules

TARGET   := coolpizza
BUILD    := build
SOURCES  := source
INCLUDES := include
NITRO    := nitro

ARCH     := -march=armv5te -mtune=arm946e-s
CFLAGS   := -g -Wall -O2 -ffunction-sections -fdata-sections \
            $(ARCH) $(INCLUDE) -DARM9
LDFLAGS   = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lfilesystem -lfat -lmm9 -lnds9 -lm
LIBDIRS  := $(LIBNDS) $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export NITRO_FILES := $(CURDIR)/$(NITRO)
export LD := $(CC)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export OFILES := $(CFILES:.c=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).nds
else
# Changes to embedded audio must also trigger ROM repacking.
$(OUTPUT).nds: $(OUTPUT).elf $(shell find $(NITRO_FILES) -type f)
$(OUTPUT).elf: $(OFILES)
-include $(DEPSDIR)/*.d
endif
