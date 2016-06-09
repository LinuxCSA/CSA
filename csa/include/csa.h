/*
 * Copyright (c) 2000 Silicon Graphics, Inc and LANL  All Rights Reserved.
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
 *  CSA (Comprehensive System Accounting)
 *  Job Accounting for Linux
 *
 *  This header file contains the definitions needed for job
 *  accounting. The CSA accounting daemon code and all
 *  user-level programs that try to write or process the binary job
 *  accounting data must include this file.
 *
 */

#ifndef _CSA_H
#define _CSA_H

#include <stdint.h>
#include <sys/types.h>

#define NULLFD -1	/* NULL file descriptor */

/*
 *  accounting flags per-process
 */
#define AFORK		0x01	/* fork, but did not exec */
#define ASU		0x02	/* super-user privileges */
#define AMORE		0x20	/* more CSA acct records for this process */

/*
 * Clock Ticks to Seconds conversion
 */
#ifndef CT_TO_SECS
#define CT_TO_SECS(x)	((x) / HZ)
#endif

/*
 * Magic number - for achead.ah_magic in the 1st header.  The magic number
 *                in the 2nd header is the inverse of this.
 */
#define ACCT_MAGIC_BIG          030510  /* big-endian */
#define ACCT_MAGIC_LITTLE       030512  /* little-endian */
#ifdef __LITTLE_ENDIAN
#define ACCT_MAGIC ACCT_MAGIC_LITTLE
#else
#define ACCT_MAGIC ACCT_MAGIC_BIG
#endif

/*
 * Record types - for achead.ah_type in the 1st header.
 */
#define	ACCT_KERNEL_CSA		0001	/* Kernel: CSA base record */
#define	ACCT_KERNEL_MEM		0002	/* Kernel: memory record */
#define	ACCT_KERNEL_IO		0004	/* Kernel: input/output record */
#define ACCT_KERNEL_DELAY	0010	/* Kernel: delay record */
#define	ACCT_KERNEL_SOJ		0012	/* Kernel: start-of-job record */
#define	ACCT_KERNEL_EOJ		0014	/* Kernel: end-of-job record */
#define	ACCT_KERNEL_CFG		0020	/* Kernel: configuration record */

#define	ACCT_KERNEL_SITE0	0100	/* Kernel: reserved for site */
#define	ACCT_KERNEL_SITE1	0101	/* Kernel: reserved for site */

#define	ACCT_DAEMON_WKMG      	0122	/* Daemon: workload management record,
					           i.e., LSF */

#define	ACCT_DAEMON_SITE0	0200	/* Daemon: reserved for site */
#define	ACCT_DAEMON_SITE1	0201	/* Daemon: reserved for site */

#define	ACCT_JOB_HEADER		0220	/* csabuild: job header record */
#define	ACCT_CACCT		0222	/* cacct:    consolidated data */
#define	ACCT_CMS		0224	/* cms:      command summary data */

/* Record types - for achead.ah_type in the 2nd header. */
#define	ACCT_MEM	1<<0	/* Process generated memory record */
#define	ACCT_IO		1<<1	/* Process generated I/O record */
#define ACCT_DELAY	1<<2	/* Process generated delay record */
/*
 * Record revision levels.
 *
 * These are incremented to indicate that a record's format has changed since
 * a previous release.
 * 
 * History:	05000	The first rev in Linux
 *              06000	Major rework to clean up unused fields and features.
 *			No binary compatibility with earlier rev.
 *              07000   Cleaned up I/O record and added delay record.
 *              010000  Changed acctjob offset type from int to int64_t.
 * NOTE: The header revision number was defined as 02400 in earlier version.
 *	 However, since ah_revision was defined as 15-bit field (ah_magic
 *	 takes up 17 bits), the revision number is read as twice the value in 
 *	 new code. So, define it to be 05000 accordingly.
 */
