# ----------------------------------------------------------- -*- mode: sh -*-
#  Directory definitions and pkg-config
# ----------------------------------------------------------------------------

# Tests specific paths
_TESTDATA       ?= /tmp/sailjail-daemon-tests
_TESTSDIR       ?= /opt/tests
_BINDIR         ?= $(_TESTSDIR)/sailjail-daemon/bin
_TESTDATADIR    ?= $(_TESTSDIR)/sailjail-daemon/data
_PREFIX         ?= $(_TESTSDIR)/sailjail-daemon/usr
_SYSCONFDIR     ?= $(_TESTSDIR)/sailjail-daemon/etc
_LOCALSTATEDIR  ?= $(_TESTDATA)/var

include ../Makefile.inc

CPPFLAGS += -DTESTDATADIR='"${_TESTDATADIR}"' -I..

# ----------------------------------------------------------------------------
#  Tests
# ----------------------------------------------------------------------------

TARGETS_BIN += test_appinfo
TARGETS_BIN += test_permissions
TARGETS_BIN += test_settings
TARGETS_BIN += test_stringset
TARGETS_BIN += test_util

TARGETS += ${TARGETS_BIN}

# ----------------------------------------------------------------------------
#  Static test data
# ----------------------------------------------------------------------------

DATA_FILES += data/keyfile1.ini
DATA_FILES += data/keyfile2.ini
DATA_FILES += data/user-1000.settings

APPLICATIONS_FILES += $(wildcard sailjail/applications/*.desktop)

# ----------------------------------------------------------------------------
# Top level targets
# ----------------------------------------------------------------------------

.PHONY: build install install-static clean mostlyclean

build:: $(TARGETS)

install:: build install-static install-tests
	install -d ${DESTDIR}${_BINDIR}
	install -m755 ${TARGETS_BIN} ${DESTDIR}${_BINDIR}

install-static:: install-data install-applications

clean:: mostlyclean
	$(RM) $(TARGETS)

mostlyclean::
	$(RM) *.o *~ *.bak */*.o */*~ */*.bak *.gcda *.gcno
	$(RM) -r $(PWD)/coverage $(PWD)/report/

# ----------------------------------------------------------------------------
# Testing targets
# ----------------------------------------------------------------------------

.PHONY: run coverage coverage-run coverage-build

# Runs without coverage
run:: _SYSCONFDIR := $(CURDIR)
run:: _DATADIR := $(CURDIR)/sailjail
run:: _TESTDATADIR := $(CURDIR)/data
run:: build copy-test-data run-tests

# Runs with coverage
coverage-run:: _SYSCONFDIR := $(CURDIR)
coverage-run:: _DATADIR := $(CURDIR)/sailjail
coverage-run:: _TESTDATADIR := $(CURDIR)/data
coverage-run:: coverage-build copy-test-data run-tests

# Builds with coverage on
coverage-build:: CFLAGS += --coverage
coverage-build:: build

# Generates report
coverage:: coverage-run
	lcov $(LCOVFLAGS) -c -d . -b $(CURDIR) -o $(PWD)/coverage
	genhtml $(GENHTMLFLAGS) -o $(PWD)/report $(PWD)/coverage
	@echo file://$(PWD)/report/index.html

# Generates report with branch coverage
branches:: LCOVFLAGS += --rc lcov_branch_coverage=1
branches:: GENHTMLFLAGS += --branch-coverage
branches:: coverage

# Keep in sync with tests.xml preparations
copy-test-data:: DESTDIR=
copy-test-data:: data/user-1000.settings install-permissions
	$(RM) -rf $(_TESTDATA)
	install -d $(_SHAREDSTATEDIR)/sailjail/settings
	install -m755 data/user-1000.settings $(_SHAREDSTATEDIR)/sailjail/settings/

run-tests:: $(TARGETS_BIN)
	$(foreach test,$(TARGETS_BIN),./$(test);)

# ----------------------------------------------------------------------------
# Static file targets
# ----------------------------------------------------------------------------

install-data:: $(DATA_FILES)
	install -d ${DESTDIR}/${_TESTDATADIR}
	install -m755 $^ ${DESTDIR}/${_TESTDATADIR}

install-applications:: $(APPLICATIONS_FILES)
	install -d ${DESTDIR}/${_DATADIR}/applications
	install -m755 $^ ${DESTDIR}/${_DATADIR}/applications

install-tests:: tests.xml.in
	install -d ${DESTDIR}/${_TESTSDIR}
	sed -e 's|@TESTDATA@|${_TESTDATA}|g' \
		-e 's|@TESTSDIR@|${_TESTSDIR}|g' \
		-e 's|@SHAREDSTATEDIR@|${_SHAREDSTATEDIR}|g' \
		-e 's|@BINDIR@|${_BINDIR}|g' \
		tests.xml.in > ${DESTDIR}/${_TESTSDIR}/sailjail-daemon/tests.xml

install-permissions::
	install -d ${DESTDIR}${_SYSCONFDIR}/sailjail/permissions
	touch ${DESTDIR}${_SYSCONFDIR}/sailjail/permissions/Audio.permission
	touch ${DESTDIR}${_SYSCONFDIR}/sailjail/permissions/Base.permission
	touch ${DESTDIR}${_SYSCONFDIR}/sailjail/permissions/Camera.permission
	touch ${DESTDIR}${_SYSCONFDIR}/sailjail/permissions/Contacts.permission
	touch ${DESTDIR}${_SYSCONFDIR}/sailjail/permissions/some-app.profile

# ----------------------------------------------------------------------------
# Mocking flags
# ----------------------------------------------------------------------------

test_appinfo: private CFLAGS += \
	-Wl,--wrap=applications_control \
	-Wl,--wrap=applications_config \
	-Wl,--wrap=config_stringset \
	-Wl,--wrap=config_boolean \
	-Wl,--wrap=control_available_permissions

test_permissions: private CFLAGS += \
	-Wl,--wrap=control_on_permissions_change

test_settings: private CFLAGS += \
	-Wl,--wrap=control_min_user \
	-Wl,--wrap=control_max_user \
	-Wl,--wrap=control_user_is_guest\
	-Wl,--wrap=control_valid_user \
	-Wl,--wrap=control_valid_application \
	-Wl,--wrap=control_available_permissions \
	-Wl,--wrap=control_appinfo\
	-Wl,--wrap=control_on_settings_change \
	-Wl,--wrap=config_string \
	-Wl,--wrap=config_stringset \
	-Wl,--wrap=config_boolean \
	-Wl,--wrap=applications_control \
	-Wl,--wrap=applications_config \
	-Wl,--wrap=migrator_create \
	-Wl,--wrap=migrator_delete_at \
	-Wl,--wrap=migrator_on_settings_saved

# ----------------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------------

test_appinfo: appinfo.o stringset.o util.o logging.o

test_permissions: permissions.o stringset.o util.o logging.o

test_settings: settings.o appinfo.o stringset.o util.o logging.o

test_stringset: stringset.o

test_util: util.o logging.o stringset.o

# ----------------------------------------------------------------------------
# Implicit rules
# ----------------------------------------------------------------------------

# Pick files from parent directory
%.o: ../%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ -c
