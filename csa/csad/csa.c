/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2004-2008 Silicon Graphics, Inc All Rights Reserved.
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

/*
 *  Description:
 *	This file, csa.c, contains the procedures that handle CSA
 *	job accounting. It configures CSA, writes CSA accounting
 *	records, and processes the acctctl requests.  This code is
 *	compiled as part of the CSA daemon program.
 *
 *  Author:
 *	Marlys Kohnke (kohnke@sgi.com)
 *
 *  Contributors:
 *
 *  Changes:
 *	January 31, 2001  (kohnke)  Changed to use semaphores rather than
 *	spinlocks.  Was seeing a spinlock deadlock sometimes when an accounting
 *	record was being written to disk with 2.4.0 (didn't happen with
 *	2.4.0-test7).
 *
 *	February 2, 2001  (kohnke)  Changed to handle being compiled directly
 *	into the kernel, not just compiled as a loadable module. Renamed
 *	init_module() as init_csa() and cleanup_module() as cleanup_csa().
 *	Added calls to module_init() and module_exit().
 *
 *	January 21, 2003 (jlan)  Changed to provide /proc ioctl interface.
 *	Also, provided MODULE_* clause.
 *
 *	Thu Mar 09 2006 Jay Lan <jlan@sgi.com>
 *	- cleanup code
 *	- break csa_kern.h into csa.h for userland and csa_kern.h for kernel
 *	- remove code for CSA record types that not supported in Linux
 *	- fix bugs
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/resource.h>
/*
 *  sys/capability.h in libcap-devel-1.10-26 (RHEL5.1)
 *  prevents linux/types.h from being included.
 *
 */
#include <linux/types.h>
#include <sys/capability.h>
#include <asm/fcntl.h>
#include <linux/acct.h>
#include <linux/taskstats.h>
#include "csa.h"
#include "csa_job.h"
#include <job.h>

// XXX TEMPORARY
#define up(x)
#define down(x)

/*
 *  getjid is the function by which CSA obtains a jid from job given a pid.
 *  It must be assigned via run_csa_daemon: to jid_lookup if the CSA daemon
 *  is spawned from the job daemon, or to job_getjid if the CSA daemon runs
 *  as a standalone process.
 *
 *  jid_lookup is defined in jobd_hash.c; job_getjid is a job API function.
 *
 */
jid_t (*getjid)(pid_t) = NULL;

static int	csa_modify_buf(char *, struct acctcsa *, struct acctmem *,
			       struct acctio *, struct acctdelay *,
			       int, int, int);
static int	csa_write(char *, int, int, uint64_t, int, struct csa_job *);
static void	csa_config_make(ac_eventtype, struct acctcfg *);
static int	csa_config_write(ac_eventtype, int);
static void	csa_header(struct achead *, int, int, int);

#define JID_ERR1 "csa_acct_eop: No job table entry for jid 0x%llx.\n"
#define JID_ERR2 "csa user job accounting write error %d, jid 0x%llx\n"
#define JID_ERR3 "Can't disable csa user job accounting jid 0x%llx\n"
#define JID_ERR4 "csa user job accounting disabled, jid 0x%llx\n"

/* #define DEBUG 1 */
/* #define CSA_DEBUG 1 */

#ifdef CSA_DEBUG
#ifdef CSA_DEBUG_STDERR
#define PRINTF(level, args...) \
	do { fprintf(stderr, args); fprintf(stderr, "\n"); } while (0)
#else
#define PRINTF(level, args...) syslog(level, args)
#endif
#else
#define PRINTF(args...)
#endif /* CSA_DEBUG */

#define USEC_PER_SEC	1000000L
#define USEC_PER_TICK	(USEC_PER_SEC/HZ)

static int csa_acctvp = NULLFD;
static time_t boottime;

static int csa_flag;			/* accounting start state flag */
static char csa_path[ACCT_PATH] = "";	/* current accounting file path name */
static char new_path[ACCT_PATH] = "";	/* new accounting file path name */

/* modify this when changes are made to ac_kdrcd in csa.h */
static char *acct_dmd_name[ACCT_MAXKDS] = {
    "CSA",
    "JOB",
    "WORKLOAD MGMT",
    "SITE1",
    "SITE2"
};

typedef enum {
    A_SYS, /* system accounting action     (0) */
    A_CJA, /* Job accounting action        (1) */
    A_DMD, /* daemon accounting action     (2) */
    A_MAX
} a_fnc;

static struct actstat acct_dmd[ACCT_MAXKDS][A_MAX];
static struct actstat acct_rcd[ACCT_MAXRCDS-ACCT_RCDS][A_MAX];

/*  Initialize the CSA accounting state information. */
#define INIT_DMD(t, i, s, p)			\
    acct_dmd[i][t].ac_ind = i;			\
    acct_dmd[i][t].ac_state = s;		\
    acct_dmd[i][t].ac_param = p;
#define INIT_RCD(t, i, s, p)			\
    acct_rcd[i-ACCT_RCDS][t].ac_ind = i;	\
    acct_rcd[i-ACCT_RCDS][t].ac_state = s;	\
    acct_rcd[i-ACCT_RCDS][t].ac_param = p;


/*
 *  Initialize the CSA accounting state table.
 *  Modify this when changes are made to ac_kdrcd in csa.h
 *
 */
