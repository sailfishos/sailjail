# home directory whitelisting
whitelist ~/.cache/ambienced

# data dir whitelisting
whitelist /usr/share/themes
whitelist /usr/share/translations
whitelist /usr/share/fonts
whitelist /usr/share/fontconfig
whitelist /usr/share/pixmaps
whitelist /usr/share/icons
whitelist /usr/share/qt5
whitelist /usr/share/glib-2.0/schemas
whitelist /usr/share/X11/xkb
whitelist /etc/fonts

# D-Bus
dbus-system.talk com.nokia.NonGraphicFeedback1.Backend
dbus-user.talk com.nokia.profiled
dbus-user.talk org.maliit.server
dbus-user.talk org.nemo.transferengine
