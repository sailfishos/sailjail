# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-contacts

# x-sailjail-translation-catalog =
# x-sailjail-translation-key =
# x-sailjail-description = Execute jolla-contacts application

### PERMISSIONS
# x-sailjail-permission = Bluetooth
# x-sailjail-permission = Phone
# x-sailjail-permission = Tracker
# x-sailjail-permission = Notifications
# x-sailjail-permission = Thumbnails
# x-sailjail-permission = Pictures
# x-sailjail-permission = Accounts
# x-sailjail-permission = Contacts
# x-sailjail-permission = SyncFW
# x-sailjail-permission = Privileged

### APPLICATION
private-bin jolla-contacts
dbus-user.own com.jolla.contacts.ui