#define	REV_CSA		06000	/* Kernel: CSA base record */
#define	REV_MEM		06000	/* Kernel: memory record */
#define	REV_IO		07000	/* Kernel: I/O record */
#define REV_DELAY	07000   /* Kernel: delay record */
#define	REV_SOJ		06000	/* Kernel: start-of-job record */
#define	REV_EOJ		06000	/* Kernel: end-of-job record */
#define	REV_CFG		06000	/* Kernel: configuration record */
#define REV_WKMG	06000 	/* Daemon: workload management (i.e., LSF)
				           record */

#define REV_JOB		010000	/* csabuild: job header record */
#define REV_CACCT	06000	/* cacct:    consolidated data */
#define REV_CMS		06000	/* cms:      command summary data */

/*
 * Record header
 */
struct achead
{
	uint16_t	ah_magic;	/* Magic */
	uint16_t	ah_revision;	/* Revision */
	uint8_t		ah_type;	/* Record type */
	uint8_t		ah_flag;	/* Record flags */
	uint16_t	ah_size;	/* Size of record */
};

/*
 *  In order to keep the accounting records the same size across different
 *  machine types, record fields will be defined to types that won't
 *  vary (i.e. uint_32_t instead of uid_t).
*/

/*
 * Per process base accounting record.
 */
struct acctcsa
{
	struct achead	ac_hdr1;	/* Header */
	struct achead	ac_hdr2;	/* 2nd header for continued records */
	uint64_t 	ac_sbu;		/* System billing units */
	uint8_t		ac_stat;	/* Exit status */
	uint8_t		ac_nice;	/* Nice value */
	uint8_t		ac_sched;	/* Scheduling discipline */
	uint8_t		pad0;		/* Unused */
	uint32_t	ac_uid;		/* User ID */
	uint32_t	ac_gid;		/* Group ID */
	uint64_t	ac_ash;		/* Array session handle */
	uint64_t	ac_jid;		/* Job ID */
	uint64_t	ac_prid;	/* Project ID -> account ID */
	uint32_t	ac_pid;		/* Process ID */
	uint32_t	ac_ppid;	/* Parent process ID */
	time_t		ac_btime;	/* Beginning time [sec since 1970] */
	char		ac_comm[16];	/* Command name */
/*	CPU resource usage information. */
	uint64_t	ac_etime;	/* Elapsed time [usecs] */
	uint64_t	ac_utime;	/* User CPU time [usec] */
	uint64_t	ac_stime;	/* System CPU time [usec] */
	uint64_t	ac_spare;	/* Spare field */
	uint64_t        ac_spare1;	/* Spare field */
};

/*
 * Memory accounting structure
 * This structure is part of the acctmem record.
 */
struct memint
{
	uint64_t	himem;	/* Hiwater memory usage [Kbytes] */
	uint64_t	mem1;	/* Memory integral 1 [Mbytes/uSec] */
	uint64_t	mem2;	/* Memory integral 2 - not used */
	uint64_t	mem3;	/* Memory integral 3 - not used */
};

/*
 * Memory accounting record
 */
struct acctmem
{
	struct achead	ac_hdr;		/* Header */
	uint64_t 	ac_sbu;		/* System billing units */
	struct memint	ac_core;	/* Core memory integrals */
	struct memint	ac_virt;	/* Virtual memory integrals */
	uint64_t	ac_pgswap;	/* # of pages swapped  */
	uint64_t	ac_minflt;	/* # of minor page faults */
	uint64_t	ac_majflt;	/* # of major page faults */
	uint64_t	ac_spare;	/* Spare field */
};

/*
 * Input/Output accounting record
 */
struct acctio
{
	struct achead	ac_hdr;	   /* Header */
	uint64_t 	ac_sbu;	   /* System billing units */
	uint64_t	ac_chr;    /* Number of chars (bytes) read */
	uint64_t	ac_chw;	   /* Number of chars (bytes) written */
	uint64_t	ac_scr;	   /* Number of read system calls */
	uint64_t	ac_scw;	   /* Number of write system calls */
	uint64_t	ac_spare;  /* Spare field */
};

