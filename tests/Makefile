CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../lib/doctest -I../lib/json -I../source
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp ../source/platform/app_settings.cpp
BIN      := run

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
	rm -rf test-saves-tmp