static void
csa_init_acct(int flag)
{
    struct timespec xtime, now;
    csa_flag = flag;

    clock_gettime(CLOCK_REALTIME, &xtime);
    clock_gettime(CLOCK_MONOTONIC, &now);
    boottime = xtime.tv_sec - now.tv_sec;
    PRINTF(LOG_INFO, "csa: boottime= %llx\n", boottime);
 
    /* Initialize system accounting states. */
    INIT_DMD(A_SYS, ACCT_KERN_CSA,          ACS_OFF, 0);
    INIT_DMD(A_SYS, ACCT_KERN_JOB_PROC,     ACS_OFF, 0);
    INIT_DMD(A_SYS, ACCT_DMD_WKMG,          ACS_OFF, 0);
    INIT_DMD(A_SYS, ACCT_DMD_SITE1,         ACS_OFF, 0);
    INIT_DMD(A_SYS, ACCT_DMD_SITE2,         ACS_OFF, 0); 
 
    INIT_RCD(A_SYS, ACCT_RCD_MEM,           ACS_OFF, 0);
    INIT_RCD(A_SYS, ACCT_RCD_IO,            ACS_OFF, 0);
    INIT_RCD(A_SYS, ACCT_RCD_DELAY,         ACS_OFF, 0);
    INIT_RCD(A_SYS, ACCT_THD_MEM,           ACS_OFF, 0);
    INIT_RCD(A_SYS, ACCT_THD_TIME,          ACS_OFF, 0);
    INIT_RCD(A_SYS, ACCT_RCD_SITE1,         ACS_OFF, 0);
    INIT_RCD(A_SYS, ACCT_RCD_SITE2,         ACS_OFF, 0);  

    return;
}

/* Initialize CSA accounting header. */
static void
csa_header(struct achead *head, int revision, int type, int size)
{
    head->ah_magic = ACCT_MAGIC;
    head->ah_revision = revision;
    head->ah_type = type;
    head->ah_flag = 0;
    head->ah_size = size;

    return;
}

/*
 *  Create a CSA end-of-process accounting record and write it to appropriate
 *  file(s).
 */
