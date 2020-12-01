# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-messages

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute jolla-messages application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS
# x-sailjail-permission = Contacts
# x-sailjail-permission = Notifications
# x-sailjail-permission = Phone

### APPLICATION
private-bin jolla-messages
# FIXME: This is a legacy D-Bus service name, we need to drop it at some point
dbus-user.own org.nemomobile.qmlmessages
# FIXME: Remove this after domain & app are handled
dbus-user.own org.sailfishos.Messages
# FIXME: another systematic dbus-name to deal with?
dbus-user.own org.freedesktop.Telepathy.Client.qmlmessages
