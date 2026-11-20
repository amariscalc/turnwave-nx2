ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to your devkitPro installation)
endif

include $(DEVKITPRO)/libounce/switch2_rules

TARGET := TurnwaveNX2
VERSION := 1.3.3
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CPPFLAGS += -I$(LIBNX)/include
CFLAGS := $(ARCH) -O2 -Wall -Wextra -ffunction-sections
LDFLAGS := $(ARCH) -specs=$(LIBNX)/switch.specs -L$(LIBNX)/lib

.PHONY: all clean
all: $(TARGET).nro

$(TARGET).elf: source/main.c build/ui.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lnx

build/ui/home.rgba build/ui/opening.rgba build/ui/failed.rgba build/ui/hex.alpha &: design/render.py
	python3 $< build/ui

build/ui.o: source/ui.S build/ui/home.rgba build/ui/opening.rgba build/ui/failed.rgba build/ui/hex.alpha
	$(CC) $(ARCH) -c -o $@ $<

$(TARGET).nacp: Makefile
	nacptool --create "Turnwave NX2" "Turnwave NX2 contributors" "$(VERSION)" $@

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp icon.jpg
	elf2nro $(TARGET).elf $@ --icon=icon.jpg --nacp=$(TARGET).nacp

clean:
	rm -f $(TARGET).elf $(TARGET).nacp $(TARGET).nro
	rm -rf build
