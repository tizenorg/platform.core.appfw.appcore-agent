Name:           appcore-agent
Version:        1.0.4
Release:        1
License:        Apache-2.0
Summary:        Service Application basic
Group:          Application Framework/Service
Source0:        %{name}-%{version}.tar.gz
Source1001:     appcore-agent.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(capi-appfw-app-control)
BuildRequires:  pkgconfig(capi-appfw-app-common)
BuildRequires:  pkgconfig(appcore-common)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(vconf-internal-keys)

%description
Service Application basic

%package devel
Summary:        Application Core Agent
Group:          Application Framework/Development
Requires:       %{name} = %{version}-%{release}
%description devel
Service Application basic (development files)

%prep
%setup -q
cp %{SOURCE1001} .

%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`

%cmake . -DFULLVER=%{version} -DMAJORVER=${MAJORVER}
%__make %{?_smp_mflags}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%license LICENSE
%defattr(-,root,root,-)
%{_libdir}/libappcore-agent.so.*

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/appcore-agent.pc
%{_libdir}/pkgconfig/capi-appfw-service-application.pc
%{_libdir}/libappcore-agent.so
%{_includedir}/appcore-agent/appcore-agent.h
%{_includedir}/appcore-agent/service_app.h
%{_includedir}/appcore-agent/service_app_extension.h
