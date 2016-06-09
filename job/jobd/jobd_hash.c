/*
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.
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
 * jobd_hash.c - JOBs manager daemon
 *
 * Code from the ELSA project (http://elsa.sourceforge.net/)
 * Author: Guillaume Thouvenin <guillaume.thouvenin@bull.net>
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#include <job.h>
#include <job_hash.h>

static void lock_proc_hash(pid_t, int);
static void unlock_proc_hash(pid_t);

struct job_entry	*job_table[JOB_HASH_SIZE];
int			job_table_refcnt = 0;

/* One mutex lock for each hash bucket in proc_table */
struct proc_entry	*proc_table[PROC_HASH_SIZE];
pthread_rwlock_t	proc_hash_lock[PROC_HASH_SIZE];

/*
 * jid_lookup - return the jid for the given pid.
 * 
 * This is a special routine called ONLY by csa (never by jobd). 
 * Since it's called from a different thread we have to lock the 
 * pid's proc_table hash entry to keep jobd from manipulating the 
 * hash links while csa is trying to follow them.  Since jobd is 
 * the only thread that ever writes to the proc table, jobd only 
 * sets a lock when writing, and csa only locks it here.
 * 
 * Since there is a possibility that there could be more than 
 * one csa thread someday, we use a rwlock.
 *
 * If the pid is not part of a job, then 0 is returned.
 *
 */
jid_t jid_lookup(pid_t pid)
{
	struct proc_entry	*proc;
	jid_t			jid = 0;

	/* lock for reading */
	lock_proc_hash(pid, 0);

	proc = findproc(pid);
	if (proc) {
		jid = proc->job->jid;
	}

	unlock_proc_hash(pid);

	return(jid);
}

/*
 * findjob - find job entry from jid
 */ 
struct job_entry *findjob(jid_t jid)
{
	struct job_entry	*jp;

	if (jid <= 0) {
		return((struct job_entry *)NULL);
	}

	jp = job_table[ jid_hash(jid) ];

	while (jp) {
		if (jp->jid == jid) {
			return(jp);
		}
		jp = jp->hashlist.next;
	}

	return((struct job_entry *)NULL);
}

/*
 * add_job_to_hash - add job to hash table
 */ 
void add_job_to_hash(struct job_entry *job)
{
	struct job_entry *np = NULL;

	if (!job) {
		return;
	}
	/* Link job into the job_table */
	np = job_table[ jid_hash(job->jid) ];
	job->hashlist.prev = NULL;
	job->hashlist.next = np;
	job_table[ jid_hash(job->jid) ] = job;
	if (np != NULL) {
		np->hashlist.prev = job;
	}
	++job_table_refcnt;

	return;
}

/*
 * unlink_job_from_hash - remove job from hash table
 */ 
void unlink_job_from_hash(struct job_entry *job)
{
	struct job_entry *pp;	/* previous entry in hashlist chain */
	struct job_entry *np;	/* next entry in hashlist chain */

	if (!job) {
		return;
	}

	pp = job->hashlist.prev;
	np = job->hashlist.next;

	if (pp) {
		pp->hashlist.next = job->hashlist.next;
	}

	if (np) {
		np->hashlist.prev = job->hashlist.prev;
	}

	if (job_table[ jid_hash(job->jid) ] == job) {
		job_table[ jid_hash(job->jid) ] = np;
	}

	job_table_refcnt--;

	job->hashlist.prev = NULL;
	job->hashlist.next = NULL;
	job->state = ZOMBIE;

	return;
}

/*
 * findproc - find proc entry from pid
 */ 
struct proc_entry *findproc(pid_t pid)
{
	struct proc_entry	*pp;

	pp = proc_table[ pid_hash(pid) ];

	while (pp) {
		if (pp->pid == pid) {
			return(pp);
		}
		pp = pp->hashlist.next;
	}

	return((struct proc_entry *)NULL);
}

/*
 * add_proc_to_job - connect a proc with a job
 */ 
void add_proc_to_job(struct proc_entry *proc, struct job_entry *job)
{
	struct proc_entry *pp = NULL;

	if (!proc || !job) {
		return;
	}

	pp = job->proclist;
	proc->jobprocs.prev = NULL;
	proc->jobprocs.next = pp;
	proc->job = job;
	job->proclist = proc;
	job->refcnt++;
	if (pp != NULL) {
		pp->jobprocs.prev = proc;
	}
}

/*
 * unlink_proc_from_job - remove a proc from a job
 */ 
