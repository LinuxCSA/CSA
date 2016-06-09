/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2007 Silicon Graphics, Inc  All Rights Reserved.
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

#ifndef _CSAACCT_H
#define _CSAACCT_H

/* Definitions and structures for daemon accounting including:
 *     - NQS
 *     - Workload management, i.e., LSF, PBS
 *     - Tape
 *     - Data migration
 */

#include <sys/types.h>
#include <csa.h>

/*
 * ********************************
 *     NQS accounting structure
 * ********************************
 */

/*
 * NQS accounting record types.
 */
typedef	enum
{
	NQ_INFO = 1,	/* Informational */
	NQ_RECV,	/* Request received */
	NQ_INIT,	/* Request initiated */
	NQ_TERM,	/* Request terminated */
	NQ_DISP,	/* Request disposed */
	NQ_SENT,	/* Request sent */
	NQ_SPOOL,	/* Request output spooled */
	NQ_CON,		/* Consolidated NQS request */
	NQ_MAX
} nq_type;

/*
 * Subtypes for NQS accounting record type NQ_INFO.
 */
#define NQ_INFO_ACCTON	1	/* Accounting started */
#define NQ_INFO_ACCTOFF	2	/* Accounting stopped */

/*
 * Subtypes for NQS accounting record type NQ_RECV.
 */
#define NQ_RECV_NEW	1	/* New request */
#define NQ_RECV_LOCAL	2	/* Local request received from pipe queue */
#define NQ_RECV_REMOTE	3	/* Remote request received from pipe queue */

/*
 * Subtypes for NQS accounting record type NQ_INIT.
 *
 * The NQ_SENT and NQ_SPOOL INIT subtypes must follow this numbering scheme.
 */
#define NQ_INIT_START	1	/* Request started for the first time */
#define NQ_INIT_RESTART	2	/* Request has been restarted */
#define NQ_INIT_RERUN	3	/* Request has been rerun */

/*
 * Subtypes for NQS accounting record type NQ_TERM.
 *
 * The NQ_SENT and NQ_SPOOL TERM subtypes must follow this numbering scheme.
 */
#define NQ_TERM_EXIT	1	/* Request exited */
#define NQ_TERM_REQUEUE	2	/* Request requeued for a restart */
#define NQ_TERM_PREEMPT	3	/* Request pre-empted */
#define NQ_TERM_HOLD	4	/* Request held */
#define NQ_TERM_OPRERUN	5	/* Request rerun by operator */
#define NQ_TERM_RERUN	8	/* Request non-operator rerun */

/*
 * Subtypes for NQS accounting record type NQ_DISP.
 */
#define NQ_DISP_NORM	1	/* Request disposed normally */
#define NQ_DISP_BOOT	2	/* Request disposed due to re-boot problem */

/*
 * Subtypes for NQS accounting record type NQ_SENT.
 *
 * The INIT and TERM subtypes must follow the NQ_INIT and NQ_TERM subtype
 * numbering scheme.
 */
#define NQ_SENT_INIT	4	/* Sending of request started */
#define NQ_SENT_TERM	6	/* Sending of request stopped */

/*
 * Subtypes for NQS accounting record type NQ_SPOOL.
 *
 * The INIT and TERM subtypes must follow the NQ_INIT and NQ_TERM subtype
 * numbering scheme.
 */
#define NQ_SPOOL_INIT	5	/* Spooling of request output started */
#define NQ_SPOOL_TERM	7	/* Spooling of request output stopped */