/*
 * Delay accounting record
 */
#include <csa_delay.h>


/*
 * Start-of-job record
 * Written when a job is created.
 */

typedef enum
{
        AC_INIT_LOGIN,          /* Initiated by login */
        AC_INIT_LSF,            /* Initiated by LSF */
        AC_INIT_CROND,          /* Initiated by crond */
        AC_INIT_FTPD,           /* Initiated by ftpd */
        AC_INIT_INETD,          /* Initiated by inetd */
        AC_INIT_TELNETD,        /* Initiated by telnetd */
        AC_INIT_MAX
} ac_inittype;

#define AC_SOJ	1	/* Start-of-job record type */
#define AC_ROJ	2	/* Restart-of-job record type */

struct acctsoj
{
	struct achead 	ac_hdr;		/* Header */
	uint8_t		ac_type;	/* Record type (AC_SOJ, AC_ROJ) */
	uint8_t		ac_init;	/* Initiator - currently not used */
	uint16_t	pad0;		/* Unused */
	uint32_t	ac_uid;		/* User ID */
	uint64_t	ac_jid;		/* Job ID */
	time_t	 	ac_btime;	/* Start time [secs since 1970] */
	time_t	 	ac_rstime;	/* Restart time [secs since 1970] */
};

/*
 * End-of-job record
 * Written when the last process of a job exits.
 */
struct accteoj
{
	struct achead	ac_hdr1;	/* Header */
	struct achead	ac_hdr2;	/* 2nd header for continued records */
	uint64_t 	ac_sbu;		/* System billing units */
	uint8_t		ac_init;	/* Initiator - currently not used */
	uint8_t		ac_nice;	/* Nice value */
	uint16_t	pad0;		/* Unused */
	uint32_t	ac_uid;		/* User ID */
	uint32_t	ac_gid;		/* Group ID */
	uint64_t	ac_ash;		/* Array session handle; not used */
	uint64_t	ac_jid;		/* Job ID */
	uint64_t	ac_prid;	/* Project ID; not used */
	time_t	 	ac_btime;	/* Job start time [secs since 1970] */
	time_t  	ac_etime;	/* Job end time   [secs since 1970] */
	uint64_t	ac_corehimem;	/* Hiwater core mem [Kbytes] */
	uint64_t	ac_virthimem;	/* Hiwater virt mem [Kbytes] */
/*	CPU resource usage information. */
	uint64_t	ac_utime;  /* User CPU time [usec]  */
	uint64_t	ac_stime; /* System CPU time [usec] */
	uint32_t	ac_spare;
};

/*
 * Accounting configuration uname structure
 * This structure is part of the acctcfg record.
 */
struct ac_utsname
{
	char	 sysname[26];
	char	 nodename[26];
	char	 release[42];
	char	 version[41];
	char	 machine[26];
};

/*
 * Accounting configuration record
 * Written for accounting configuration changes.
 */
typedef enum
{
        AC_CONFCHG_BOOT,        /* Boot time (always first) */
        AC_CONFCHG_FILE,        /* Reporting pacct file change */
        AC_CONFCHG_ON,          /* Reporting xxx ON */
        AC_CONFCHG_OFF,         /* Reporting xxx OFF */
        AC_CONFCHG_MAX
} ac_eventtype;

struct acctcfg
{
	struct achead	ac_hdr;		/* Header */
	unsigned int	ac_kdmask;	/* Kernel and daemon config mask */
	unsigned int	ac_rmask;	/* Record configuration mask */
	int64_t		ac_uptimelen;	/* Bytes from the end of the boot
					   record to the next boot record */
	uint8_t		ac_event;	/* Accounting configuration event */
	uint8_t		pad0[3];		/* Unused */
	time_t		ac_boottime;	/* System boot time [secs since 1970]*/
	time_t		ac_curtime;	/* Current time [secs since 1970] */
	struct ac_utsname  ac_uname;	/* Condensed uname information */
};


