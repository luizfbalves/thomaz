CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../lib/doctest
SRCS     := test_main.cpp test_smoke.cpp
BIN      := run

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
