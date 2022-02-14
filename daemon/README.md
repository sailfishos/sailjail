Sailjail Daemon
===============

This document briefly describes internal structure of sailjail daemon
itself, and how / what kinds of clients it is expected to serve.

Client Behavior
===============

Modifying Settings
------------------

Expectation is that direct manipulation of sandboxing related application
data is limited to privileged applications such as Settings UI. This is
enforced by checking credentials of clients making such method calls.

Tracking known applications can be done by obtaining initial set of
applications via GetApplications() method call and adjusting the active set
when ApplicationAdded or ApplicationRemoved signals are received.

System wide information about each application can be obtained via
GetAppInfo() method call. This data originates from read-only configuration
/ desktop / etc files and can be assumed to be fairly static. Still, if such
information is cached long term, it should be refreshed upon receiving
ApplicationChanged signal.

User specific application settings can be obtained via:

- GetLaunchAllowed()
- GetGrantedPermissions()

Privileged applications can modify these via:

- SetLaunchAllowed()
- SetGrantedPermissions()

Also changes in user specific settings are reported via ApplicationChanged
signal. In case of long term caching, client should refresh all settings for
all users that are of interest to the client.

Launching Applications
----------------------

Launchers are assumed to be fairly simple procedural programs that do not
employ event based mainloop. To facilitate such behavior sailjail daemon has
two helper methods that can be used to obtain launch permissions via a
single blocking D-Bus calls:

- PromptLaunchPermissions()
- QueryLaunchPermissions()

QueryLaunchPermissions() basically handles (within sailjail daemon)
obtaining uid of the currently active user session [e.g. sd_seat_get_active()],
checking whether that UID can launch application [GetLaunchAllowed()], and
returns list of permissions the application has been granted
[SetGrantedPermissions()] or error reply.

PromptLaunchPermissions() behaves similarly, but in case application launch
has not been explicitly allowed or denied, user is first prompted for launch
permission. In such cases reply message is delayed until user responds to
permission prompt and thus client should use either long or infinite timeout
for the D-Bus method call.

Sandboxed Application
---------------------

Expectation is that applications do not need to do IPC with sailjail daemon,
and in case of normal application launch it is not even possible as sailjail
D-Bus service is not visible from within sandbox.

In case application has been lauched via application specific sandboxed
booster, portion of sailjail D-Bus interface relevant to boosting logic
is available but applications should not rely on that.

Daemon Internals
================

Existing User Tracking
----------------------

- tracking is done by:
  - monitoring /etc/passwd file

- maintained data:
  - minimum UID
  - maximum UID
  - set of valid UID values

- UID added:
  - no action needed

- UID removed:
  - user must be dropped from settings data

User Session Tracking
---------------------

- tracks UID of current user

- UID is used for:
  - permissions checking/prompting

- UID changed:
  - cancels pending/queued prompts

Permission File Tracking
------------------------

- tracking is done by:
  - monitoring /etc/sailjail/permissions directory

- maintained data:
  - set of PERMISSION names

- PERMISSION added/removed:
  - triggers application data re-evaluation

Desktop File Tracking
---------------------

- tracking is done by monitoring *.desktop files in directories:
  - /usr/share/applications
    - regular desktop files
    - optionally with sailjail specific section
  - /etc/sailjail/applications
    - uses desktop file format
    - can be used for defining permissions for ui-less applications
    - or augmenting / overriding values from /usr/share/applications

- maintained data:
  - set of APPLICATION data, each with:
    - [Desktop Entry] properties:
      - Name
      - Type
      - Exec
      - Icon
      - NoDisplay
      - X-Maemo-Service
      - X-Maemo-Object-Path
      - X-Maemo-Method
    - [Sailjail] properties:
      - Permissions
      - OrganizationName
      - ApplicationName
      - DataDirectory
      - Sandboxing
      - ExecDBus

- APPLICATION added:
  - broadcast ApplicationAdded signal
  - no other actions

- APPLICATION removed:
  - broadcast ApplicationAdded signal
  - application settings for each user must be purged

