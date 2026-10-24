# SaveNX's native build uses only public devkitPro libraries.
ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to the devkitPro installation directory)
endif
DEVKITA64 ?= $(DEVKITPRO)/devkitA64
PORTLIBS := $(DEVKITPRO)/portlibs/switch
LIBNX := $(DEVKITPRO)/libnx
CXX := $(DEVKITA64)/bin/aarch64-none-elf-g++
TOOLS := $(DEVKITPRO)/tools/bin
export PATH := $(PORTLIBS)/bin:$(TOOLS):$(PATH)
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CPPFLAGS := -D__SWITCH__ -Iinclude -isystem $(LIBNX)/include -isystem $(PORTLIBS)/include -isystem $(PORTLIBS)/include/freetype2
CXXFLAGS := $(ARCH) -std=c++20 -O2 -g -Wall -Wextra -Wno-misleading-indentation -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS := $(ARCH) -specs=$(LIBNX)/switch.specs -L$(LIBNX)/lib -L$(PORTLIBS)/lib -Wl,--gc-sections,-Map,build/SaveNX.map
LIBS = -Wl,--start-group $(shell $(PORTLIBS)/bin/sdl2-config --libs) $(shell $(PORTLIBS)/bin/curl-config --libs) -lfreetype -lharfbuzz -lminizip -ljson-c -lbz2 -lpng -lz -lnx -Wl,--end-group
OBJECTS := $(patsubst source/%.cpp,build/%.o,$(wildcard source/*.cpp))

.PHONY: all clean
all: SaveNX.nro
build/%.o: source/%.cpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(shell $(PORTLIBS)/bin/sdl2-config --cflags) $(shell $(PORTLIBS)/bin/curl-config --cflags) $(CXXFLAGS) -c $< -o $@
SaveNX.elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
SaveNX.nacp: Makefile
	$(TOOLS)/nacptool --create "SaveNX" "SaveNX contributors" "0.4.0" $@
SaveNX.nro: SaveNX.elf SaveNX.nacp icon.jpg $(shell find romfs -type f)
	$(TOOLS)/elf2nro SaveNX.elf $@ --icon=icon.jpg --nacp=SaveNX.nacp --romfsdir=romfs
clean:
	python3 -c "import shutil,pathlib; shutil.rmtree('build',ignore_errors=True); [p.unlink(missing_ok=True) for p in map(pathlib.Path,['SaveNX.elf','SaveNX.nro','SaveNX.nacp'])]"
-include $(OBJECTS:.o=.d)
