#
# spec file for package csa
#

Name: csa
Version: 5.0.0
Release: 1
Summary: System job accounting
Source0: %{name}-%{version}.tar.bz2
License: GPL
Group: System Environment/Base
Obsoletes: csa-job-acct
Requires(pre,post,preun,postun,prereq): coreutils grep
BuildRequires: job gettext-devel libnl libnl-devel libcap libcap-devel pam pam-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-root
URL: https://github.com/LinuxCSA/CSA

%description
This package contains the CSA commands and man pages.  There is
also a dependency upon the Linux job daemon. 

Linux Comprehensive System Accounting (CSA) is a combination of
a daemon program and a set of C programs and shell scripts.
CSA provides methods for collecting per-process resource usage data,
monitoring disk usage, and charging fees to specific login accounts.
CSA takes this per-process accounting information and combines it
by job identifier (jid) within system boot uptime periods.

# Don't check for files that were installed but not in the package list.
%define _unpackaged_files_terminate_build 0

%prep
rm -rf $RPM_BUILD_ROOT
%setup -q -n %{name}
cfg=/usr/share/gettext/config.rpath
if [ -f $cfg ]; then ln -s $cfg .; else touch `basename $cfg`; fi
libtoolize -c
aclocal
autoheader
automake -a -c -i
autoconf

# Below, we tell configure that we're going to set root:sys for file
# permissions instead of the default 'csaacct' for 'make install'
# This means this spec file needs to set proper permissions.  This way, we
# don't require build servers to have the csaacct user/group.
./configure --prefix=/usr --libdir=%{_libdir} --sysconfdir=/etc --mandir=/usr/share/man --with-csainstalluser=root --with-csainstallgroup=sys

%build
make

# Install step will attempt to populate the install area only if both
# DIST_ROOT and DIST_MANIFEST are not set, otherwise we assume somebody
# already done it.
%install
make DESTDIR="$RPM_BUILD_ROOT" install
# symlink to /usr/sbin/rccsa to be suse-like
mkdir -p $RPM_BUILD_ROOT/usr/sbin
ln -sf ../../etc/init.d/csa $RPM_BUILD_ROOT/usr/sbin/rccsa

%clean
rm -rf $RPM_BUILD_ROOT
rm -f %{name}-%{version}.tar.gz

%pre
# If the csaacct user and group don't already exist (ie from a previous
# install of this RPM), we create it.  We don't blindly create it because
# we could be upgrading and we don't want to fail trying to create
# duplicate accounts.  We also want the original csaacct UID/GID to be
# used after an upgrade.  We assume if the csaacct user exists, the group
# does also.

# If csaacct doesn't alredy exist, create the user and group.  We use -r to 
# get the system UID and GID in the system dynamic range.  See /etc/login.defs
if ! id -u -n csaacct > /dev/null 2>&1; then
	/usr/sbin/groupadd -r csaacct
	/usr/sbin/useradd -r -g csaacct -s /bin/false -c "Comprehensive System Accounting Admin User" -d /var/csa csaacct
fi

%files
%defattr(-,root,root)
%config /etc/csa.conf
%config /etc/csa.holidays
/etc/init.d/csa
/usr/include/csa.h
/usr/include/csa_delay.h
/usr/include/csa_api.h
/usr/include/csaacct.h
%{_libdir}/libcsa.la
%{_libdir}/libcsa.so*
%{_libdir}/libcsad.la
%{_libdir}/libcsad.so*
/usr/sbin/acctdisk
/usr/sbin/acctdusg
/usr/sbin/csaaddc
/usr/sbin/csachargefee
/usr/sbin/csacms
/usr/sbin/csacon
/usr/sbin/csacrep
/usr/sbin/csadrep
/usr/sbin/csaedit
/usr/sbin/csagetconfig
/usr/sbin/csajrep
/usr/sbin/csarecy
/usr/sbin/csaswitch
/usr/sbin/csaverify
/usr/sbin/csackpacct
/usr/sbin/csaperiod
/usr/sbin/csarun
/usr/sbin/dodisk
/usr/sbin/lastlogin
/usr/sbin/nulladm
/usr/sbin/rccsa
%doc AUTHORS COPYING INSTALL ChangeLog NEWS README ABOUT-NLS
/usr/share/man/*/*
# Below here should be owned by csaacct
%defattr(-,csaacct,csaacct)
/usr/sbin/csabuild
/usr/bin/csacom
/usr/bin/ja
/var/csa/nite/statefile
%dir /var/csa
%dir /var/csa/day
%dir /var/csa/fiscal
%dir /var/csa/nite
%dir /var/csa/sum
%dir /var/csa/work

# Stop accounting before erasing the last package
# Note: We could also delete the csaacct user and group here
# but I think we should skip that.
%preun
if [ "$1" = 0 ]; then
    /usr/sbin/csaswitch -c halt
    which chkconfig > /dev/null
    if [ $? -eq 0 ] ; then
	chkconfig --del csa > /dev/null
    fi
fi
exit 0

# Run chkconfig after installing
%post
echo "The csa accounting files have been installed.  Please review the"
echo "settings in /etc/csa.conf and meets your site needs and then"
echo "configure accounting to start on boot 'chkconfig --add csa'"
echo "Remember to invoke the csarun script from a cron job to ensure"
echo "pacct file cleanup occurs.  Remember to watch the logs directory"
echo "to ensure the disk does not overfill."
exit 0

# The noship RPM contains scripts and binary test programs
%package test-noship
Group: System Environment/Base
Summary: A test suites for linux CSA accounting
%description test-noship
A simple test suite to sanity check some aspects of the csa package.
%files test-noship
%defattr(-,root,root)
/var/csa/tests/csa-test.ks
/var/csa/tests/csatest_api
/var/csa/tests/csa_ja_cpu.ks
/var/csa/tests/csa_ja-n.ks
/var/csa/tests/csa_ja-o.ks
/var/csa/tests/csa_ja-s.ks
/var/csa/tests/csa_ja-t.ks
/var/csa/tests/csacom_io.ks
/var/csa/tests/csacom_cpu.ks
/var/csa/tests/README

%changelog
* Wed Jun  8 2016 Jay Lan <jay.j.lan@nasa.gov>
  - Converted csa package from the last SGI-supported version 4.1.1 to
    community version 5.0.0
