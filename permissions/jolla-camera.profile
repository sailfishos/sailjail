# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-camera

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute jolla-camera application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS
# x-sailjail-permission = Camera,Pictures,Videos,MediaIndexing,Location,Privileged

### APPLICATION
private-bin jolla-camera,jolla-camera-lockscreen
dbus-user.own com.jolla.camera