void
csa_acct_eop(struct taskstats *p)
{
    struct csa_job *job_acctbuf;
    /* 2* 336 bytes on the stack ;-( */
    char acctent[sizeof(struct acctcsa) +
		 sizeof(struct acctmem) +
		 sizeof(struct acctio)  +
		 sizeof(struct acctdelay)];
    char modacctent[sizeof(struct acctcsa) +
		    sizeof(struct acctmem) +
		    sizeof(struct acctio)  +
		    sizeof(struct acctdelay)];
    struct acctcsa *csa;
    struct acctmem *mem = NULL;
    struct acctio *io = NULL;
    struct acctdelay *delay = NULL;
    struct achead *hdr1, *hdr2;
    char *cb = acctent;
    uint64_t jid;
    int size;
    int	len = 0;
    int	csa_enabled = 0;
    int	ja_enabled = 0;
    int	io_enabled = 0;
    int	mem_enabled = 0;
    int delay_enabled = 0;
    uint64_t memtime;
    __u64 elapsed;

    if (p == NULL) {
	syslog(LOG_ERR, "csa_acct_eop: CSA null task pointer\n");
	return;
    }
    jid = (*getjid)(p->ac_pid);
    if (jid <= 0) {
	/* no job table entry; not all processes are part of a job */
	return;
    }
    job_acctbuf = csa_getjob(jid);
    if (job_acctbuf == NULL) {
	/* couldn't get accounting info stored in the job table entry */
	syslog(LOG_WARNING, JID_ERR1, (long long) jid);
	return;
    }

    down(&csa_sem);
    /*
     *  figure out what's turned on, which determines which record types
     *  need to be written.  All records are written to a user job
     *  accounting file.  Only those record types configured on are
     *  written to the system pacct file
     */
    if (job_acctbuf->job_acctfile != NULLFD)
	ja_enabled = 1;
    if (acct_dmd[ACCT_KERN_CSA][A_SYS].ac_state == ACS_ON)
	csa_enabled = 1;
    if (acct_rcd[ACCT_RCD_IO-ACCT_RCDS][A_SYS].ac_state == ACS_ON)
	io_enabled = 1;
    if (acct_rcd[ACCT_RCD_MEM-ACCT_RCDS][A_SYS].ac_state == ACS_ON)
	mem_enabled = 1;
    if (acct_rcd[ACCT_RCD_DELAY-ACCT_RCDS][A_SYS].ac_state == ACS_ON)
	delay_enabled = 1;

    if (!ja_enabled && !csa_enabled) {
	/* nothing to do */
	up(&csa_sem);
	return;
    }
    up(&csa_sem);

    csa = (struct acctcsa *) acctent;
    size = sizeof(struct acctcsa);
    memset(csa, 0, size);
    hdr1 = &csa->ac_hdr1;
    csa_header(hdr1, REV_CSA, ACCT_KERNEL_CSA, size);
    hdr2 = &csa->ac_hdr2;
    csa_header(hdr2, REV_CSA, ACCT_KERNEL_CSA, 0);
    hdr2->ah_magic = ~ACCT_MAGIC;

    csa->ac_stat = p->ac_exitcode >> 8;
    csa->ac_uid  = p->ac_uid;
    csa->ac_gid  = p->ac_gid;

    /* XXX change this when array session handle info available */
    csa->ac_ash  = 0;
    csa->ac_jid  = job_acctbuf->job_id;
    /* XXX change this when project ids are available */
    csa->ac_prid = 0;
    csa->ac_nice = p->ac_nice;
    csa->ac_sched = p->ac_sched;

    csa->ac_pid  = p->ac_pid;
    csa->ac_ppid = p->ac_ppid;
    if (p->ac_flag & AFORK)
	csa->ac_hdr1.ah_flag |= AFORK;
    if (p->ac_flag & ASU)
	csa->ac_hdr1.ah_flag |= ASU;
    if (p->ac_flag & ACORE)
	csa->ac_hdr1.ah_flag |= ACORE;
    if (p->ac_flag & AXSIG)
	csa->ac_hdr1.ah_flag |= AXSIG;

    strncpy(csa->ac_comm, p->ac_comm, sizeof(csa->ac_comm));
    csa->ac_btime = p->ac_btime;
    csa->ac_etime = p->ac_etime;

    cb += size;
    len += size;

    csa->ac_utime = p->ac_utime;
    csa->ac_stime = p->ac_stime;
    /* Each process gets a minimum of a half tick cpu time */
    if ((csa->ac_utime == 0) && (csa->ac_stime == 0))
	csa->ac_stime = USEC_PER_TICK/2;

    /* Create the memory record if needed */
    if (ja_enabled || mem_enabled) {
#if defined(CONFIG_BSD_PROCESS_ACCT)
	PRINTF(LOG_INFO, "csa_acct_eop: utime=%d ticks, stime=%d "
	       "ticks, acct_rss_mem1=%d, acct_vm_mem1=%d\n",
	       p->utime, p->stime, p->acct_rss_mem1, p->acct_vm_mem1);
#else
	PRINTF(LOG_INFO, "csa_acct_eop: utime=%d ticks, stime=%d "
 	       "ticks\n", p->ac_utime, p->ac_stime);
#endif
	mem = (struct acctmem *) cb;
	size = sizeof(struct acctmem);
	memset(mem, 0, size);
	hdr1->ah_flag |= AMORE;
	hdr2->ah_type |= ACCT_MEM;
	hdr1 = &mem->ac_hdr;
	csa_header(hdr1, REV_MEM, ACCT_KERNEL_MEM, size);

	mem->ac_core.mem1 = p->coremem;
	mem->ac_virt.mem1 = p->virtmem;
	PRINTF(LOG_INFO, "\tac_core.mem1=%d, ac_virt.mem1=%d\n",
	       mem->ac_core.mem1, mem->ac_virt.mem1);

	/* adjust page size to 1K units */
	if (1) {
	    PRINTF(LOG_INFO, "csa_acct_eop: hiwater_rss=%x, "
		   "hiwater_vm=%x\n", p->hiwater_rss, p->hiwater_vm);
	    mem->ac_virt.himem = p->hiwater_vm;
	    mem->ac_core.himem = p->hiwater_rss;
	    /*
	     *  For processes with zero systime, set the integral
	     *  to the highwater mark rather than leave at zero
	     */
	    if (mem->ac_core.mem1 == 0) {
		mem->ac_core.mem1 = mem->ac_core.himem / 1024;
		PRINTF(LOG_INFO, "csa_acct_eop: adjusted "
		       "ac_core.mem1=%x\n", mem->ac_core.mem1);
	    }
	    if (mem->ac_virt.mem1 == 0) {
		mem->ac_virt.mem1 = mem->ac_virt.himem / 1024;
		PRINTF(LOG_INFO, "csa_acct_eop: adjusted "
		       "ac_virt.mem1=%x\n", mem->ac_virt.mem1);
	    }
	    if (job_acctbuf->job_corehimem < mem->ac_core.himem)
		job_acctbuf->job_corehimem = mem->ac_core.himem;
	    if (job_acctbuf->job_virthimem < mem->ac_virt.himem)
	 	job_acctbuf->job_virthimem = mem->ac_virt.himem;
	}

	mem->ac_minflt = p->ac_minflt;
	mem->ac_majflt = p->ac_majflt;

	cb += size;
	hdr2->ah_size += size;
	len += size;
    }
    /* Create the I/O record */
    if (ja_enabled || io_enabled) {
	io = (struct acctio *) cb;
	size = sizeof(struct acctio);
	memset(io, 0, size);
	hdr1->ah_flag |= AMORE;
	hdr2->ah_type |= ACCT_IO;
	hdr1 = &io->ac_hdr;
	csa_header(hdr1, REV_IO, ACCT_KERNEL_IO, size);

	io->ac_chr = p->read_char;
	io->ac_chw = p->write_char;
	io->ac_scr = p->read_syscalls;
	io->ac_scw = p->write_syscalls;

	cb += size;
	hdr2->ah_size += size;
	len += size;
    }
    /* Create the delay record */
    if (ja_enabled || delay_enabled) {
	delay = (struct acctdelay *) cb;
	size = sizeof(struct acctdelay);
	memset(delay, 0, size);
	hdr1->ah_flag |= AMORE;
	hdr2->ah_type |= ACCT_DELAY;
	hdr1 = &delay->ac_hdr;
	csa_header(hdr1, REV_DELAY, ACCT_KERNEL_DELAY, size);

	delay->ac_cpu_count = p->cpu_count;
	delay->ac_cpu_delay_total = p->cpu_delay_total;
	delay->ac_blkio_count = p->blkio_count;
	delay->ac_blkio_delay_total = p->blkio_delay_total;
	delay->ac_swapin_count = p->swapin_count;
	delay->ac_swapin_delay_total = p->swapin_delay_total;
	delay->ac_cpu_run_real_total = p->cpu_run_real_total;
	delay->ac_cpu_run_virtual_total = p->cpu_run_virtual_total;

	cb += size;
	hdr2->ah_size += size;
	len += size;
    }

    /* record always written to a user job accounting file */
    if ((len > 0) && (job_acctbuf->job_acctfile != NULLFD))
	csa_write((char *) &acctent, ACCT_KERN_CSA,
		  len, jid, A_CJA, job_acctbuf);
    /*
     *  check the cpu time and virtual memory thresholds before writing
     *  this record to the system pacct file
     */
    if ((acct_rcd[ACCT_THD_MEM-ACCT_RCDS][A_SYS].ac_state == ACS_ON) &&
	(ja_enabled || mem_enabled))
	if (mem->ac_virt.himem <
            acct_rcd[ACCT_THD_MEM-ACCT_RCDS][A_SYS].ac_param)
	    /* don't write record to pacct */
	    return;
    if (acct_rcd[ACCT_THD_TIME-ACCT_RCDS][A_SYS].ac_state == ACS_ON)
	if ((csa->ac_utime + csa->ac_stime) <
	    acct_rcd[ACCT_THD_TIME-ACCT_RCDS][A_SYS].ac_param)
	    /* don't write record to pacct */
	    return;

    if ((len > 0) && (csa_acctvp != NULLFD) && csa_enabled) {
	if (io_enabled && mem_enabled && delay_enabled)
	    /* write out buffer as is to system pacct file */
	    csa_write((char *) &acctent, ACCT_KERN_CSA,
		      len, jid, A_SYS, job_acctbuf);
	else {
	    /* only write out record types turned on */
	    len = csa_modify_buf(modacctent, csa, mem, io, delay,
			         io_enabled, mem_enabled, delay_enabled);
	    csa_write((char *) &modacctent, ACCT_KERN_CSA,
		      len, jid, A_SYS, job_acctbuf);
	}
    }

    return;
}

