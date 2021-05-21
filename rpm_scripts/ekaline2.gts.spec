Name:		ekaline2
Version:	%{version}
Release:	1%{?dist}
Summary:	Ekaline Trading Acceleration

Group:		Applications/System
License:	Proprietary
URL:		http://www.ekaline.com/
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-root
%global debug_package %{nil} 

%description
Ekaline libraries for Silicom SmartNIC card

%prep

%setup

%build

%install

rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -r * %{buildroot}

%files
%dir ${EkaSnDir}
#%dir ${EkaInclDir}
#%dir ${EkaLibDir}
#%dir ${EkaUtilsDir}
${EkaSnDir}/smartnic.ko
${EkaSnDir}/*.sh
#${EkaLibDir}/lib%{name}.so
#${EkaUtilsDir}/*
#${EkaInclDir}/*
${EkaUtilsDir}/eka_tcpdump
${EkaUtilsDir}/eka_version
${EkaUtilsDir}/eka_Mold64PcapGap
${EkaUtilsDir}/efh_dump_igmp_acl
${EkaUtilsDir}/efh_state
${EkaUtilsDir}/eka_BoxPcapGap

%post

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
/*
