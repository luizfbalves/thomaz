CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../lib/doctest -I../lib/json -I../source
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/mods/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) $(wildcard ../source/core/themes/*.cpp) $(wildcard ../source/core/sysmod/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/app_settings.cpp ../source/platform/saves/fake_cloud_save_client.cpp ../source/platform/saves/http_cloud_save_client.cpp ../source/platform/saves/save_backup_io.cpp ../source/platform/mods/mod_store.cpp ../source/platform/themes/theme_paths.cpp ../source/platform/sysmod/sysmod_store.cpp ../source/platform/themes/cfw_paths.cpp ../source/platform/themes/active_theme_store.cpp
BIN      := run

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
	rm -rf test-saves-tmp
