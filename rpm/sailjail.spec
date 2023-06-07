Name:     sailjail
Summary:  Firejail-based sanboxing tool
Version:  1.1.19
Release:  1
License:  BSD
URL:      https://github.com/sailfishos/sailjail
Source0:  %{name}-%{version}.tar.bz2

%define glib_version 2.44

Requires: firejail >= 0.9.64.4+git3
Requires: xdg-dbus-proxy
Requires: sailjail-permissions
Requires(pre): sailfish-setup

Requires: glib2 >= %{glib_version}
Requires: sailjail-daemon
Provides: sailjail-launch-approval

BuildRequires: meson
BuildRequires: ccache
BuildRequires: pkgconfig(glib-2.0) >= %{glib_version}
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(libsystemd)
BuildRequires: pkgconfig(libdbusaccess)
BuildRequires: sed

# Keep settings in encrypted home partition
%define _sharedstatedir /home/.system/var/lib
# Directory for D-Bus policy
%define _dbuspolicydir %{_datadir}/dbus-1/system.d
# Tests directory
%define _testsdir /opt/tests

%description
Firejail-based sanboxing for Sailfish OS.

%package tools
Summary: Tools for developing %{name}'d apps
Requires: python3-base

%description tools
%{summary}.
Contains a script to measure launching time.

%package daemon
Summary: Daemon for managing application sandboxing permissions

Requires: mapplauncherd >= 4.2.8

%description daemon
This package contains daemon that keeps track of:
- defined permissions (under /etc/sailjail/permissions)
- permissions required by applications (under /usr/share/applications)
- what permissions user has granted to each application

%package daemon-tests
Summary: QA tests for %{name}-daemon

%description daemon-tests
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build
%meson \
    -Dversion=%{version} \
    -Duserunitdir=%{_userunitdir} \
    -Dunitdir=%{_unitdir} \
    -Ddbuspolicydir=%{_dbuspolicydir} \
    -Dtestsdir=%{_testsdir} \

%meson_build

%install
%meson_install

install -D -m755 tools/measure_launch_time.py \
        %{buildroot}%{_bindir}/measure_launch_time

install -d %{buildroot}%{_unitdir}/multi-user.target.wants
ln -s ../sailjaild.service %{buildroot}%{_unitdir}/multi-user.target.wants/
install -d %{buildroot}%{_sysconfdir}/sailjail/config
install -d %{buildroot}%{_sysconfdir}/sailjail/applications

install -D -m755 daemon/conf/user-grantlist.conf \
        %{buildroot}%{_sysconfdir}/sailjail/config

%files
%defattr(-,root,root,-)
%license COPYING
%attr(2755,root,privileged) %{_bindir}/sailjail

%files tools
%defattr(-,root,root,-)
%{_bindir}/measure_launch_time

%files daemon
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/sailjaild
%{_unitdir}/*.service
%{_unitdir}/multi-user.target.wants/*.service
%{_dbuspolicydir}/sailjaild.conf
%dir %{_sysconfdir}/sailjail
%dir %{_sysconfdir}/sailjail/config
%dir %{_sysconfdir}/sailjail/applications
%{_sysconfdir}/sailjail/config/user-grantlist.conf

%files daemon-tests
%defattr(-,root,root,-)
%license COPYING
%{_testsdir}/%{name}-daemon