void unlink_proc_from_job(struct proc_entry *proc, int do_job_cleanup)
{
	struct job_entry *jp = NULL;	/* the job entry for this proc */
	struct proc_entry *pp = NULL;	/* previous entry in jobprocs chain */
	struct proc_entry *np = NULL;	/* next entry in jobprocs chain */

	if (!proc) {
		return;
	}

	jp = proc->job;
	pp = proc->jobprocs.prev;
	np = proc->jobprocs.next;

	if (pp) {
		pp->jobprocs.next = proc->jobprocs.next;
	}

	if (np) {
		np->jobprocs.prev = proc->jobprocs.prev;
	}

	if (jp->proclist == proc) {
		jp->proclist = np;
	}

	jp->waitstat = proc->status;
	jp->refcnt--;

	proc->jobprocs.prev = NULL;
	proc->jobprocs.next = NULL;

	/* If this was the last proc in job, clean up job too. */
	if (do_job_cleanup && (jp->refcnt == 0)) {
		job_cleanup(jp);
	}

	/* No need to clear proc->job.  In most cases, proc will be  
	 * freed next anyway.  And if the proc is being reassigned 
	 * to a new job, this will avoid having a window between old 
	 * and new when proc->job = NULL. 
	 * 
	 * proc->job = NULL;
	 * 
	 */

	return;
}

/*
 * add_proc_to_hash - add proc to hash table
 */ 
void add_proc_to_hash(struct proc_entry *proc)
{
	struct proc_entry *pp = NULL;

	if (!proc) {
		return;
	}
/* 
 * 	As long as the following conditions are true, we don't need to 
 * 	grab the lock here!
 * 
 * 	1. We are only defending against jid_lookup() reading this hash 
 * 	     bucket.
 * 	2. jid_lookup() (and findproc() which it calls to do the lookup) 
 * 	     only follow the chain using the .next pointer, never the .prev 
 * 	     pointer.  (So it's OK that we modify pp->hashlist.prev before 
 * 	     linking the new proc in.)
 * 	3. We totally set up the links in the new proc structure before we 
 * 	     link it into the chain.
 * 	4. The last thing we do is to switch the hash table pointer to 
 * 	     point at the new proc, linking it into the chain.
 * 
 *	lock_proc_hash(proc->pid, 1);
 */
	pp = (proc_table[ pid_hash(proc->pid) ]);
	proc->hashlist.prev = NULL;
	proc->hashlist.next = pp;
	if (pp != NULL) {
		pp->hashlist.prev = proc;
	}
	proc_table[ pid_hash(proc->pid) ] = proc;
/* 
 *	No need for this (see above).
 * 
 *	unlock_proc_hash(proc->pid);
 */
	return;
}

/*
 * unlink_proc_from_hash - remove proc from hash table
 */ 
void unlink_proc_from_hash(struct proc_entry *proc)
{
	struct proc_entry *pp = NULL;	/* previous entry in hashlist chain */
	struct proc_entry *np = NULL;	/* next entry in hashlist chain */

	if (!proc) {
		return;
	}

	/* lock for writing */
	lock_proc_hash(proc->pid, 1);

	pp = proc->hashlist.prev;
	np = proc->hashlist.next;

	if (pp) {
		pp->hashlist.next = proc->hashlist.next;
	}

	if (np) {
		np->hashlist.prev = proc->hashlist.prev;
	}

	if (proc_table[ pid_hash(proc->pid) ] == proc) {
		proc_table[ pid_hash(proc->pid) ] = np;
	}

	proc->hashlist.prev = NULL;
	proc->hashlist.next = NULL;

	/* unlock */
	unlock_proc_hash(proc->pid);

	return;
}

/*
 * init_proc_hash_locks - initialize proc table hash bin locks
 */ 
int init_proc_hash_locks(void)
{
	int	i, n;

	/* initialize the proc hash table mutex locks */
	for (i = 0 ; i < PROC_HASH_SIZE ; i++) {
		n = pthread_rwlock_init(&(proc_hash_lock[i]), NULL);
		if (n) {
			return(-1);
		}
	}
	return(0);
}

/*
 * lock_proc_hash - lock proc table hash bin
 */ 
static void lock_proc_hash(pid_t pid, int write)
{
	if (write) {	/* writing */
		pthread_rwlock_wrlock(&(proc_hash_lock[pid_hash(pid)]));
	} else {	/* reading */
		pthread_rwlock_rdlock(&(proc_hash_lock[pid_hash(pid)]));
	}
}

/*
 * unlock_proc_hash - unlock proc table hash bin
 */ 
static void unlock_proc_hash(pid_t pid)
{
	pthread_rwlock_unlock(&(proc_hash_lock[pid_hash(pid)]));
}

