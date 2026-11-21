ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to your devkitPro installation)
endif
include $(DEVKITPRO)/libounce/switch2_rules
TARGET := TurnwaveNX2
VERSION := 0.5.0-dev
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CPPFLAGS += -I$(LIBNX)/include
CFLAGS := $(ARCH) -O2 -Wall -Wextra -ffunction-sections
LDFLAGS := $(ARCH) -specs=$(LIBNX)/switch.specs -L$(LIBNX)/lib
.PHONY: all clean
all: $(TARGET).nro
$(TARGET).elf: source/main.c build/ui.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lnx
build/ui/home.rgba build/ui/font16.alpha build/ui/font16.widths build/ui/font22.alpha build/ui/font22.widths build/ui/font32.alpha build/ui/font32.widths &: design/render.py
	python3 $< build/ui
build/ui.o: source/ui.S build/ui/home.rgba build/ui/font16.alpha build/ui/font16.widths build/ui/font22.alpha build/ui/font22.widths build/ui/font32.alpha build/ui/font32.widths
	$(CC) $(ARCH) -c -o $@ $<
$(TARGET).nacp: Makefile
	nacptool --create "Turnwave NX2" "Turnwave NX2 contributors" "$(VERSION)" $@
$(TARGET).nro: $(TARGET).elf $(TARGET).nacp icon.jpg
	elf2nro $(TARGET).elf $@ --icon=icon.jpg --nacp=$(TARGET).nacp
clean:
	rm -f $(TARGET).elf $(TARGET).nacp $(TARGET).nro
	rm -rf build
