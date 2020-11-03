# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-messages

# x-sailjail-translation-catalog =
# x-sailjail-translation-key =
# x-sailjail-description = Execute jolla-messages application

### PERMISSIONS
# x-sailjail-permission = Contacts
# x-sailjail-permission = Notifications
# x-sailjail-permission = Phone
# x-sailjail-permission = UDisks

### APPLICATION
private-bin jolla-messages
# FIXME: jolla-messages has odd app service name?
dbus-user.own org.nemomobile.qmlmessages
# FIXME: another systematic dbus-name to deal with?
dbus-user.own org.freedesktop.Telepathy.Client.qmlmessages
