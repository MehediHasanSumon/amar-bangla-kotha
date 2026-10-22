CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O3 -fPIC -Iinclude
AR ?= ar
ARFLAGS = rcs

BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

SRCS = src/utf8_util.cpp \
       src/transliterator.cpp \
       src/trie.cpp \
       src/dictionary.cpp \
       src/autocorrect.cpp \
       src/engine.cpp \
       src/c_api.cpp

OBJS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

STATIC_LIB = $(LIB_DIR)/libabk.a
SHARED_LIB = $(LIB_DIR)/libabk.so
CLI_BIN = $(BIN_DIR)/abk_cli
TEST_BIN = $(BIN_DIR)/abk_tests
BENCH_BIN = $(BIN_DIR)/abk_benchmark
FUZZ_BIN = $(BIN_DIR)/abk_fuzz

.PHONY: all clean run test benchmark smoke-test fuzz-test sanitize-test dirs install install-user uninstall

all: dirs $(STATIC_LIB) $(SHARED_LIB) $(CLI_BIN) $(TEST_BIN) $(BENCH_BIN)

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)

$(BUILD_DIR)/%.o: src/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/catch_main.o: tests/catch_main.cpp | dirs
	$(CXX) $(CXXFLAGS) -Itests -c $< -o $@

$(BUILD_DIR)/test_engine.o: tests/test_engine.cpp | dirs
	$(CXX) $(CXXFLAGS) -Itests -c $< -o $@

$(STATIC_LIB): $(OBJS) | dirs
	$(AR) $(ARFLAGS) $@ $^

$(SHARED_LIB): $(OBJS) | dirs
	$(CXX) $(CXXFLAGS) -shared -o $@ $^

$(CLI_BIN): cli/main.cpp $(STATIC_LIB) | dirs
	$(CXX) $(CXXFLAGS) $< $(STATIC_LIB) -o $@

$(TEST_BIN): $(BUILD_DIR)/catch_main.o $(BUILD_DIR)/test_engine.o $(STATIC_LIB) | dirs
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BENCH_BIN): tests/benchmark.cpp $(STATIC_LIB) | dirs
	$(CXX) $(CXXFLAGS) $< $(STATIC_LIB) -o $@

$(FUZZ_BIN): tests/fuzz_test.cpp $(STATIC_LIB) | dirs
	$(CXX) $(CXXFLAGS) $< $(STATIC_LIB) -o $@

