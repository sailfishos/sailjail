# ----------------------------------------------------------------------------
# Common defines
# ----------------------------------------------------------------------------
common_args = [
  '-DBINDIR="' + bindir + '"',
  '-DDATADIR="' + datadir + '"',
  '-DSYSCONFDIR="' + sysconfdir + '"',
  '-DSHAREDSTATEDIR="' + sharedstatedir + '"',
]

# ----------------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------------

# Glib deps are used by daemon and client
glib_deps = [
  dependency('glib-2.0', version : '>= 2.44'),
  dependency('gobject-2.0'),
  dependency('gio-2.0'),
]

# These deps are only used by daemon
libdbusaccess = dependency('libdbusaccess', required : get_option('libdbusaccess'))
libsystemd    = dependency('libsystemd')

# ----------------------------------------------------------------------------
# Daemon
# ----------------------------------------------------------------------------
daemon_src = files([
  'sailjaild.c',
  'appinfo.c',
  'applications.c',
  'appservices.c',
  'config.c',
  'control.c',
  'later.c',
  'logging.c',
  'mainloop.c',
  'migrator.c',
  'permissions.c',
  'prompter.c',
  'service.c',
  'session.c',
  'settings.c',
  'stringset.c',
  'users.c',
  'util.c',
])

if libdbusaccess.found()
  daemon_args = [common_args, '-DHAVE_LIBDBUSACCESS']
else
  daemon_args = [common_args]
endif

executable('sailjaild',
  daemon_src,
  dependencies : [glib_deps, libsystemd, libdbusaccess],
  c_args : daemon_args,
  install : true,
  install_dir : bindir)

install_data(
  files('systemd/sailjaild.service'),
  install_dir : unitdir)

install_data(
  files('dbus/sailjaild.conf'),
  install_dir : dbuspolicydir)

# ----------------------------------------------------------------------------
# Client
# ----------------------------------------------------------------------------
client_src = files([
  'sailjailclient.c',
  'config.c',
  'logging.c',
  'stringset.c',
  'util.c',
])

executable('sailjail',
  client_src,
  dependencies : glib_deps,
  c_args : common_args,
  install : true,
  install_dir : bindir)

# ----------------------------------------------------------------------------
# Diagrams
# ----------------------------------------------------------------------------
if dot.found()
  custom_target('sailjaild.png',
    input : files('sailjaild.dot'),
    output: 'sailjaild.png',
    command: [dot, '-Tpng', '@INPUT@', '-o', '@OUTPUT@'],
    install : false)

  custom_target('dataflow.png',
    input : files('dataflow.dot'),
    output: 'dataflow.png',
    command: [dot, '-Tpng', '@INPUT@', '-o', '@OUTPUT@'],
    install : false)

  custom_target('prompter.png',
    input : files('prompter.dot'),
    output: 'prompter.png',
    command: [dot, '-Tpng', '@INPUT@', '-o', '@OUTPUT@'],
    install : false)
endif

# ----------------------------------------------------------------------------
# Desktop data tool
# ----------------------------------------------------------------------------
executable('desktop_data',
  ['desktop_data.c', 'util.c', 'logging.c', 'stringset.c'],
  dependencies : glib_deps,
  install : false,
  build_by_default : false)

# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------

# Define locations for sources and headers
appinfo        = files('appinfo.c')
applications   = files('applications.c')
config         = files('config.c')
control        = files('control.c')
later          = files('later.c')
logging        = files('logging.c')
mainloop       = files('mainloop.c')
migrator       = files('migrator.c')
permissions    = files('permissions.c')
prompter       = files('prompter.c')
service        = files('service.c')
session        = files('session.c')
settings       = files('settings.c')
stringset      = files('stringset.c')
users          = files('users.c')
util           = files('util.c')
daemon_headers = include_directories('.')

subdir('test')

# To make coverage reports work properly, tests must be built in this
# directory. Their definitions are in test/meson.build

foreach test : tests
  executable(test[0],
    test[1],
    dependencies : glib_deps,
    install : true,
    install_dir : tests_bindir,
    include_directories : daemon_headers,
    c_args : test_options,
    link_args : test[2])
endforeach

foreach test : tests
  unit_bins += {
    test[0] : executable(test[0] + '_unit',
      test[1],
      dependencies : glib_deps,
      build_by_default : false,
      install : false,
      include_directories : daemon_headers,
      c_args : unit_options,
      link_args : test[2])
  }
endforeach

foreach unit : units
  test(unit[0],
    unit_bins.get(unit[1]),
    args : unit[2],
    suite : unit[3],
    depends : unit_depends)
endforeach
