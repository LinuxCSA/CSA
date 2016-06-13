#
# spec file for package job
#

Name: job
Version: 3.0.0
Release: 1
Summary: Commands, init scripts, libraries and PAM module for using Linux Jobs
Source0: %{name}-%{version}.tar.bz2
License: GPL
URL: https://github.com/LinuxCSA/CSA
Group: System Environment/Base
BuildRequires: pam-devel pam libcap libcap-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
The job package provides a set of commands, a PAM module, man pages,
and configuration files.  The commands are used to send signals to jobs,
wait on jobs, get status information about jobs, and for administrators
to control process attachment to jobs.  The PAM module allows the
administrator to specify which point-of-entry services on the system
(rlogin, gdm, xdm, ftp, etc.) should create new jobs.

Install the job package if you are interested in using Linux Jobs to manage
work on the system.

Most Job users will want to modify PAM to create jobs when users log in.
Please see /usr/share/doc/packages/job/README

# Don't check for files that were installed but not in the package list.
%define _unpackaged_files_terminate_build 0

%prep
rm -rf $RPM_BUILD_ROOT
#non-lbs - %setup -c job-%{version}-%{release}
%setup -q -n %{name}
libtoolize -c
aclocal
autoheader
automake -a -c -i
autoconf
./configure --prefix=/usr --libdir=%{_libdir} --sysconfdir=/etc --mandir=/usr/share/man

%build
make

%install
make DESTDIR="$RPM_BUILD_ROOT" install
# symlink to /usr/sbin/rcjob to be suse-like
mkdir -p $RPM_BUILD_ROOT/usr/sbin
ln -sf ../../etc/init.d/job $RPM_BUILD_ROOT/usr/sbin/rcjob

%clean
rm -rf $RPM_BUILD_ROOT
rm -f %{name}-%{version}.tar.gz

%files
%defattr(-,root,root)
/etc/init.d/job
/%{_lib}/security/pam_job.so
/usr/include/job.h
/usr/include/job_csa.h
%{_libdir}/libjob.la
%{_libdir}/libjob.so*
/usr/bin/jattach
/usr/bin/jdetach
/usr/bin/jkill
/usr/bin/jsethid
/usr/bin/jstat
/usr/bin/jwait
/usr/bin/jxcsa
/usr/sbin/jobd
/usr/share/man/*/*
/usr/sbin/rcjob
%doc AUTHORS COPYING NEWS README

%preun
if [ "$1" = 0 ] ; then 
	SAFE=`grep -v '^#' /etc/pam.d/* | grep -s pam_job.so | wc -l`
	if [ $SAFE -ne 0 ] ; then
		echo "You must remove all references to pam_job.so in the "
		echo "/etc/pam.d/* config files before attempting to uninstall"
		echo "the job package."
		echo ""
		exit 1
	fi
	if [ "%_vendor" = "suse" ] ; then
		%stop_on_removal job
	else
		/sbin/service job stop > /dev/null 2>&1
	fi
fi

%post
/bin/mkdir -p /var/run/jobd
/bin/chmod 0755 /var/run/jobd
/sbin/ldconfig
if [ "%_vendor" = "suse" ] ; then
	%fillup_and_insserv -f -y job
else
	/sbin/chkconfig --add job
fi

%postun
/sbin/ldconfig
if [ "%_vendor" = "suse" ] ; then
	%insserv_cleanup
else
	/sbin/chkconfig --del job
fi
if [ "$1" = "0" ] ; then
	/bin/rm -rf /var/run/jobd
fi

# The noship RPM contains simply the jobtest program right now
%package test-noship
Group: System Environment/Base
Summary: A test program for linux jobs.
%description test-noship
A simple test program to sanity check some aspects of the jobs package.
%files test-noship
%defattr(-,root,root)
/usr/bin/jobtest

%changelog
* Wed Jun  8 2016 Jay Lan <jay.j.lan@nasa.gov>
- Converted job package from the last SGI-supported version 2.0.1 to
  community version 3.0.0

* Mon Apr 30 2007 Jonathan Lim <jlim@sgi.com>
- job-2.0.1
- Fixed capabilities memory leak in jobd. 
                
* Fri Jul 21 2006 Jonathan Lim <jlim@sgi.com>
- job-2.0.0     
- Job kernel functionality implemented as a userspace daemon.
  The daemon receives process creation notification from the kernel via a
  generic netlink socket.  Process exits are communicated similarly, or
  through a socket from the CSA daemon (Comprehensive System Accounting)
  if enabled.  Job commands also communicate with the daemon via socket.
                
* Fri Nov 7 2003 Erik Jacobson <erikj@sgi.com>
- job-pagg-1.4.0
- Job library made available and should be used in place of jobctl.
  This allows the implementation of how job communicates with pagg to
  change without having to re-code applications that make use of job.

* Thu Feb 15 2001 Sam Watters <watters@sgi.com>
- job-pagg-1.0.0 (available Feb 15, 2001)
- Kernel module changed to kernel patch (can build as built-in or
  as a module.
- Added two new commands: jsethid & jdetach.
  * jsethid: admin command used to set the Host ID (hid)
    for jid generation.
  * jclean: allows a single process in a job, or all processes
    in a job to be detached from a job. Need to be
    root or have CAP_SYS_RESOURCE capability to
    execute this command.
- Cleaned up the job user commands package build process to
  allow it to be packaged via RPM.

* Fri Dec 15 2000 Sam Watters <watters@sgi.com>
- job-pagg-0.7.0 (available December 15, 2000)
- initial Job Container and Process Aggregation interface
  open source release.
