# Copyright (c) 2000-2007 Silicon Graphics, Inc. 
# All Rights Reserved.
#
# This Makefile controls a build process of the pam_job module
# for Linux-PAM. You should not modify this Makefile.

lib_LTLIBRARIES = pam_job.la

pam_job_la_SOURCES = pam_job.c

INCLUDES = -I@top_srcdir@/include

AM_CFLAGS = -DPAM_DYNAMIC -DLINUX_PAM -D__USE_XOPEN

pam_job_la_LDFLAGS = -module -avoid-version
pam_job_la_LIBADD = -L../lib -ljob

libdir = @jobpamdir@
