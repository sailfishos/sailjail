# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-clock

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute jolla-clock application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS

### APPLICATION
private-bin jolla-clock
dbus-user.own com.jolla.clock
whitelist /usr/share/jolla-settings/pages/jolla-clock
