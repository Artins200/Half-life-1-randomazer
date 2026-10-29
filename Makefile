ZIG_CXX ?= zig c++
HOST_CXX ?= g++

COMMON_SRCS = src/main_dll.cpp src/chaos_events.cpp src/death_counter.cpp
INCLUDES    = -Iinclude -Isrc

.PHONY: all win32 linux test clean

all: win32 linux

win32: hl_randomizer/dlls/hl.dll

linux: hl_randomizer/dlls/hl.so

hl_randomizer/dlls/hl.dll: $(COMMON_SRCS) src/dll_bridge_win32.cpp src/hl.def include/hlsdk_mini.h src/chaos_events.h src/death_counter.h
	@mkdir -p hl_randomizer/dlls
	$(ZIG_CXX) -target x86-windows-gnu -O2 -s -fno-exceptions -fno-rtti -nostdlib++ \
		-D_WIN32 -DWIN32 $(INCLUDES) -shared \
		$(COMMON_SRCS) src/dll_bridge_win32.cpp src/hl.def \
		-o hl_randomizer/dlls/hl.dll
	@rm -f hl_randomizer/dlls/main_dll.lib hl_randomizer/dlls/hl.lib

hl_randomizer/dlls/hl.so: $(COMMON_SRCS) src/dll_bridge_linux.cpp src/hl_entity_list.inc include/hlsdk_mini.h src/chaos_events.h src/death_counter.h
	@mkdir -p hl_randomizer/dlls
	$(ZIG_CXX) -target x86-linux-gnu.2.17 -O2 -s -fno-exceptions -fno-rtti -nostdlib++ \
		-fPIC -fvisibility=hidden $(INCLUDES) -shared \
		$(COMMON_SRCS) src/dll_bridge_linux.cpp -ldl -lm -lc \
		-o hl_randomizer/dlls/hl.so

test:
	@mkdir -p /tmp/hl_chaos_test_bin
	$(HOST_CXX) -std=c++14 -O2 -Wall -Wextra -DCHAOS_STANDALONE_SIMULATION $(INCLUDES) \
		tests/test_chaos_simulation.cpp $(COMMON_SRCS) -lm \
		-o /tmp/hl_chaos_test_bin/test_chaos_simulation
	/tmp/hl_chaos_test_bin/test_chaos_simulation
	python3 tests/test_pe_and_abi.py

clean:
	rm -rf /tmp/hl_chaos_test_bin