/*
 * Accounting control status values.
 */
typedef	enum
{
	ACS_OFF,	/* Accounting stopped for this entry */
	ACS_ERROFF,	/* Accounting turned off by kernel */
	ACS_ON		/* Accounting started for this entry */
} ac_status;

/*
 * Function codes for CSA library interface
 */
typedef	enum
{
	AC_START,	/* Start kernel, daemon, or record accounting */
	AC_STOP,	/* Stop kernel, daemon, or record accounting */
	AC_HALT,	/* Stop all kernel, daemon, and record accounting */
	AC_CHECK,	/* Check a kernel, daemon, or record accounting state*/
	AC_KDSTAT,	/* Check all kernel & daemon accounting states */
	AC_RCDSTAT,	/* Check all record accounting states */
	AC_JASTART,	/* Start user job accounting  */
	AC_JASTOP,	/* Stop user job accounting */
	AC_WRACCT,	/* Write accounting record for daemon */
	AC_AUTH,	/* Verify executing user is authorized */
	AC_MREQ
} ac_request;

/*
 * Define the CSA accounting record type indices.
 */
typedef	enum
{
	ACCT_KERN_CSA,		/* Kernel CSA accounting */
	ACCT_KERN_JOB_PROC,	/* Kernel job process summary accounting */
	ACCT_DMD_WKMG, 		/* Daemon workload management (i.e. LSF) acct */
	ACCT_DMD_SITE1,		/* Site reserved daemon acct */
	ACCT_DMD_SITE2,		/* Site reserved daemon acct */
	ACCT_MAXKDS,		/* Max # kernel and daemon entries */

	ACCT_RCD_MEM,		/* Record acct for memory */
	ACCT_RCD_IO,		/* Record acct for input/output */
	ACCT_RCD_DELAY,		/* Record acct for delay */
	ACCT_THD_MEM,		/* Record acct for memory size threshhold */
	ACCT_THD_TIME,		/* Record acct for CPU time threshhold */
	ACCT_RCD_SITE1,		/* Site reserved record acct */
	ACCT_RCD_SITE2,		/* Site reserved record acct */
	ACCT_MAXRCDS		/* Max # record entries */
} ac_kdrcd;

#define	ACCT_RCDS	ACCT_RCD_MEM /* Record acct low range definition */
#define	NUM_KDS		(ACCT_MAXKDS - ACCT_KERN_CSA)
#define	NUM_RCDS	(ACCT_MAXRCDS - ACCT_RCDS)
#define	NUM_KDRCDS	(NUM_KDS + NUM_RCDS)


/*
 * The following structures are used to get status of a CSA accounting type.
 */

/*
 * Accounting entry status structure
 */
struct actstat
{
	ac_kdrcd	ac_ind;		/* Entry index */
	ac_status	ac_state;	/* Entry status */
	int64_t		ac_param;	/* Entry parameter */
};

/*
 * Accounting control and status structure
 */
#define	ACCT_PATH	128	/* Max path length for accounting file */

struct actctl
{
	int	ac_sttnum;		/* Number of status array entries */
	char	ac_path[ACCT_PATH];	/* Path name for accounting file */
	struct actstat	ac_stat[NUM_KDRCDS];	/* Entry status array */
};

/*
 * Daemon write accounting structure
 */
#define	MAX_WRACCT	1024	/* Maximum buffer size of wracct() */

struct actwra
{
	int	 ac_did;		/* Daemon index */
	int	 ac_len;		/* Length of buffer (bytes) */
	uint64_t ac_jid;		/* Job ID */
	char	 ac_buf[MAX_WRACCT];	/* Daemon accounting buffer */
};

#endif	/* _CSA_H */
