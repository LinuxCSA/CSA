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

/*
 *	NQS table entry
 */
struct nqsja {
	char		reqname[16];	/* Request name */
	short		type;		/* Type of record */
	short		subtype;	/* Subtype of record */
	time_t		time;		/* Time of record */
	uint64_t	ash;		/* Array Session ID */
	uint64_t	jid;		/* Job ID */
	struct nqsja	*nxt;		/* Next structure */
};

struct nqshdr {
	struct nqsja	*head;
	struct nqsja	*last;
};

/*
 *	Data Migration table entry
 */
struct dmja {
	uint32_t	uid;		/* user ID */
	uint32_t	gid;		/* group ID */
	uint64_t	ash;		/* Array Session ID */
	uint64_t	jid;		/* job ID */
	uint64_t	prid;		/* project ID */
	int		request;	/* request type */
	char		filesys[32];	/* filesystem name */
	int		inode;		/* inode number */
	int64_t		xferred;	/* bytes transferred */
	time_t		stime;		/* start time */
	int64_t		runtime;	/* runtime (rtclocks) */
	int64_t		usertime;	/* user cpu time - clocks */
	int64_t		systime;	/* system cpu time - clocks */
	struct dmja	*nxt;		/* next structure */
};

struct dmhdr {
	struct dmja	*head;
	struct dmja	*last;
};

struct dmtotal {
	int		request;	/* request type */
	struct	dmbs	*tot;		/* totals */
};

/*
 *	Tape definitions
 */
#define	NDEVNAME	16	/* max # char in device group name */
/*
 *	Individual tape/cartridge info
 */
struct tpja {
	uint32_t	uid;		/* user ID */
	uint64_t	prid;		/* project ID */
	uint64_t	ash;		/* Array Session ID */
	uint64_t	jid;		/* job ID */
	time_t		btime;		/* time stamp */
	int64_t		systime;	/* daemon system cpu time */
	int64_t		usertime;	/* daemon user cpu time */
	char		vsn[8];		/* internal vsn */
	int64_t		nread;		/* bytes read */
	int64_t		nwrite;		/* bytes written */
	struct tpja	*nxt;		/* next structure */
};

struct tphdr {
	struct tphdr	*nxt;
	char		devname[NDEVNAME]; /* device group name */
	struct tpja	*last;
	struct tpja	*head;
};

/*
 *	Device reservation info
 */
struct tprsv {
	uint32_t	uid;		/* user ID */
	uint64_t	prid;		/* project ID */
	uint64_t	ash;		/* Array Session ID */
	uint64_t	jid;		/* job ID */
	time_t		btime;		/* time stamp */
	int64_t		rtime;		/* reservation time per device?? */
	struct tprsv	*nxt;		/* next structure */
};

struct rsvhdr {
	struct rsvhdr	*nxt;
	char		devname[NDEVNAME]; /* device group name */
	struct tprsv	*last;		
	struct tprsv	*head;
};

/*
 *	Tape list head
 */
struct tplist {
	struct tphdr	*bs;
	struct rsvhdr	*rsv;
};
