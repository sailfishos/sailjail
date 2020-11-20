# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-clock

# x-sailjail-translation-catalog =
# x-sailjail-translation-key =
# x-sailjail-description = Execute jolla-clock application

### PERMISSIONS

### APPLICATION
private-bin jolla-clock
dbus-user.own com.jolla.clock
whitelist /usr/share/jolla-settings/pages/jolla-clock
