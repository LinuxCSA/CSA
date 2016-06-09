## Process this file with automake to produce Makefile.in

##  Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
##  Copyright (c) 2004-2008 Silicon Graphics, Inc All Rights Reserved.
## 
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as 
##  published by the Free Software Foundation; either version 2 of
##  the License, or (at your option) any later version.
##  
##  This program is distributed in the hope that it would be useful, but 
##  WITHOUT ANY WARRANTY; without even the implied warranty of 
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details. 
## 
##  You should have received a copy of the GNU General Public License along 
##  with this program; if not, write to the Free Software Foundation, Inc.,
##  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
## 
##  Contact information:  Silicon Graphics, Inc., 1140 East Arques Avenue,
##  Sunnyvale, CA  94085, or:
##  
##  http://www.sgi.com 

SUBDIRS = csabuild ja

WKDIR = $(DESTDIR)$(csalocalstatedir) \
	$(DESTDIR)$(csalocalstatedir)/day \
	$(DESTDIR)$(csalocalstatedir)/fiscal \
	$(DESTDIR)$(csalocalstatedir)/nite \
	$(DESTDIR)$(csalocalstatedir)/sum \
	$(DESTDIR)$(csalocalstatedir)/work

noinst_SCRIPTS = csa.conf \
		  csa

sbin_PROGRAMS = acctdisk \
		acctdusg \
		csaaddc \
		csachargefee \
		csacms \
		csacon \
		csacrep \
		csadrep \
		csaedit \
		csagetconfig \
		csajrep \
		csarecy \
		csaswitch \
		csaverify

bin_PROGRAMS = csacom

sbin_SCRIPTS =	csackpacct \
		csaperiod \
		csarun \
		dodisk \
		lastlogin \
		nulladm

## csa_conf_SOURCES   = csa.conf.in
## csa_SOURCES        = csa.in
## csackpacct_SOURCES = csackpacct.in
## csaperiod_SOURCES  = csaperiod.in
## csarun_SOURCES     = csarun.in
## dodisk_SOURCES     = dodisk.in
## lastlogin_SOURCES  = lastlogin.in
## nulladm_SOURCES    = nulladm.in

acctdisk_DEPENDENCIES     = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
acctdusg_DEPENDENCIES     = @top_srcdir@/include/acctdef.h
csaaddc_DEPENDENCIES      = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csachargefee_DEPENDENCIES = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csacms_DEPENDENCIES       = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csacom_DEPENDENCIES       = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csacon_DEPENDENCIES       = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csacrep_DEPENDENCIES      = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csadrep_DEPENDENCIES      = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csaedit_DEPENDENCIES      = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csagetconfig_DEPENDENCIES = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csajrep_DEPENDENCIES      = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csarecy_DEPENDENCIES      = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a
csaswitch_DEPENDENCIES    = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a @top_srcdir@/lib/libcsa.la
csaverify_DEPENDENCIES    = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a

INCLUDES = -I@top_srcdir@/include -D__USE_GNU -Dlinux

acctdisk_LDADD	= @top_srcdir@/lib/libacct.a
acctdusg_LDADD	= @top_srcdir@/lib/libacct.a
csaaddc_LDADD	= @top_srcdir@/lib/libacct.a
csachargefee_LDADD	= @top_srcdir@/lib/libacct.a
csacms_LDADD	= @top_srcdir@/lib/libacct.a
csacom_LDADD	= @top_srcdir@/lib/libacct.a
csacon_LDADD	= @top_srcdir@/lib/libacct.a
csacrep_LDADD	= @top_srcdir@/lib/libacct.a
csadrep_LDADD	= @top_srcdir@/lib/libacct.a
csaedit_LDADD	= @top_srcdir@/lib/libacct.a
csagetconfig_LDADD	= @top_srcdir@/lib/libacct.a
csajrep_LDADD	= @top_srcdir@/lib/libacct.a
csarecy_LDADD	= @top_srcdir@/lib/libacct.a
csaswitch_LDADD	= @top_srcdir@/lib/libacct.a @top_srcdir@/lib/libcsa.la
csaverify_LDADD	= @top_srcdir@/lib/libacct.a


