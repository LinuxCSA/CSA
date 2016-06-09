/*
 * Copyright (c) 2004-2008 Silicon Graphics, Inc.
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
 * jobd.c - JOBs manager daemon
 *
 * Code from the ELSA project (http://elsa.sourceforge.net/)
 * Author: Guillaume Thouvenin <guillaume.thouvenin@bull.net>
 */

#include <pthread.h>
#include <assert.h>		/* for assertion */
#include <dirent.h>		/* opendir() */
#include <errno.h>		/* errno */
#include <fcntl.h>		/* open(), opendir() */
#include <stdio.h>		/* perror() */
#include <stdlib.h>		/* exit() */
#include <string.h>		/* strerror() */
#include <syslog.h>		/* syslog() */
#include <unistd.h>		/* sysconf(), setsid(), fork(), dup(), close() */
#include <time.h>		/* time() */
#include <signal.h>
#include <dlfcn.h>		/* dlopen(), dlerror(), dlsym(), dlclose() */
#include <sys/file.h>		/* flock() */
#include <sys/socket.h>
#include <sys/types.h>		/* fork(), open(), waitpid(), IPC */
#include <sys/un.h>

#include <linux/netlink.h>
/* 
 * As connector is not included in the libc we need to include it through
 * kernel source. The problem is that there is a conflit with some 
 * definitions in time.h. Some variable are defined in sys/time.h and
 * redefined in linux/time.h (included in cn_proc.h). Thus we add this
 * ugly definition to be able to compile. We will remove that hack as 
 * soon as libc includes connectors.
 */
#include <linux/connector.h>
#define _LINUX_TIME_H
#include <linux/cn_proc.h>
#undef _LINUX_TIME_H

#include <job.h>
#include <jobctl.h>
#include <job_csa.h>
#include <job_hash.h>

/* need to turn off _POSIX_SOURCE to get capgetp() from sys/capability.h */
#undef _POSIX_SOURCE
#include <sys/capability.h>

#ifdef DEBUG_JOBD
#ifdef DEBUG_JOBD_STDERR
#define JOBD_DEBUG(args...)	fprintf(stderr, args);fprintf(stderr,"\n");
#else
#define JOBD_DEBUG(args...)	syslog(LOG_INFO, args)
#endif
#else
#define JOBD_DEBUG(args...)	do {} while(0)
#endif				/* DEBUG_JOBD */

#define JOBD_LOGIDENT	"jobd"
#define PROC_BASE	"/proc"

#define	JOBD_LOCKFILE	"/var/run/jobd/lock"

#define MESSAGE_SIZE    (sizeof(struct nlmsghdr) + \
                         sizeof(struct cn_msg)   + \
                         sizeof(int))

/* The states for a job */
#define	RUNNING	1	/* Running job */
#define	ZOMBIE	2	/* Dead job */

/* Job creation tags for the job HID (host ID) */
#define	DISABLED	0xffffffff	/* New job creation disabled */
#define	LOCAL		0x0		/* Only creating local sys jobs */

extern struct job_entry	*job_table[JOB_HASH_SIZE];
extern int		job_table_refcnt;

static	u_int32_t	jid_hid = 0;

struct mesg_info {
	uid_t			user;
	int			priv;
	int			sockfd;
	void			*mesg;
	int			msglen;
};

struct wait_queue {
	struct links		links;
	struct mesg_info	mesg_info;
};

static int sk_nl;		/* socket used by netlink */
static int sk_unix;		/* socket used for local communication */
#define CSA
#ifdef CSA
static int sk_csa;			/* socket used for csa communication */
static int sk_csa_act;			/* currently accepted csa socket */
static int CSA_is_active;		/* Is CSA active? */
static void *libcsad_handle = NULL;	/* handle for libcsad.so */
static pthread_t csad_thread = 0;	/* csa daemon thread identifier */
static void *(*run_csad)(void *fp);	/* pointer to run_csa_daemon() */
static void *(*end_csad)(int unused);	/* pointer to end_csa_daemon() */
#endif

static int lockfd;

static int invalid_pid(pid_t);
static void cleanup(int);
static void fork_handler(int, int);
static void exit_handler(int, int);
static void closeall(void);
static void reply_to_waiters(struct job_entry *);
static void do_job_create(struct mesg_info *);
static void do_job_getjid(struct mesg_info *);
static void do_job_waitjid(struct mesg_info *);
static void do_job_killjid(struct mesg_info *);
static void do_job_getjidcnt(struct mesg_info *);
static void do_job_getjidlst(struct mesg_info *);
static void do_job_getpidcnt(struct mesg_info *);
static void do_job_getpidlst(struct mesg_info *);
static void do_job_getuid(struct mesg_info *);
static void do_job_getprimepid(struct mesg_info *);
static void do_job_sethid(struct mesg_info *);
static void do_job_detachjid(struct mesg_info *);
static void do_job_detachpid(struct mesg_info *);
static void do_job_attachpid(struct mesg_info *);
static void do_job_runcsa(struct mesg_info *);
static void process_mesg(struct mesg_info *);
static void send_reply(struct mesg_info *);
static void send_list(struct mesg_info *, void *, int);
static void recv_sk_nl(int);
static void recv_sk_unix(int);
#ifdef CSA
static void CSA_jobstart(struct job_entry *);
static void CSA_jobend(struct job_entry *);
static void send_sk_csa(int, struct job_event *);
static void recv_sk_csa(int);
static int spawn_csa_daemon(int);
static int abort_csa_daemon(int);
#endif
static void grab_lockfile(void);
static int daemonize(int, int);


/*
 * cn_listen - register to listen to fork/exit connector
 */
static inline void cn_listen(int sock)
{
	char buff[MESSAGE_SIZE+8];	/* must be > MESSAGE_SIZE */
	struct nlmsghdr *hdr;
	struct cn_msg *msg;
	int n;

	/* Clear the buffer */
	memset(buff, '\0', sizeof(buff));

	/* fill the message header */
	hdr = (struct nlmsghdr *) buff;

	hdr->nlmsg_len = MESSAGE_SIZE;
	hdr->nlmsg_type = NLMSG_DONE;
	hdr->nlmsg_flags = 0;
	hdr->nlmsg_seq = 0;
	hdr->nlmsg_pid = getpid();

	/* the message */
	msg = (struct cn_msg *) NLMSG_DATA(hdr);
	msg->id.idx = CN_IDX_PROC;
	msg->id.val = CN_VAL_PROC;
	msg->seq = 0;
	msg->ack = 0;
	msg->len = sizeof(int);
	msg->data[0] = PROC_CN_MCAST_LISTEN;

	n = send(sock, hdr, hdr->nlmsg_len, 0);
	if (n < 0) {
		syslog(LOG_ERR, "netlink fork listen send error: %m");
		cleanup(0);
	}
}

