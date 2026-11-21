BUILD_DIR ?= build/cmake
BUILD_TYPE ?= Release
CONFIG ?= config.cfg

.PHONY: build configure run test clean

build: configure
	@cmake --build "$(BUILD_DIR)" --parallel

configure:
	@cmake -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"

run: build
	@"$(BUILD_DIR)/modules/cache_exec" "$(CONFIG)"

test: build
	@ctest --test-dir "$(BUILD_DIR)" --output-on-failure

clean:
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		cmake --build "$(BUILD_DIR)" --target clean; \
	fi
