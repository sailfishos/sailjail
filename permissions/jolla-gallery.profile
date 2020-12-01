# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-gallery

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute jolla-gallery application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS
# x-sailjail-permission = Pictures
# x-sailjail-permission = Videos
# x-sailjail-permission = Downloads
# x-sailjail-permission = MediaIndexing
# x-sailjail-permission = Ambience

### APPLICATION
private-bin jolla-gallery
dbus-user.own com.jolla.gallery
