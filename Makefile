# -*- Mode: makefile-gmake -*-

.PHONY: all debug release pkgconfig clean install install-dev
.PHONY: debug_test_lib print_debug_test_lib
.PHONY: release_test_lib print_release_test_lib
.PHONY: coverage_test_lib print_coverage_test_lib

#
# Required packages
#

LIB_PKGS = libglibutil glib-2.0 gobject-2.0 gio-2.0
PKGS = $(LIB_PKGS)

#
# Default target
#

all:: debug release pkgconfig

#
# Sources
#

TEST_SRC = \
  jail_conf.c \
  jail_creds.c \
  jail_launch.c \
  jail_launch_hook.c \
  jail_plugin.c \
  jail_plugins.c \
  jail_rules.c

SRC = $(TEST_SRC) \
  jail_free.c \
  sailjail.c

HAVE_FIREJAIL ?= 1
ifneq ($(HAVE_FIREJAIL),0)
SRC += jail_run.c
endif

#
# Directories
#

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
SPEC_DIR = .
DEBUG_BUILD_DIR = $(BUILD_DIR)/debug
RELEASE_BUILD_DIR = $(BUILD_DIR)/release
COVERAGE_BUILD_DIR = $(BUILD_DIR)/coverage

LIBDIR ?= usr/lib
ABS_LIBDIR := $(shell echo /$(LIBDIR) | sed -r 's|/+|/|g')
DEFAULT_PLUGIN_DIR := $(ABS_LIBDIR)/sailjail/plugins

#
# Pull sailjail version from jail_version.h file
#

VERSION_FILE = $(INCLUDE_DIR)/jail_version.h
get_version = $(shell grep -E "^ *\\\#define +JAIL_VERSION_$1 +[0-9]+$$" $(VERSION_FILE) | sed "s|  *| |g" | cut -d " " -f 3)

VERSION_MAJOR = $(call get_version,MAJOR)
VERSION_MINOR = $(call get_version,MINOR)
VERSION_RELEASE = $(call get_version,RELEASE)

# Version for pkg-config
PCVERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_RELEASE)
#
# Tools and flags
#

CC = $(CROSS_COMPILE)gcc
LD = $(CC)
DEBUG_FLAGS = -g
RELEASE_FLAGS =
COVERAGE_FLAGS = -g
RELEASE_DEFS =
WARNINGS = -Wall -Wstrict-aliasing -Wunused-result
DEFINES += -DDEFAULT_PLUGIN_DIR='"$(DEFAULT_PLUGIN_DIR)"' \
  -DHAVE_FIREJAIL=$(HAVE_FIREJAIL)
INCLUDES = -I$(SRC_DIR) -I$(INCLUDE_DIR)
FULL_CFLAGS = -fPIC $(CFLAGS) $(DEFINES) $(WARNINGS) $(INCLUDES) \
  -MMD -MP $(shell pkg-config --cflags $(PKGS))
FULL_LDFLAGS = $(LDFLAGS) -Wl,-export-dynamic \
  -Wl,--version-script=$(SRC_DIR)/sailjail.map

KEEP_SYMBOLS ?= 0
ifneq ($(KEEP_SYMBOLS),0)
RELEASE_FLAGS += -g
endif

DEBUG_CFLAGS = $(DEBUG_FLAGS) $(FULL_CFLAGS) -DDEBUG
RELEASE_CFLAGS = $(RELEASE_FLAGS) $(FULL_CFLAGS) -O2
COVERAGE_CFLAGS = $(FULL_CFLAGS) $(COVERAGE_FLAGS) --coverage
DEBUG_LDFLAGS = $(DEBUG_FLAGS) $(FULL_LDFLAGS)
RELEASE_LDFLAGS = $(RELEASE_FLAGS) $(FULL_LDFLAGS)

LIBS = $(shell pkg-config --libs $(LIB_PKGS)) -ldl
DEBUG_LIBS = $(LIBS)
RELEASE_LIBS = $(LIBS)

#
# Files
#

PKGCONFIG_NAME = sailjail-plugin.pc
PKGCONFIG_TEMPLATE = $(PKGCONFIG_NAME).in
PKGCONFIG = $(BUILD_DIR)/$(PKGCONFIG_NAME)