/*
 * cn_ignore - unregister from fork/exit connector
 */
static inline void cn_ignore(int sock)
{
	char buff[MESSAGE_SIZE+8];	/* must be > MESSAGE_SIZE */
	struct nlmsghdr *hdr;
	struct cn_msg *msg;
	int n;

	/* Clear the buffer */
	memset(buff, '\0', sizeof(buff));

	/* fill the message header */
	hdr = (struct nlmsghdr *) buff;

	hdr->nlmsg_len = MESSAGE_SIZE;
	hdr->nlmsg_type = NLMSG_DONE;
	hdr->nlmsg_flags = 0;
	hdr->nlmsg_seq = 0;
	hdr->nlmsg_pid = getpid();

	/* the message */
	msg = (struct cn_msg *) NLMSG_DATA(hdr);
	msg->id.idx = CN_IDX_PROC;
	msg->id.val = CN_VAL_PROC;
	msg->seq = 0;
	msg->ack = 0;
	msg->len = sizeof(int);
	msg->data[0] = PROC_CN_MCAST_IGNORE;

	n = send(sock, hdr, hdr->nlmsg_len, 0);
	if (n < 0) {	/* no big deal */
		syslog(LOG_ERR, "netlink fork ignore send error: %m");
	}
}

/*
 * invalid_pid - test if pid is a valid one
 * @pid: the pid to be tested
 *
 * Test if a process with ID #pid exists or not. We use kill() with
 * signal 0 to test the validity. 
 * Function returns 1 if the pid is _NOT_ valid, 0 otherwise.
 */
static int invalid_pid(pid_t pid)
{
	if (kill(pid, 0) == 0) {
		return 0;	/* pid does exist */
	} else {
		return 1;	/* pid is not an existing process ID */
	}
}

/*
 * cleanup - close sockets and unregister from connector
 */
static void cleanup(int unused)
{
	cn_ignore(sk_nl);
	close(sk_nl);
#ifdef CSA
	close(sk_csa_act);
	close(sk_csa);
	unlink(JOB_CSA_SOCKET);

	if (csad_thread != 0) {
		(*end_csad)(0);
		if (pthread_join(csad_thread, NULL) != 0)
			syslog(LOG_ERR, "pthread_join: %m");
	}

	if (libcsad_handle != NULL)
		dlclose(libcsad_handle);
#endif
	close(sk_unix);
	unlink(JOBD_SOCKET);
	close(lockfd);
	unlink(JOBD_LOCKFILE);

	exit(EXIT_SUCCESS);
}

/*
 * fork_handler - fork handler
 * @ppid: parent PID
 * @cpid: child PID
 *
 * It checks if the parent that creates a child is in a job.  If it is, 
 * we add the child in the same job, otherwise we do nothing. 
 */
static void fork_handler(int ppid, int cpid)
{
	struct proc_entry *child;	/* the child */
	struct proc_entry *parent;	/* the parent */

	/* 
	 * we check if the parent is already registered in the jobs 
	 * hash table 
	 */
	parent = findproc(ppid);
	if ((!parent) || (parent->pid != ppid) || (!parent->job)) {
		/* parent is not in a job so there is nothing to do */
		return;
	}

	JOBD_DEBUG("[fork_handler] pid %d is in a job", ppid);

	/* create and add a new entry in the hash table */
	child = (struct proc_entry *) calloc(1, sizeof(struct proc_entry));
	if (!child) {
		syslog(LOG_ERR, "cannot allocate space for a proc item: %m");
		return;
	}

	/* sanity check */
	if (findproc(cpid)) {
		/* We're in deep doo-doo! */
		syslog(LOG_ERR, "%d already exists.  Did we wrap pids?", cpid);
		JOBD_DEBUG("[fork_handler] pid %d already exists, but it shouldn't!", cpid);
		return;
	}

	/* Initialize proc entry values */
	child->pid = cpid;
	child->status = 0;

	/* Link proc into the job */
	add_proc_to_job(child, parent->job);

	/* Link proc into the proc_table */
	add_proc_to_hash(child);

	return;
}

/*
 * exit_handler - exit handler
 * @pid: PID
 * @status: exit code
 *
 * It checks if the process is in a job. If it is, we remove the pid 
 * from the job, otherwise we do nothing. 
 */
static void exit_handler(int pid, int status)
{
	struct proc_entry *proc;

	/* 
	 * we check if the process is registered in the proc hash table 
	 */

	proc = findproc(pid);
	if ((!proc) || (proc->pid != pid)) {
		/* process is not in a job so there is nothing to do */
		return;
	}

	JOBD_DEBUG("[exit_handler] pid %d is in a job", pid);

	/* save status, in case we need it later */
	proc->status = status;

	/* Unlink proc from the proc_table */
	unlink_proc_from_hash(proc);

	/* Unlink proc from the job */
	unlink_proc_from_job(proc, 1);

	free(proc);

	return;
}

/*
 * closeall - close all FDs 
 */
static void closeall()
{
	int	fd = 0;
	int	fdlimit = sysconf(_SC_OPEN_MAX);

	while (fd < fdlimit) {
		close(fd++);
	}

	return;
}

/*
 * job_cleanup - end-of-job processing
 */ 
void job_cleanup(struct job_entry *job)
{
	if (!job) {
		return;
	}

	/* 
	 * If there are no more processes in the job, we can free 
	 * it.  If there are waiters we need to reply to them first.
	 */
	if (job->refcnt == 0) {
		/* don't let anyone use this job entry */
		unlink_job_from_hash(job);

		/* If there are waiters, reply to them. */
		if (job->waitcnt > 0) {
			/* return job status back to waiters */
			reply_to_waiters(job);
		}

#ifdef CSA
		if (CSA_is_active) {
			CSA_jobend(job);
		}
#endif

		free(job);
	}
}

/*
 * reply_to_waiters - The job is finished.  Send job status 
 *                    to callers of job_waitjid().
 */ 
