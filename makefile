# generic makefile

BUILD_OUTPUT:=./bin/executable
BUILD_FLAGS:=-I/usr/include/ccommon/ -I./src/include -L/usr/lib/ccommon -lcclog -lccstd -Wall -ggdb3
BUILD_MAIN_FILE:=./src/main.c
BUILD_FILES:=$(wildcard ./src/*.c)
BUILD_FILES:=$(filter-out ${BUILD_MAIN_FILE}, $(BUILD_FILES))

TEST_OUTPUT:=./test/bin/testrun
TEST_MAIN_FILE:=./test/main.c
TEST_FILES:=$(wildcard ./test/*.c)
TEST_FILES:=$(filter-out ${TEST_MAIN_FILE}, $(TEST_FILES))

.PHONY: all
all: 
	$(MAKE) build
	$(MAKE) test

.PHONY: build
build: ${BUILD_FILES} ${BUILD_MAIN_FILE}
	bear -- gcc ${BUILD_MAIN_FILE} ${BUILD_FILES} -o ${BUILD_OUTPUT} ${BUILD_FLAGS}

run: ${BUILD_OUTPUT}
	${BUILD_OUTPUT}

debug: ${BUILD_OUTPUT}
	gf2 ${BUILD_OUTPUT}

.PHONY: test
test: ${TEST_MAIN_FILE} ${TEST_FILES} ${BUILD_FILES}
	gcc ${TEST_MAIN_FILE} ${TEST_FILES} ${BUILD_FILES} -o ${TEST_OUTPUT} ${BUILD_FLAGS}
	${TEST_OUTPUT}