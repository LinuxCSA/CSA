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

sbin_PROGRAMS = csabuild

csabuild_SOURCES = build.c \
		   dump.c \
		   files.c \
		   init.c \
		   main.c \
		   process.c \
		   sbu.c \
		   special.c \
		   wkmgmt.c \
		   write.c

csabuild_DEPENDENCIES = @top_srcdir@/include/acctdef.h @top_srcdir@/lib/libacct.a

INCLUDES = -I@top_srcdir@/include -D__USE_GNU -Dlinux
LIBS = @top_srcdir@/lib/libacct.a
AM_LDFLAGS = -lm

@top_srcdir@/lib/libacct.a:
	cd @top_srcdir@/lib && ${MAKE} libacct.a

@top_srcdir@/include/acctdef.h:
	cd @top_srcdir@/include && ${MAKE} acctdef.h

install-exec-hook:
	if [ $(USER) == "root" ]; then					\
	  @list='$(sbin_PROGRAMS)'; for p in $$list; do 		\
	    echo "  chown $(csainstalluser) $(DESTDIR)$(sbindir)/$$p"; 	\
	     chown $(csainstalluser) $(DESTDIR)$(sbindir)/$$p; 		\
	    echo "  chgrp $(csainstallgroup) $(DESTDIR)$(sbindir)/$$p"; \
	     chgrp $(csainstallgroup) $(DESTDIR)$(sbindir)/$$p; 	\
	  done;								\
	fi