- APPLICATION changed:
  - broadcast ApplicationAdded signal
  - triggers settings data re-evaluation

- dbus ipc:
  - method QStringList GetApplications()
    -> list of available application names

  - method QVariantMap GetAppInfo(QString application)
    -> application names to Name/Type/.. mapping

  - signal void ApplicationAdded(QString application)
  - signal void ApplicationRemoved(QString application)
  - signal void ApplicationChanged(QString application)
    - note that changed signal can occur also due to
      - changes in permission data
      - changes in user settings data

User / Application Settings
---------------------------

- maintained data:
  - tree of: settings(1) -> users(n) -> applications (n)
  - leaf node holds:
    - launch\_allowed = UNSET|ALWAYS|NEVER
    - granted\_permissions:
      - subset of: intersection of PERMISSIONS and APPLICATION.permissions
      - cleared/filled on launch_allowed changes
      - possible to modify while launch_allowed=ALWAYS
    - license\_agreed = UNSET|YES|NO
      - proof of concept / placeholder (=how to add new data)

- persistent storage:
  - one file / user -> "applications for user"
  - when loading / saving:
    - data for invalid users / applications is ignored

- dbus ipc:
  - access to application / settings data
    - UID / APPLICATION parameters are validated
      - even if stale data is still held internally it is not exposed
        over dbus interface

  - method int GetLaunchAllowed(uint uid, QString application)
  - method void SetLaunchAllowed(uint uid, QString application, int allowed)
    - values: UNSET=0|ALWAYS=1|NEVER=2
    - setter needs to be under access control
    - changing value affects also granted permissions

  - method QStringList GetGrantedPermissions(uint uid, QString application)
  - method void SetGrantedPermissions(uint uid, QString application, QStringList permissions)
    -> set of permissions granted to UID/APPLICATION

  - method QStringList QueryLaunchPermissions(QString application)
    - if application launch is allowed for current UID, returns permissions

  - method QStringList PromptLaunchPermissions(QString application)
    - if application launch is not allowed for current UID,
      - prompt user and set allowed/granted according to responce
    - if application launch is allowed for current UID, returns permissions

  - method int GetLicenseAgreed(uint uid, QString application)
  - method void SetLicenseAgreed(uint uid, QString application, int agreed)
    - values: UNSET=0|YES=1|NO=2
    - used only for: "how to add more data" demonstration

Autogenerated D-Bus autostart configuration
-------------------------------------------

  - stored under /run/user/UID/dbus-1/services
  - for each application that defines:
        - OrganizationName
        - ApplicationName
        - ExecDBus
  - when application configuration data is installed / updated / removed
    or current user changes, D-Bus activation configuration files are updated
    accordingly and SessionBus daemon (for current user) is instructed to
    reload configuration
  - ExecDBus allows to tell apart D-Bus autostart and launching from launcher.
    It defines the application command line to use.

Internal Structure
------------------

- names below map directly to source files
- also used in architecture graph
  - ref: dot -Tpng sailjaild.dot -o sailjaild.png

- CONTROL: "root object"
  - CONFIG: access to configuration data
  - USERS: existing users tracking
  - SESSION: user session tracking
  - PERMISSIONS: *.permission file tracking, Exec/Permissions/etc properties
  - SETTINGS: top level settings api/logic
    - USER SETTINGS: persistent storage in user-UID.settings files
      - APPLICATION SETTINGS: allowed/granted properties
  - SERVICE: dbus service, incoming method calls, outgoing signals
    - PROMPTER: session bus connection, queue/execute window prompt ipc
  - MIGRATOR: migrates approval files from older sailjail versions
    - APPROVAL: approval data from older sailjail versions
  - APPSERVICES: maintaining autogenerated D-Bus activation configuration

Directories Used
----------------

- CONFIG:       "/etc/sailjail/config"
- PERMISSIONS:  "/etc/sailjail/permissions"
- APPLICATIONS: "/usr/share/applications" and "/etc/sailjail/applications"
- SETTINGS:     "/home/.system/var/lib/sailjail/settings"
