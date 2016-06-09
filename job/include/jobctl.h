/*
 * Copyright (c) 2003-2007 Silicon Graphics, Inc.  
 * All Rights Reserved.
 * 
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/*
 *
 * Description:   This file, include/linux/jobctl.h, contains the data
 *       definitions used by job to communicate with jobd.
 *
 */

#ifndef _JOBCTL_H
#define _JOBCTL_H

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <job.h>

/*
 *
 * Define requests available in the job daemon
 *
 */

#define JOB_NOOP	0	/* No-op options */
#define JOB_CREATE	1	/* Create a job - uid = 0 only */
#define JOB_ATTACH	2	/* RESERVED */
#define JOB_DETACH	3	/* RESERVED */
#define JOB_GETJID	4	/* Get Job ID for specificed pid */
#define JOB_WAITJID	5	/* Wait for job to complete */
#define JOB_KILLJID	6	/* Send signal to job */
#define JOB_GETJIDCNT	9	/* Get number of JIDs on system */
#define JOB_GETJIDLST	10	/* Get list of JIDs on system */
#define JOB_GETPIDCNT	11	/* Get number of PIDs in JID */
#define JOB_GETPIDLST	12	/* Get list of PIDs in JID */
#define JOB_SETJLIMIT	13	/* Future: set job limits info */
#define JOB_GETJLIMIT	14	/* Future: get job limits info */
#define JOB_GETJUSAGE	15	/* Future: get job res. usage */
#define JOB_FREE	16	/* Future: Free job entry */
#define JOB_GETUSER	17	/* Get owner for job */
#define JOB_GETPRIMEPID	18	/* Get prime pid for job */
#define JOB_SETHID	19	/* Set HID for jid values */
#define JOB_DETACHJID	20	/* Detach all tasks from job */
#define JOB_DETACHPID	21	/* Detach a task from job */
#define JOB_ATTACHPID	22	/* Attach a task to a job */
#define JOB_RUNCSA	23	/* Start/stop CSA daemon */
#define JOB_OPT_MAX	24	/* Should always be highest number */


/*
 * Define request structures for job module
 */

/* generic request header */
struct job_hdr {
	int		req;	/* request type */
	int		err;	/* errno */
	int		size;	/* message size */
};

struct job_create {
	struct job_hdr	hdr;	/* generic header */
	jid_t	 	r_jid;	/* Return value of JID */
	jid_t	 	jid;	/* Jid value requested */
	pid_t		pid;	/* Pid of calling process */
	int 		user;	/* UID of user associated with job */
	int 		options;/* creation options - unused */
};


struct job_getjid {
	struct job_hdr	hdr;	/* generic header */
	jid_t	 	r_jid;	/* Returned value of JID */
	pid_t 		pid;	/* Info requested for PID */
};


struct job_waitjid {
	struct job_hdr	hdr;	/* generic header */
	jid_t	 	r_jid;	/* Returned value of JID */
	jid_t	 	jid;	/* Waiting on specified JID */
	int 		stat;	/* Status information on JID */
	int 		options;/* Waiting options */
};


struct job_killjid {
	struct job_hdr	hdr;	/* generic header */
	int		r_val;	/* Return value of kill request */
	jid_t		jid;	/* Sending signal to all PIDs in JID */
	int		sig;	/* Signal to send */
};


struct job_jidcnt {
	struct job_hdr	hdr;	/* generic header */
	int		r_val;	/* Number of JIDs on system */
};


struct job_jidlst {
	struct job_hdr	hdr;	/* generic header */
	int		r_val;	/* Number of JIDs in list */
	jid_t		*jid;	/* List of JIDs */
};


struct job_pidcnt {
	struct job_hdr	hdr;	/* generic header */
	int		r_val;	/* Number of PIDs in JID */
	jid_t		jid;	/* Getting count of JID */
};


struct job_pidlst {
	struct job_hdr	hdr;	/* generic header */
	int		r_val;	/* Number of PIDs in list */
	pid_t		*pid;	/* List of PIDs */
	jid_t		jid;
};


struct job_user {
	struct job_hdr	hdr;	/* generic header */
	int		r_user; /* The UID of the owning user */
	jid_t		jid;    /* Get the UID for this job */
};

struct job_primepid {
	struct job_hdr	hdr;	/* generic header */
	pid_t		r_pid; /* The prime pid */
	jid_t		jid;   /* Get the prime pid for this job */
};

struct job_sethid {
	struct job_hdr	hdr;	/* generic header */
	unsigned long	r_hid; /* Value that was set */
	unsigned long	hid;   /* Value to set to */
};

struct job_detachjid {
	struct job_hdr	hdr;	/* generic header */
	int		r_val; /* Number of tasks detached from job */
	jid_t		jid;   /* Job to detach processes from */
};

struct job_detachpid {
	struct job_hdr	hdr;	/* generic header */
	jid_t		r_jid; /* Jod ID task was attached to */
	pid_t		pid;   /* Task to detach from job */
};

struct job_attachpid {
	struct job_hdr	hdr;	/* generic header */
	jid_t		r_jid; /* Job ID task is to attach to */
	pid_t		pid;   /* Task to be attached */
};

struct job_runcsa {
	struct job_hdr	hdr;	/* generic header */
	int		r_val;	/* Return value of request */
	int		run;	/* 1 to start CSA daemon, 0 to stop */
};

/*
 * union job_message is just a structure that is guaranteed 
 * to be at least as large as the largest request structure.
 */
union job_message {
	struct job_hdr		hdr;
	struct job_create	create;
	struct job_getjid	getjid;
	struct job_waitjid	waitjid;
	struct job_killjid	killjid;
	struct job_jidcnt	jidcnt;
	struct job_jidlst	jidlst;
	struct job_pidcnt	pidcnt;
	struct job_pidlst	pidlst;
	struct job_user		user;
	struct job_primepid	primepid;
	struct job_sethid	sethid;
	struct job_detachjid	detachjid;
	struct job_detachpid	detachpid;
	struct job_attachpid	attachpid;
	struct job_runcsa	runcsa;
};

#define	JOBD_SOCKET	"/var/run/jobd/jobd-socket"

int jobctl(int request, void *data, int datasz);

#endif /* _JOBCTL_H */
