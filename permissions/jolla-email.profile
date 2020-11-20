# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-email

# x-sailjail-translation-catalog =
# x-sailjail-translation-key =
# x-sailjail-description = Execute jolla-email application

### PERMISSIONS
# x-sailjail-permission = Accounts
# x-sailjail-permission = Phone
# x-sailjail-permission = EMail
# x-sailjail-permission = Mozilla
# x-sailjail-permission = Location
# x-sailjail-permission = Privileged
# x-sailjail-permission = Internet
# x-sailjail-permission = Connman
# x-sailjail-permission = UDisks
# for starting messageserver5
# x-sailjail-permission = AppLaunch

### APPLICATION
private-bin jolla-email
dbus-user.own com.jolla.email.ui
