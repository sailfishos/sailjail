Existing User Tracking
======================

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
=====================

- tracks UID of current user

- UID is used for:
  - permissions checking/prompting

- UID changed:
  - cancels pending/queued prompts

Permission File Tracking
========================

- tracking is done by:
  - monitoring /etc/sailjail/permissions directory

- maintained data:
  - set of PERMISSION names

- PERMISSION added/removed:
  - triggers application data re-evaluation

Desktop File Tracking
=====================

- tracking is done by:
  - monitoring /usr/share/applications directory

- maintained data:
  - set of APPLICATION data, each with:
    - Desktop properties: Name, Type, Exec, Icon
    - Sailjail properties: Permissions

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
===========================

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

  - method void Quit()
    - terminates daemon
    - debug feature, to be removed

Internal Structure
==================

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

Directories Used
================

- CONFIG:       "/etc/sailjail/config"
- PERMISSIONS:  "/etc/sailjail/permissions"
- APPLICATIONS: "/usr/share/applications"
- SETTINGS:     "/home/.system/var/lib/sailjail/settings"

What Is Missing
===============

- user prompting
  - does asynchronous call to window prompt service
    - but does not handle cancellation yet
