# ----------------------------------------------------------------------------
# Custom directories for tests
# ----------------------------------------------------------------------------
testsdir             = get_option('testsdir') / 'sailjail-daemon'
tests_bindir         = testsdir / 'bin'
tests_prefix         = testsdir / 'usr'
tests_datadir        = tests_prefix / 'share'

testdatadir          = testsdir / 'data'

testtmpdata          = get_option('testtmpdata')
tests_sysconfdir     = testtmpdata / 'etc'
tests_sharedstatedir = testtmpdata

summary({
    'testsdir' : testsdir,
    'bindir': tests_bindir,
    'testdatadir': testdatadir,
    'sysconfdir': tests_sysconfdir,
    'sharedstatedir' : tests_sharedstatedir
  },
  section : 'QA test paths')

# ----------------------------------------------------------------------------
# Static data
# ----------------------------------------------------------------------------
install_subdir('data',
  install_dir : testsdir)

install_subdir('sailjail/applications',
  install_dir : tests_datadir)

custom_target('tests.xml',
  input : 'tests.xml.in',
  output : 'tests.xml',
  capture : true,
  command : [sed, 
	'-e', 's|@TESTTMPDATA@|' + testtmpdata + '|g',
	'-e', 's|@TESTSDIR@|' + testsdir + '|g',
	'-e', 's|@TESTDATADIR@|' + testdatadir + '|g',
	'-e', 's|@SHAREDSTATEDIR@|' + tests_sharedstatedir + '|g',
	'-e', 's|@BINDIR@|' + tests_bindir + '|g',
	'@INPUT@'],
  install : true,
  install_dir : testsdir)

permission_files = [
  'Audio.permission', 
  'Base.permission',
  'Camera.permission',
  'Contacts.permission',
  'some-app.profile',
]
create_permission_files = custom_target('permissions',
  command : [touch, '@OUTPUT@'],
  output : permission_files,
  install : true,
  install_dir : testsdir / 'etc/sailjail/permissions')

# ----------------------------------------------------------------------------
# Tests for QA
# ----------------------------------------------------------------------------
test_options = [
    '-DBINDIR="' + tests_bindir + '"',
    '-DSYSCONFDIR="' + tests_sysconfdir + '"',
    '-DSHAREDSTATEDIR="' + tests_sharedstatedir + '"',
    '-DDATADIR="' + tests_datadir + '"',
    '-DTESTDATADIR="' + testdatadir + '"',
    '-DUNITTEST',
]

tests = [
  ['test_appinfo',
    [files('test_appinfo.c'), appinfo, stringset, util, logging],
    [
      '-Wl,--wrap=applications_control',
      '-Wl,--wrap=applications_config',
      '-Wl,--wrap=config_stringset',
      '-Wl,--wrap=config_boolean',
      '-Wl,--wrap=control_available_permissions',
    ]
  ],
  ['test_permissions',
    [files('test_permissions.c'), logging, permissions, stringset, util],
    [
      '-Wl,--wrap=control_on_permissions_change',
    ]
  ],
  ['test_sailjailclient',
    [files(['test_sailjailclient.c']), logging, sailjailclient, stringset, util],
    [
      '-Wl,--wrap=config_create',
      '-Wl,--wrap=config_delete',
      '-Wl,--wrap=main',
    ]
  ],
  ['test_settings',
    [files('test_settings.c'), appinfo, logging, settings, stringset, util],
    [
      '-Wl,--wrap=control_min_user',
      '-Wl,--wrap=control_max_user',
      '-Wl,--wrap=control_user_is_guest',
      '-Wl,--wrap=control_valid_user',
      '-Wl,--wrap=control_valid_application',
      '-Wl,--wrap=control_available_permissions',
      '-Wl,--wrap=control_appinfo',
      '-Wl,--wrap=control_on_settings_change',
      '-Wl,--wrap=config_string',
      '-Wl,--wrap=config_stringset',
      '-Wl,--wrap=config_boolean',
      '-Wl,--wrap=applications_control',
      '-Wl,--wrap=applications_config',
      '-Wl,--wrap=migrator_create',
      '-Wl,--wrap=migrator_delete_at',
      '-Wl,--wrap=migrator_on_settings_saved',
    ]
  ],
  ['test_stringset',
    [files('test_stringset.c'), stringset],
    [],
  ],
  ['test_util',
    [files('test_util.c'), logging, stringset, util],
    [],
  ],
]

# ----------------------------------------------------------------------------
# Tests for unit testing with 'meson test'
# ----------------------------------------------------------------------------
unit_sharedstatedir = meson.current_build_dir() / 'tmp/sailjail-daemon-tests'

unit_options = [
    '-DSYSCONFDIR="' + meson.current_build_dir() + '"',
    '-DSHAREDSTATEDIR="' + unit_sharedstatedir + '"',
    '-DDATADIR="' + meson.current_source_dir() / 'sailjail' + '"',
    '-DTESTDATADIR="' + meson.current_source_dir() / 'data' + '"',
    '-DUNITTEST',
]

unit_bins = {}

# The same preparation steps as in tests.xml.in
# These are built with custom_target commands to be able to use depends.

cleanup_test_dir = custom_target('unit_cleanup_test_dir',
  command : [rm, '-rf', unit_sharedstatedir],
  build_by_default : false,
  output : 'bogus_1')
create_test_dir = custom_target('unit_create_test_dir',
  depends : cleanup_test_dir,
  command : [mkdir, '-p',
    unit_sharedstatedir / 'sailjail/settings'],
  build_by_default : false,
  output : 'bogus_2')
copy_test_data = custom_target('unit_copy_test_dir',
  depends : create_test_dir,
  command : [cp,
    meson.current_source_dir() / 'data/user-1000.settings',
    unit_sharedstatedir / 'sailjail/settings/'
  ],
  build_by_default : false,
  output : 'bogus_3')
unit_permissions_dir = meson.current_build_dir() / 'sailjail' / 'permissions'
create_permissions_dir = custom_target('unit_create_permissions_dir',
  command : [mkdir, '-p', unit_permissions_dir],
  build_by_default : false,
  output : 'bogus_4')
permission_files_to_copy = []
foreach file : permission_files
  permission_files_to_copy += meson.current_build_dir() / file
endforeach
copy_permissions = custom_target('unit_copy_permissions',
  depends : [create_permission_files, create_permissions_dir],
  command : [cp, permission_files_to_copy, unit_permissions_dir],
  build_by_default : false,
  output : 'bogus_5')

unit_depends = [copy_test_data, copy_permissions]

# The same tests as in tests.xml.in
units = [
  ['util_strip', 'test_util', ['-p', '/sailjaild/util/strip'], 'util'],
  ['util_path', 'test_util', ['-p', '/sailjaild/util/path'], 'util'],
  ['util_change', 'test_util', ['-p', '/sailjaild/util/change'], 'util'],
  ['util_keyfile', 'test_util', ['-p', '/sailjaild/util/keyfile'], 'util'],
  ['stringset', 'test_stringset', [], 'stringset'],
  ['appinfo', 'test_appinfo', [], 'appinfo'],
  ['permissions', 'test_permissions', [], 'permissions'],
  ['settings', 'test_settings', ['-p', '/sailjaild/settings/settings'], 'settings'],
  ['sailjailclient', 'test_sailjailclient', [], 'sailjailclient'],
]