SUFFIXES = .in .sh

.in.sh: $(top_builddir)/config.status
	rm -f $@
	cat $< \
	   | sed 's%\@bindir\@%$(bindir)%g' \
	   | sed 's%\@sbindir\@%$(sbindir)%g' \
	   | sed 's%\@libexecdir\@%$(libexecdir)%g' \
	   | sed 's%\@datadir\@%$(datadir)%g' \
	   | sed 's%\@sysconfdir\@%$(sysconfdir)%g' \
	   | sed 's%\@sharedstatedir\@%$(sharedstatedir)%g' \
	   | sed 's%\@localstatedir\@%$(localstatedir)%g' \
	   | sed 's%\@libdir\@%$(libdir)%g' \
	   | sed 's%\@includedir\@%$(includedir)%g' \
	   | sed 's%\@oldincludedir\@%$(oldincludedir)%g' \
	   | sed 's%\@infodir\@%$(infodir)%g' \
	   | sed 's%\@mandir\@%$(mandir)%g' \
	   | sed 's%\@srcdir\@%$(srcdir)%g' \
	   | sed 's%\@pkglibdir\@%$(pkglibdir)%g' \
	   | sed 's%\@PACKAGE\@%$(PACKAGE)%g' \
	   | sed 's%\@csalocalstatedir\@%$(csalocalstatedir)%g' \
	   | sed 's%\@PERL\@%$(PERL)%g' \
	   > $@

csa$(EXEEXT): csa_@distro@.sh csa_@distro@.in
	rm -f $@
	cp csa_@distro@.sh $@
	chmod 755 $@

csackpacct$(EXEEXT): csackpacct.sh csackpacct.in
	rm -f $@
	cat $@.sh > $@
	chmod 755 $@

csaperiod$(EXEEXT): csaperiod.sh csaperiod.in
	rm -f $@
	cat $@.sh > $@
	chmod 755 $@

csarun$(EXEEXT): csarun.sh csarun.in
	rm -f $@
	cat $@.sh > $@
	chmod 755 $@

dodisk$(EXEEXT): dodisk.sh dodisk.in
	rm -f $@
	cat $@.sh > $@
	chmod 755 $@

nulladm$(EXEEXT): nulladm.sh nulladm.in
	rm -f $@
	cat $@.sh > $@
	chmod 755 $@

lastlogin$(EXEEXT): lastlogin.sh lastlogin.in $(top_builddir)/config.status
	rm -f $@
	cat $@.sh > $@
	chmod 755 $@

csa.conf$(EXEEXT): csa.conf.in $(top_builddir)/config.status
	rm -f $@
	cat $< \
	   | sed 's%\@bindir\@%$(bindir)%g' \
	   | sed 's%\@sbindir\@%$(sbindir)%g' \
	   | sed 's%\@libexecdir\@%$(libexecdir)%g' \
	   | sed 's%\@datadir\@%$(datadir)%g' \
	   | sed 's%\@sysconfdir\@%$(sysconfdir)%g' \
	   | sed 's%\@sharedstatedir\@%$(sharedstatedir)%g' \
	   | sed 's%\@localstatedir\@%$(localstatedir)%g' \
	   | sed 's%\@libdir\@%$(libdir)%g' \
	   | sed 's%\@includedir\@%$(includedir)%g' \
	   | sed 's%\@oldincludedir\@%$(oldincludedir)%g' \
	   | sed 's%\@infodir\@%$(infodir)%g' \
	   | sed 's%\@mandir\@%$(mandir)%g' \
	   | sed 's%\@srcdir\@%$(srcdir)%g' \
	   | sed 's%\@pkglibdir\@%$(pkglibdir)%g' \
	   | sed 's%\@PACKAGE\@%$(PACKAGE)%g' \
	   | sed 's%\@csalocalstatedir\@%$(csalocalstatedir)%g' \
	   | sed 's%\@PERL\@%$(PERL)%g' \
	   > $@

@top_srcdir@/lib/libacct.a:
	cd @top_srcdir@/lib && ${MAKE} libacct.a

