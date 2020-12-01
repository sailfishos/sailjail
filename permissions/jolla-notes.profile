# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-notes

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute jolla-notes application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS
# x-sailjail-permission = Notes

### APPLICATION
private-bin jolla-notes
dbus-user.own com.jolla.notes
