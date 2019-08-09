/*
 * Copyright (c) 2007-2008 Silicon Graphics, Inc All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1140 East Arques Avenue,
 * Sunnyvale, CA  94085, or:
 *
 * http://www.sgi.com
 */

#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <asm/types.h>
#include <linux/taskstats.h>
#include "csa.h"
#include "csa_nl.h"
#include "csa_job.h"
#include "csa_ctl.h"
#include "job_csa.h"
#include "acctdef.h"

/*
 *  SLES11 and RHEL5 versions define _LINUX_TYPES_H which
 *  causes a compile error if netlink.h is included later.
 */
#include <sys/capability.h>

/* #define CSAD_DEBUG */
/* #define CSAD_DEBUG_STDERR */

#ifdef CSAD_DEBUG
#ifdef CSAD_DEBUG_STDERR
#define PRINTF(level, args...)	\
    do { fprintf(stderr, args); fprintf(stderr, "\n"); } while (0)
#else
#define PRINTF(level, args...) syslog(level, args)
#endif
#else
#define PRINTF(args...)
#endif

struct mesg_info {
    pid_t pid;
    cap_t cap_p;
    int sockfd;
    struct csa_ctl_hdr hdr;
    void *data;
};

static void recv_sk_nl(struct nl_handle *nlh);
static void send_proc_exit(struct taskstats *task);
static void send_sk_job(int sk, struct proc_exit *msg);
static void recv_sk_job(int sk);
static void process_mesg(struct mesg_info *info);
static void send_reply(struct mesg_info *info);
static void recv_sk_ctl(int sk);
static int grab_lockfile(void);
static int set_cpumask(void);
static int unset_cpumask(void);
static int init_csa(void);
static void cleanup_csa(void);
int run_csa_daemon(void *gj);
void end_csa_daemon(int unused);

#define CSAD_LOCKFILE "/var/run/csad/lock"
#define MASK_LEN 4096  /* size of resp_string in lib/config.c */

static int lockfd = -1;
static struct nl_handle *nlh = NULL;
static int sk_nl = -1, sk_job = -1, sk_ctl = -1;
static int job_is_active = 0;
static int done = -1;
static char cpumask[MASK_LEN] = "";

static void
recv_sk_nl(struct nl_handle *nlh)
{
    struct taskstats *task = NULL;
    csa_nl_stats(nlh, &task);
    if (task != NULL) {
	csa_acct_eop(task);
	send_proc_exit(task);
	free(task);
    }

    return;
}

static void
send_proc_exit(struct taskstats *task)
{
    struct proc_exit msg;
    msg.pid = task->ac_pid;
    msg.exitstat = task->ac_exitcode;
    send_sk_job(sk_job, &msg);

    return;
}

static void
send_sk_job(int sk, struct proc_exit *msg)
{
    int sz;

    sz = write(sk, msg, sizeof(struct proc_exit));
    if (sz != sizeof(struct proc_exit)) {
	if (sz < 0) {
	    PRINTF(LOG_ERR, "send_sk_job: write error: %m");
	    job_is_active = 0;	/* restart job daemon */
	}
	else
	    PRINTF(LOG_ERR, "send_sk_job: short write: %d bytes", sz);
    }
    PRINTF(LOG_INFO, "CSA telling JOB that pid %d exited", msg->pid);

    return;
}

static void
recv_sk_job(int sk)
{
    struct job_event msg;
    int sz;
    struct csa_job *job_acctbuf;

    sz = read(sk, &msg, sizeof(struct job_event));
    if (sz != sizeof(struct job_event)) {                        
	if (sz < 0)
	    PRINTF(LOG_ERR, "recv_sk_job: read error: %m");
	else {
	    PRINTF(LOG_ERR, "recv_sk_job: short read: %d bytes", sz);  
	    job_is_active = 0;	/* restart job daemon */
	}
	return;                                              
    }                                                            

    if (msg.type == JOB_EVENT_START) {
	job_acctbuf = csa_newjob(msg.jid, msg.user, msg.start);
	if (job_acctbuf == NULL)
	    return;
	csa_jstart(job_acctbuf);
    }
    else if (msg.type == JOB_EVENT_END) {
	job_acctbuf = csa_getjob(msg.jid);
	if (job_acctbuf == NULL)
	    return;
	csa_jexit(job_acctbuf);
	csa_freejob(msg.jid);
    }

    return;
}