/*
 *  Copy needed accounting records into buffer, skipping record types which are
 *  not enabled.  May need to adjust downward the second header size if not
 *  both memory and io continuation records are written, plus adjust the second 
 *  header types and first header flags.
 */
static int
csa_modify_buf(char *modacctent, struct acctcsa *csa, struct acctmem *mem,
	       struct acctio *io, struct acctdelay *delay,
	       int io_enabled, int mem_enabled, int delay_enabled)
{
    int size;
    int len = 0;
    char *bufptr;
    struct achead *hdr1, *hdr2;

    size = sizeof(struct acctcsa) + sizeof(struct acctmem) +
	   sizeof(struct acctio) + sizeof(struct acctdelay);
    memset(modacctent, 0, size);
    bufptr = modacctent;
    /*
     *  adjust values that might not be correct anymore if all of
     *  the continuation records aren't written out to the pacct file
     */
    hdr1 = &csa->ac_hdr1;
    hdr2 = &csa->ac_hdr2;
    hdr1->ah_flag &= ~AMORE;
    hdr2->ah_type = ACCT_KERNEL_CSA;
    hdr2->ah_size = 0;
    if (mem_enabled) {
	hdr1->ah_flag |= AMORE;
	hdr2->ah_type |= ACCT_MEM;
	hdr2->ah_size += sizeof(struct acctmem);
	hdr1 = &mem->ac_hdr;
	hdr1->ah_flag &= ~AMORE;
    }
    if (io_enabled) {
	hdr1->ah_flag |= AMORE;
	hdr2->ah_type |= ACCT_IO;
	hdr2->ah_size += sizeof(struct acctio);
	hdr1 = &io->ac_hdr;
	hdr1->ah_flag &= ~AMORE;
    }
    if (delay_enabled) {
	hdr1->ah_flag |= AMORE;
	hdr2->ah_type |= ACCT_DELAY;
	hdr2->ah_size += sizeof(struct acctdelay);
	hdr1 = &delay->ac_hdr;
	hdr1->ah_flag &= ~AMORE;
    }
    size = sizeof(struct acctcsa);
    memcpy(bufptr, csa, size);
    bufptr += size;
    len += size;

    if (mem_enabled) {
	size = sizeof(struct acctmem);
	memcpy(bufptr, mem, size);
	bufptr += size;
	len += size;
    }
    if (io_enabled) {
	size = sizeof(struct acctio);
	memcpy(bufptr, io, size);
	bufptr += size;
	len += size;
    }
    if (delay_enabled) {
	size = sizeof(struct acctdelay);
	memcpy(bufptr, delay, size);
	len += size;
    }

    return len;
}

static int
capable(cap_t cap_p, cap_value_t cap)
{
    cap_flag_value_t value_p;

    if (cap_p == NULL)
	return 0;

    cap_get_flag(cap_p, cap, CAP_EFFECTIVE, &value_p);
    return ((value_p == CAP_SET) ? 1 : 0);
}

/*
 *  csa_doctl
 *
 */
