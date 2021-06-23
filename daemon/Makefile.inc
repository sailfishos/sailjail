# ----------------------------------------------------------- -*- mode: sh -*-
# Installation directories
# ----------------------------------------------------------------------------

# Dummy default install dir - override from packaging scripts
DESTDIR         ?= /tmp/sailjaild-test-install

# Standard install directories
_PREFIX         ?= /usr#                         # /usr
_INCLUDEDIR     ?= $(_PREFIX)/include#           # /usr/include
_EXEC_PREFIX    ?= $(_PREFIX)#                   # /usr
_BINDIR         ?= $(_EXEC_PREFIX)/bin#          # /usr/bin
_SBINDIR        ?= $(_EXEC_PREFIX)/sbin#         # /usr/sbin
_LIBEXECDIR     ?= $(_EXEC_PREFIX)/libexec#      # /usr/libexec
_LIBDIR         ?= $(_EXEC_PREFIX)/lib#          # /usr/lib
_SYSCONFDIR     ?= /etc#                         # /etc
_DATADIR        ?= $(_PREFIX)/share#             # /usr/share
_MANDIR         ?= $(_DATADIR)/man#              # /usr/share/man
_INFODIR        ?= $(_DATADIR)/info#             # /usr/share/info
_DEFAULTDOCDIR  ?= $(_DATADIR)/doc#              # /usr/share/doc
_LOCALSTATEDIR  ?= /var#                         # /var
_SHAREDSTATEDIR ?= $(_LOCALSTATEDIR)/lib#        # /var/lib
_UNITDIR        ?= /lib/systemd/system#
_USERUNITDIR    ?= /lib/systemd/user#
_TESTSDIR       ?= /opt/tests#                   # /opt/tests

# ----------------------------------------------------------------------------
# Default flags
# ----------------------------------------------------------------------------

CPPFLAGS += -DVERSION='"${VERSION}"'
CPPFLAGS += -DSYSCONFDIR='"${_SYSCONFDIR}"'
CPPFLAGS += -DSHAREDSTATEDIR='"${_SHAREDSTATEDIR}"'
CPPFLAGS += -DDATADIR='"${_DATADIR}"'

CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64

COMMON   += -Wall
COMMON   += -Os
#COMMON   += -O0
COMMON   += -g

CFLAGS   += $(COMMON)
CFLAGS   += -std=c99

CXXFLAGS += $(COMMON)

LDFLAGS  += -g

LDLIBS   += -Wl,--as-needed

# ----------------------------------------------------------------------------
# Flags from pkg-config
# ----------------------------------------------------------------------------

PKG_NAMES += glib-2.0
PKG_NAMES += gobject-2.0
PKG_NAMES += gio-2.0
PKG_NAMES += libsystemd

HAVE_LIBDBUSACCESS := $(strip $(shell pkg-config --exists libdbusaccess && echo y))

ifdef HAVE_LIBDBUSACCESS
PKG_NAMES += libdbusaccess
CPPFLAGS += -DHAVE_LIBDBUSACCESS
endif

maintenance  = normalize clean distclean mostlyclean
intersection = $(strip $(foreach w,$1, $(filter $w,$2)))
ifneq ($(call intersection,$(maintenance),$(MAKECMDGOALS)),)
PKG_CONFIG   ?= true
endif

ifneq ($(strip $(PKG_NAMES)),)
PKG_CONFIG   ?= pkg-config
PKG_CFLAGS   := $(shell $(PKG_CONFIG) --cflags $(PKG_NAMES))
PKG_LDLIBS   := $(shell $(PKG_CONFIG) --libs   $(PKG_NAMES))
PKG_CPPFLAGS := $(filter -D%,$(PKG_CFLAGS)) $(filter -I%,$(PKG_CFLAGS))
PKG_CFLAGS   := $(filter-out -I%, $(filter-out -D%, $(PKG_CFLAGS)))
endif

CPPFLAGS += $(PKG_CPPFLAGS)
CFLAGS   += $(PKG_CFLAGS)
LDLIBS   += $(PKG_LDLIBS)

# vim: set filetype=make:
