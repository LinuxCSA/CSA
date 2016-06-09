# Copyright (c) 2000-2007 Silicon Graphics, Inc. 
# All Rights Reserved.

sbin_PROGRAMS = jobd

jobd_SOURCES = jobd.c jobd_hash.c

AM_CFLAGS = -D_GNU_SOURCE

INCLUDES = -I@top_srcdir@/include

jobd_LDFLAGS = -rdynamic
jobd_LDADD = -lcap -lpthread -ldl