/*
 * NQS accounting record for type NQ_INFO:
 *
 *	short	type;			Type of record (NQ_INFO)
 *	short	subtype;		Subtype of record
 *	time_t	time;			Time of record
 *	uid_t	uid;			ID of user who initiated action
 *
 *
 * NQS accounting record for type NQ_RECV:
 *
 *	short	type;			Type of record (NQ_RECV)
 *	short	subtype;		Subtype of record
 *	time_t	time;			Time of record
 *	uid_t	uid;			User ID
 *	int64_t	seqno;			Request sequence number
 *	int64_t	mid;			Originating machine ID
 *	char	machname[16];		Originating machine name
 *	char	reqname[16];		Request name
 *	char	quename[16];		NQS queue name
 *	int	qtype;			NQS queue type (see nqs.h):
 *					{ QUE_BATCH = 1, QUE_PIPE = 4 }
 *
 *
 * NQS accounting record for type NQ_INIT:
 *
 *	short	type;			Type of record (NQ_INIT)
 *	short	subtype;		Subtype of record
 *	time_t	time;			Time of record
 *	uid_t	uid;			User ID
 *	uint64_t prid;			Project ID
 *	uint64_t ash;			Array session handle
 *	uint64_t jid;			Job ID
 *	int64_t	seqno;			Request sequence number
 *	int64_t	mid;			Originating machine ID
 *	char	machname[16];		Originating machine name
 *	char	reqname[16];		Request name
 *	char	quename[16];		NQS queue name
 *	int	qtype;			NQS queue type (see nqs.h):
 *					{ QUE_BATCH = 1, QUE_PIPE = 4 }
 *
 *
 * NQS accounting record for type NQ_TERM:
 *
 *	short	type;			Type of record (NQ_TERM)
 *	short	subtype;		Subtype of record
 *	time_t	time;			Time of record
 *	uid_t	uid;			User ID
 *	uint64_t prid;			Project ID
 *	uint64_t ash;			Array session handle
 *	uint64_t jid;			Job ID
 *	int64_t	seqno;			Request sequence number
 *	int64_t	mid;			Originating machine ID
 *	char	machname[16];		Originating machine name
 *	char	reqname[16];		Request name
 *	char	quename[16];		NQS queue name
 *	int	qtype;			NQS queue type (see nqs.h):
 *					{ QUE_BATCH = 1, QUE_PIPE = 4 }
 *	int64_t	code;			Request completion code RCM_ (see
 *					requestcc.h)
 *	int64_t	utime;			User CPU time used by the shepherd
 *	int64_t	stime;			System CPU time used by the shepherd
 *
 *
 * NQS accounting record for type NQ_DISP:
 *
 *	short	type;			Type of record (NQ_DISP)
 *	short	subtype;		Subtype of record
 *	time_t	time;			Time of record
 *	uid_t	uid;			User ID
 *	int64_t	seqno;			Request sequence number
 *	int64_t	mid;			Originating machine ID
 *	char	machname[16];		Originating machine name
 *	char	reqname[16];		Request name
 *	char	quename[16];		NQS queue name
 *	int	qtype;			NQS queue type (see nqs.h):
 *					{ QUE_BATCH = 1, QUE_PIPE = 4 }
 *
 *
 * NQS accounting record for type NQ_SENT:
 *
 *	short	type;			Type of record (NQ_SENT)
 *	short	subtype;		Subtype of record
 *	time_t	time;			Time of record
 *	uid_t	uid;			User ID
 *	uint64_t prid;			Project ID
 *	uint64_t ash;			Array session handle
 *	uint64_t jid;			Job ID
 *	int64_t	seqno;			Request sequence number
 *	int64_t	mid;			Originating machine ID
 *	char	machname[16];		Originating machine name
 *	char	reqname[16];		Request name
 *	char	quename[16];		NQS queue name
 *	int	qtype;			NQS queue type (see nqs.h):
 *					{ QUE_BATCH = 1, QUE_PIPE = 4 }
 *	int64_t	code;			Request completion code RCM_ (see
 *					requestcc.h) for NQ_SENT_TERM only
 *	int64_t	utime;			User CPU time used by the shepherd
 *					(NQ_SENT_TERM only)
 *	int64_t	stime;			System CPU time used by the shepherd
 *					(NQ_SENT_TERM only)
 *
 *
 * NQS accounting record for type NQ_SPOOL:
 *
 *	short	type;			Type of record (NQ_SPOOL)
 *	short	subtype;		Subtype of record
 *	time_t	time;			Time of record
 *	uid_t	uid;			User id
 *	uint64_t prid;			Project ID
 *	uint64_t ash;			Array session handle
 *	uint64_t jid;			Job ID of netclient
 *	int64_t	seqno;			Request sequence number
 *	int64_t	mid;			Originating machine ID
 *	char	machname[16];		Originating machine name
 *	char	reqname[16];		Request name
 *	char	quename[16];		NQS queue name
 *	int	qtype;			NQS queue type (see nqs.h):
 *					{ QUE_BATCH = 1, QUE_PIPE = 4 }
 *	int64_t	code;			Request completion code RCM_ (see
 *					requestcc.h) for NQ_SPOOL_TERM only
 *
 *
 * NQS accounting record for type NQ_CON:
 *
 *	short	type;			Type of record (NQ_CON)
 *	short	init_subtype;		Initiation subtype (NQ_INIT)
 *	time_t	start_time;		RECV or INIT time
 *	uid_t	uid;			User ID
 *	uint64_t prid;			Project ID
 *	uint64_t ash;			Array session handle
 *	uint64_t jid;			Job ID
 *	int64_t	seqno;			Request sequence number
 *	int64_t	mid;			Originating machine ID
 *	char	machname[16];		Originating machine name
 *	char	reqname[16];		Request name
 *	char	quename[16];		NQS queue name
 *	int	qtype;			NQS queue type (see nqs.h):
 *					{ QUE_BATCH = 1, QUE_PIPE = 4 }
 *	int64_t	code;			Request completion code RCM_ (see
 *					requestcc.h) for NQ_SENT_TERM only
 *	int64_t	utime;			User CPU time used by the shepherd
 *	int64_t	stime;			System CPU time used by the shepherd
 *
 *	short	term_subtype;		Termination subtype (NQ_TERM)
 *	short	disp_subtype;		Dispose subtype (NQ_DISP)
 *	time_t	tmtime;			NQ_TERM timestamp if not NQ_DISP
 *	int64_t	qwtime;			Queue wait time 
 *	double	sbu_wt;			SBU weighting factor
 *
 */