static void
process_mesg(struct mesg_info *info)
{
    info->hdr.err =
	csa_doctl(info->pid, info->cap_p, info->hdr.req, info->data);

    send_reply(info);

    return;
}

static void
send_reply(struct mesg_info *info)
{
    int sz;

    sz = write(info->sockfd, &info->hdr, sizeof(struct csa_ctl_hdr));
    if ((sz < 0) && (errno != EPIPE))
	PRINTF(LOG_ERR, "send_reply: write error: %m");

    if ((sz > 0) && info->hdr.update) {
	sz = write(info->sockfd, info->data, info->hdr.size);
	if ((sz < 0) && (errno != EPIPE))
	    PRINTF(LOG_ERR, "send_reply: write error: %m");
    }

    close(info->sockfd);

    return;
}

static void
recv_sk_ctl(int sk)
{
    cap_t cap_p;
    struct ucred cr;
    unsigned int cl = sizeof(cr);
    int fromfd, sz;
    struct mesg_info info;
    union csa_ctl_data data;

    cap_p = cap_init();
    if (cap_p == NULL)
	PRINTF(LOG_ERR, "recv_sk_ctl: cap_init error: %m");

    fromfd = accept(sk, NULL, NULL);
    if (fromfd < 0) {
	PRINTF(LOG_ERR, "recv_sk_ctl: accept error: %m");
	cap_free(cap_p);
	return;
    }

    if (getsockopt(fromfd, SOL_SOCKET, SO_PEERCRED, &cr, &cl) == 0) {
	if (capgetp(cr.pid, cap_p)) {
	    PRINTF(LOG_ERR, "recv_sk_ctl: capabilities error: %m");
	    info.cap_p = NULL;
	}
	else
	    info.cap_p = cap_p;
    }
    else {
	PRINTF(LOG_ERR, "recv_sk_ctl: credentials error: %m");
	cr.pid = cr.uid = cr.gid = ~0;
	info.cap_p = NULL;
    }

    sz = read(fromfd, &info.hdr, sizeof(struct csa_ctl_hdr));
    if (sz != sizeof(struct csa_ctl_hdr)) {
	if (sz < 0)
	    PRINTF(LOG_ERR, "recv_sk_ctl: read error: %m");
	else
	    PRINTF(LOG_ERR, "recv_sk_ctl: short read: %d bytes", sz);
	close(fromfd);
	cap_free(cap_p);
	return;
    }

    if (info.hdr.size > 0) {
	sz = read(fromfd, (char *) &data, info.hdr.size);
	if (sz != info.hdr.size) {
	    if (sz < 0)
		PRINTF(LOG_ERR, "recv_sk_ctl: read error: %m");
	    else
		PRINTF(LOG_ERR, "recv_sk_ctl: short read: %d bytes", sz);
	    close(fromfd);
	    cap_free(cap_p);
	    return;
	}
    }

    info.pid = cr.pid;
    info.cap_p = cap_p;
    info.sockfd = fromfd;
    info.data = (info.hdr.size > 0) ? &data : NULL;

    process_mesg(&info);

    cap_free(cap_p);

    return;
}

/*
 *  Create a lockfile to ensure there is not another jobd already running.
 */
