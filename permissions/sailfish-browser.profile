# -*- mode: sh -*-

# Firejail profile for /usr/bin/sailfish-browser

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute sailfish-browser application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS
# x-sailjail-permission = Accounts
# x-sailjail-permission = Phone
# x-sailjail-permission = Tracker
# x-sailjail-permission = Thumbnails
# x-sailjail-permission = Audio
# x-sailjail-permission = Location
# x-sailjail-permission = Downloads
# x-sailjail-permission = Mozilla
# x-sailjail-permission = Notifications
# x-sailjail-permission = Privileged
# x-sailjail-permission = Internet
# x-sailjail-permission = Connman
# for starting messageserver5
# x-sailjail-permission = AppLaunch
# x-sailjail-permission = SyncFW

### APPLICATION
private-bin sailfish-browser
dbus-user.own org.sailfishos.browser
dbus-user.own org.sailfishos.browser.ui

mkdir     ${HOME}/.cache/org.sailfishos/sailfish-browser
whitelist ${HOME}/.cache/org.sailfishos/sailfish-browser

mkdir     ${HOME}/.local/share/org.sailfishos/sailfish-browser
whitelist ${HOME}/.local/share/org.sailfishos/sailfish-browser

mkdir     ${HOME}/.mozilla/captiveportal
whitelist ${HOME}/.mozilla/captiveportal
