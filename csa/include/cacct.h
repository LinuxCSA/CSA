/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2004-2007 Silicon Graphics, Inc All Rights Reserved.
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

#ifndef	_CACCT_H
#define	_CACCT_H

#define CACCT_NO_ID	-2	/* Missing uid, jid, prid, or gid */


struct cumuAcctInfo {
	double		cpuTime;		/* Cpu time, in secs */
	int64_t		charsXfr;		/* Chars xfered, bytes */
	int64_t		logicalIoReqs;		/* Logical I/O requests */
	int64_t		minorFaultCount;	/* Number of minor faults */
	int64_t		majorFaultCount;	/* Number of major faults */
	double		kcoreTime;		/* Kcore-minutes, in (kbytes * min) */
	double		kvirtualTime;		/* Kvirtual-minutes */
	double		cpuDelayTime;		/* CPU delay, secs */
	double		blkDelayTime;		/* Block I/O  delay, secs */
	double		swpDelayTime;		/* Swap in delay, secs */
};


/*
 * Condensed accounting record for a day/accounting period
 */

struct cacct
{
	struct achead	ca_hdr;		/* Header */
	
	/* Identification */
	uint32_t	ca_uid;		/* User ID */
	uint32_t	ca_gid;		/* Group ID */
	uint64_t	ca_prid;	/* Project ID */
	uint64_t	ca_jid;		/* Job ID */
	uint64_t	ca_ash;		/* Array session handle - added for */
					/* future consolidating by ash */
	 /* Job info */
	int64_t		ca_njobs;	/* Total number of jobs */

	/*
	 * pacct file info, prime/nonprime
	 */
	struct cumuAcctInfo	prime; /* prime time */
	struct cumuAcctInfo	nonprime; /* non prime time */

	double		ca_du;		/* Cum. disk usage count*/
	int		ca_dc;		/* disk count */

	/* Process info */
	int64_t		ca_pc;		/* process count */
	
	/* Billing info */
	double		ca_sbu;		/* System Billing Units */
	double 		ca_fee;		/* Fee charged */
	
	/* Tape info by device group */
	struct
	{
		int	cat_mounts;	/* Mounts on this device */
		int64_t	cat_reads;	/* Bytes read */
		int64_t	cat_writes;	/* Bytes written */
		int64_t	cat_reserv;	/* Reserve time [usec] */
		int64_t	cat_utime;	/* User cpu time [usec] */
		int64_t	cat_stime;	/* System cpu time [usec] */
		char	cat_devname[16]; /* Device group name */
	} ca_tpio[TP_MAXDEVGRPS];
	
	/* Workload management info */
	int64_t		can_nreq;	/* Number of Workload mgmt reqs */
	int64_t		can_utime;	/* Shepherd user cpu time [usec] */
	int64_t		can_stime;	/* Shepherd system cpu time [usec] */
};

#endif	/* !_CACCT_H */