static void reply_to_waiters(struct job_entry *job)
{
	struct mesg_info	*info;
	struct job_waitjid	*mesg;
	struct wait_queue	*waiter, *next;

	if (!job) {
		return;
	}

	waiter = job->wait.head;
	while (waiter) {
		/* De-queue waiter */
		next = waiter->links.next;
		if (next) {
			next->links.prev = waiter->links.prev;
		}
		job->wait.head = next;
		if (!job->wait.head) {	/* empty queue */
			job->wait.tail = NULL;
		}
		job->waitcnt--;

		/* Send reply */
		info = &(waiter->mesg_info);
		mesg = info->mesg;
		mesg->stat = job->waitstat;
		mesg->r_jid = job->jid;

		send_reply(info);

		/* Free info and info->mesg */
		free(info->mesg);
		free(waiter);

		waiter = next;	/* next, please */
	}

	return;
}

/*
 * do_job_create - process calls to job_create().
 */ 
static void do_job_create(struct mesg_info *info)
{
	struct job_create	*mesg = info->mesg;
	struct job_entry	*job = NULL;
	struct job_entry	*oldjob = NULL;
	struct proc_entry	*proc = NULL;
	static u_int32_t	jid_count = 1;
	u_int32_t		initial_jid_count;
	int			newproc = 0;


	if (jid_hid == DISABLED) {
		mesg->hdr.err = 0;
		goto err_reply;
	}
	if (!info->priv) {
		mesg->hdr.err = EPERM;
		goto err_reply;
	}

	if (invalid_pid(mesg->pid)) {
		syslog(LOG_ERR, "%d is not a valid pid", mesg->pid);
		mesg->hdr.err = ESRCH;
		goto err_reply;
	}

	proc = findproc(mesg->pid);
	if (!proc) {
		proc = (struct proc_entry *) calloc(1, sizeof(struct proc_entry));
		if (!proc) {
			mesg->hdr.err = ENOMEM;
			goto err_reply;
		}
		newproc = 1;
	}

	job = (struct job_entry *) calloc(1, sizeof(struct job_entry));
	if (!job) {
		mesg->hdr.err = ENOMEM;
		goto err_reply;
	}

	if (mesg->jid != 0) {
		/* We use the specified JID value */
		job->jid = mesg->jid;

		/* Does the supplied JID conflict with an existing one? */
		if (findjob(job->jid)) {
			/* JID already in use, bail. */
			mesg->hdr.err = EBUSY;
			goto err_reply;
		}
	} else {
		/* We generate a new JID value */
		*(iptr_hid(job->jid)) = jid_hid;
		*(iptr_sid(job->jid)) = jid_count;
		initial_jid_count = jid_count++;
		while (((job->jid == 0) || (findjob(job->jid))) && jid_count != initial_jid_count) {
			/* JID was in use or was zero, try a new one */
			*(iptr_sid(job->jid)) = jid_count++;
		}
		/* If all of the JIDs are in use, fail */
		if (jid_count == initial_jid_count) {
			mesg->hdr.err = EBUSY;
			goto err_reply;
		}
	}

        /* Initialize job entry values & lists */
	job->refcnt = 0;
	job->user = mesg->user;
	job->start = time(NULL);;
	job->state = RUNNING;
	job->proclist = NULL;
	job->wait.head = NULL;
	job->wait.tail = NULL;
	job->waitcnt = 0;
	job->waitstat = 0;

	/* Initialize proc entry values */
	proc->pid = mesg->pid;
	proc->status = 0;

	/* If we are using an existing proc... */
	if (!newproc) {
		/* save pointer to old job */
		oldjob = proc->job;
		/* Unlink proc from old job */
		unlink_proc_from_job(proc, 0);
	}

	/* Link proc into new job */
	add_proc_to_job(proc, job);

	/* proc has been reassigned to the new job, so now 
	 * we can clean up the old job, if necessary. */
	if (oldjob) {
		job_cleanup(oldjob);
	}

	/* Link new proc into the proc_table */
	if (newproc) {
		add_proc_to_hash(proc);
	}

	/* Link job into the job_table */
	add_job_to_hash(job);

	/* All done, return jid */
	mesg->r_jid = job->jid;

#ifdef CSA
	if (CSA_is_active) {
		CSA_jobstart(job);
	}
#endif

	send_reply(info);

	return;

err_reply:
	if (newproc && proc) {
		free(proc);
	}
	if (job) {
		free(job);
	}

	mesg->r_jid = 0;

	send_reply(info);

	return;
}

/*
 * do_job_getjid - process calls to job_getjid().
 */
static void do_job_getjid(struct mesg_info *info)
{
	struct job_getjid	*mesg = info->mesg;
	struct proc_entry	*proc;

	proc = findproc(mesg->pid);
	if (proc) {
		mesg->r_jid = proc->job->jid;
		if (mesg->r_jid == 0) {
			mesg->hdr.err = ENODATA;
		}
	} else {
		mesg->r_jid = 0;
		if (invalid_pid(mesg->pid)) {
			mesg->hdr.err = ESRCH;
		} else {
			mesg->hdr.err = ENODATA;
		}
	}

	send_reply(info);

	return;
}

/*
 * do_job_waitjid - process calls to job_waitjid().
 */
static void do_job_waitjid(struct mesg_info *info)
{
	struct job_entry	*job;
	struct job_waitjid	*new = NULL, *mesg = info->mesg;
	struct wait_queue	*waiter = NULL, *last;

	mesg->r_jid = mesg->stat = 0;

	if (mesg->options != 0) {
		/* options?  We got no options. */
		mesg->hdr.err = EINVAL;
		goto err_reply;
	}

	job = findjob(mesg->jid);
	if (!job) {
		mesg->hdr.err = ENODATA;
		goto err_reply;
	}

	/* We need to save all of the mesg info, since the info and info->mesg
	 * that were passed in are not static, and they will go away.
	 */
	waiter = (struct wait_queue *) calloc(1, sizeof(struct wait_queue));
	if (!waiter) {
		mesg->hdr.err = ENOMEM;
		goto err_reply;
	}

	new = (struct job_waitjid *) calloc(1, sizeof(struct job_waitjid));
	if (!new) {
		mesg->hdr.err = ENOMEM;
		goto err_reply;
	}

	memcpy(new, mesg, sizeof(struct job_waitjid));
	memcpy(&(waiter->mesg_info), info, sizeof(struct mesg_info));
	waiter->mesg_info.mesg = new;

	/* Now just put it on the queue for later. */
	last = job->wait.tail;
	job->wait.tail = waiter;
	waiter->links.prev = last;
	if (last) {
		last->links.next = waiter;
	}
	if (!job->wait.head) {	/* first in line */
		job->wait.head = waiter;
	}

	job->waitcnt++;

	return;

err_reply:
	if (waiter) {
		free(waiter);
	}
	if (new) {
		free(new);
	}
	send_reply(info);

	return;
}

