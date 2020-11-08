private-tmp
writable-run-user

caps.drop all
nonewprivs
seccomp
netfilter

# system directories

blacklist /sbin
blacklist /usr/local/sbin
blacklist /usr/sbin
blacklist /var

whitelist /usr/share/zoneinfo

whitelist /etc/dconf
whitelist /etc/environment
whitelist /etc/group
whitelist /etc/host.conf
whitelist /etc/hostname
whitelist /etc/hosts
whitelist /etc/hw-release
whitelist /etc/inputrc
whitelist /etc/locale.conf
whitelist /etc/localtime
whitelist /etc/machine-id
whitelist /etc/machine-info
whitelist /etc/passwd
whitelist /etc/profile
whitelist /etc/profile.d
whitelist /etc/protocols
whitelist /etc/sailfish-release
whitelist /etc/services
whitelist /etc/shells
whitelist /etc/xdg/user-dirs.conf
whitelist /etc/xdg/user-dirs.defaults

# D-Bus
dbus-system filter
dbus-user filter
dbus-user.talk ca.desrt.dconf

# home directory whitelisting
whitelist ~/.config/dconf
