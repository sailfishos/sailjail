project('sailjail', 'c',
        default_options : ['c_std=c99'])

# ----------------------------------------------------------------------------
# Installation directories
# ----------------------------------------------------------------------------
prefix         = get_option('prefix')              # /usr
includedir     = prefix / get_option('includedir') # /usr/include
bindir         = prefix / get_option('bindir')     # /usr/bin
sbindir        = prefix / get_option('sbindir')    # /usr/sbin
libexecdir     = prefix / get_option('libexecdir') # /usr/libexec
libdir         = prefix / get_option('libdir')     # /usr/lib
sysconfdir     = get_option('sysconfdir')          # etc
datadir        = prefix / get_option('datadir')    # /usr/share
mandir         = prefix / get_option('mandir')     # /usr/share/man
infodir        = prefix / get_option('infodir')    # /usr/share/info
localstatedir  = get_option('localstatedir')       # /var
sharedstatedir = get_option('sharedstatedir')      # /var/lib
unitdir        = get_option('unitdir')             # /usr/lib/systemd/system
userunitdir    = get_option('userunitdir')         # /usr/lib/systemd/user
dbuspolicydir  = get_option('dbuspolicydir')       # /usr/share/dbus-1/system.d
testsdir       = get_option('testsdir')            # /opt/tests

summary({
    'prefix' : prefix,
    'includedir' : includedir,
    'bindir' : bindir,
    'sbindir' : sbindir,
    'libexecdir' : libexecdir,
    'libdir' : libdir,
    'sysconfdir' : sysconfdir,
    'datadir' : datadir,
    'mandir' : mandir,
    'infodir' : infodir,
    'localstatedir' : localstatedir,
    'sharedstatedir' : sharedstatedir,
    'unitdir' : unitdir,
    'userunitdir' : userunitdir,
    'dbuspolicydir' : dbuspolicydir,
    'testsdir' : testsdir,
  },
  section : 'Installation directories')

# ----------------------------------------------------------------------------
# Common default flags
# ----------------------------------------------------------------------------
add_project_arguments('-DVERSION="' + get_option('version') + '"', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')
add_project_arguments('-D_FILE_OFFSET_BITS=64', language : 'c')

# ----------------------------------------------------------------------------
# Utility programs
# ----------------------------------------------------------------------------
cp    = find_program('cp')
mkdir = find_program('mkdir')
rm    = find_program('rm')
sed   = find_program('sed')
touch = find_program('touch')
dot   = find_program('dot', required : false)

# ----------------------------------------------------------------------------
# Daemon and client
# ----------------------------------------------------------------------------
subdir('daemon')
