ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to your devkitPro installation)
endif
include $(DEVKITPRO)/libounce/switch2_rules

TARGET := TurnwaveNX2
VERSION := 0.7.0-native-dev
PORTLIBS := $(DEVKITPRO)/portlibs/switch2
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CPPFLAGS += -Isource -I$(LIBNX)/include -I$(PORTLIBS)/include
CFLAGS := $(ARCH) -std=gnu11 -O2 -Wall -Wextra -ffunction-sections
LDFLAGS := $(ARCH) -specs=$(LIBNX)/switch.specs -L$(LIBNX)/lib -L$(PORTLIBS)/lib
LDLIBS := -lcurl -lz -lnx -lm
SOURCES := source/main.c source/wav.c source/audio.c source/catalog_json.c source/catalog_net.c source/library.c
ICON := $(if $(wildcard ../native/icon.jpg),../native/icon.jpg,native/icon.jpg)

.PHONY: all clean
all: $(TARGET).nro
$(TARGET).elf: $(SOURCES) build/ui.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
build/ui/home.rgba build/ui/font16.alpha build/ui/font16.widths build/ui/font22.alpha build/ui/font22.widths build/ui/font32.alpha build/ui/font32.widths &: design/render.py
	python3 $< build/ui
build/ui.o: source/ui.S build/ui/home.rgba build/ui/font16.alpha build/ui/font16.widths build/ui/font22.alpha build/ui/font22.widths build/ui/font32.alpha build/ui/font32.widths
	$(CC) $(ARCH) -c -o $@ $<
$(TARGET).nacp: Makefile
	nacptool --create "Turnwave NX2" "Alberto Mariscal" "$(VERSION)" $@
$(TARGET).nro: $(TARGET).elf $(TARGET).nacp $(ICON)
	elf2nro $(TARGET).elf $@ --icon=$(ICON) --nacp=$(TARGET).nacp
clean:
	rm -f $(TARGET).elf $(TARGET).nacp $(TARGET).nro
	rm -rf build
