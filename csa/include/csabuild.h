/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2006-2007 Silicon Graphics, Inc. All Rights Reserved.
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
 *	NOTE: JIDWRAP		Job ID wrap conditions
 *
 *	The following explanation covers how job ID wrap conditions are
 *	handled.  Occasionally the job ID tag will be used on multiple
 *	jobs.  In order to handle this the following scheme is used
 *
 *	  Uptime Table      (indexed by jobid)
 *	  ------------	 Segment array for Uptime 0
 *     0 | pointers   |--------> ----------
 *	  ------------    jid 0 | ptrs to  |------->[Frec][Frec][Frec]->NULL
 *     1 |   to       |		 ----------     \
 *	  ------------    jid 1 | frecs    |     \--> -------------
 *     2 | segment    |          ----------          | extra jid 0 |-> jidNULL
 *	  ------------    jid 2 | and to   |          -------------
 *     . | structs    |          ----------              \->[Frec][Frec]->NULL
 *	  ------------        . | extra    |
 *     n |            |          ----------
 *	  ------------    jid n | segments |
 *			          . . . . .
 *
 *	So all original (no wrap) job ids will go into the segment array.
 *	But if another job comes along with the same jid we need to keep it
 *	separate.  To do so we allocate a single segment structure and put
 *	the job into that structure.  We repeat this as often as necessary.
 *	While it may be faster to allocate another whole array if wraps
 *	occur frequently, we felt at design and test time that except for
 *	the occasional job which was often recovered, the job wrap case did
 *	not occur frequently enough to justify the memory use.
 *
 *	How do we tell if a job is done?  First when a job exits, an accteoj
 *	record is written. Second, when a NQS or Workload Mgmt job terminates,
 *	a daemon record is written.  So if no NQS or Workload Mgmt records 
 *	are present we assume the job is dead and buried.  However, if an
 *	NQS or Workload Mgmt record exists it takes precedence.
 *
 *	The diagram above is simplified. The uptime table up_seg field
 *	points to a hash table.  The hash table contains pointers to
 *	segments.  A HASH_MASK is applied to the lower (currently 15)
 *	bits of a jid to get the index into the hash table.  The segments
 *	which hash to the same index are chained together.  Each
 *	segment then handles the jid wrap as noted above and has pointers
 *	to Crecs and Frecs.  A global linked list of the segments from
 *	the hash table is kept (jid_head) and used to sort the jids
 *	in ascending order before records are written into
 *	the sorted pacct file.
 */

/*
 *	some basic macros
 */
#ifndef	MIN
#define	MIN(A,B)	((A) < (B) ? (A) : (B))
#endif

#ifndef	MAX
#define	MAX(A,B)	((A) > (B) ? (A) : (B))
#endif

#ifndef	ABS
#define	ABS(A)		((A) >= 0 ? (A) : (-(A)))
#endif

#define HASH_MASK 0x3fff		/* use lower 15 bits for hash index */
#define HASH_TBL_LEN 32768
#define HASH_INDEX(jid)		(jid & HASH_MASK)

/*
 *	Definitions for the file table.
 */
#define	F_READ		O_RDONLY		/* 0 */
#define	F_WRITE		O_WRONLY		/* 1 */
#define	F_RDWR		O_RDWR			/* 2 */
#define	RWMODE		(O_RDONLY|O_WRONLY|O_RDWR)
#define	F_UNUSED	04
#define	F_OPEN		020
#define F_CLOSED	040

#define	MAXFILENAME		1500

struct	File	{
	int	status;		/* file status */
	int	fd;		/* file descriptor */
	char	*name;		/* file name */
};

/*
 *	Definitions for Core records.
 */
struct Crec {
	uint	cp_type:16;		/* record type */
	struct  wkmgmtbs *cp_wm;	/* pointer to workload mgmt record */
	struct	Crec	*cp_next;	/* pointer to next Core record */
};

/*
 *	Definitions for File records.
 */
struct Frec {
	uint	fp_type:16;		/* record type */
	uint	fp_ind:16;		/* index to file */
	off_t	fp_offset;		/* byte offset into file */
	struct	Frec	*fp_next;	/* pointer to next File record */
};

/*
 *	Define record types to be manipulated.
 */
typedef	enum {
	REC_CFG,	/* Config record */
	REC_EOP,	/* End of Process record */
	REC_SOJ,	/* Start of Job record */
	REC_EOJ,	/* End of Job record */
	REC_JOB,	/* Job record */
	REC_WKMG,	/* Workload Management record */
	REC_MAX
} rec_flag;