@top_srcdir@/lib/libcsa.la:
	cd @top_srcdir@/lib && ${MAKE} libcsa.la

@top_srcdir@/include/acctdef.h:
	cd @top_srcdir@/include && ${MAKE} acctdef.h

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	for p in csa.holidays; do \
	  if test -f $$p; then \
	    echo "  $(INSTALL_DATA) $$p $(DESTDIR)$(sysconfdir)/$$p"; \
	     $(INSTALL_DATA) $$p $(DESTDIR)$(sysconfdir)/$$p; \
	  else :; fi; \
	done

	$(mkinstalldirs) $(DESTDIR)/etc
	for p in csa.conf; do \
          if test -f $$p; then \
	    echo "  $(INSTALL_DATA) $$p $(DESTDIR)/etc/$$p"; \
	      $(INSTALL_DATA) $$p $(DESTDIR)/etc/$$p; \
	    else :; fi; \
	done 

	$(mkinstalldirs) $(DESTDIR)$(csainitdir)
	for p in csa; do \
	  if test -f $$p; then \
	    echo "  $(INSTALL_PROGRAM) $$p $(DESTDIR)$(csainitdir)/$$p"; \
	     $(INSTALL_PROGRAM) $$p $(DESTDIR)$(csainitdir)/$$p; \
	  else :; fi; \
	done

	$(mkinstalldirs) $(WKDIR)
	if [ $(USER) == "root" ]; then			\
	  chown $(csainstalluser) $(WKDIR);		\
	  chgrp $(csainstallgroup) $(WKDIR);		\
	fi

	for p in statefile; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(csalocalstatedir)/nite/$$p";\
	     $(INSTALL_DATA) $$p $(DESTDIR)$(csalocalstatedir)/nite/$$p; \
	    if [ $(USER) == "root" ]; then	\
	      echo "chown $(csainstalluser) $(DESTDIR)$(csalocalstatedir)/nite/$$p"; \
	       chown $(csainstalluser) $(DESTDIR)$(csalocalstatedir)/nite/$$p; \
	      echo "chgrp $(csainstallgroup) $(DESTDIR)$(csalocalstatedir)/nite/$$p"; \
	       chgrp $(csainstallgroup) $(DESTDIR)$(csalocalstatedir)/nite/$$p; \
	    fi;		\
	  else :; fi; \
	done

	@list='$(sbin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_PROGRAM) $$p $(DESTDIR)$(sbindir)/$$p"; \
	      $(INSTALL_PROGRAM) $$p $(DESTDIR)$(sbindir)/$$p; \
	    if [ $(USER) == "root" ]; then	\
	      echo "  chown $(csainstalluser) $(DESTDIR)$(sbindir)/$$p"; \
	       chown $(csainstalluser) $(DESTDIR)$(sbindir)/$$p; \
	      echo "  chgrp $(csainstallgroup) $(DESTDIR)$(sbindir)/$$p"; \
	       chgrp $(csainstallgroup) $(DESTDIR)$(sbindir)/$$p; \
	    fi;		\
	  else :; fi; \
	done

	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/$$p"; \
	      $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/$$p; \
	    if [ $(USER) == "root" ]; then	\
	      echo "  chown $(csainstalluser) $(DESTDIR)$(bindir)/$$p"; \
	       chown $(csainstalluser) $(DESTDIR)$(bindir)/$$p; \
	      echo "  chgrp $(csainstallgroup) $(DESTDIR)$(bindir)/$$p"; \
	       chgrp $(csainstallgroup) $(DESTDIR)$(bindir)/$$p; \
	    fi;		\
	  else :; fi; \
	done

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/csa.holidays;
	rm -f $(DESTDIR)/etc/csa.conf;
	rm -f $(DESTDIR)$(csainitdir)/csa;
	echo " Manually remove $(DESTDIR)$(csalocalstatedir) directories"

DISTCLEANFILES = csackpacct.sh \
		 csaperiod.sh \
		 csa_@distro@.sh \
		 csarun.sh \
		 dodisk.sh \
		 lastlogin.sh \
		 nulladm.sh

EXTRA_DIST = csa.conf.in \
	     csa.holidays \
	     statefile
