# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-mediaplayer

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute jolla-mediaplayer application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS
# x-sailjail-permission = Music
# x-sailjail-permission = Audio
# x-sailjail-permission = Notifications
# x-sailjail-permission = Tracker
# x-sailjail-permission = Bluetooth
# x-sailjail-permission = Videos
# x-sailjail-permission = Connman

### APPLICATION
private-bin jolla-mediaplayer
private-etc machine-id
dbus-user.own com.jolla.mediaplayer
dbus-user.own org.mpris.MediaPlayer2.jolla-mediaplayer