struct nqsbs
{
	struct achead	hdr;		/* Header */

	double		sbu;		/* System billing units; cpusets only */

	short		type;		/* Type of record */
	short		subtype;	/* Subtype of record */
	time_t		time;		/* Time of record [sec since 1970] */
	int		qtype;		/* NQS queue type */
	uint32_t	uid;		/* User ID */
	uint64_t	prid;		/* Project ID */
	uint64_t	ash;		/* Array session handle */
	uint64_t	jid;		/* Job ID */
	int64_t		seqno;		/* Request sequence number */
	int64_t		mid;		/* Originating machine ID */
	char		machname[16];	/* Originating machine name */
	char		reqname[16];	/* Request name */
	char		quename[16];	/* NQS queue name */
	int64_t		code;		/* Request completion code */
	int64_t		utime;		/* User CPU time [usec] */
	int64_t		stime;		/* System CPU time [usec] */
	int64_t		ctime;		/* Cpuset time [usec] */
	int64_t		mem_reserved;   /* Cpuset memory [kbyte] */
	short		ncpus;		/* Cpuset number of cpus */
	short		spare1;		/* Spare field */
	int		spare2;		/* Spare field */

	short		term_subtype;	/* Termination subtype (NQ_TERM) */
	short		disp_subtype;	/* Dispose subtype (NQ_DISP) */
	time_t		tmtime;		/* NQ_TERM timestamp if not NQ_DISP */
	int64_t		qwtime;		/* Queue wait time */
	double		sbu_wt;		/* SBU weighting factor */
};

/* Field names defined for type NQ_CON and WM_CON. */
#define	init_subtype	subtype	/* Initiation subtype (NQ_INIT) */
#define	start_time	time	/* RECV or INIT time [sec since 1970] */

#define NQS_NO_JID	-2	/* Only NQ_RECV and/or NQ_DISP recs found */
#define NQS_NO_DISP	-1	/* No NQ_DISP record found */
#define NQS_NO_INIT	-1	/* No NQ_INIT record found */
#define NQS_NO_TERM	-2	/* No NQ_TERM record found */


/*
 * ************************************************
 *     Workload management accounting structure
 * ************************************************
 */

/*
 * Workload management accounting record types.  Currently, for LSF, only
 * WM_INIT, WM_TERM, and WM_CON record types are created.
 */
typedef enum
{
	WM_INFO = 1,	/* Informational */
	WM_RECV,	/* Request received */
	WM_INIT,	/* Request initiated */
	WM_SPOOL,	/* Request output spooled */
	WM_TERM,	/* Request terminated */
	WM_CON,		/* Consolidated workload management request */
	WM_MAX
} wm_type;

/*
 * Subtypes for workload management accounting record type WM_INFO.
 */
#define WM_INFO_ACCTON  1       /* Accounting started */
#define WM_INFO_ACCTOFF 2       /* Accounting stopped */

/*
 * Subtypes for workload management accounting record type WM_RECV.
 */
#define WM_RECV_NEW	1	/* New request */

/*
 * Subtypes for workload management accounting record type WM_INIT.
 */
#define WM_INIT_START	1	/* Request started for first time */
#define WM_INIT_RESTART	2	/* Request restarted */
#define WM_INIT_RERUN	3	/* Request rerun */

/*
 * Subtypes for workload management accounting record type WM_SPOOL.
 * The INIT and TERM subtypes must follow the WM_INIT and WM_TERM subtype
 * numbering scheme to ensure that values are unique among all of the 
 * WM_INIT and WM_TERM subtypes.
 */
#define WM_SPOOL_INIT	4	/* Output return started */
#define WM_SPOOL_TERM	6	/* Output return stopped */

/*
 * Subtypes for workload management accounting record type WM_TERM.
 */
#define WM_TERM_EXIT	1	/* Request exited */
#define WM_TERM_REQUEUE	2	/* Request requeued */
#define WM_TERM_HOLD	3	/* Request checkpointed and held */
#define WM_TERM_RERUN	4	/* Request will be rerun */
#define WM_TERM_MIGRATE	5	/* Request will be migrated */

/*
 * LSF accounting record for type WM_INIT:
 *
 *      short   type;                   Type of record (WM_INIT)
 *      short   subtype;                Subtype of record
 *      int     arrayid;                Array ID
 *      char    serv_provider[16];      Service provider - "LSF"
 *      time_t  time;                   Time of record
 *      time_t  enter_time;             Time the request was queued
 *      uid_t   uid;                    User ID
 *      prid_t  prid;                   Project ID
 *      ash_t   ash;                    Array session handle
 *      jid_t   jid;                    Job ID
 *      char    machname[16];           Originating machine name
 *      char    reqname[16];            Request name
 *      char    quename[16];            Queue name
 *      int64_t reqid;                  Request ID
 *
 *
 * LSF accounting record for type WM_TERM:
 *
 *      short   type;                   Type of record (WM_TERM)
 *      short   subtype;                Subtype of record
 *      int     arrayid;                Array ID
 *      char    serv_provider[16];      Service provider - "LSF"
 *      time_t  time;                   Time of record
 *      uid_t   uid;                    User ID
 *      prid_t  prid;                   Project ID
 *      ash_t   ash;                    Array session handle
 *      jid_t   jid;                    Job ID
 *      char    machname[16];           Originating machine name
 *      char    reqname[16];            Request name
 *      char    quename[16];            Queue name
 *      int64_t reqid;                  Request ID
 *      int64_t code;                   Request completion code
 *
 *
 * LSF accounting record for type WM_CON:
 *
 *      short   type;                   Type of record (WM_CON)
 *      short   init_subtype;           Initiation subtype (WM_INIT)
 *      int     arrayid;                Array ID
 *      char    serv_provider[16];      Service provider - "LSF"
 *      time_t  start_time;             INIT time
 *      uid_t   uid;                    User ID
 *      prid_t  prid;                   Project ID
 *      ash_t   ash;                    Array session handle
 *      jid_t   jid;                    Job ID
 *      char    machname[16];           Originating machine name
 *      char    reqname[16];            Request name
 *      char    quename[16];            Queue name
 *      int64_t reqid;                  Request ID
 *      int64_t code;                   Request completion code
 *      short   term_subtype;           Termination subtype (WM_TERM)
 *      int64_t qwtime;                 Queue wait time
 *      double  sbu_wt;                 SBU weighting factor
 *
 */

struct wkmgmtbs
{
	struct achead	hdr;		/* Header */

	double		sbu;		/* System billing units */

	short		type;		/* Type of record */
	short		subtype;	/* Subtype of record */
	int		arrayid;	/* Array ID (LSF) */
	char		serv_provider[16];  /* Service provider, i.e., "LSF",
					       "PBS", "CODINE" */
	time_t		time;		/* Time of record [sec since 1970] */
	time_t		enter_time;	/* Time the request was queued
					   [sec since 1970] */
	int		qtype;		/* Queue type */
	uint32_t	uid;		/* User ID */
	uint64_t	prid;		/* Project ID */
	uint64_t	ash;		/* Array session handle */
	uint64_t	jid;		/* Job ID */
	char		machname[16];	/* Originating machine name */
	char		reqname[16];	/* Request name */
	char		quename[16];	/* Queue name */
	int64_t		reqid;		/* Request ID */
	int64_t		code;		/* Request completion code */
	int64_t		utime;		/* User CPU time [usec] */
	int64_t		stime;		/* System CPU time [usec] */
	int64_t 	ctime;		/* Cpuset time [usec] */
	int64_t		mem_reserved;	/* Cpuset reserved memory [kbyte] */
	short		ncpus;		/* Cpuset number of cpus */
	short		term_subtype;	/* Termination subtype (WM_TERM) */
	int		spare;		/* Spare field */
	int64_t		qwtime;		/* Queue wait time [sec] */
	double		sbu_wt;		/* SBU weighting factor */
};

#define WM_NO_JID       -3      /* Only WM_RECV record found */
#define WM_NO_INIT      -1      /* No WM_INIT record found */
#define WM_NO_TERM      -2      /* No WM_TERM record found */


/*
 * *********************************
 *     Tape accounting structure
 * *********************************
 */
#define TP_MAXDEVGRPS	8	/* Max # of device groups expected */

/*
 * Tape accounting record types.
 */
typedef	enum
{
	TAPE_BASE,	/* Tape base record */
	TAPE_RSV,	/* Tape reservation record */
	TAPE_ERR,	/* Tape error record */
	TAPE_MAX
} tape_type;

/*
 * Tape status types.
 */
typedef	enum
{
	TPST_ABTERM,	/* Abnormal termination	*/
	TPST_BOF,	/* Beginning of file */
	TPST_CLSV,	/* Close volume */
	TPST_EOT,	/* End of tape */
	TPST_EOV,	/* End of volume */
	TPST_HWERR,	/* Hardware error */
	TPST_POS,	/* Positioning */
	TPST_RLS,	/* Release device */
	TPST_WERR,	/* Write error */
	TPST_MAX
} tape_stat;

/*
 * Tape accounting record for type TAPE_BASE:
 *
 *	short	type;		Record type (TAPE_BASE)
 *	uid_t	uid;		Effective user ID
 *	uint64_t prid;		Project ID
 *	uint64_t ash;		Array session handle
 *	uint64_t jid;		Job ID
 *	time_t	btime;		Time of record
 *	time_t	actime;		Timestamp used internally by CSA 
 *	char	devgrp[16];	Name of this device group
 *	char	devname[16];	Name of this device
 *	int64_t	stime;		System CPU time
 *	int64_t	utime;		User CPU time
 *	char	vsn[8];		Internal vsn of the tape
 *	char	evsn[8];	External vsn of the tape
 *	int	status;		Status
 *	int64_t	nread;		Number of bytes read
 *	int64_t	nwrite;		Number of bytes written
 *
 *
 * Tape accounting record for type TAPE_RSV:
 *
 *	short	type;		Record type (TAPE_RSV)
 *	uid_t	uid;		Effective user ID
 *	uint64_t prid;		Project ID
 *	uint64_t ash;		Array session handle
 *	uint64_t jid;		Job ID
 *	time_t	btime;		Time of record
 *	time_t	actime;		Timestamp used internally by CSA
 *	char	devgrp[16];	Name of this device group
 *	char	devname[16];	Name of this device
 *	int64_t	rtime;		Reservation time of this device group
 *
 */
struct tapebs
{
	struct achead	hdr;		/* Header */

	double		sbu;		/* System billing units */

	short		type;		/* Record type */
	short		spare1;		/* Spare field */
	uint32_t	uid;		/* Effective user ID */
	uint64_t	prid;		/* Project ID */
	uint64_t	ash;		/* Array session handle */
	uint64_t	jid;		/* Job ID */
	time_t		btime;		/* Time of record [sec since 1970] */
	time_t		actime;		/* Timestamp used internally by CSA */
	char		devgrp[16];	/* Name of this device group */
	char		devname[16];	/* Name of this device */
	int64_t		stime;		/* System CPU time [usec] */
	int64_t		utime;		/* User CPU time [usec] */
	int64_t		rtime;		/* Reservation time [usec] */
	char		vsn[8];		/* Internal vsn of the tape */
	char		evsn[8];	/* External vsn of the tape */
	int		status;		/* Status */
	int		spare2;		/* Spare field */
	int64_t		nread;		/* Number of bytes read */
	int64_t		nwrite;		/* Number of bytes written */
};


/*
 * *******************************************
 *     Data migration accounting structure
 * *******************************************
 */
#define DM_MAXREQ	3	/* Max # request types */

struct dmigbs
{
	struct achead	hdr;
	uint32_t	uid;		/* User ID */
	uint32_t	gid;		/* Group ID */
	uint64_t	ash;		/* Array session handle */
	uint64_t	jid;		/* Job ID */
	uint64_t	prid;		/* Project ID */
	int		request;	/* Request type */
	char		filesys[32];	/* File system */
	int		inode;		/* Inode number */
	int64_t		xferred;	/* Bytes transferred */
	time_t		stime;		/* Start time of this request */
	int64_t		runtime;	/* Runtime */
	int64_t		usertime;	/* Cpu time spent on behalf of user */
	int64_t		systime;	/* Sys time spent on behalf of user */
};

#endif /* _CSAACCT_H */
