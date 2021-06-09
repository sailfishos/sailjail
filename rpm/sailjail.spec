Name:     sailjail
Summary:  Firejail-based sanboxing tool
Version:  1.0.30
Release:  1
License:  BSD
URL:      https://github.com/sailfishos/sailjail
Source0:  %{name}-%{version}.tar.bz2

%define glib_version 2.44
%define libglibutil_version 1.0.43

%{!?jailfish: %define jailfish 1}
%if %{jailfish}
Requires: firejail >= 0.9.63+git3
Requires: xdg-dbus-proxy
%else
Provides: sailjail-launch-approval
%endif
Requires: sailjail-permissions

Requires: glib2 >= %{glib_version}
Requires: libglibutil >= %{libglibutil_version}
Requires: sailjail-daemon

# libglibutil >= 1.0.49 is required for gutil_slice_free() macro
BuildRequires: pkgconfig(libglibutil) >= 1.0.49
BuildRequires: pkgconfig(glib-2.0) >= %{glib_version}
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(libsystemd)
BuildRequires: pkgconfig(libdbusaccess)

# Keep settings in encrypted home partition
%define _sharedstatedir /home/.system/var/lib

%description
Firejail-based sanboxing for Sailfish OS.

%package plugin-devel
Summary: Development files for %{name} plugins
Requires: pkgconfig
Requires: pkgconfig(libglibutil)

%description plugin-devel
This package contains development files for %{name} plugins.

%package tools
Summary: Tools for developing %{name}'d apps
Requires: python3-base

%description tools
%{summary}.
Contains a script to measure launching time.

%package daemon
Summary: Daemon for managing application sandboxing permissions

%description daemon
This package contains daemon that keeps track of:
- defined permissions (under /etc/sailjail/permissions)
- permissions required by applications (under /usr/share/applications)
- what permissions user has granted to each application

%prep
%setup -q -n %{name}-%{version}

%build
make %{_smp_mflags} \
  LIBDIR=%{_libdir} \
  KEEP_SYMBOLS=1 \
  HAVE_FIREJAIL=%{jailfish} \
  release pkgconfig

make %{_smp_mflags} \
  VERSION=%{version}\
  _LIBDIR=%{_libdir}\
  _SHAREDSTATEDIR=%{_sharedstatedir}\
  -C daemon build

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} LIBDIR=%{_libdir} install install-dev

install -D -m755 tools/measure_launch_time.py \
        %{buildroot}%{_bindir}/measure_launch_time

make \
  _SYSCONFDIR=%{_sysconfdir}\
  _DATADIR=%{_datadir}\
  _LIBDIR=%{_libdir}\
  _USERUNITDIR=%{_userunitdir}\
  _UNITDIR=%{_unitdir}\
  _SHAREDSTATEDIR=%{_sharedstatedir}\
  DESTDIR=%{buildroot}\
  -C daemon install

install -d %{buildroot}%{_unitdir}/multi-user.target.wants
install -m644 daemon/systemd/sailjaild.service %{buildroot}%{_unitdir}
ln -s ../sailjaild.service %{buildroot}%{_unitdir}/multi-user.target.wants/
install -d %{buildroot}%{_sysconfdir}/dbus-1/system.d
install -m644 daemon/dbus/sailjaild.conf %{buildroot}%{_sysconfdir}/dbus-1/system.d/sailjaild.conf
install -d %{buildroot}%{_sharedstatedir}/sailjail/settings
install -d %{buildroot}%{_sysconfdir}/sailjail/config
install -d %{buildroot}%{_sysconfdir}/sailjail/applications

%check
make HAVE_FIREJAIL=%{jailfish} -C unit test

%files
%defattr(-,root,root,-)
%license COPYING
%if %{jailfish}
%attr(2755,root,privileged) %{_bindir}/sailjail
%endif
%{_bindir}/sailjail

%files plugin-devel
%defattr(-,root,root,-)
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/*.h
%{_libdir}/pkgconfig/*.pc

%files tools
%defattr(-,root,root,-)
%{_bindir}/measure_launch_time

%files daemon
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/sailjaild
%{_unitdir}/*.service
%{_unitdir}/multi-user.target.wants/*.service
%config %{_sysconfdir}/dbus-1/system.d/sailjaild.conf
%attr(0755,root,root) %dir %ghost %{_sharedstatedir}/sailjail
%attr(0750,root,root) %dir %ghost %{_sharedstatedir}/sailjail/settings
%dir %{_sysconfdir}/sailjail
%dir %{_sysconfdir}/sailjail/config
%dir %{_sysconfdir}/sailjail/applications
