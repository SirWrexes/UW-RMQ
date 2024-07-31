PROJECT_ROOT := .
BUILD_DIR := "${PROJECT_ROOT}"/build

CONFIG_OPTS := -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
CONFIG_OPTS += -H${PROJECT_ROOT}
CONFIG_OPTS += -B${BUILD_DIR}

BUILD_OPTS :=
BUILD_OPTS += --build ${BUILD_DIR}

BIN_NAME = $(shell cat .name.bin)
BIN = ${BUILD_DIR}/bin/$(shell cat .name.bin)

CMAKE_BUILD_TYPE ?= Debug

.DEFAULT_GOAL := run

.PHONY: configure
configure:
	cmake ${CONFIG_OPTS}

.PHONY: build
build: | configure
	cmake ${BUILD_OPTS}

.PHONY: run
run: | build
	${BIN}

.PHONY: clean
clean:
	rm -rf ${BUILD_DIR}
	rm -f .name.*

.PHONY: reconfigure
reconfigure: | clean
reconfigure: | configure

.PHONY: rebuild
rebuild: | reconfigure
rebuild: | build

.PHONY: rerun
rerun: | rebuild
rerun: | run
