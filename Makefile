BUILD_DIR := build
BUILD_TYPE := Debug
JOBS := $(shell nproc)

.PHONY: all
all: build

.PHONY: build
build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json
	cmake --build $(BUILD_DIR) -j$(JOBS)

.PHONY: test
test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: rebuild
rebuild: clean build

.PHONY: run-search
run-search: build
	./$(BUILD_DIR)/search $(ARGS)

.PHONY: run-indexer
run-indexer: build
	./$(BUILD_DIR)/indexer $(ARGS)

.PHONY: benchmark
benchmark:
	BUILD_TYPE=Release ./benchmarks/run_benchmarks.sh

.PHONY: benchmark-quick
benchmark-quick:
	BUILD_TYPE=Release ./benchmarks/run_benchmarks.sh --quick --config benchmarks/config/quick.json