/*
 * do_job_killjid - process calls to job_killjid().
 * 
 * Question: Would it be better to do a setuid(user) before killing 
 *           the processes in the job?
 * 
 *           Pro: It just seems like it would be "safer", and 
 *                "The right thing to do".
 * 
 *           Con: We'd prefer not to have to keep jerking our uid back and 
 *                forth between root and user.  Also, the "prime" process, 
 *                or initiating process for the job may not be owned by 
 *                the user.  So, we would get an error back from kill() 
 *                for that process.  The job kernel module used to handle 
 *                this by ignoring the error for that specific process, 
 *                assuming that it should exit when all of it's child 
 *                processes exit anyway.  Eew.
 * 
 * Answer:   We had decided to play it safe and switch to the user, but we'd 
 *           have to set both real and effective uid to the user to have any
 *           effect on kill().  And it appears that in Linux there's no way 
 *           to switch our uids back after doing that :( and we're certainly 
 *           not going to fork a child to do it.  So I guess for now we'll 
 *           have to do the killing as root.
 */

static void do_job_killjid(struct mesg_info *info)
{
	struct job_killjid	*mesg = info->mesg;
	struct job_entry	*job;
	struct proc_entry	*proc;
	int			ret = 0;

	mesg->r_val = -1;

	if (mesg->sig < 0) {
		mesg->hdr.err = EINVAL;
		goto killjidreply;
	}

	job = findjob(mesg->jid);
	if (!job) {
		mesg->hdr.err = ENODATA;
		goto killjidreply;
	}

	/* The signaling user must be privileged, 
	 * or must be the owner of the job.
	 */
	if (!info->priv) {
		if (info->user != job->user) {
			mesg->hdr.err = EPERM;
			goto killjidreply;
		}
	}

	mesg->r_val = 0;

	/* 
	 * Here's where we would setuid to the user if we were
	 * doing the kill() as the user.
	 */

	proc = job->proclist;
	while (proc) {
		ret = kill(proc->pid, mesg->sig);
		if (ret != 0) {
			/* 
			 * If we were doing the kill() as the user, we'd
			 * have to ignore bad status for the last (prime) 
			 * process.  See note above for explanation.
			 *
			 * if (proc->jobprocs.next) {
			 * 	mesg->r_val = -1;
			 * 	mesg->hdr.err = errno;
			 * }
			 */

			mesg->r_val = -1;
			mesg->hdr.err = errno;
		}
		proc = proc->jobprocs.next;
	}

	/* 
	 * Here's where we would setuid back to root if we were
	 * doing the kill() as the user.
	 */

killjidreply:
	send_reply(info);

	return;
}

/*
 * do_job_getjidcnt - process calls to job_getjidcnt().
 */
static void do_job_getjidcnt(struct mesg_info *info)
{
	struct job_jidcnt	*mesg = info->mesg;

	mesg->r_val = job_table_refcnt;

	send_reply(info);

	return;
}

/*
 * do_job_getjidlist - process calls to job_getjidlist().
 */
static void do_job_getjidlst(struct mesg_info *info)
{
	struct job_jidlst	*mesg = info->mesg;
	struct job_entry	*job;
	int	count, i, index = 0;
	jid_t	*jid = NULL;

	if (mesg->r_val == 0) {
		goto getjidlstreply;
	}

	count = (mesg->r_val < job_table_refcnt) ? mesg->r_val : job_table_refcnt;

	jid = (jid_t *) calloc(count, sizeof(jid_t));
	if (!jid) {
		mesg->hdr.err = ENOMEM;
		goto getjidlstreply;
	}

	index = 0;
	for (i = 0; i < JOB_HASH_SIZE && index < count; i++) {
		job = job_table[i];
		while (job) {
			jid[index++] = job->jid;
			if (index == count) {
				break;
			}
			job = job->hashlist.next;
		}
	}

	mesg->r_val = index;

getjidlstreply:
	send_list(info, jid, (index * sizeof(jid_t *)));

	if (jid) {
		free(jid);
	}

	return;
}

/*
 * do_job_getpidcnt - process calls to job_getpidcnt().
 */
static void do_job_getpidcnt(struct mesg_info *info)
{
	struct job_pidcnt	*mesg = info->mesg;
	struct job_entry	*job;

	job = findjob(mesg->jid);
	if (job) {
		mesg->r_val = job->refcnt;
	} else {
		mesg->r_val = 0;
		mesg->hdr.err = ENODATA;
	}

	send_reply(info);

	return;
}

/*
 * do_job_getpidlist - process calls to job_getpidlist().
 */
static void do_job_getpidlst(struct mesg_info *info)
{
	struct job_pidlst	*mesg = info->mesg;
	struct job_entry	*job;
	struct proc_entry	*proc;
	int	count, index = 0;
	pid_t	*pid = NULL;

	if (mesg->r_val == 0) {
		goto getpidlstreply;
	}

	job = findjob(mesg->jid);
	if (!job) {
		mesg->hdr.err = ENODATA;
		goto getpidlstreply;
	}

	count = (mesg->r_val < job->refcnt) ? mesg->r_val : job->refcnt;

	pid = (pid_t *) calloc(count, sizeof(pid_t));
	if (!pid) {
		mesg->hdr.err = ENOMEM;
		goto getpidlstreply;
	}

	index = 0;
	proc = job->proclist;
	while (proc && index < count) {
		pid[index++] = proc->pid;
		if (index == count) {
			break;
		}
		proc = proc->jobprocs.next;
	}

	mesg->r_val = index;

getpidlstreply:
	send_list(info, pid, (index * sizeof(pid_t *)));

	if (pid) {
		free(pid);
	}

	return;
}

/*
 * do_job_getuid - process calls to job_getuid().
 */
static void do_job_getuid(struct mesg_info *info)
{
	struct job_user	*mesg = info->mesg;
	struct job_entry	*job;

	job = findjob(mesg->jid);
	if (job) {
		mesg->r_user = job->user;
	} else {
		mesg->r_user = 0;
		mesg->hdr.err = ENODATA;
	}

	send_reply(info);

	return;
}

/*
 * do_job_getprimepid - process calls to job_getprimepid().
 */
static void do_job_getprimepid(struct mesg_info *info)
{
	struct job_primepid	*mesg = info->mesg;
	struct job_entry	*job;
	struct proc_entry	*proc;

	job = findjob(mesg->jid);
	if (job) {
		if (job->proclist) {
			proc = job->proclist;
			/* find the oldest proc */
			while (proc->jobprocs.next) {
				proc = proc->jobprocs.next;
			}
			mesg->r_pid = proc->pid;
		} else {	/* shouldn't happen? */
			mesg->r_pid = 0;
			mesg->hdr.err = ESRCH;
		}
	} else {
		mesg->r_pid = 0;
		mesg->hdr.err = ENODATA;
	}

	send_reply(info);

	return;
}

