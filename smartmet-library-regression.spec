%define DIRNAME regression
%define LIBNAME smartmet-%{DIRNAME}
%define SPECNAME smartmet-library-%{DIRNAME}
Summary: regression test library
Name: %{SPECNAME}
Version: 16.12.20
Release: 1%{?dist}.fmi
License: MIT
Group: Development/Libraries
URL: https://github.com/fmidev/smartmet-library-regression
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
Provides: %{LIBNAME}
BuildRequires: rpm-build
BuildRequires: make
BuildRequires: imake
Obsoletes: libsmartmet-regression < 16.12.20
Obsoletes: libsmartmet-regression-debuginfo < 16.12.20

%description
FMI regression test library

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}
 
%build
make %{_smp_mflags}

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0775)
%{_includedir}/smartmet/%{DIRNAME}
%{_libdir}/libsmartmet-%{DIRNAME}.a

%changelog
* Tue Dec 20 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.12.20-1.fmi
- Switched to open source naming conventions

* Wed Jun 15 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.6.15-1.el6.fmi
- RHEL6 release

* Mon Sep 24 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.1-4.el5.fmi
- Fixed "make depend"
- Fixed typos in Makefile

* Fri Sep 14 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.1-3.el5.fmi
- Added "make tag" feature

* Thu Sep 13 2007 mheiskan <mika.heiskanen@fmi.fi> - 1.0.1-2.el5.fmi
- Improved make system

* Wed Jul  25 2007 tervo <tervo@xodin.weatherproof.fi> - 
- changed file ownerships into root

* Thu Jun  7 2007 tervo <tervo@xodin.weatherproof.fi> - 
- Initial build

