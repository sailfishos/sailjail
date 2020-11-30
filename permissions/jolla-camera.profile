# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-camera

# x-sailjail-translation-catalog =
# x-sailjail-translation-key =
# x-sailjail-description = Execute jolla-camera application

### PERMISSIONS
# x-sailjail-permission = Camera,Pictures,Videos,MediaIndexing,Location,UDisks,Privileged

### APPLICATION
private-bin jolla-camera,jolla-camera-lockscreen
dbus-user.own com.jolla.camera
