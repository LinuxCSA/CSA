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


noinst_LIBRARIES = libacct.a
lib_LTLIBRARIES = libcsa.la

libacct_a_SOURCES = \
	acctio.c \
	basic.c \
	config.c \
	convert_cacct.c \
	convert_cms.c \
	convert_hdr.c \
	convert_pacct.c \
	error.c \
	header.c \
	init_pacct.c \
	init_value.c \
	init_wm.c \
	name_to_gid.c \
	name_to_prid.c \
	name_to_uid.c \
	peak_offpeak.c \
	sbu.c \
	ucputime.c \
	user_sbu.c \
	valid.c

libacct_a_DEPENDENCIES = @top_srcdir@/include/acctdef.h

INCLUDES = -I@top_srcdir@/include -D__USE_GNU -Dlinux

libcsa_la_SOURCES = csa_api.c

libcsa_la_LDFLAGS = -version-info @csalibversion@

@top_srcdir@/include/acctdef.h:
	cd @top_srcdir@/include && ${MAKE} acctdef.h

