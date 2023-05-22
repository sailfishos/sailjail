# Sailjail

Sailfish OS application sandboxes are launched through **sailjail** command that is a thin Firejail
wrapper. Sailjail has also a daemon that manages sandboxing permissions and keeps a list of
applications. Firejail documentation can be found [here](https://firejail.wordpress.com/).

In this document we are following terminology defined by Firejail.

## Application permissions

Application
[permissions](https://github.com/sailfishos/sailjail-permissions#sailfish-os-application-sandboxing-and-permissions)
defined in [sailjail-permissions](https://github.com/sailfishos/sailjail-permissions) are Firejail
profiles. Application developer defines set of required permissions in the application desktop file.

Application desktop file changes are described in the [Sailjail Permissions
documentation](https://github.com/sailfishos/sailjail-permissions#enable-sandboxing-for-an-application).

Sailjail parses the desktop file and builds Firejail command line arguments out of the requested
permissions and finally Sailjail executes Firejail with the command line arguments.

## Application data structure

There are three directories that are whitelisted automatically by Sailjail based on desktop file
content.

    $HOME/.local/share/<OrganizationName>/<ApplicationName>
    $HOME/.cache/<OrganizationName>/<ApplicationName>
    $HOME/.config/<OrganizationName>/<ApplicationName>

Default user data directories such as [*Pictures*, *Videos*, *Music* and
*Documents*](https://www.freedesktop.org/wiki/Software/xdg-user-dirs/) are not whitelisted by
default rather application developer needs to request access with predefined
[permissions](https://github.com/sailfishos/sailjail-permissions#permissions).

## Implicit D-Bus user service ownership

Based on [*OrganizationName* and
*ApplicationName* values](https://github.com/sailfishos/sailjail-permissions#desktop-file-changes)
application is granted rights to own a D-Bus service name on user session bus.

Example desktop file

    [Desktop Entry]
    Type=Application
    Name=My Application
    Icon=my-app-icon
    Exec=/usr/bin/org.foobar.MyApp

    [X-Sailjail]
    Permissions=Internet;Pictures
    OrganizationName=org.foobar
    ApplicationName=MyApp
    ExecDBus=/usr/bin/org.foobar.MyApp -prestart

With above example application desktop file Firejail command line arguments contain implicitly
**--dbus-user.own=org.foobar.MyApp** when launched through Sailjail.
When application's executable matches desktop file name, it is enough to define it in Exec value. If
the names differ you may define the desktop file with **sailjail** command's **-p** argument:

    Exec=/usr/bin/sailjail -p foobar-myapp.desktop -- /usr/bin/org.foobar.MyApp

Use of absolute paths is required, except for the desktop file which must be located in
_/usr/share/applications_ or _/etc/sailjail/applications_.

The ExecDBus value is an optional command line which will be used to auto start the application
to provide the \<OrganizationName\>.\<ApplicationName\> DBus service.

## Sailjail daemon

Sailjail has a daemon called sailjaild. It keeps track of applications' desktop files in
_/usr/share/applications_ and _/etc/sailjail/applications_. It can be queried over D-Bus for
information regarding these applications, their sandboxing status and permissions. The daemon also
handles prompting user for permissions.

## Sailfish OS specific changes to Firejail

### Handling privileged user data

In Sailfish OS portion of user data is considered private and although it resides within user's home
directory the files / directories are owned by _privileged_ user / group and only select
applications are granted access via executing them with effective group id set to _privileged_. For
sandboxed applications _private-data \<directory\>_ Firejail rule or command line option is used to
provide more fine grained access to privileged data.

Also noteworthy: sandboxed applications are executed with _no-new-privs_ set. As this makes it
impossible to have effective gid that differs from real gid, privileged applications are running
with real group id set to _privileged_.

### General fixes that might be eligible for upstreaming

- Rule templating. Sailjail defines keywords used in Firejail profiles that Firejail substitutes
  automatically while parsing the rules.
- Loading profiles only after parsing arguments instead of loading while parsing.

## Debugging sandboxed applications

See [Application debugging instructions](APPDEBUG.md).
