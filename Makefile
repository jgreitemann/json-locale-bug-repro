CXX       := clang++
CXXFLAGS  := -std=c++20 -Wall -Wextra -Wpedantic -Ithird-party/nlohmann-json/include
RELEASE_FLAGS   := -O2 -DNDEBUG
DEBUG_FLAGS     := -O0 -g3 -DDEBUG

NDEBUG ?= 0

ifneq ($(NDEBUG),0)
  CXXFLAGS += $(RELEASE_FLAGS)
  TARGET    := build/repro
else
  CXXFLAGS += $(DEBUG_FLAGS)
  TARGET    := build/repro-debug
endif

.PHONY: all clean release debug

all: $(TARGET) compile_commands.json

$(TARGET): main.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $<

compile_commands.json:
	@echo '[' > compile_commands.json
	@echo '  {' >> compile_commands.json
	@printf '    "directory": "%s",\n' "$(CURDIR)" >> compile_commands.json
	@printf '    "command": "%s %s -o %s %s",\n' "$(CXX)" "$(CXXFLAGS)" "$(TARGET)" "main.cpp" >> compile_commands.json
	@printf '    "file": "main.cpp"\n' >> compile_commands.json
	@echo '  }' >> compile_commands.json
	@echo ']' >> compile_commands.json

clean:
	rm -rf build compile_commands.json

release:
	$(MAKE) all NDEBUG=1

debug:
	$(MAKE) all NDEBUG=0
