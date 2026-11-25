.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error Set DEVKITPPC to your devkitPPC directory)
endif
include $(DEVKITPPC)/wii_rules

TARGET := nsogc_probe
BUILD := build
SOURCES := source
INCLUDES := source

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export OFILES := $(CFILES:.c=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) -I$(LIBOGC_INC)
export LIBPATHS := -L$(LIBOGC_LIB)
export CFLAGS := -O2 -g -Wall -Wextra $(MACHDEP) $(INCLUDE)
export LDFLAGS := -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
export LIBS := -lwiiuse -lbte -logc -lm
.PHONY: all clean
all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
