# -*- Mode: makefile-gmake -*-

.PHONY: clean all debug release coverage debug_lib release_lib coverage_lib

#
# Real test makefile defines EXE (and possibly SRC) and includes this one.
#

ifndef EXE
${error EXE not defined}
endif

SRC ?= $(EXE).c
COMMON_SRC ?= test_common.c

#
# Required packages
#

PKGS += libglibutil glib-2.0 gio-2.0 gobject-2.0

#
# Default target
#

all: debug release

#
# Directories
#

SRC_DIR = .
LIB_DIR = ../..
COMMON_DIR = ../common
BUILD_DIR = build
DEBUG_BUILD_DIR = $(BUILD_DIR)/debug
RELEASE_BUILD_DIR = $(BUILD_DIR)/release
COVERAGE_BUILD_DIR = $(BUILD_DIR)/coverage

#
# Tools and flags
#

CC = $(CROSS_COMPILE)gcc
LD = $(CC)
WARNINGS += -Wall
INCLUDES += -I$(COMMON_DIR) -I$(LIB_DIR)/src -I$(LIB_DIR)/include
BASE_FLAGS = -fPIC
BASE_LDFLAGS = $(BASE_FLAGS) $(LDFLAGS)
BASE_CFLAGS = $(BASE_FLAGS) $(CFLAGS)
FULL_CFLAGS = $(BASE_CFLAGS) $(DEFINES) $(WARNINGS) $(INCLUDES) -MMD -MP \
  $(shell pkg-config --cflags $(PKGS))
FULL_LDFLAGS = $(BASE_LDFLAGS)
LIBS = $(shell pkg-config --libs $(PKGS)) -ldl
QUIET_MAKE = make --no-print-directory
DEBUG_FLAGS = -g
RELEASE_FLAGS =
COVERAGE_FLAGS = -g

DEBUG_LDFLAGS = $(FULL_LDFLAGS) $(DEBUG_FLAGS)
RELEASE_LDFLAGS = $(FULL_LDFLAGS) $(RELEASE_FLAGS)
COVERAGE_LDFLAGS = $(FULL_LDFLAGS) $(COVERAGE_FLAGS) --coverage

DEBUG_CFLAGS = $(FULL_CFLAGS) $(DEBUG_FLAGS) -DDEBUG
RELEASE_CFLAGS = $(FULL_CFLAGS) $(RELEASE_FLAGS) -O2
COVERAGE_CFLAGS = $(FULL_CFLAGS) $(COVERAGE_FLAGS) --coverage

#
# Files
#

DEBUG_OBJS = \
  $(SRC:%.c=$(DEBUG_BUILD_DIR)/%.o) \
  $(COMMON_SRC:%.c=$(DEBUG_BUILD_DIR)/common_%.o)
RELEASE_OBJS = \
  $(SRC:%.c=$(RELEASE_BUILD_DIR)/%.o) \
  $(COMMON_SRC:%.c=$(RELEASE_BUILD_DIR)/common_%.o)
COVERAGE_OBJS = \
  $(SRC:%.c=$(COVERAGE_BUILD_DIR)/%.o) \
  $(COMMON_SRC:%.c=$(COVERAGE_BUILD_DIR)/common_%.o)

DEBUG_LIB_FILE := $(shell $(QUIET_MAKE) -C $(LIB_DIR) print_debug_test_lib)
RELEASE_LIB_FILE := $(shell $(QUIET_MAKE) -C $(LIB_DIR) print_release_test_lib)
COVERAGE_LIB_FILE := $(shell $(QUIET_MAKE) -C $(LIB_DIR) print_coverage_test_lib)

DEBUG_LIB := $(LIB_DIR)/$(DEBUG_LIB_FILE)
RELEASE_LIB := $(LIB_DIR)/$(RELEASE_LIB_FILE)
COVERAGE_LIB := $(LIB_DIR)/$(COVERAGE_LIB_FILE)

#
# Dependencies
#

DEPS = $(DEBUG_OBJS:%.o=%.d) $(RELEASE_OBJS:%.o=%.d)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

$(DEBUG_LIB): | debug_lib
$(RELEASE_LIB): | release_lib
$(COVERAGE_LIB): | coverage_lib

$(DEBUG_OBJS): | $(DEBUG_BUILD_DIR)
$(RELEASE_OBJS): | $(RELEASE_BUILD_DIR)
$(COVERAGE_OBJS): | $(COVERAGE_BUILD_DIR)

#
# Rules
#

DEBUG_EXE = $(DEBUG_BUILD_DIR)/$(EXE)
RELEASE_EXE = $(RELEASE_BUILD_DIR)/$(EXE)
COVERAGE_EXE = $(COVERAGE_BUILD_DIR)/$(EXE)

debug: debug_lib $(DEBUG_EXE)

release: release_lib $(RELEASE_EXE)

coverage: coverage_lib $(COVERAGE_EXE)

unitclean:
	rm -f *~
	rm -fr $(BUILD_DIR)

clean: unitclean

cleaner: unitclean
	@make -C $(LIB_DIR) clean

test_banner:
	@echo "===========" $(EXE) "=========== "

test: test_banner debug
	@LD_LIBRARY_PATH="$(LIB_DIR)/$(DEBUG_LIB_PATH)" $(DEBUG_EXE)

valgrind: test_banner debug
	@LD_LIBRARY_PATH="$(LIB_DIR)/$(DEBUG_LIB_PATH)" G_DEBUG=gc-friendly G_SLICE=always-malloc valgrind --tool=memcheck --leak-check=full --show-possibly-lost=no $(DEBUG_EXE)

$(DEBUG_BUILD_DIR):
	mkdir -p $@

$(RELEASE_BUILD_DIR):
	mkdir -p $@

$(COVERAGE_BUILD_DIR):
	mkdir -p $@

$(DEBUG_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(COVERAGE_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(COVERAGE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_BUILD_DIR)/common_%.o : $(COMMON_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/common_%.o : $(COMMON_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(COVERAGE_BUILD_DIR)/common_%.o : $(COMMON_DIR)/%.c
	$(CC) -c $(COVERAGE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_EXE): $(DEBUG_LIB) $(DEBUG_BUILD_DIR) $(DEBUG_OBJS)
	$(LD) $(DEBUG_LDFLAGS) $(DEBUG_OBJS) $< $(LIBS) -o $@

$(RELEASE_EXE): $(RELEASE_LIB) $(RELEASE_BUILD_DIR) $(RELEASE_OBJS)
	$(LD) $(RELEASE_LDFLAGS) $(RELEASE_OBJS) $< $(LIBS) -o $@

$(COVERAGE_EXE): $(COVERAGE_LIB) $(COVERAGE_BUILD_DIR) $(COVERAGE_OBJS)
	$(LD) $(COVERAGE_LDFLAGS) $(COVERAGE_OBJS) $< $(LIBS) -o $@

debug_lib:
	@make $(SUBMAKE_OPTS) -C $(LIB_DIR) debug_test_lib

release_lib:
	@make $(SUBMAKE_OPTS) -C $(LIB_DIR) release_test_lib

coverage_lib:
	@make $(SUBMAKE_OPTS) -C $(LIB_DIR) coverage_test_lib
