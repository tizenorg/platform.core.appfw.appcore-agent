Name:           appcore-agent
Version:        1.0
Release:        0
License:        Apache-2.0
Summary:        Agent Application basic
Group:          Application Framework/Service
Source0:        appcore-agent-%{version}.tar.gz
Source1001:     appcore-agent.manifest
BuildRequires:  cmake
BuildRequires:  sysman-devel
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(vconf)

%description
SLP agent application basic

%package devel
Summary:        Applocation Core Agent
Group:          Application Framework/Development
Requires:       %{name} = %{version}
%description devel
appcore agent (developement files)

%package -n capi-appfw-service-application-devel
Summary:    service appliation
Group:      Development/Libraries
Requires:    appcore-agent-devel = %{version}-%{release}
%description -n capi-appfw-service-application-devel
service application (developement files)

%prep
%setup -q
cp %{SOURCE1001} .

%build
%cmake .
%__make %{?_smp_mflags}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%license LICENSE
%defattr(-,root,root,-)
%{_libdir}/libappcore-agent.so.1
%{_libdir}/libappcore-agent.so.1.1

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/appcore-agent.pc
%{_libdir}/libappcore-agent.so
%{_includedir}/appcore-agent/appcore-agent.h
%{_includedir}/appcore-agent/service_app.h

%files -n capi-appfw-service-application-devel
%{_libdir}/pkgconfig/capi-appfw-service-application.pc
%{_libdir}/libappcore-agent.so
%{_includedir}/appcore-agent/appcore-agent.h
%{_includedir}/appcore-agent/service_app.h