/*
 * do_job_sethid - process calls to job_sethid().
 */
static void do_job_sethid(struct mesg_info *info)
{
	struct job_sethid	*mesg = info->mesg;

	if (!info->priv) {
		mesg->hdr.err = EPERM;
	} else {
		mesg->r_hid = jid_hid = mesg->hid;
	}

	send_reply(info);

	if (jid_hid == DISABLED) {
		cleanup(0);	/* bye bye */
	}

	return;
}

/*
 * do_job_detachjid - process calls to job_detachjid().
 */
static void do_job_detachjid(struct mesg_info *info)
{
	struct job_detachjid	*mesg = info->mesg;
	struct job_entry	*job;
	struct proc_entry	*proc;
	int			count;

	mesg->r_val = 0;

	if (!info->priv) {
		mesg->hdr.err = EPERM;
		goto detachjidreply;
	}

	job = findjob(mesg->jid);
	if (job) {
		/* 
		 * We cannot use:
		 * 
		 *    while (job->refcnt) ...
		 * 
		 * because when the last proc is unlinked, unlink_proc_from_job() 
		 * will free the job as soon as it decrements job->refcnt to zero.
		 * So we will never see job->refcnt == 0 here.  In fact we have to 
		 * be very careful not to use job at all, once we get into the loop.
		 */
		mesg->r_val = count = job->refcnt;

		while (count--) {
			proc = job->proclist;

			/* Unlink proc from the proc_table */
			unlink_proc_from_hash(proc);

			/* Unlink proc from the job */
			unlink_proc_from_job(proc, 1);

			free(proc);
		}
	} else {
		mesg->r_val = 0;
		mesg->hdr.err = ENODATA;
	}

detachjidreply:
	send_reply(info);

	return;
}

/*
 * do_job_detachpid - process calls to job_detachpid().
 */
static void do_job_detachpid(struct mesg_info *info)
{
	struct job_detachpid	*mesg = info->mesg;
	struct proc_entry	*proc;

	mesg->r_jid = 0;

	if (!info->priv) {
		mesg->hdr.err = EPERM;
		goto detachpidreply;
	}

	proc = findproc(mesg->pid);
	if (proc) {
		mesg->r_jid = proc->job->jid;
		if (mesg->r_jid == 0) {
			mesg->hdr.err = ENODATA;
		}

		/* Unlink proc from the proc_table */
		unlink_proc_from_hash(proc);

		/* Unlink proc from the job */
		unlink_proc_from_job(proc, 1);

		free(proc);
	} else {
		mesg->r_jid = 0;
		mesg->hdr.err = ESRCH;
	}

detachpidreply:
	send_reply(info);

	return;
}

/*
 * do_job_attachpid - process calls to job_attachpid().
 */
static void do_job_attachpid(struct mesg_info *info)
{
	struct job_attachpid	*mesg = info->mesg;
	struct proc_entry	*proc;
	struct job_entry	*job;
	jid_t			jid = mesg->r_jid;

	mesg->r_jid = 0;

	if (!info->priv) {
		mesg->hdr.err = EPERM;
		goto attachpidreply;
	}

	proc = findproc(mesg->pid);
	if (proc) {
		/* already part of a job */
		mesg->hdr.err = EINVAL;
		goto attachpidreply;
	}

	if (invalid_pid(mesg->pid)) {
		/* this pid is no good */
		mesg->hdr.err = ESRCH;
		goto attachpidreply;
	}

	job = findjob(jid);
	if (!job) {
		mesg->hdr.err = ENODATA;
		goto attachpidreply;
	}

	proc = (struct proc_entry *) calloc(1, sizeof(struct proc_entry));
	if (!proc) {
		mesg->hdr.err = ENOMEM;
		goto attachpidreply;
	}

	/* Initialize proc entry values */
	proc->pid = mesg->pid;
	proc->status = 0;

	/* Link proc into the job */
	add_proc_to_job(proc, job);

	/* Link proc into the proc_table */
	add_proc_to_hash(proc);

	mesg->r_jid = jid;

attachpidreply:
	send_reply(info);

	return;
}

/*
 * do_job_runcsa - process calls to job_runcsa().
 */
static void do_job_runcsa(struct mesg_info *info)
{
	struct job_runcsa	*mesg = info->mesg;

	if (!info->priv)
		mesg->hdr.err = EPERM;
	else {
		switch (mesg->run) {
#ifdef CSA
		case 1:
			mesg->r_val = spawn_csa_daemon(0);
			break;
		case 0:
			mesg->r_val = abort_csa_daemon(0);
			break;
#endif
		default:
			mesg->r_val = -1;
		}
	}

	send_reply(info);

	return;
}


/*
 * process_mesg - process a request from the local socket
 */
static void process_mesg(struct mesg_info *info)
{
	struct job_hdr	*hdr = info->mesg;

	switch (hdr->req) {
	case JOB_CREATE:
		do_job_create(info);
		break;
	case JOB_GETJID:
		do_job_getjid(info);
		break;
	case JOB_WAITJID:
		do_job_waitjid(info);
		break;
	case JOB_KILLJID:
		do_job_killjid(info);
		break;
	case JOB_GETJIDCNT:
		do_job_getjidcnt(info);
		break;
	case JOB_GETJIDLST:
		do_job_getjidlst(info);
		break;
	case JOB_GETPIDCNT:
		do_job_getpidcnt(info);
		break;
	case JOB_GETPIDLST:
		do_job_getpidlst(info);
		break;
	case JOB_GETUSER:
		do_job_getuid(info);
		break;
	case JOB_GETPRIMEPID:
		do_job_getprimepid(info);
		break;
	case JOB_SETHID:
		do_job_sethid(info);
		break;
	case JOB_DETACHJID:
		do_job_detachjid(info);
		break;
	case JOB_DETACHPID:
		do_job_detachpid(info);
		break;
	case JOB_ATTACHPID:
		do_job_attachpid(info);
		break;
	case JOB_RUNCSA:
		do_job_runcsa(info);
		break;
/* 
/* 
 * === NOT IMPLEMENTED ===
 * As far as I can tell, these requests have never been used, and 
 * at this point they never will be used.  They are only documented 
 * here for historical purposes.
 *
 *	case JOB_ATTACH:
 *	case JOB_DETACH:
 *	case JOB_SETJLIMIT:
 *	case JOB_GETJLIMIT:
 *	case JOB_GETJUSAGE:
 *	case JOB_FREE:
 *
 * === NOT IMPLEMENTED ===
 */
	default:
		syslog(LOG_ERR, "Unknown command: %d", hdr->req);
		hdr->err = EBADRQC;
		send_reply(info);
	}

	return;
}

