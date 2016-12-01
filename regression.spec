%define LIBNAME regression
Summary: regression library
Name: libsmartmet-%{LIBNAME}
Version: 11.6.15
Release: 1%{?dist}.fmi
License: FMI
Group: Development/Libraries
URL: http://www.weatherproof.fi
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
Provides: %{LIBNAME}
BuildRequires: imake

%description
FMI regression library

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{LIBNAME}
 
%build
make %{_smp_mflags}

%install
%makeinstall includedir=%{buildroot}%{_includedir}/smartmet

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0775)
%{_includedir}/smartmet/%{LIBNAME}
%{_libdir}/libsmartmet_%{LIBNAME}.a

%changelog
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