int
csa_doctl(pid_t pid, cap_t cap_p, unsigned int req, unsigned long data)
{
    struct actctl actctl;
    struct actstat actstat;

    int	daemon = 0;
    int	error = 0;
    static int flag = 010000;
    int	ind;
    int	id;
    int	len;
    int	num;

    PRINTF(LOG_INFO, "CSA: csa_doctl, req=%d\n", req);
    down(&csa_sem);
    if (!csa_flag)
	csa_init_acct(flag++);
    up(&csa_sem);

    num = (req & 0x0ff);
    if ((num < AC_START) || (num >= AC_MREQ))
	return EINVAL;

    memset(&actctl, 0, sizeof(struct actctl));
    memset(&actstat, 0, sizeof(struct actstat));

    switch (req) {
    /*
     *  Start specified types of accounting.
     */
    case AC_START:
    {
	int id, ind;
	int newvp;
	struct stat sbuf;

	PRINTF(LOG_INFO, "CSA: AC_START\n");
	if (!capable(cap_p, CAP_SYS_PACCT)) {
	    error = EPERM;
	    break;
	}

	memcpy(&actctl, (void *) data, sizeof(int));

	num = (actctl.ac_sttnum == 0) ? 1 : actctl.ac_sttnum;
	if ((num < 0) || (num > NUM_KDRCDS)) {
	    error = EINVAL;
	    break;
	}

	len = sizeof(struct actctl) - sizeof(struct actstat) * NUM_KDRCDS +
	      sizeof(struct actstat) * num;
	memcpy(&actctl, (void *) data, len);

	/*
	 *  Verify all indexes in actstat structures specified.
 	 */
	for (ind = 0; ind < num; ind++) {
	    id = actctl.ac_stat[ind].ac_ind;
	    if ((id < 0) || (id >= ACCT_MAXRCDS)) {
		error = EINVAL;
		break;
	    }

	    if (id == ACCT_MAXKDS) {
		error = EINVAL;
		break;
	    }
	}

	down(&csa_sem);

	/*
	 *  If an accounting file was specified, make sure
	 *  that we can access it.
	 */
	if (strlen(actctl.ac_path)) {
	    strncpy(new_path, actctl.ac_path, ACCT_PATH);
#if BITS_PER_LONG != 32
	    newvp = open(new_path, O_WRONLY | O_APPEND | O_LARGEFILE, 0);
#else
	    newvp = open(new_path, O_WRONLY | O_APPEND, 0);
#endif
	    fstat(newvp, &sbuf);

	    if (!S_ISREG(sbuf.st_mode)) {
		error = EACCES;
		close(newvp);
		up(&csa_sem);
		break;
	    }
	    if ((csa_acctvp != NULLFD) && csa_acctvp == newvp) {
		/*
		 *  This file already being used, so ignore
		 *  request to use this file; just continue on.
		 */
		close(newvp);
		newvp = NULLFD;
	    }
	}
	else
	    newvp = NULLFD;

	/*
	 *  If a new accounting file was specified and there's
	 *  an old accounting file, stop writing to it.
	 */
	if (newvp != NULLFD) {
	    if (csa_acctvp != NULLFD) {
	        error = csa_config_write(AC_CONFCHG_FILE, NULLFD);
	        close(csa_acctvp);
	    }
	    else if (!csa_flag)
		csa_init_acct(flag++);

	    strncpy(csa_path, new_path, ACCT_PATH);
	    down(&csa_write_sem);
	    csa_acctvp = newvp;
	    up(&csa_write_sem);
	}
	else if (csa_acctvp == NULLFD) {
	    error = EINVAL;
	    up(&csa_sem);
	    break;
	}

	/*
	 *  Loop through each actstat block and turn ON that accounting.
	 */
	for (ind = 0; ind < num; ind++) {
	    struct actstat *stat;

	    id = actctl.ac_stat[ind].ac_ind;
	    stat = &actctl.ac_stat[ind];
	    if (id < ACCT_RCDS)  {
	        acct_dmd[id][A_SYS].ac_state = ACS_ON;
		acct_dmd[id][A_SYS].ac_param = stat->ac_param;

		stat->ac_state = acct_dmd[id][A_SYS].ac_state;
		stat->ac_param = acct_dmd[id][A_SYS].ac_param;
	    }
	    else {
		int tid = id - ACCT_RCDS;

		acct_rcd[tid][A_SYS].ac_state = ACS_ON;
		acct_rcd[tid][A_SYS].ac_param = stat->ac_param;

		stat->ac_state = acct_rcd[tid][A_SYS].ac_state;
		stat->ac_param = acct_rcd[tid][A_SYS].ac_param;
	    }
	}

	up(&csa_sem);

	error = csa_config_write(AC_CONFCHG_ON, NULLFD);

	/*
	 *  Return the accounting states to the user.
	 */
	memcpy((void *) data, &actctl, len);
	break;
    }

    /*
     *  Stop specified types of accounting.
     */
    case AC_STOP:
    {
	int id, ind;

	PRINTF(LOG_INFO, "CSA: AC_STOP\n");
	if (!capable(cap_p, CAP_SYS_PACCT)) {
	    error = EPERM;
	    break;
	}

	memcpy(&actctl, (void *) data, sizeof(int));

	num = (actctl.ac_sttnum == 0) ? 1 : actctl.ac_sttnum;
	if ((num <= 0) || (num > NUM_KDRCDS)) {
	    error = EINVAL;
	    break;
	}

	len = sizeof(struct actctl) - sizeof(struct actstat) * NUM_KDRCDS +
	      sizeof(struct actstat) * num;
	memcpy(&actctl, (void *) data, len);

	/*
	 *  Verify all of the indexes in actstat structures specified.
	 */
	for (ind = 0; ind < num; ind++) {
	    id = actctl.ac_stat[ind].ac_ind;
	    if ((id < 0) || (id >= NUM_KDRCDS)) {
		error = EINVAL;
		break;
	    }
	}

	/*
	 *  Loop through each actstat block and turn off that accounting.
	 */

	down(&csa_sem);

	/*
	 *  Disable accounting for this entry.
	 */
	for (ind = 0; ind < num; ind++) {
	    id = actctl.ac_stat[ind].ac_ind;
	    if (id < ACCT_RCDS) {
		acct_dmd[id][A_SYS].ac_state = ACS_OFF;
		acct_dmd[id][A_SYS].ac_param = 0;

		actctl.ac_stat[ind].ac_state = acct_dmd[id][A_SYS].ac_state;
		actctl.ac_stat[ind].ac_param = 0;
	    }
	    else {
		int tid = id - ACCT_RCDS;

		acct_rcd[tid][A_SYS].ac_state = ACS_OFF;
		acct_rcd[tid][A_SYS].ac_param = 0;
		actctl.ac_stat[ind].ac_state = acct_rcd[tid][A_SYS].ac_state;
		actctl.ac_stat[ind].ac_param = acct_rcd[tid][A_SYS].ac_param;
	    }
	}

	/*
	 *  Check the daemons to see if any are still on.
	 */
	for (ind = 0; ind < ACCT_MAXKDS; ind++)
	    if (acct_dmd[ind][A_SYS].ac_state == ACS_ON)
		daemon += 1 << ind;

	up(&csa_sem);

	/*
	 *  If all daemons are off and there's an old accounting file,
	 *  stop writing to it.
	 */
	if (!daemon && (csa_acctvp != NULLFD)) {
	    error = csa_config_write(AC_CONFCHG_OFF, NULLFD);
	    close(csa_acctvp);
	    down(&csa_write_sem);
	    csa_acctvp = NULLFD;
	    up(&csa_write_sem);
	}
	else
	    error = csa_config_write(AC_CONFCHG_OFF, NULLFD);

	/*
	 *  Return the accounting states to the user.
 	 */
	memcpy((void *) data, &actctl, len);
	break;
    }

    /*
     *  Halt all accounting.
     */
    case AC_HALT:
    {
	int ind;

	PRINTF(LOG_INFO, "CSA: AC_HALT\n");
	if (!capable(cap_p, CAP_SYS_PACCT)) {
	    error = EPERM;
	    break;
	}

	down(&csa_sem);

	/* Turn off all accounting if any is on. */
	for (ind = 0; ind <ACCT_MAXKDS; ind++) {
	    acct_dmd[ind][A_SYS].ac_state = ACS_OFF;
	    acct_dmd[ind][A_SYS].ac_param = 0;
	}

	for (ind = ACCT_RCDS; ind < ACCT_MAXRCDS; ind++) {
	    int tid = ind - ACCT_RCDS;

	    acct_rcd[tid][A_SYS].ac_state = ACS_OFF;
	    acct_rcd[tid][A_SYS].ac_param = 0;
	}

	up(&csa_sem);

	/* If there's an old accounting file, stop writing to it. */
	if (csa_acctvp != NULLFD) {
	    error = csa_config_write(AC_CONFCHG_OFF, NULLFD);
	    close(csa_acctvp);
	    down(&csa_write_sem);
	    csa_acctvp = NULLFD;
	    up(&csa_write_sem);
	}
	break;
    }

    /*
     * Process daemon/record status function.
     */
    case AC_CHECK:
	PRINTF(LOG_INFO, "CSA: AC_CHECK\n");
	memcpy(&actstat, (void *) data, sizeof(struct actstat));
	id = actstat.ac_ind;
	if ((id >= 0) && (id < ACCT_MAXKDS)) {
	    actstat.ac_state = acct_dmd[id][A_SYS].ac_state;
	    actstat.ac_param = acct_dmd[id][A_SYS].ac_param;
	}
	else if ((id >= ACCT_RCDS) && (id < ACCT_MAXRCDS)) {
	    int	tid = id-ACCT_RCDS;

	    actstat.ac_state = acct_rcd[tid][A_SYS].ac_state;
	    actstat.ac_param = acct_rcd[tid][A_SYS].ac_param;
	}
	else {
	    error = EINVAL;
	    break;
	}
	memcpy((void *) data, &actstat, sizeof(struct actstat));
	break;

    /*
     *  Process daemon status function.
     */
    case AC_KDSTAT:
	PRINTF(LOG_INFO, "CSA: AC_KDSTAT\n");
	memcpy(&actctl, (void *) data, sizeof(int));
	num = actctl.ac_sttnum;

	if (num <= 0) {
	    error = EINVAL;
	    break;
	} else if (num > NUM_KDS)
	    num = NUM_KDS;

	for (ind = 0; ind < num; ind++) {
	    actctl.ac_stat[ind].ac_ind   = acct_dmd[ind][A_SYS].ac_ind;
	    actctl.ac_stat[ind].ac_state = acct_dmd[ind][A_SYS].ac_state;
	    actctl.ac_stat[ind].ac_param = acct_dmd[ind][A_SYS].ac_param;
	}
	actctl.ac_sttnum = num;
	strncpy(actctl.ac_path, csa_path, ACCT_PATH);

	len = sizeof(struct actctl) - sizeof(struct actstat) * NUM_KDRCDS +
	      sizeof(struct actstat) * num;
	memcpy((void *) data, &actctl, len);
	break;

    /*
     *  Process record status function.
     */
    case AC_RCDSTAT:
	PRINTF(LOG_INFO, "CSA: AC_RCDSTAT\n");
	memcpy(&actctl, (void *) data, sizeof(int));
	num = actctl.ac_sttnum;

	if (num <= 0) {
	    error = EINVAL;
	    break;
	} else if (num > NUM_RCDS)
	    num = NUM_RCDS;

	for (ind = 0; ind < num; ind++) {
	    actctl.ac_stat[ind].ac_ind = acct_rcd[ind][A_SYS].ac_ind;
	    actctl.ac_stat[ind].ac_state = acct_rcd[ind][A_SYS].ac_state;
	    actctl.ac_stat[ind].ac_param = acct_rcd[ind][A_SYS].ac_param;
	}
	actctl.ac_sttnum = num;
	strncpy(actctl.ac_path, csa_path, ACCT_PATH);

	len = sizeof(struct actctl) - sizeof(struct actstat) * NUM_KDRCDS +
	      sizeof(struct actstat) * num;
	memcpy((void *) data, &actctl, len);
	break;

    /*
     *  Turn user job accounting ON or OFF.
     */
    case AC_JASTART:
    case AC_JASTOP:
    {
	char localpath[ACCT_PATH];
	int newvp = NULLFD;
	int oldvp;
	struct stat sbuf;
	uint64_t jid;
	struct csa_job *job_acctbuf;

	if (req == AC_JASTART)
	    PRINTF(LOG_INFO, "CSA: AC_JASTART\n");
	else
	    PRINTF(LOG_INFO, "CSA: AC_JASTOP\n");

	len = sizeof(struct actctl) - sizeof(struct actstat) * (NUM_KDRCDS -1);
	memcpy(&actctl, (void *) data, len);

	/*
	 *  If an accounting file was specified, make sure
	 *  that we can access it.
	 */
	if (strlen(actctl.ac_path)) {
	    strncpy(localpath, actctl.ac_path, ACCT_PATH);
#if BITS_PER_LONG != 32
	    newvp = open(localpath, O_WRONLY | O_APPEND | O_LARGEFILE, 0);
#else
	    newvp = open(localpath, O_WRONLY | O_APPEND, 0);
#endif
	    fstat(newvp, &sbuf);

	    if (!S_ISREG(sbuf.st_mode)) {
		error = EACCES;
		close(newvp);
		break;
	    }
	}
	else if (req == AC_JASTART) {
	    error = EINVAL;
	    break;
	}
	if (req == AC_JASTOP)
	    newvp = NULLFD;

  	jid = (*getjid)(pid);
	if (jid <= 0) {
	    /* no job table entry */
	    error = ENODATA;
	    break;
	}

	job_acctbuf = csa_getjob(jid);
	if (job_acctbuf == NULL) {
	    /* couldn't get csa info in the job table entry */
	    error = ENODATA;
	    break;
	}
	/*
	 *  Use this semaphore since csa_write() can also change this
	 *  file pointer.
	 */
	down(&csa_write_sem);
	if ((oldvp = job_acctbuf->job_acctfile) != NULLFD) {
	    /* Stop writing to the old job accounting file */
	    close(oldvp);
	}

	/* Establish new job accounting file or stop job accounting */
	job_acctbuf->job_acctfile = newvp;
	up(&csa_write_sem);
	/* Write a config record so ja has uname info */
	if (req == AC_JASTART)
	    error = csa_config_write(AC_CONFCHG_ON, job_acctbuf->job_acctfile);

	break;
    }

    /*
     *  Write an accounting record for a system daemon.
     */
    case AC_WRACCT:
    {
	int len;
	uint64_t jid;
	struct csa_job *job_acctbuf;
	struct actwra actwra;

	PRINTF(LOG_INFO, "CSA: AC_WRACCT\n");
	if (!capable(cap_p, CAP_SYS_PACCT)) {
	    error = EPERM;
	    break;
	}
	memcpy(&actwra, (void *) data, sizeof(struct actwra));

	/*  Verify the parameters. */
	jid = actwra.ac_jid;
	if (jid < 0) {
	    error = EINVAL;
	    break;
	}

	id = actwra.ac_did;
	if ((id < 0) || (id >= ACCT_MAXKDS)) {
	    error = EINVAL;
	    break;
	}

	len = actwra.ac_len;
	if ((len <= 0) || (len > MAX_WRACCT)) {
	    error = EINVAL;
	    break;
	}

	if (actwra.ac_buf == NULL) {
	    error = EINVAL;
	    break;
	}

	/*  If the daemon type is on, write out the daemon buffer. */
	if ((acct_dmd[id][A_SYS].ac_state == ACS_ON) && (csa_acctvp != NULLFD))
	    error = csa_write(actwra.ac_buf, id, len, jid, A_DMD, NULL);

	/* get the job table entry for this jid */
	job_acctbuf = csa_getjob(jid);
	if (job_acctbuf == NULL) {
	    /* couldn't get accounting info stored in job table */
	    error = ENODATA;
	    break;
	}

	/* maybe write out daemon record to ja user accounting file */
	if (job_acctbuf->job_acctfile != NULLFD)
	    error = csa_write(actwra.ac_buf, id, len, jid, A_CJA, job_acctbuf);

	break;
    }

    /*
     *  Return authorized state information.
     */
    case AC_AUTH:
	PRINTF(LOG_INFO, "CSA: AC_AUTH\n");
	if (!capable(cap_p, CAP_SYS_PACCT)) {
	    error = EPERM;
	    break;
	}
	/*
	 *  Process user authorization request...If we get to this spot,
	 *  the user is authorized.
	 */
	break;

    default:
	PRINTF(LOG_INFO, "CSA: Unknown request %d\n", req);
	error = EINVAL;
    }

    return error;
}