/*
 * send_reply - return status and requested info to client
 */
static void send_reply(struct mesg_info *info)
{
	int	n;

	n = write(info->sockfd, info->mesg, info->msglen);
	if (n < 0) {
		if (errno != EPIPE) {	/* they've closed their end */
			syslog(LOG_ERR, "socket write error: %m");
		}
	}

	close(info->sockfd);

	return;
}

/*
 * send_list - return status plus list data to client
 */
static void send_list(struct mesg_info *info, void *buf, int len)
{
	int	n;

	n = write(info->sockfd, info->mesg, info->msglen);
	if (n < 0) {
		if (errno != EPIPE) {	/* they've closed their end */
			syslog(LOG_ERR, "socket write error: %m");
			goto reply_done;
		}
	}

	if (len > 0) {
		n = write(info->sockfd, buf, len);
		if (n < 0) {
			if (errno != EPIPE) {
				syslog(LOG_ERR, "socket write error: %m");
			}
		}
	}

reply_done:
	close(info->sockfd);

	return;
}

/*
 * recv_sk_nl - receive a message from the netlink socket
 */
static void recv_sk_nl(int sk)
{
	char buff[CONNECTOR_MAX_MSG_SIZE];
	struct nlmsghdr *hdr;
	struct cn_msg *msg;
	struct proc_event *ev;
	int len;
	int cpuid;
	int ppid;
	int ptgid;
	int cpid;
	int ctgid;
	int status = 0;

	/* Clear the buffer */
	memset(buff, '\0', sizeof(buff));

	/* Listen */
	while (recv(sk, buff, sizeof(buff), 0) < 0) {
		if (errno != EINTR && errno != ENOBUFS) {
			syslog(LOG_ERR, "netlink recv error: %m");
			return;
		}
	}

	/* point to the message header */
	hdr = (struct nlmsghdr *)buff;

	switch (hdr->nlmsg_type) {
	case NLMSG_DONE:
		msg = (struct cn_msg *)NLMSG_DATA(hdr);
		ev = (struct proc_event *)msg->data;
		if (ev->what == PROC_EVENT_FORK) {
			cpuid = ev->cpu;
			ppid  = ev->event_data.fork.parent_pid;
			ptgid = ev->event_data.fork.parent_tgid;
			cpid  = ev->event_data.fork.child_pid;
			ctgid = ev->event_data.fork.child_tgid;
			JOBD_DEBUG("[seq#%d] %d %d - %d %d",
			    msg->seq, ppid, ptgid, cpid, ctgid);
			fork_handler(ppid, cpid);
		} else if (ev->what == PROC_EVENT_EXIT) {
#ifdef CSA
			if (CSA_is_active) {
				/* 
				 * We'll get exit notification 
				 * from CSA instead of here. 
				 */
				break;
			}
#endif
			cpuid = ev->cpu;
			ppid  = ev->event_data.exit.process_pid;
			ptgid = ev->event_data.exit.process_tgid;
			ptgid = ev->event_data.exit.process_tgid;
/* 
 * NOTE - Everything is returned in ev->event_data.exit.exit_code.
 *        It's in the same format that is returned by wait() -- 
 *            8 bits status and 8 bits signal
 *        We don't care about ev->event_data.exit.exit_signal. 
 *        The job kernel module just returned task->exit_code -- 
 *        it didn't do anything with task->exit_signal.  So we
 *        do the same. 
*/
			status = ev->event_data.exit.exit_code;

			JOBD_DEBUG("[seq#%d] %d %d",
			    msg->seq, ppid, ptgid);
			exit_handler(ppid, status);
		}
		break;
	case NLMSG_ERROR:
		syslog(LOG_ERR, "recv_sk_nl error: %m");
		/* Fall through */
	default:
		break;

		return;
	}
}

/*
 * recv_sk_unix - receive a message from the local socket
 */
static void recv_sk_unix(int sk)
{
	struct sockaddr_un fromaddr;
	union job_message mesg;
	struct mesg_info info;
	unsigned int    fromlen;
	int	fromfd;
	int    sz, remaining;
	struct ucred cr;
	unsigned int cl=sizeof(cr);
	static cap_t cap_d = NULL;
	cap_flag_value_t cap;

	fromlen = sizeof(fromaddr);

	if (cap_d != NULL)
		cap_free(cap_d);

	cap_d = cap_init();  /* allocate cap_t */
	if (cap_d == NULL) {
		syslog(LOG_ERR, "cap_init error: %m");
	}

	fromfd = accept(sk, (struct sockaddr *)&fromaddr, &fromlen);
	if (fromfd < 0) {
		syslog(LOG_ERR, "error accepting connection: %m");
		JOBD_DEBUG("accept(...) failed, errno=%d", errno);
		return;
	}

	/* get credentials of caller */
	if (getsockopt(fromfd, SOL_SOCKET, SO_PEERCRED, &cr, &cl) == 0) {
		/* get capabilities of caller */
		if (capgetp(cr.pid, cap_d)) {
			syslog(LOG_ERR, "error getting capabilities: %m");
			/* No special privileges for you! */
			info.priv = 0;
		} else {
			/* CAP_SYS_RESOURCE is required for priveleged requests */
			cap_get_flag(cap_d, CAP_SYS_RESOURCE, CAP_EFFECTIVE, &cap);
			if (cap == CAP_SET) {
				info.priv = 1;
			} else {
				info.priv = 0;
			}
		}
	} else {
		/* FAILED!
		 * OK, we'll accept this connection, but you are NOBODY. 
		 * If you're trying to do anything that requires ANY type 
		 * of authentication, tough luck, you're screwed. 
		 */
		syslog(LOG_ERR, "error getting credentials: %m");
		cr.pid = cr.uid = cr.gid = (~0);
		info.priv = 0;
	}
	JOBD_DEBUG("[recv_sk_unix] Peer's pid=%d, uid=%d, gid=%d", cr.pid, cr.uid, cr.gid);

	sz = read(fromfd, &mesg, sizeof(struct job_hdr));
	if (sz != sizeof(struct job_hdr)) {
		if (sz < 0) {
			syslog(LOG_ERR, "socket read error: %m");
		} else {
			syslog(LOG_ERR, "socket short read: %d bytes", sz);
		}
		close(fromfd);
		return;
	}

	remaining = mesg.hdr.size - sz;

	sz = read(fromfd, ((char*)&mesg + sz), remaining);
	if (sz != remaining) {
		if (sz < 0) {
			syslog(LOG_ERR, "socket read error: %m");
		} else {
			syslog(LOG_ERR, "socket short read: %d bytes", sz);
		}
		close(fromfd);
		return;
	}

	info.user = cr.uid;
	info.sockfd = fromfd;
	info.mesg = &mesg;
	info.msglen = mesg.hdr.size;

	process_mesg(&info);

	return;
}

