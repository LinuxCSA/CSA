#
# linuxcsa top level Makefile
#

CSA_VERSION=5.0.0
JOB_VERSION=3.0.0

LIBDIR=/usr/lib64

default:: job csa

job::
	cd job;				\
	libtoolize -c;			\
	aclocal;			\
	autoheader;			\
	automake -a -c -i;		\
	autoconf;			\
	./configure --prefix=/usr --libdir=$(LIBDIR) --sysconfdir=/etc --mandir=/usr/share/man;			\
	make

csa::
	cd csa;				\
	if [ -a /usr/share/gettext/config.rpath ] ; \
	then \
		ln -s /usr/share/gettext/config.rpath . ; \
	else \
		touch config.rpath ;	\
	fi;				\
	libtoolize -c;			\
	aclocal;			\
	autoheader;			\
	automake -a -c -i;		\
	autoconf;			\
	./configure --prefix=/usr --libdir=$(LIBDIR) --sysconfdir=/etc --mandir=/usr/share/man --with-csainstalluser=root --with-csainstallgroup=sys;	\
	make

rpm::
	@echo To build rpms, please run "create_rpm.sh <RPM_BUILD_ROOT pathname>"
	@echo "    where RPM_BUILD_ROOT is %_topdir in ~/.rpmmacros"