/*
 *  Create a configuration change accounting record.
 */
static void
csa_config_make(ac_eventtype event, struct acctcfg *cfg)
{
    int	daemon = 0;
    int	record = 0;
    int	ind;
    int	nmsize = 0;
    struct utsname system_utsname;
    struct timespec xtime, now;

    memset(cfg, 0, sizeof(struct acctcfg));
    /*  Setup the record and header. */
    csa_header(&cfg->ac_hdr, REV_CFG, ACCT_KERNEL_CFG, sizeof(struct acctcfg));
    cfg->ac_event = event;
    clock_gettime(CLOCK_REALTIME, &xtime);
    if (!boottime) {
	clock_gettime(CLOCK_MONOTONIC, &now);
	boottime = xtime.tv_sec - now.tv_sec;
    }
    cfg->ac_boottime = boottime;
    cfg->ac_curtime  = xtime.tv_sec;

    /*
    *  Create the masks of the types that are on.
    */
    for (ind = 0; ind < ACCT_MAXKDS; ind++)
	if (acct_dmd[ind][A_SYS].ac_state == ACS_ON)
	    daemon += 1 << ind;
    for (ind = ACCT_RCDS; ind < ACCT_MAXRCDS; ind++) {
	int tid = ind - ACCT_RCDS;

	if (acct_rcd[tid][A_SYS].ac_state == ACS_ON)
	    record += 1 << tid;
    }
    cfg->ac_kdmask = daemon;
    cfg->ac_rmask = record;

    uname(&system_utsname);

    nmsize = sizeof(cfg->ac_uname.sysname);
    memcpy(cfg->ac_uname.sysname, system_utsname.sysname, nmsize-1);
    cfg->ac_uname.sysname[nmsize-1] = '\0';
    nmsize = sizeof(cfg->ac_uname.nodename);
    memcpy(cfg->ac_uname.nodename, system_utsname.nodename, nmsize-1);
    cfg->ac_uname.nodename[nmsize-1] = '\0';
    nmsize = sizeof(cfg->ac_uname.release);
    memcpy(cfg->ac_uname.release, system_utsname.release, nmsize-1);
    cfg->ac_uname.release[nmsize-1] = '\0';
    nmsize = sizeof(cfg->ac_uname.version);
    memcpy(cfg->ac_uname.version, system_utsname.version, nmsize-1);
    cfg->ac_uname.version[nmsize-1] = '\0';
    nmsize = sizeof(cfg->ac_uname.machine);
    memcpy(cfg->ac_uname.machine, system_utsname.machine, nmsize-1);
    cfg->ac_uname.machine[nmsize-1] = '\0';

    return;
}