#ifdef CSA
static void CSA_jobstart(struct job_entry *job)
{
	struct job_event	msg;

	msg.type = JOB_EVENT_START;
	msg.jid = job->jid;
	msg.user = job->user;
	msg.start = job->start;

	send_sk_csa(sk_csa_act, &msg);

	return;
}

static void CSA_jobend(struct job_entry *job)
{
	struct job_event	msg;

	msg.type = JOB_EVENT_END;
	msg.jid = job->jid;
	msg.user = job->user;
	msg.start = job->start;

	send_sk_csa(sk_csa_act, &msg);

	return;
}

/*
 * send_sk_csa - send a message to the csa socket
 */
static void send_sk_csa(int sk, struct job_event *msg)
{
	int    			sz;

	sz = write(sk, msg, sizeof(struct job_event));
	if (sz != sizeof(struct job_event)) {
		if (sz < 0) {
			syslog(LOG_ERR, "CSA socket write error: %m");
			CSA_is_active = 0;
		} else {
			syslog(LOG_ERR, "CSA socket short write: %d bytes", sz);
		}
		return;
	}

	return;
}

/*
 * recv_sk_csa - receive a message from the csa socket
 */
static void recv_sk_csa(int sk)
{
	struct proc_exit	msg;
	int    			sz;

	sz = read(sk, &msg, sizeof(struct proc_exit));
	if (sz != sizeof(struct proc_exit)) {
		if (sz < 0) {
			syslog(LOG_ERR, "CSA socket read error: %m");
		} else {
			syslog(LOG_ERR, "CSA socket short read: %d bytes", sz);
			CSA_is_active = 0;
		}
		return;
	}

	exit_handler(msg.pid, msg.exitstat);

	return;
}

static int spawn_csa_daemon(int unused)
{
	char *error;

	if (CSA_is_active)
		return 0;

	if (libcsad_handle == NULL) {
		libcsad_handle = dlopen("libcsad.so", RTLD_LAZY);
		if (libcsad_handle == NULL) {
			syslog(LOG_ERR, "dlopen: %s", dlerror());
			return -1;
		}
	}

	dlerror();

	*((void **) &run_csad) = dlsym(libcsad_handle, "run_csa_daemon");
	if ((error = dlerror()) != NULL) {
		syslog(LOG_ERR, "dlsym: %s", error);
		return -1;
	}

	*((void **) &end_csad) = dlsym(libcsad_handle, "end_csa_daemon");
	if ((error = dlerror()) != NULL) {
		syslog(LOG_ERR, "dlsym: %s", error);
		return -1;
	}

	if (csad_thread != 0) {
		(*end_csad)(0);
		if (pthread_join(csad_thread, NULL) != 0)
			syslog(LOG_ERR, "pthread_join: %m");
	}

	if (pthread_create(&csad_thread, NULL, run_csad, jid_lookup) != 0) {
		syslog(LOG_ERR, "pthread_create: %m");
		csad_thread = 0;
		return -1;
	}

	return 0;
}

static int abort_csa_daemon(int unused)
{
	if ((libcsad_handle == NULL) || (csad_thread == 0))
		return -1;

	(*end_csad)(0);

	if (pthread_join(csad_thread, NULL) != 0) {
		syslog(LOG_ERR, "pthread_join: %m");
		return -1;
	} else {
		csad_thread = 0;
		return 0;
	}
}
#endif

/* 
 * Lock the lockfile to ensure there is not another jobd already running.
 */
static void grab_lockfile()
{
	int	rc;

	lockfd = open(JOBD_LOCKFILE, O_RDWR|O_CREAT, 0600);
	if (lockfd < 0) {
		perror(JOBD_LOCKFILE);
		exit(1);
	}

	rc = flock(lockfd, LOCK_EX|LOCK_NB);
	if (rc < 0) {
		if (errno == EWOULDBLOCK) {
			fprintf(stderr, "Job daemon is already running.\n");
		} else {
			perror(JOBD_LOCKFILE);
		}
		exit(1);
	}

	return;
}

/*
 * daemonize - detach process from user and disappear into the background
 * @nochdir: set directory to "/" if not set
 * @noclose: close all opened file descriptors if not set
 *
 * returns -1 on failure, but you can't do much except exit in that case
 * since we may already have forked. 
 */
static int daemonize(int nochdir, int noclose)
{
	/* 
	 * First, we fork to allow parent to exit. This returns control to 
	 * the command line. 
	 */
	switch (fork()) {
	case -1:
		/* error */
		return -1;
	case 0:
		/* child */
		break;
	default:
		/* Parent can exit */
		exit(EXIT_SUCCESS);
	}

	/* Now we're not a process group leader */
	if (setsid() < 0)	/* shoudn't fail */
		return -1;

	/* 
	 * New session is created and we are the leader. 
	 * Since a  controlling terminal is associated with a session, 
	 * and this new session has not yet acquired a controlling terminal 
	 * our process now has no controlling terminal, which is a Good Thing 
	 * for daemons.
	 * fork() again so the parent, (the session group leader), can exit.
	 * This means that we, as a non-session group leader, can never regain a
	 * controlling terminal.
	 */
	switch (fork()) {
	case -1:
		/* error */
		return -1;
	case 0:
		/* child */
		break;
	default:
		/* Parent can exit */
		exit(EXIT_SUCCESS);
	}

	if (!nochdir)
		chdir("/");

	if (!noclose) {
		closeall();
		open("/dev/null", O_RDWR);
		dup(0);
		dup(0);
	}

	return 0;
}

/*
 * main - Principal routine and also a river rising in eastern Germany.
 *
 * Here are the different tasks:
 *    1) Become a daemon
 *    2) Open log file and run.
 *    3) Install signal handler
 *    4) Create sockets
 *    5) Run the infinite loop and wait for messages (or signal)
 */