static int
grab_lockfile(void)
{
    int rc;
    mode_t mask;
    static int PID_LEN = 12;
    char pid[PID_LEN];

    mask = umask(077);
    lockfd = open(CSAD_LOCKFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    umask(mask);
    if (lockfd < 0) {
	PRINTF(LOG_ERR, "%s: %m", CSAD_LOCKFILE);
	return -1;
    }

    rc = flock(lockfd, LOCK_EX | LOCK_NB);
    if (rc < 0) {
	if (errno == EWOULDBLOCK)
	    PRINTF(LOG_NOTICE, "grab_lockfile: CSA daemon is already running.");
	else
	    PRINTF(LOG_ERR, "%s: %m", CSAD_LOCKFILE);
	return -1;
    }

    rc = ftruncate(lockfd, 0);
    if (rc < 0) {
	PRINTF(LOG_ERR, "%s: %m", CSAD_LOCKFILE);
	return -1;
    }

    rc = snprintf(pid, PID_LEN, "%10ld\n", getpid());
    if (rc >= PID_LEN) {
	PRINTF(LOG_ERR, "%s: %m", CSAD_LOCKFILE);
	return -1;
    }

    rc = write(lockfd, pid, rc);
    if (rc < 0) {
	PRINTF(LOG_ERR, "%s: %m", CSAD_LOCKFILE);
	return -1;
    }

    return 0;
}

/*
 *  Get cpumask from the csa.conf file.  If there is nothing there,
 *  we'll use our default, DEFCPUMASK.  If the mask is set to "all",
 *  we'll get the processor information from /proc/cpuinfo.
 */
static int
set_cpumask(void)
{
    FILE *conf;

    conf = popen("csagetconfig CPUMASK", "r");
    if (conf == NULL) {
	PRINTF(LOG_ERR, "set_cpumask: failed to run csagetconfig");
	return -1;
    }
    if ((fgets(cpumask, MASK_LEN, conf) == NULL) || (strlen(cpumask) == 0))
	strncpy(cpumask, DEFCPUMASK, MASK_LEN);
    pclose(conf);

    if (strcmp(cpumask, "all") == 0) {
	FILE *cpuinfo;
	char desc[10];
	int num, cpu[2], ncpu = 0 ;

	cpumask[0] = '\0';

	cpuinfo = fopen("/proc/cpuinfo", "r");
	if (cpuinfo == NULL) {
	    PRINTF(LOG_ERR, "set_cpumask: failed to open /proc/cpuinfo");
	    return -1;
	}

	/* Assume processors numbered sequentially; take first and last. */

	while (fscanf(cpuinfo, "%9s : %d", desc, &num) >= 0) {
	    if (strncmp(desc, "processor", 9) == 0)
		cpu[(ncpu++ > 0)] = num;
	}

	fclose(cpuinfo);

	switch (ncpu) {
	case 0:
	    PRINTF(LOG_ERR, "set_cpumask: no processors in /proc/cpuinfo");
	    return -1;
	case 1:
	    sprintf(cpumask, "%d", cpu[0]);
	    break;
	default:
	    sprintf(cpumask, "%d-%d", cpu[0], cpu[1]);
	}
    }

    return csa_nl_cpumask_on(nlh, cpumask);
}

static int
unset_cpumask(void)
{
    if (strlen(cpumask))
	return csa_nl_cpumask_off(nlh, cpumask);
    else
	return 0;
}

static int
init_csa(void)
{
    struct sigaction act;
    struct sockaddr_un sa_unix;
    int rc, on;

    if (grab_lockfile() < 0)
	return -1;

    /* Setup signal handlers. */

    act.sa_handler = end_csa_daemon;
    if (sigaction(SIGTERM, &act, NULL) < 0) {
	PRINTF(LOG_ERR, "init_csa: sigaction SIGTERM error");
	return -1;
    }

    act.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &act, NULL) < 0) {
	PRINTF(LOG_ERR, "init_csa: sigaction SIGPIPE error");
	return -1;
    }

    /* Setup hash table for job entries. */

    if (csa_newtable(MAX_JOB_ENTRIES) < 0) {
	PRINTF(LOG_ERR, "init_csa: CSA job table error");
	return -1;
    }

    /* Create socket to communicate with netlink/taskstats. */

    if ((nlh = csa_nl_create()) == NULL) {
	PRINTF(LOG_ERR, "init_csa: netlink socket error");
	return -1;
    }
    sk_nl = csa_nl_fd(nlh);

    /* Register taskstats CPU mask. */

    if (set_cpumask() < 0)
	return -1;

    /* Create socket to communicate with job daemon. */

    job_is_active = 0;

    bzero((char *) &sa_unix, sizeof(sa_unix));
    sa_unix.sun_family = AF_UNIX;
    strcpy(sa_unix.sun_path, JOB_CSA_SOCKET);

    sk_job = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sk_job == -1) {
	PRINTF(LOG_ERR, "init_csa: job socket error: %m");
	return -1;
    }

    rc = connect(sk_job, (struct sockaddr *) &sa_unix, sizeof(sa_unix));
    if (rc == -1) {
	PRINTF(LOG_ERR, "init_csa: job socket connect error: %m");
	return -1;
    }

    job_is_active = 1;

    /* Create socket to communicate with csaswitch and ja commands. */

    bzero((char *) &sa_unix, sizeof(sa_unix));
    sa_unix.sun_family = AF_UNIX;
    strcpy(sa_unix.sun_path, CSA_CTL_SOCKET);
    unlink(CSA_CTL_SOCKET);

    sk_ctl = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sk_ctl == -1) {
	PRINTF(LOG_ERR, "init_csa: ctl socket error: %m");
	return -1;
    }

    on = 1;
    rc = setsockopt(sk_ctl, SOL_SOCKET, SO_PASSCRED, &on, sizeof(int));
    if (rc < 0)
	PRINTF(LOG_ERR, "init_csa: ctl setsockopt error: %m");

    rc = bind(sk_ctl, (struct sockaddr *) &sa_unix, sizeof(sa_unix));
    if (rc == -1) {
	PRINTF(LOG_ERR, "init_csa: ctl bind error: %m");
	return -1;
    }

    rc = listen(sk_ctl, 5);
    if (rc < 0) {
	PRINTF(LOG_ERR, "init_csa: ctl listen error: %m");
	return -1;
    }

    return 0;
}

