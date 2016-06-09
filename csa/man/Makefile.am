## Process this file with automake to produce Makefile.in

##  Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
##  Copyright (c) 2004-2007 Silicon Graphics, Inc All Rights Reserved.
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

SUBDIRS = man3

man_MANS =      acctdisk.8 \
		acctdusg.8 \
		csa.8 \
		csaaddc.8 \
		csabuild.8 \
		csachargefee.8 \
		csackpacct.8 \
		csacms.8 \
		csacom.1 \
		csacon.8 \
		csacrep.8 \
		csadrep.8 \
		csaedit.8 \
		csagetconfig.8 \
		csajrep.8 \
		csaperiod.8 \
		csarecy.8 \
		csarun.8 \
		csaswitch.8 \
		csaverify.8 \
		dodisk.8 \
		ja.1 \
		lastlogin.8 \
		nulladm.8

EXTRA_DIST =    acctdisk.8 \
                acctdusg.8 \
                csa.8 \
                csaaddc.8 \
                csabuild.8 \
                csachargefee.8 \
                csackpacct.8 \
                csacms.8 \
                csacom.1 \
                csacon.8 \
                csacrep.8 \
                csadrep.8 \
                csaedit.8 \
                csagetconfig.8 \
                csajrep.8 \
                csaperiod.8 \
                csarecy.8 \
                csarun.8 \
                csaswitch.8 \
                csaverify.8 \
                dodisk.8 \
                ja.1 \
                lastlogin.8 \
                nulladm.8

%: %.in $(top_builddir)/config.status
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

