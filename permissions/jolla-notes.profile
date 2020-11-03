# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-notes

# x-sailjail-translation-catalog =
# x-sailjail-translation-key =
# x-sailjail-description = Execute jolla-notes application

### PERMISSIONS
# x-sailjail-permission = Notes

### APPLICATION
private-bin jolla-notes
dbus-user.own com.jolla.notes
