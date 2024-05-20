Name: pt_func_perf
Version: 1.0.0
Release: %(echo $RELEASE)%{?dist} 
# if you want use the parameter of rpm_create on build time,
# uncomment below
Summary: Intel processor trace tools for analyzing performance of function. 
Group: alibaba/application
License: Commercial
AutoReqProv: none
BuildRequires: binutils = 2.27
BuildRequires: binutils-devel = 2.27
BuildRequires: elfutils-libelf-devel
BuildRequires: devtoolset-7-gcc
BuildRequires: devtoolset-7-gcc-c++
BuildRequires: devtoolset-7-binutils
BuildRequires: devtoolset-7-libasan-devel
BuildRequires: devtoolset-7-libatomic-devel
BuildRequires: python-3.8.2
BuildRequires: kernel-headers = 4.19.91
BuildRequires: slang = 2.2.4
%define _prefix /usr/share/pt_func_perf

%description
# if you want publish current svn URL or Revision use these macros
Intel processor trace tools for analyzing performance of function

# prepare your files
%install

# create dirs
mkdir -p ${RPM_BUILD_ROOT}%{_prefix}
cd $OLDPWD/../;
make CC=/opt/rh/devtoolset-7/root/bin/gcc CXX=/opt/rh/devtoolset-7/root/bin/g++
make install PREFIX=${RPM_BUILD_ROOT}%{_prefix}

# package infomation
%files
# set file attribute here
%defattr(-,root,root)
# need not list every file here, keep it as this
%{_prefix}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig 

%changelog
* Fri Mar 8 2024 xierongbiao.xrb 
- add spec of pt_func_perf