int main(int argc, char **argv)
{
	int err;
	struct sockaddr_nl sa_nl;	/* info for the netlink interface */
	struct sockaddr_un sa_unix;	/* describs an PF_UNIX socket */
	fd_set fds;			/* file descriptor set */
	int max_fds, n, on=1;

	umask(077);

#ifndef DEBUG_JOBD_STDERR
	if (daemonize(0, 0) < 0) {
		perror("daemonize");
		exit(EXIT_FAILURE);
	}
#endif

	grab_lockfile();

	umask(0);

	/* open connection to the system logger */
	openlog(JOBD_LOGIDENT, LOG_PID, LOG_DAEMON);

	/* Install signal handler */
	signal(SIGUSR1, cleanup);
	signal(SIGPIPE, SIG_IGN);

	/* initialize the proc hash table mutex locks */
	n = init_proc_hash_locks();
	if (n) {
		syslog(LOG_ERR, "pthread_rwlock_init: %m");
		exit(EXIT_FAILURE);
	}

	/* 
	 * Now we can start the communication.
	 * First, the jobd daemon listens for forks and exits and it also 
	 * listens for user request. So we will use select() to be aware
	 * of the two events.
	 */

	/*
	 * The netlink communication 
	 * 
	 * Create an endpoint for communication. Use the kernel user
	 * interface device (PF_NETLINK) which is a datagram oriented
	 * service (SOCK_DGRAM). The netlink family used is
	 * NETLINK_CONNECTOR because we use the Process Events Connector
	 * patch.
	 */
	sk_nl = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (sk_nl == -1) {
		syslog(LOG_ERR, "socket sk_nl error: %m");
		return -1;
	}

	sa_nl.nl_family = AF_NETLINK;
	sa_nl.nl_groups = CN_IDX_PROC;
	sa_nl.nl_pid    = getpid();

	err = bind(sk_nl, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
	if (err == -1) {
		syslog(LOG_ERR, "binding sk_nl error: %m");
		close(sk_nl);
		return -1;
	}

	/*
	 * The local communication
	 *
	 * creates a socket for local interprocess communication 
	 */
	bzero((char *) &sa_unix, sizeof(sa_unix));
	sa_unix.sun_family = AF_UNIX;
	strcpy(sa_unix.sun_path, JOBD_SOCKET);

	sk_unix = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sk_unix == -1) {
		syslog(LOG_ERR, "socket sk_unix error: %m");
		close(sk_unix);
		close(sk_nl);
		return -1;
	}

	/* we need credentials for authentication */
	n = setsockopt(sk_unix, SOL_SOCKET, SO_PASSCRED, &on, sizeof(int));
	if (n < 0) {
		syslog(LOG_ERR, "setsockopt sk_unix error: %m");
	}

	unlink(JOBD_SOCKET);

	err = bind(sk_unix, (struct sockaddr *)&sa_unix, sizeof(sa_unix));
	if (err == -1) {
		syslog(LOG_ERR, "binding sk_unix error: %m");
		close(sk_unix);
		close(sk_nl);
		return -1;
	}

#ifdef CSA
	/*
	 * CSA communication.
	 *
	 */
	bzero((char *) &sa_unix, sizeof(sa_unix));
	sa_unix.sun_family = AF_UNIX;
	strcpy(sa_unix.sun_path, JOB_CSA_SOCKET);

	sk_csa = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sk_csa == -1) {
		syslog(LOG_ERR, "socket sk_csa error: %m");
		close(sk_unix);
		close(sk_nl);
		return -1;
	}

	unlink(JOB_CSA_SOCKET);

	err = bind(sk_csa, (struct sockaddr *)&sa_unix, sizeof(sa_unix));
	if (err == -1) {
		syslog(LOG_ERR, "binding sk_csa error: %m");
		close(sk_csa);
		close(sk_unix);
		close(sk_nl);
		return -1;
	}

	sk_csa_act = 0;
	CSA_is_active = 0;
#endif

	/* We're ready to receive netlink and local messages */
	cn_listen(sk_nl);

	n = listen(sk_unix, 5);
	if (n < 0) {
		syslog(LOG_ERR, "listen sk_unix failed: %m");
		cleanup(0);
	}

#ifdef CSA
	n = listen(sk_csa, 5);
	if (n < 0) {
		syslog(LOG_ERR, "listen sk_csa failed: %m");
		cleanup(0);
	}
#endif

	/* Doing synchronous I/O multiplexing */
	max_fds = (sk_nl > sk_unix) ? sk_nl + 1 : sk_unix + 1;
#ifdef CSA
	max_fds = (sk_csa >= max_fds) ? sk_csa + 1 : max_fds;
#endif

	for (;;) {
		/* waiting for sk_nl and sk_unix socket changes */
		FD_ZERO(&fds);
		FD_SET(sk_nl, &fds);
#ifdef CSA
		if (!CSA_is_active)
			FD_SET(sk_csa, &fds);
		else
			FD_SET(sk_csa_act, &fds);
#endif
		FD_SET(sk_unix, &fds);

		err = select(max_fds, &fds, NULL, NULL, NULL);
		if (err < 0) {
			syslog(LOG_ERR, " !!! select error: %m");
			continue;
		}

		if (FD_ISSET(sk_nl, &fds)) {
			recv_sk_nl(sk_nl);
			/* We can't afford to miss anything on this socket,
			 * so we give it priority over the others.  That's 
			 * why we check for more on sk_nl before looking at 
			 * anything else.
			 */
			continue;
		}

#ifdef CSA
		if (!CSA_is_active && FD_ISSET(sk_csa, &fds)) {
			if (sk_csa_act)
				close(sk_csa_act);
			sk_csa_act = accept(sk_csa, NULL, NULL);
			if (sk_csa_act < 0) {
				syslog(LOG_ERR, "accept sk_csa failed: %m");
				cleanup(0);
			}
			max_fds = (sk_csa_act >= max_fds) ? sk_csa_act + 1 : max_fds;
			CSA_is_active = 1;
			continue;
		}

		if (CSA_is_active && FD_ISSET(sk_csa_act, &fds)) {
			recv_sk_csa(sk_csa_act);
			/* We don't want to hold up csa, so we give this 
			 * socket priority over the local userland socket. 
			 * We go back check for more on sk_nl and sk_csa 
			 * before looking at sk_unix. 
			 */
			continue;
		}
#endif

		if (FD_ISSET(sk_unix, &fds)) {
			/* Lowest priority.  We can let these callers block 
			 * if absolutely necessary.
			 */
			recv_sk_unix(sk_unix);
		}
	};

	return 0;
}

