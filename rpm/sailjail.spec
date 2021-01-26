Name:     sailjail
Summary:  Firejail-based sanboxing tool
Version:  1.0.21
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

# libglibutil >= 1.0.49 is required for gutil_slice_free() macro
BuildRequires: pkgconfig(libglibutil) >= 1.0.49
BuildRequires: pkgconfig(glib-2.0) >= %{glib_version}

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

%prep
%setup -q -n %{name}-%{version}

%build
make %{_smp_mflags} \
  LIBDIR=%{_libdir} \
  KEEP_SYMBOLS=1 \
  HAVE_FIREJAIL=%{jailfish} \
  release pkgconfig

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} LIBDIR=%{_libdir} install install-dev

install -D -m755 tools/measure_launch_time.py \
        %{buildroot}%{_bindir}/measure_launch_time

%check
make HAVE_FIREJAIL=%{jailfish} -C unit test

%files
%defattr(-,root,root,-)
%license COPYING
%if %{jailfish}
%attr(6755,root,root) %{_bindir}/sailjail
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
