ifeq ($(strip $(DEVKITPRO)),)
$(error Configura DEVKITPRO con la ruta a devkitPro)
endif

include $(DEVKITPRO)/libounce/switch2_rules

TARGET := TurnwaveNX2
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CPPFLAGS += -I$(LIBNX)/include
CFLAGS := $(ARCH) -O2 -Wall -Wextra -ffunction-sections
LDFLAGS := $(ARCH) -specs=$(LIBNX)/switch.specs -L$(LIBNX)/lib

.PHONY: all clean
all: $(TARGET).nro

$(TARGET).elf: source/main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< -lnx

$(TARGET).nacp:
	nacptool --create "Turnwave NX2" "Turnwave NX2 contributors" "1.2.0" $@

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp icon.jpg
	elf2nro $(TARGET).elf $@ --icon=icon.jpg --nacp=$(TARGET).nacp

clean:
	rm -f $(TARGET).elf $(TARGET).nacp $(TARGET).nro
