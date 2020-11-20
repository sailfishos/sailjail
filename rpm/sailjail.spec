Name:     sailjail
Summary:  Firejail-based sanboxing tool
Version:  1.0.2
Release:  1
License:  BSD
URL:      https://github.com/sailfishos/sailjail
Source0:  %{name}-%{version}.tar.bz2

%define glib_version 2.44
%define libglibutil_version 1.0.43

Requires: firejail
Requires: xdg-dbus-proxy
Requires: glib2 >= %{glib_version}
Requires: libglibutil >= %{libglibutil_version}

# libglibutil >= 1.0.49 is required for gutil_slice_free() macro
BuildRequires: pkgconfig(libglibutil) >= 1.0.49
BuildRequires: pkgconfig(glib-2.0) >= %{glib_version}

%define permissions_dir /etc/sailjail/permissions

%description
Firejail-based sanboxing for Sailfish OS.

%package plugin-devel
Summary: Development files for %{name} plugins
Requires: pkgconfig
Requires: pkgconfig(libglibutil)

%description plugin-devel
This package contains development files for %{name} plugins.

%prep
%setup -q -n %{name}-%{version}

%build
make %{_smp_mflags} LIBDIR=%{_libdir} KEEP_SYMBOLS=1 release pkgconfig

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} LIBDIR=%{_libdir} install install-dev

%check
make -C unit test

%files
%defattr(-,root,root,-)
%dir %attr(755,root,root) %{permissions_dir}
%attr(6755,root,root) %{_bindir}/sailjail
%{_bindir}/sailjail
%{permissions_dir}/*

%files plugin-devel
%defattr(-,root,root,-)
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/*.h
%{_libdir}/pkgconfig/*.pc
