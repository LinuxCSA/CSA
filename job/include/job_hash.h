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
 * jobd_hash.h 
 *
 * Code from the ELSA project (http://elsa.sourceforge.net/)
 * Author: Guillaume Thouvenin <guillaume.thouvenin@bull.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#define	JOB_HASH_SIZE	256
#define	PROC_HASH_SIZE	1024

/* The states for a job */
#define	RUNNING	1	/* Running job */
#define	ZOMBIE	2	/* Dead job */

#if	__BYTE_ORDER == __BIG_ENDIAN
#define	iptr_hid(ll)	((u_int32_t *)&(ll))
#define	iptr_sid(ll)	(((u_int32_t *)(&(ll) + 1)) - 1)
#else	/* __BYTE_ORDER == __LITTLE_ENDIAN */
#define	iptr_hid(ll)	(((u_int32_t *)(&(ll) + 1)) - 1)
#define	iptr_sid(ll)	((u_int32_t *)&(ll))
#endif

#define	jid_hash(ll)	(*(iptr_sid(ll)) % JOB_HASH_SIZE)
#define	pid_hash(ll)	((pid_t)(ll) % PROC_HASH_SIZE)

struct links {
	void		*next;
	void		*prev;
};

struct queue {
	void		*head;
	void		*tail;
};

/* Process table entry */
struct proc_entry {
	pid_t			pid;	   /* process ID */
	int			status;	   /* exit status of process */ 
	struct job_entry	*job;	   /* Pointer to associated job */
	struct links		jobprocs;   /* More processes in this job */
	struct links		hashlist;   /* More processes - same hash */
};

/* Job table entry */
struct job_entry {
	jid_t			jid;	   /* job ID */
	int			refcnt;	   /* Number of processes attached to job */
	int			state;	   /* State of job - RUNNING,... */
	uid_t			user;	   /* user that owns the job */
	time_t			start;	   /* When the job began */
	int			waitstat;  /* exit status of last process */ 
	struct queue		wait;	   /* queue of requests waiting on job */
	int			waitcnt;   /* Number of requests waiting on job */
	struct proc_entry	*proclist; /* List of attached process */
	struct links		hashlist;  /* More jobs - same hash */
};

/* from jobd_hash.c */
struct job_entry *findjob(jid_t);
void add_job_to_hash(struct job_entry *);
void unlink_job_from_hash(struct job_entry *);
struct proc_entry *findproc(pid_t);
void add_proc_to_job(struct proc_entry *, struct job_entry *);
void unlink_proc_from_job(struct proc_entry *, int);
void add_proc_to_hash(struct proc_entry *);
void unlink_proc_from_hash(struct proc_entry *);
int init_proc_hash_locks(void);

/* from jobd.c */
void job_cleanup(struct job_entry *);

