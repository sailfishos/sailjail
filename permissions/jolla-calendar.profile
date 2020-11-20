# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-calendar

# x-sailjail-translation-catalog =
# x-sailjail-translation-key =
# x-sailjail-description = Execute jolla-calendar application

### PERMISSIONS
# x-sailjail-permission = Accounts
# x-sailjail-permission = Notifications
# x-sailjail-permission = Calendar
# x-sailjail-permission = Privileged
# x-sailjail-permission = SyncFW

### APPLICATION
private-bin jolla-calendar
dbus-user.own com.jolla.calendar.ui
mkdir     ${HOME}/.local/share/system/privileged/autofill
whitelist ${HOME}/.local/share/system/privileged/autofill
