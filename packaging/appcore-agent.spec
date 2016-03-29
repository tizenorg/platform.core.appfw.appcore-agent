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

%if "%{?profile}" == "wearable"
%define appfw_feature_background_management 1
%else
%if "%{?profile}" == "mobile"
%define appfw_feature_background_management 1
%else
%if "%{?profile}" == "tv"
%define appfw_feature_background_management 0
%endif
%endif
%endif

%package devel
Summary:        Application Core Agent
Group:          Application Framework/Development
Requires:       %{name} = %{version}-%{release}

%description devel
appcore agent (developement files)

%package -n capi-appfw-service-application-devel
Summary:    service appliation
Group:      Development/Libraries
Requires:    appcore-agent-devel = %{version}-%{release}

%description -n capi-appfw-service-application-devel
Service Application basic (developement files)

%prep
%setup -q
cp %{SOURCE1001} .

%build

%if 0%{?appfw_feature_background_management}
_APPFW_FEATURE_BACKGROUND_MANAGEMENT=ON
%endif

MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`

%cmake -DFULLVER=%{version} -DMAJORVER=${MAJORVER} \
	-D_APPFW_FEATURE_BACKGROUND_MANAGEMENT:BOOL=${_APPFW_FEATURE_BACKGROUND_MANAGEMENT} \
	.
%__make %{?_smp_mflags}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%license LICENSE
%defattr(-,root,root,-)
%{_libdir}/libappcore-agent.so*

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/appcore-agent.pc
%{_includedir}/appcore-agent/appcore-agent.h


%files -n capi-appfw-service-application-devel
%{_includedir}/appcore-agent/service_app.h
%{_includedir}/appcore-agent/service_app_extension.h
%{_libdir}/pkgconfig/capi-appfw-service-application.pc