/*
 *  Create and write a configuration change accounting record.
 */
static int
csa_config_write(ac_eventtype event, int job_acctfile)
{
    int	error = 0; /* errno */
    struct acctcfg acctcfg;

    /* write record to process accounting file. */
    csa_config_make(event, &acctcfg);

    down(&csa_write_sem);
    if (csa_acctvp != NULLFD)
	error = write(csa_acctvp, &acctcfg, sizeof(struct acctcfg));
    if (job_acctfile != NULLFD)
	error = write(job_acctfile, &acctcfg, sizeof(struct acctcfg));

    /*
     * This is bad!
     */
    if (error >= 0)
	error = 0;
    up(&csa_write_sem);
    return error;
}

/*
 *  When first process in a job is created.
 */
int
csa_jstart(struct csa_job *job_sojbuf)
{
    struct acctsoj soj;

    if (csa_acctvp == NULLFD)
	return 0;

    if (!job_sojbuf)
	return -1;

    memset(&soj, 0, sizeof(struct acctsoj));
    csa_header(&soj.ac_hdr, REV_SOJ, ACCT_KERNEL_SOJ, sizeof(struct acctsoj));
    soj.ac_type = AC_SOJ;

    soj.ac_uid = job_sojbuf->job_uid;
    soj.ac_jid = job_sojbuf->job_id;
    soj.ac_btime = job_sojbuf->job_start;

    /*
     *  Write the accounting record to the process accounting file if any
     *  accounting is enabled.
     */
    if (csa_acctvp != NULLFD)
	csa_write((char *) &soj, ACCT_KERN_CSA, sizeof(struct acctsoj),
		  job_sojbuf->job_id, A_SYS, job_sojbuf);

    return 0;
}

