ZIG_CXX ?= zig c++
HOST_CXX ?= g++

COMMON_SRCS = src/main_dll.cpp src/chaos_events.cpp src/chaos_catalog.cpp src/chaos_features.cpp src/death_counter.cpp
INCLUDES    = -Iinclude -Isrc
COMMON_HEADERS = include/hlsdk_mini.h include/hlsdk_pmove.h src/chaos_events.h src/chaos_features.h src/chaos_types.h src/chaos_catalog.h src/death_counter.h src/dll_bridge.h

.PHONY: all win32 linux test test-x86 test-x86-glibc test-loader-x86 clean

all: win32 linux

win32: hl_randomizer/dlls/hl.dll hl_randomizer_opfor/dlls/hl.dll hl_randomizer_opfor/install_hev_hud.exe

linux: hl_randomizer/dlls/hl.so hl_randomizer_opfor/dlls/hl.so hl_randomizer_opfor/install_hev_hud_linux

# Local overlay helper: it only reads game-owned sprites at install time.
hl_randomizer_opfor/install_hev_hud.exe: tools/install_hev_hud.cpp
	$(ZIG_CXX) -target x86-windows-gnu -std=c++14 -O2 -s -static -fno-exceptions -fno-rtti -fno-builtin-memchr $< -o $@
	@rm -f hl_randomizer_opfor/install_hev_hud.lib

hl_randomizer_opfor/install_hev_hud_linux: tools/install_hev_hud.cpp
	$(ZIG_CXX) -target x86_64-linux-gnu.2.17 -std=c++14 -O2 -s -fno-exceptions -fno-rtti $< -o $@

# Both profiles use the exact same proxy; its directory selects Valve or OF.
hl_randomizer_opfor/dlls/hl.dll: hl_randomizer/dlls/hl.dll
	@mkdir -p hl_randomizer_opfor/dlls
	cp $< $@

hl_randomizer_opfor/dlls/hl.so: hl_randomizer/dlls/hl.so
	@mkdir -p hl_randomizer_opfor/dlls
	cp $< $@

hl_randomizer/dlls/hl.dll: $(COMMON_SRCS) src/dll_bridge_win32.cpp src/hl.def src/hl_entity_list.inc src/opfor_entity_list.inc $(COMMON_HEADERS)
	@mkdir -p hl_randomizer/dlls
	$(ZIG_CXX) -target x86-windows-gnu -O2 -s -fno-exceptions -fno-rtti -nostdlib++ \
		-D_WIN32 -DWIN32 $(INCLUDES) -shared \
		$(COMMON_SRCS) src/dll_bridge_win32.cpp src/hl.def \
		-o hl_randomizer/dlls/hl.dll
	@rm -f hl_randomizer/dlls/main_dll.lib hl_randomizer/dlls/hl.lib

hl_randomizer/dlls/hl.so: $(COMMON_SRCS) src/dll_bridge_linux.cpp src/hl_entity_list.inc src/opfor_entity_list.inc $(COMMON_HEADERS)
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
	python3 tests/test_bridge_loader.py
	python3 tests/test_pe_and_abi.py
	python3 tests/test_opfor_setup.py
	$(HOST_CXX) -std=c++14 -O2 -Wall -Wextra tools/install_hev_hud.cpp -o /tmp/hl_chaos_test_bin/install_hev_hud
	python3 tests/test_hev_hud_setup.py /tmp/hl_chaos_test_bin/install_hev_hud
	python3 tests/test_hev_hud_setup.py hl_randomizer_opfor/install_hev_hud_linux

test-x86:
	@mkdir -p /tmp/hl_chaos_test_bin
	$(ZIG_CXX) -target x86-linux-musl -static -std=c++14 -O1 -DCHAOS_STANDALONE_SIMULATION $(INCLUDES) \
		tests/test_chaos_simulation.cpp $(COMMON_SRCS) -lm \
		-o /tmp/hl_chaos_test_bin/test_chaos_simulation_x86
	/tmp/hl_chaos_test_bin/test_chaos_simulation_x86

# Test the same 32-bit glibc ABI as the shipped Linux proxy. A private runtime
# can be supplied with CHAOS_X86_LD and CHAOS_X86_LIBDIR.
test-x86-glibc:
	@mkdir -p /tmp/hl_chaos_test_bin
	$(ZIG_CXX) -target x86-linux-gnu.2.17 -std=c++14 -O1 -fPIC -pie -DCHAOS_STANDALONE_SIMULATION $(INCLUDES) \
		tests/test_chaos_simulation.cpp $(COMMON_SRCS) -lm \
		-o /tmp/hl_chaos_test_bin/test_chaos_simulation_x86_glibc
	@if [ -n "$$CHAOS_X86_LD" ]; then \
		test -n "$$CHAOS_X86_LIBDIR" && "$$CHAOS_X86_LD" --library-path "$$CHAOS_X86_LIBDIR" /tmp/hl_chaos_test_bin/test_chaos_simulation_x86_glibc; \
	else /tmp/hl_chaos_test_bin/test_chaos_simulation_x86_glibc; fi

# Requires a system i386 glibc, or CHAOS_X86_LD and CHAOS_X86_LIBDIR.
test-loader-x86: hl_randomizer/dlls/hl.so
	ZIG_CXX="$(ZIG_CXX)" python3 tests/test_bridge_loader.py --x86

clean:
	rm -rf /tmp/hl_chaos_test_bin
