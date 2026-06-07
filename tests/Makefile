CXX      ?= g++
CXXFLAGS := -std=c++20 -Wall -Wextra -DDOCTEST_CONFIG_NO_POSIX_SIGNALS -I../lib/doctest -I../lib/json -I../source -I../lib/switchthemes
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/games/*.cpp) $(wildcard ../source/core/mods/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) $(wildcard ../source/core/themes/*.cpp) $(wildcard ../source/core/sysmod/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/app_settings.cpp ../source/platform/games/index_fetch_util.cpp ../source/platform/games/source_store.cpp ../source/platform/games/catalog_cache.cpp ../source/platform/games/http_source_sync_client.cpp ../source/platform/games/local_source.cpp ../source/platform/saves/fake_cloud_save_client.cpp ../source/platform/saves/http_cloud_save_client.cpp ../source/platform/saves/save_backup_io.cpp ../source/platform/mods/mod_store.cpp ../source/platform/themes/theme_paths.cpp ../source/platform/sysmod/sysmod_store.cpp ../source/platform/themes/cfw_paths.cpp ../source/platform/themes/active_theme_store.cpp ../source/platform/themes/szs_validate.cpp ../source/platform/fs_util.cpp ../source/platform/title_visibility_store.cpp ../lib/switchthemes/SarcLib/Yaz0.cpp ../lib/switchthemes/SarcLib/Sarc.cpp ../lib/switchthemes/BinaryReadWrite/Buffer.cpp
BIN      := run

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
	rm -rf test-saves-tmp