/*
 *  When last process in a job is done, write an EOJ record
 */
int
csa_jexit(struct csa_job *job_eojbuf)
{
    struct accteoj eoj;
    struct timespec xtime;

    if (csa_acctvp == NULLFD)
	return 0;

    if (!job_eojbuf)
	return -1;

    memset(&eoj, 0, sizeof(struct accteoj));
    csa_header(&eoj.ac_hdr1, REV_EOJ, ACCT_KERNEL_EOJ, sizeof(struct accteoj));
    csa_header(&eoj.ac_hdr2, REV_EOJ, ACCT_KERNEL_EOJ, 0);
    eoj.ac_hdr2.ah_magic = ~ACCT_MAGIC;

    eoj.ac_nice = getpriority(PRIO_PROCESS, job_eojbuf->job_uid);
    eoj.ac_uid = job_eojbuf->job_uid;
    eoj.ac_gid = getpgid(job_eojbuf->job_uid);
    eoj.ac_jid = job_eojbuf->job_id;

    eoj.ac_btime = job_eojbuf->job_start;
    clock_gettime(CLOCK_REALTIME, &xtime);
    eoj.ac_etime = xtime.tv_sec;

    /*
     *  XXX Once we have real values in these two fields, convert them
     *  to Kbytes.
     */
    eoj.ac_corehimem = job_eojbuf->job_corehimem;
    eoj.ac_virthimem = job_eojbuf->job_virthimem;

    /*
     *  Write the accounting record to the process accounting
     *  file if job accounting is enabled.
     */
    if (csa_acctvp != NULLFD)
	csa_write((char *) &eoj, ACCT_KERN_CSA, sizeof(struct accteoj),
		  job_eojbuf->job_id, A_SYS, job_eojbuf);

    return 0;
}

/*
 *  Write buf out to the accounting file.  If an error occurs, return the error 
 *  code to the caller.
 */
static int
csa_write(char *buf, int did, int nbyte, uint64_t jid, int type,
	  struct csa_job *jp)
{
    int	error;	/* errno */
    int vp;	/* acct file */
    struct stat sbuf;
    unsigned long limit;

    down(&csa_write_sem);
    /*  Locate the accounting type. */
    switch (type) {
    case A_SYS:
    case A_DMD:
	vp = csa_acctvp;
	break;
    case A_CJA:
	vp = (jp != NULL) ? jp->job_acctfile : NULLFD;
	break;
    default:
	up(&csa_write_sem);
	return EINVAL;
    }	/* end of switch(type) */

    /* Check if this type of accounting is turned on. */
    if (vp == NULLFD) {
	up(&csa_write_sem);
	return 0;
    }

    /* Protect ourselves just in case ... */
    if (fstat(vp, &sbuf) < 0) {
	syslog(LOG_ERR, "csa_write: bad file\n");

	switch(type) {
	case A_SYS:
	case A_DMD:
	    csa_acctvp = NULLFD;
	    break;
	case A_CJA:
	    jp->job_acctfile = NULLFD;
	    break;
	}
	up(&csa_write_sem);
	return EBADF;
    }

    error = write(vp, buf, nbyte);

    /*
     *  If we had a write error we ...... ignore it
     */
    if (error >= 0)
	error = 0;

    /* If an error occurred, disable this type of accounting. */
    if (error) {
	switch(type) {
	case A_SYS:
	case A_DMD:
	    csa_acctvp = NULLFD;
	    acct_dmd[did][A_SYS].ac_state = ACS_ERROFF;
	    acct_dmd[ACCT_KERN_CSA][A_SYS].ac_state = ACS_ERROFF;
	    syslog(LOG_ALERT, "csa accounting pacct write "
	       	   "error %d; %s disabled\n", error, acct_dmd_name[did]);
	    close(vp);
	    break;
	case A_CJA:
	    jp->job_acctfile = NULLFD;
	    syslog(LOG_WARNING, JID_ERR2, error, (unsigned long long) jid);
	    close(vp);
	    break;
	}
    }

    up(&csa_write_sem);
    return error;
}