typedef	enum {
	RC_GOOD,	/* good record flag */
	RC_BAD,		/* bad record flag */
	RC_CFG,		/* Configuration record flag */
	RC_NOJOB,	/* no job match flag */
	RC_UPIND,	/* uptime range error flag */
	RC_MAX
} rc_flag;

/*
 *	Definitions for Job segments; sg_ptr is set if the job is done or
 *	it can point to a chain of segments with the same jid; 
 *	sg_hashnext points to a chain of segments which hash to the same
 *	hash table index; sg_jidlistnext points to a chain of all segments
 *	that have unique jids (that is, it does not directly link in the
 *	segments from sg_ptr).
 */
struct segment {
	struct	segment	*sg_ptr;	/* if job is closed this is set */
	struct  segment *sg_hashnext;   /* chain of jids with same hash index */
	struct  segment *sg_jidlistnext; /* chain of all segments with jids */
	uint64_t	sg_jid;		/* jid for this segment */
	int64_t		sg_seqno;	/* NQS sequence and */
	int64_t		sg_mid;		/* NQS machine ID */
	int64_t		sg_reqid;	/* Workload Mgmt request id */
	int64_t		sg_arrayid;	/* Workload Mgmt array id (LSF) */
	time_t		sg_start;	/* job start time from soj */
	time_t		sg_stop;	/* job stop time from eoj */
	struct	Frec	*sg_proc;	/* pointer to front of process chain */
	struct	Frec	**sg_lproc;	/* pointer to last process in chain */
	struct  Crec	*sg_wm;		/* pointer to front of Workload chain */
	struct  Crec	**sg_lwm;	/* pointer to last Workload in chain */
};

#define	NOJOBFOUND	(struct segment *)NULL	/* null job pointer */
#define	BADPOINTER	((struct segment *)01)	/* bad job pointer */

/*
 *	Definitions for Uptime structure.
 */
struct Uptime {
	time_t	up_start;		/* start of Uptime */
	time_t	up_stop;		/* end of Uptime */
	int	up_size;		/* number of segments */
	struct	segment	**up_seg;	/* hash table of pointers to segments */
					/* for jids in this up period*/
	struct  segment *up_wm;		/* segments for Workload Mgmt reqs */
					/* with job ID == WM_NO_JID */
	struct  segment *up_jidhead;	/* start of segment chain */
	struct  segment *up_jidlast;	/* pointer to last segment */
};

/*
 * Number of seconds between first process and an accounting system restart.
 * Used because pacct record time stamps are process start time not stop times.
 *	Only important on the first uptime because from then on we use the
 *	stop times not the start times as the boundary.
 */
#define	PACCT_FUDGE	(60*1)
#define	A_DAY		(3600*24)	/* seconds in a day */
#define	A_YEAR		(A_DAY*365)	/* seconds in a year */
#define	EARLY_FUDGE	(A_YEAR*28)	/* ~ 28 years in seconds (1998) */
#define ZAPJID		-9		/* job ID zapped entries are set to */

/*
 *      Definitions for tracking workload management requests across
 *      uptimes...
 */
struct  wm_track        {
        int64_t         wt_reqid;       /* workload management request ID & */
        int64_t         wt_arrayid;     /* array ID */
        int             wt_upind;       /* uptime index */
        uint64_t        wt_jid;         /* job id */
        int             wt_njob;        /* # jobs for this request */
        struct  wm_track *wt_next;      /* next pointer */
};

/*
 *      Initial value for variable
 */
#define DEFAULT_QWTIME          0

/*
 *      Definitions to flag NQS/workload management records belonging to the
 *      "normal" part of an NQS/workload management job and those belonging
 *      to the routing and spooling portions of the job.
 */
#define NORM_REQ        0
#define AUX_REQ         1       /* NQ_SENT, NQ_SPOOL, WM_SPOOL */

#ifdef DEBUG_ADDRS
/*
 *	For debugging.
 */
#define	INITADDRS()						\
{								\
	long	*DBwp;						\
	int	DBindex;					\
	DBwp = (long *)0;					\
	for(DBindex = 0; DBindex < 100; DBindex++) {		\
		zeroword[DBindex] = DBwp[DBindex];		\
	}							\
}
#define	CHECKADDRS(x)						\
{								\
	long	*DBwp;						\
	int	DBindex;					\
	DBwp = (long *)0;					\
	for(DBindex = 0; DBindex < 100; DBindex++) {		\
		if (zeroword[DBindex] != DBwp[DBindex] ) {	\
			Ndebug("%d Zeroword[%d] changed is 0%o was 0%o\n", \
				x, DBindex, DBwp[DBindex], zeroword[DBindex]); \
		}						\
	}							\
}
#else
  #define INITADDRS()	
  #define CHECKADDRS(x)	
#endif
