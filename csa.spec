#
# spec file for package csa
#

Name: csa
Version: 5.0.1
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

* Fri Apr 27 2007 Jay Lan <jlan@sgi.com>
* Mon Apr 30 2007 Jonathan Lim <jlim@sgi.com>
- csa-3.0.2
- SBU's data was all zero in csa-3.0.0 and csa-3.0.1
- Clarify csacom(1) man page on '-f' option
- Obsolete csacom '-a' & '-q' option
- Fix a problem of csadrep reading input from a binary file
- Fixed csadrep(8) man page: replaced '-w' with '-n' and removed '-t'

* Wed Jul 5 2006 Jay Lan <jlan@sgi.com>
- csa-3.0.1
- 'csacom -b' was broken in csa-3.0.0
- 'csacom -b' drops the last record of a pacct file

* Thu Feb 24 2006 Jay Lan <jlan@sgi.com>
- csa-3.0.0
- csacom -[esES] option was broken.
- create /usr/sbin/rccsa symlink at %install in the spec file
- remove code for CSA record types not supported in Linux
- spliting up csa_kern.h into kernel version and userland version
- support pacct files created in csa-2.x.x
- incompatible with csa-2.x.x releases. User appls need recompilation.
- needs csa_module kernel module of 2.6.16.21.

* Wed Jul 27 2005 Chris Sturtivant <csturtiv@sgi.com>
- csa-2.2.2
- Disabled perror for permission denied in csa_ctl when called by csa_auth

* Mon May 2 2005 Jay Lan <jlan@sgi.com>
- csa-2.2.1
- clean up csa init script
- csa_wracct(3) man page update
- 'csacom -o' wrote only one record
- 'csacom -b' does not work
- csajrep man page references invalid option '-W'
- 'csajrep -r' does not work
- 'dodisk' created files with incorrect user and group name 'adm'
- CSA does not handle gid's > 65535
- 'csacom -j' failed if job id greater than max value of an int

* Fri Mar 25 2005 Jay Lan <jlan@sgi.com>
- csa-2.1.5 (final release to support 2.4 kernels)
- 'csacom -o' wrote only one record
- 'csacom -b' does not work
- csajrep man page references invalid option '-W'
- 'csajrep -r' does not work

* Wed Jan 12 2005 Jay Lan <jlan@sgi.com>
- csa-2.1.4
- csa_wracct(3) man page update

* Fri Dec 17 2004 Jay Lan <jlan@sgi.com>
- csa-2.1.3
- csacom & csacms usage otuput missing carriage return
- csarecy misses a carriage return on certain condition
- csacom -W segfaults after printing one bad time
- clean up csa init script

* Mon Nov 22 2004 Jay Lan <jlan@sgi.com>
- csa-2.2.0
- Fixed compilation issue with gcc 3.3.3
- CSA assumes existence of 'adm' user/group account
- csacom & csacms usage otuput missing carriage return
- csarecy misses a carriage return on certain condition
- csacom -W segfaults after printing one bad time

* Fri Oct 8 2004 Jay Lan <jlan@sgi.com>
- csa-2.1.2
- Fixed 'csaswitch -c halt' bug
- Fixed 'csaedit dumped core on bad wkmg record' bug

* Tue Jul 27 2004 Jay Lan <jlan@sgi.com>
- csa-2.1.1
- Remove static API library libcsa.a from rpm package
- Fixed a bug at csacom with invalid -u option
- cascom headers and data not aligned correctly at output

* Wed Jun 30 2004 Jay Lan <jlan@sgi.com>
- csa-2.1.0
- Fixed a problem dealing with accounting files > 2GB
- Now derives "Day of Year" from other fields of the holidays file
- Removed "Pages Swapped" from csacom and csacms - not supported in Linux
- Fixed a bug of not mapping UID 0 to root

* Tue Feb 17 2004 Jay Lan <jlan@sgi.com>
- csa-2.0.0
- Implemented /proc ioctl interface
- Renamed rpm from csa-job-acct to csa
- Renamed /etc/init.d/csaacct to /etc/init.d/csa

* Fri Nov 7 2003 Jay Lan <jlan@sgi.com>
- csa-job-acct-2.0.0
- Provided API library libcsa.so and associated header file
