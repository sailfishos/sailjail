# -*- mode: sh -*-

# Firejail profile for /usr/bin/jolla-email

# x-sailjail-translation-catalog =
# x-sailjail-translation-key-description =
# x-sailjail-description = Execute jolla-email application
# x-sailjail-translation-key-long-description =
# x-sailjail-long-description =

### PERMISSIONS
# x-sailjail-permission = Accounts
# x-sailjail-permission = Phone
# x-sailjail-permission = Email
# x-sailjail-permission = Mozilla
# x-sailjail-permission = Location
# x-sailjail-permission = Privileged
# x-sailjail-permission = Internet
# x-sailjail-permission = Connman
# for starting messageserver5
# x-sailjail-permission = AppLaunch

### APPLICATION
private-bin jolla-email
dbus-user.own com.jolla.email.ui
