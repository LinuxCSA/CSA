# Copyright (c) 2003-2007 Silicon Graphics, Inc.  
# All Rights Reserved.
# 
# This Makefile controls a build process of the job library for Linux.

lib_LTLIBRARIES = libjob.la

libjob_la_SOURCES = job.c jobctl.c

INCLUDES = -I@top_srcdir@/include

AM_CFLAGS = -D__USE_XOPEN

libjob_la_LDFLAGS = -version-info @joblibversion@
