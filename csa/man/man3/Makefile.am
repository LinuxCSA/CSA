## Process this file with automake to produce Makefile.in

##  Copyright (c) 2004-2007 Silicon Graphics, Inc. All Rights Reserved.
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

man_MANS =      csa_start.3		\
		csa_stop.3		\
		csa_halt.3		\
		csa_check.3		\
		csa_kdstat.3		\
		csa_rcdstat.3		\
		csa_jastart.3		\
		csa_jastop.3		\
		csa_wracct.3		\
		csa_auth.3

EXTRA_DIST = $(man_MANS)

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