DEBUG_OBJS = $(SRC:%.c=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_OBJS = $(SRC:%.c=$(RELEASE_BUILD_DIR)/%.o)
COVERAGE_OBJS = $(SRC:%.c=$(COVERAGE_BUILD_DIR)/%.o)

DEBUG_TEST_OBJS = $(TEST_SRC:%.c=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_TEST_OBJS = $(TEST_SRC:%.c=$(RELEASE_BUILD_DIR)/%.o)
COVERAGE_TEST_OBJS = $(TEST_SRC:%.c=$(COVERAGE_BUILD_DIR)/%.o)

TEST_LIB = libsailjail.a
DEBUG_TEST_LIB = $(DEBUG_BUILD_DIR)/$(TEST_LIB)
RELEASE_TEST_LIB = $(RELEASE_BUILD_DIR)/$(TEST_LIB)
COVERAGE_TEST_LIB = $(COVERAGE_BUILD_DIR)/$(TEST_LIB)

#
# Dependencies
#

DEPS = \
  $(DEBUG_OBJS:%.o=%.d) \
  $(RELEASE_OBJS:%.o=%.d)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

$(PKGCONFIG): | $(BUILD_DIR)
$(DEBUG_OBJS): | $(DEBUG_BUILD_DIR)
$(RELEASE_OBJS): | $(RELEASE_BUILD_DIR)
$(COVERAGE_OBJS): | $(COVERAGE_BUILD_DIR)

#
# Rules
#

EXE = sailjail
DEBUG_EXE = $(DEBUG_BUILD_DIR)/$(EXE)
RELEASE_EXE = $(RELEASE_BUILD_DIR)/$(EXE)

debug: $(DEBUG_EXE)

release: $(RELEASE_EXE)

pkgconfig: $(PKGCONFIG)

debug_test_lib: $(DEBUG_TEST_LIB)

release_test_lib: $(RELEASE_TEST_LIB)

coverage_test_lib: $(COVERAGE_TEST_LIB)

print_debug_test_lib:
	@echo $(DEBUG_TEST_LIB)

print_release_test_lib:
	@echo $(RELEASE_TEST_LIB)

print_coverage_test_lib:
	@echo $(COVERAGE_TEST_LIB)

clean::
	rm -fr $(BUILD_DIR)
	make -C unit clean

test:
	make -C unit test

$(BUILD_DIR):
	mkdir -p $@

$(DEBUG_BUILD_DIR):
	mkdir -p $@

$(RELEASE_BUILD_DIR):
	mkdir -p $@

$(COVERAGE_BUILD_DIR):
	mkdir -p $@

$(DEBUG_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $(DEBUG_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(RELEASE_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(COVERAGE_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $(COVERAGE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(DEBUG_EXE): $(DEBUG_OBJS)
	$(LD) $(DEBUG_LDFLAGS) $(DEBUG_OBJS) $(DEBUG_LIBS) -o $@

$(RELEASE_EXE): $(RELEASE_OBJS)
	$(LD) $(RELEASE_LDFLAGS) $(RELEASE_OBJS) $(RELEASE_LIBS) -o $@
ifeq ($(KEEP_SYMBOLS),0)
	strip $@
endif

$(PKGCONFIG): $(PKGCONFIG_TEMPLATE) $(VERSION_FILE)
	sed -e 's|@version@|$(PCVERSION)|g' $(PKGCONFIG_TEMPLATE) > $@

$(DEBUG_TEST_LIB): $(DEBUG_TEST_OBJS)
	$(AR) rc $@ $?

$(RELEASE_TEST_LIB): $(RELEASE_TEST_OBJS)
	$(AR) rc $@ $?

$(COVERAGE_TEST_LIB): $(COVERAGE_TEST_OBJS)
	$(AR) rc $@ $?

#
# Install
#

INSTALL = install
INSTALL_DIRS = $(INSTALL) -d
INSTALL_BIN = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644

INSTALL_BIN_DIR = $(DESTDIR)/usr/bin
INSTALL_INCLUDE_DIR = $(DESTDIR)/usr/include/sailjail
INSTALL_PKGCONFIG_DIR = $(DESTDIR)$(ABS_LIBDIR)/pkgconfig

install:: $(INSTALL_BIN_DIR)
	$(INSTALL_BIN) $(RELEASE_EXE) $(INSTALL_BIN_DIR)

install-dev: $(INSTALL_INCLUDE_DIR) $(INSTALL_PKGCONFIG_DIR)
	$(INSTALL_DATA) $(INCLUDE_DIR)/*.h $(INSTALL_INCLUDE_DIR)
	$(INSTALL_DATA) $(PKGCONFIG) $(INSTALL_PKGCONFIG_DIR)

$(INSTALL_BIN_DIR):
	$(INSTALL_DIRS) $@

$(INSTALL_INCLUDE_DIR):
	$(INSTALL_DIRS) $@

$(INSTALL_PKGCONFIG_DIR):
	$(INSTALL_DIRS) $@