run: $(CLI_BIN)
	./$(CLI_BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

benchmark: $(BENCH_BIN)
	./$(BENCH_BIN) tests/data/golden.tsv data/bangla_wordlist.txt

smoke-test: $(SHARED_LIB)
	python3 tests/smoke_test.py

fuzz-test: $(FUZZ_BIN)
	./$(FUZZ_BIN)

sanitize-test:
	@echo "Building and running under AddressSanitizer and UndefinedBehaviorSanitizer..."
	$(MAKE) clean
	$(MAKE) all fuzz-test CXXFLAGS="-std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fPIC -Iinclude"
	@echo "Running Catch2 Test Suite under ASan/UBSan..."
	./$(TEST_BIN)
	@echo "Running Fuzz Suite under ASan/UBSan..."
	./$(FUZZ_BIN)
	@echo "All sanitizer runs passed cleanly! Rebuilding release binaries..."
	$(MAKE) clean
	$(MAKE) all
	@echo "Running Release Smoke Test..."
	python3 tests/smoke_test.py
	@echo "Sanitizer and Release verification complete!"

PREFIX ?= /usr/local
USER_PREFIX ?= $(HOME)/.local
DATA_DIR ?= $(PREFIX)/share/amar-bangla-kotha
USER_DATA_DIR ?= $(USER_PREFIX)/share/amar-bangla-kotha

install: all
	@echo "Installing Amar Bangla Kotha system-wide to $(PREFIX)..."
	@mkdir -p $(PREFIX)/bin $(PREFIX)/lib $(PREFIX)/include/abk $(DATA_DIR)
	@cp $(CLI_BIN) $(PREFIX)/bin/abk
	@cp $(STATIC_LIB) $(PREFIX)/lib/
	@cp $(SHARED_LIB) $(PREFIX)/lib/
	@cp include/abk/*.h $(PREFIX)/include/abk/
	@cp data/bangla_wordlist.txt $(DATA_DIR)/
	@cp data/phonetic_rules.tsv $(DATA_DIR)/
	@echo "Installation complete! Run 'abk' from terminal."

install-user: all
	@echo "Installing Amar Bangla Kotha for current user to $(USER_PREFIX)..."
	@mkdir -p $(USER_PREFIX)/bin $(USER_PREFIX)/lib $(USER_PREFIX)/include/abk $(USER_DATA_DIR)
	@cp $(CLI_BIN) $(USER_PREFIX)/bin/abk
	@cp $(STATIC_LIB) $(USER_PREFIX)/lib/
	@cp $(SHARED_LIB) $(USER_PREFIX)/lib/
	@cp include/abk/*.h $(USER_PREFIX)/include/abk/
	@cp data/bangla_wordlist.txt $(USER_DATA_DIR)/
	@cp data/phonetic_rules.tsv $(USER_DATA_DIR)/
	@echo "Installation complete! Run 'abk' (make sure ~/.local/bin is in your PATH)."

install-gui: install-user
	@echo "Installing Avro-style GUI application..."
	@mkdir -p $(USER_PREFIX)/bin
	@mkdir -p $(USER_PREFIX)/share/applications
	@mkdir -p $(USER_PREFIX)/share/icons/hicolor/scalable/apps
	@cp gui/app.py $(USER_PREFIX)/bin/amar-bangla-kotha
	@chmod +x $(USER_PREFIX)/bin/amar-bangla-kotha
	@cp gui/icon.svg $(USER_PREFIX)/share/icons/hicolor/scalable/apps/amar-bangla-kotha.svg
	@cp gui/amar-bangla-kotha.desktop $(USER_PREFIX)/share/applications/amar-bangla-kotha.desktop
	@cp gui/amar-bangla-kotha.desktop $(HOME)/Desktop/"Amar Bangla Kotha.desktop"
	@chmod +x $(HOME)/Desktop/"Amar Bangla Kotha.desktop"
	@echo "================================================================="
	@echo "  Amar Bangla Kotha GUI installed successfully!"
	@echo "  • Open from Ubuntu App Grid: search 'Amar Bangla Kotha'"
	@echo "  • Or double click on Desktop icon: 'Amar Bangla Kotha.desktop'"
	@echo "  • Or type 'amar-bangla-kotha' in terminal"
	@echo "================================================================="

install-ibus: install-user
	@echo "Registering Amar Bangla Kotha as a Linux IBus OS Keyboard..."
	@mkdir -p $(USER_PREFIX)/share/ibus/component
	@cp bindings/linux/abk_ibus_engine.py $(USER_PREFIX)/bin/abk-ibus-daemon
	@chmod +x $(USER_PREFIX)/bin/abk-ibus-daemon
	@cp bindings/linux/amar-bangla-kotha.xml $(USER_PREFIX)/share/ibus/component/amar-bangla-kotha.xml
	@echo "IBus component registered in $(USER_PREFIX)/share/ibus/component/amar-bangla-kotha.xml"
	@echo "Restarting ibus..."
	@ibus restart >/dev/null 2>&1 || true
	@echo "Done! You can now add 'Amar Bangla Kotha (আমার বাংলা কথা)' from Settings -> Keyboard -> Input Sources."

uninstall:
	@echo "Uninstalling Amar Bangla Kotha..."
	@rm -f $(PREFIX)/bin/abk $(PREFIX)/lib/libabk.a $(PREFIX)/lib/libabk.so
	@rm -rf $(PREFIX)/include/abk $(DATA_DIR)
	@rm -f $(USER_PREFIX)/bin/abk $(USER_PREFIX)/lib/libabk.a $(USER_PREFIX)/lib/libabk.so
	@rm -rf $(USER_PREFIX)/include/abk $(USER_DATA_DIR)
	@echo "Uninstalled."

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
