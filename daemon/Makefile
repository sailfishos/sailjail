# ----------------------------------------------------------- -*- mode: sh -*-
# Package version
# ----------------------------------------------------------------------------

VERSION   ?= $(shell awk '/^Version:/ {print $$2}' ../rpm/sailjail.spec)

# ----------------------------------------------------------------------------
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
# List of targets to build
# ----------------------------------------------------------------------------

TARGETS_BIN += sailjaild
TARGETS_BIN += sailjail

## QUARANTINE TARGETS_PNG += sailjaild.png

TARGETS += ${TARGETS_BIN} ${TARGETS_PNG}

# ----------------------------------------------------------------------------
# Top level targets
# ----------------------------------------------------------------------------

.PHONY: build install clean distclean mostlyclean

build:: $(TARGETS)

install:: build
	install -d ${DESTDIR}${_BINDIR}
	install -m755 ${TARGETS_BIN} ${DESTDIR}${_BINDIR}

clean:: mostlyclean
	$(RM) $(TARGETS)

distclean:: clean
	$(RM) tags

mostlyclean::
	$(RM) *.o *~ *.bak */*.o */*~ */*.bak

# ----------------------------------------------------------------------------
# Default flags
# ----------------------------------------------------------------------------

CPPFLAGS += -DVERSION='"${VERSION}"'
CPPFLAGS += -DSYSCONFDIR='"${_SYSCONFDIR}"'
CPPFLAGS += -DSHAREDSTATEDIR='"${_SHAREDSTATEDIR}"'
CPPFLAGS += -DDATADIR='"${_DATADIR}"'
CPPFLAGS += -DBINDIR='"${_BINDIR}"'

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

# ----------------------------------------------------------------------------
# Implicit rules
# ----------------------------------------------------------------------------

%.so : %.o
	$(CC) -o $@ -shared $^ $(LDFLAGS) $(LDLIBS)

%.png : %.dot
	dot -Tpng $^ -o $@

# ----------------------------------------------------------------------------
# Explicit dependencies
# ----------------------------------------------------------------------------

# - - - - - - - - - - - - - - - - - - - -
# sailjaild
# - - - - - - - - - - - - - - - - - - - -

sailjaild_objs += sailjaild.o

sailjaild_objs += appinfo.o
sailjaild_objs += applications.o
sailjaild_objs += config.o
sailjaild_objs += control.o
sailjaild_objs += later.o
sailjaild_objs += logging.o
sailjaild_objs += mainloop.o
sailjaild_objs += migrator.o
sailjaild_objs += permissions.o
sailjaild_objs += prompter.o
sailjaild_objs += service.o
sailjaild_objs += session.o
sailjaild_objs += settings.o
sailjaild_objs += stringset.o
sailjaild_objs += users.o
sailjaild_objs += util.o

sailjaild : ${sailjaild_objs}

# - - - - - - - - - - - - - - - - - - - -
# sailjailclient
# - - - - - - - - - - - - - - - - - - - -

sailjailclient : sailjailclient.o util.o logging.o util.o stringset.o config.o

clean::
	$(RM) sailjailclient

# - - - - - - - - - - - - - - - - - - - -
# sailjail
# - - - - - - - - - - - - - - - - - - - -

sailjail : sailjailclient
	cp $< $@

# - - - - - - - - - - - - - - - - - - - -
# desktop_data
# - - - - - - - - - - - - - - - - - - - -

desktop_data : desktop_data.o util.o logging.o stringset.o

clean::
	$(RM) desktop_data

# ----------------------------------------------------------------------------
# Source code normalization
# ----------------------------------------------------------------------------

.PHONY: normalize
normalize::
	normalize_whitespace -M Makefile
	normalize_whitespace -a $(wildcard *.[ch] *.cc *.cpp)
	normalize_whitespace -a README $(wildcard *.dot)

# ----------------------------------------------------------------------------
# AUTOMATIC PROTOTYPE GENERFATION
# ----------------------------------------------------------------------------

.SUFFIXES: .q .p .g
.PRECIOUS: .q

ALL_SOURCES += appinfo.c
ALL_SOURCES += applications.c
ALL_SOURCES += config.c
ALL_SOURCES += control.c
ALL_SOURCES += desktop_data.c
ALL_SOURCES += later.c
ALL_SOURCES += logging.c
ALL_SOURCES += mainloop.c
ALL_SOURCES += migrator.c
ALL_SOURCES += permissions.c
ALL_SOURCES += prompter.c
ALL_SOURCES += sailjaild.c
ALL_SOURCES += service.c
ALL_SOURCES += session.c
ALL_SOURCES += settings.c
ALL_SOURCES += stringset.c
ALL_SOURCES += users.c
ALL_SOURCES += util.c

ALL_SOURCES += sailjailclient.c

ALL_HEADERS += appinfo.h
ALL_HEADERS += applications.h
ALL_HEADERS += config.h
ALL_HEADERS += control.h
ALL_HEADERS += later.h
ALL_HEADERS += logging.h
ALL_HEADERS += mainloop.h
ALL_HEADERS += permissions.h
ALL_HEADERS += prompter.h
ALL_HEADERS += service.h
ALL_HEADERS += session.h
ALL_HEADERS += settings.h
ALL_HEADERS += stringset.h
ALL_HEADERS += users.h
ALL_HEADERS += util.h

PROTO_CPPFLAGS += -D_Float32=float
PROTO_CPPFLAGS += -D_Float64=double
PROTO_CPPFLAGS += -D_Float128="long double"
PROTO_CPPFLAGS += -D_Float32x=float
PROTO_CPPFLAGS += -D_Float64x=double
PROTO_CPPFLAGS += -D_Float128x="long double"

%.q : CPPFLAGS += $(PROTO_CPPFLAGS)
%.q : %.c ; $(CC) -o $@ -E $< $(CPPFLAGS) -O
%.p : %.q prettyproto.groups ; cproto -s < $< | prettyproto.py > $@
%.g : %.q prettyproto.groups ; cproto    < $< | prettyproto.py > $@

distclean::
	$(RM) *.[qpg]

protos-pre: $(patsubst %.c,%.q,$(ALL_SOURCES))
	touch $@

protos-post: $(patsubst %.c,%.p,$(ALL_SOURCES)) $(patsubst %.c,%.g,$(ALL_SOURCES))
	touch $@

clean::
	$(RM) protos-pre protos-post

# ----------------------------------------------------------------------------
# AUTOMATIC HEADER DEPENDENCIES
# ----------------------------------------------------------------------------

.PHONY: depend
depend::
	@echo "Updating .depend"
	$(CC) -MM $(CPPFLAGS) $(ALL_SOURCES) |\
	./depend_filter.py > .depend

ifneq ($(MAKECMDGOALS),depend) # not while: make depend
ifneq (,$(wildcard .depend))   # not if .depend does not exist
include .depend
endif
endif

fixme:
	grep -nE '(FIXME|TODO|XXX)' -- *.[ch]

# ----------------------------------------------------------------------------
# REDUNDANT HEADERS
# ----------------------------------------------------------------------------

.SUFFIXES: %.trim
.PRECIOUS: %.trim

include_trim:: include_trim_headers include_trim_sources

%.trim : %
	find_unneeded_includes.py $(CPPFLAGS) $(CFLAGS) -- $<
	touch $@

include_trim_headers: $(patsubst %,%.trim,$(ALL_HEADERS))

include_trim_sources: $(patsubst %,%.trim,$(ALL_SOURCES))

distclean::
	$(RM) *.trim