static void
cleanup_csa(void)
{
    if (lockfd != -1) {
	close(lockfd);
	unlink(CSAD_LOCKFILE);
	lockfd = -1;
    }

    csa_freetable();

    unset_cpumask();

    if (nlh) {
	csa_nl_cleanup(nlh);
	nlh = NULL;
	sk_nl = -1;
    }

    if (sk_job != -1) {
	close(sk_job);
	sk_job = -1;
    }

    if (sk_ctl != -1) {
	close(sk_ctl);
	unlink(CSA_CTL_SOCKET);
	sk_ctl = -1;
    }

    return;
}

int
run_csa_daemon(void *gj)
{
    extern jid_t (*getjid)(pid_t);
    fd_set fds;
    int rc, max_fds;
    struct timeval timeout;

    if (gj == NULL) {
	PRINTF(LOG_ERR, "run_csa_daemon: NULL function pointer for getjid");
	return -1;
    }
    *((void **) &getjid) = gj;

    if (init_csa() < 0) {
	cleanup_csa();
	return -1;
    }

    max_fds = (sk_nl > sk_job) ? sk_nl + 1 : sk_job + 1;
    max_fds = (sk_ctl >= max_fds) ? sk_ctl + 1 : max_fds;

    done = 0;

    while (!done) {
	FD_ZERO(&fds);
	FD_SET(sk_nl, &fds);
	if (job_is_active)
	    FD_SET(sk_job, &fds);
	FD_SET(sk_ctl, &fds);

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	rc = select(max_fds, &fds, NULL, NULL, &timeout);
	if (rc < 0) {
	    PRINTF(LOG_ERR, "main: select error: %m");
	    continue;
	}

	if (FD_ISSET(sk_nl, &fds)) {
	    recv_sk_nl(nlh);
	    /* sk_nl gets priority over others. */
	    continue;
	}

	if (job_is_active && FD_ISSET(sk_job, &fds)) {
	    recv_sk_job(sk_job);
	    /* sk_job gets priority over sk_ctl. */
	    continue;
	}

	if (FD_ISSET(sk_ctl, &fds))
	    recv_sk_ctl(sk_ctl);
    }

    cleanup_csa();

    return 0;
}

void
end_csa_daemon(int unused)
{
    done = 1;

    return;
}
