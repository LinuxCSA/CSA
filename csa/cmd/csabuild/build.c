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
 *	csabuild - Program to create sorted pacct file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "csaacct.h"

#include <ctype.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csabuild.h"
#include "extern_build.h"

char	acctentbuf[ACCTENTSIZ];		/* accounting record buffer */
struct acctent	acctent = {acctentbuf};

struct segment 		**hashtbl;

int	Total_crecs = 0;	/* Total core records this pass */
int	Total_frecs = 0;	/* Total file records this pass */
int	mark;			/* file table entry index */
int64_t	Pacct_offset;		/* offset into spacct file */

int	upind = 0;		/* Current uptime table index */
int	Last_upTime = 0;	/* last in use uptime value. */
int	File_Index = 0;		/* Pacct file index */

extern	int	File_index;	/* Index of last open_file request */

static int Build_file();
static void initialize_segment(struct segment *, uint64_t);

/*
 * =========================== Build_tables() ==========================
 *	Build tables - Build up internal structure so we can write a
 *		ordered s-rec file.
 */
void
Build_tables() 
{
	/*
	 *	Process the pacct files.
	 */
	if (db_flag > 0) {
		Ndebug("Build_tables(1): called.\n");
	}
	Build_file();

	/*
	 * Now add the workload management records to segments 
	 */
	add_segs_wm();

	/*
	 *      pacct files may reunite some jobs 
	 */
	if ( !n_opt) {
		Condense_wm();
	}

	return;
}

/*
 * ============================ Build_segment_hash_tbl() ====================
 *	Build the hash table of segment pointers for each up period
 */
void
Build_segment_hash_tbl(int index)
{
	int	tblsize = 0;

	tblsize = HASH_TBL_LEN * sizeof(struct segment *);
	if (db_flag > 0) {
		Ndebug("Build_segment_hash_tbl(1) called, index(%d).\n", index);
		Ndebug("Build_segment_hash_tbl(1) table size is (%d).\n",
			tblsize);
	}

	if ((hashtbl = (struct segment **)malloc(tblsize)) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"segment hash table");
	}
	memset(hashtbl, 0, tblsize);

	uptime[index].up_seg = hashtbl;
	return;
}


/*
 * =========================== Build_segment() ============================
 */
int
Build_segment(int fd)
{
	int	nread = 0;	/* number of records in file */
	int	rcnt;		/* record size */
	int	stt;		/* return status */
	int	type;		/* prime record type */

	Pacct_offset = 0;
	if (db_flag > 0) {
		Ndebug("Build_segment(1): called.\n");
	}
	/*
	 *   Process the pacct files, we must be able to process both a 
	 *   sorted pacct file (pacct0) and regular pacct files (pacct1-n).
	 */
	while(TRUE) {
		if (readacctent(fd, &acctent, FORWARD) <= 0) {
			if (!nread) {
				fprintf(stderr,
				      _("%s: CAUTION: Empty pacct file found.\n"),
					Prg_name);
			} 
			break;
		}

		nread++;
		type = acctent.prime->ah_type;
		rcnt = acctent.prime->ah_size;
		Last_upTime = upind;
		stt = RC_GOOD;

		if (db_flag > 3) {
			Ndebug("Build_segment(4): Record(%d), offset(%d, %#o):",
				nread, Pacct_offset, Pacct_offset);
			Dump_acct_hdr(acctent.prime);
		}

		switch(type) {

		case (ACCT_KERNEL_CSA):
			rcnt = acctent.csa->ac_hdr1.ah_size +
				acctent.csa->ac_hdr2.ah_size;
			stt = process_eop(fd, File_Index, mark);
			break;

		case (ACCT_KERNEL_CFG):
			stt = process_special(fd, File_Index, mark);
			break;

		case (ACCT_KERNEL_EOJ):
			rcnt = acctent.eoj->ac_hdr1.ah_size +
				acctent.eoj->ac_hdr2.ah_size;
			stt = process_special(fd, File_Index, mark);
			break;

		case (ACCT_KERNEL_SOJ):
			stt = process_special(fd, File_Index, mark);
			break;

		case (ACCT_DAEMON_WKMG):
			stt = process_wm(fd, File_Index);
			break;

		default:
			acct_err(ACCT_WARN,
			       _("Bad daemon identifier (%d) in header field."),
				type);
		}  /* end of switch(type) */

	 	/*  Check the processing routine's reply. */
		if ((stt == RC_BAD) || (stt == RC_CFG) ) {
			Pacct_offset += rcnt;
			continue;

		} else if (stt == RC_NOJOB) {
			if (i_opt) {
				acct_err(ACCT_INFO,
				       _("No Job ID was found for a record from file '%s%d' near offset %lld."),
					pacct, File_Index, Pacct_offset);
				acct_err(ACCT_INFO,
				       _("The record was ignored.")
					);
				if (db_flag > 2) {
					Ndebug("Uptime reset to %d\n",
						Last_upTime);
				}
				upind = Last_upTime;
				continue;

			} else {
				acct_err(ACCT_INFO,
				       _("No Job ID was found for a record from file '%s%d' near offset %lld."),
					pacct, File_Index, Pacct_offset);
			}

		}

		/*  Update the file position. */
		Pacct_offset += rcnt;

	}  /* while(TRUE) */

	return(rcnt);
}


/*
 * =========================== Build_file() ============================
 */
static	int
Build_file()
{
	int 	fd;		/* file desc */
	int	lastfile;	/* last fileindex opened sucessfully */

	Total_crecs = Total_frecs = 0;

	/*
	 *	Open first account file if it does not exist no problem.
	 *	UNLESS it is pacct then exit.....
	 */
	if (db_flag > 0) {
		Ndebug("Build_file(1): called.\n");
	}
	/*	This 'for' loop is necessary for the case where the file with
	 *	suffix 0 cannot be opened, i.e., doesn't exist.  The 'for'
	 *	loop makes sure that we do try to open the file with suffix 1.
	 */
	Last_upTime = upind = 0;
	for(lastfile = File_Index = 0; File_Index < 2; File_Index++) {
		while((fd = get_next_file(pacct, File_Index, F_OPEN)) >= 0) {
			/*	Save info about this file. */
			lastfile = File_Index;
			mark = File_index;
			Build_segment(fd);

			/*	Move to next file if it exists. */
			get_next_file(pacct, File_Index, F_CLOSED);
			File_Index++;
		} 
	} 

	/*  Did we get any of these files open? */
	if (lastfile == 0) {
		char		string[40];
		if (db_flag > 0) {
			Ndebug("Build_file(1): Could not open first "
				"'%s' file.\n", pacct);
		}

		sprintf(string, "%s%d", pacct, lastfile+1);
		acct_err(ACCT_ABORT,
		       _("Could not open the first pacct file '%s'."),
			string);
	}

	/*  Did we get to the last uptime?  */
	if (uptime[upind+1].up_stop != 0) {
		if (db_flag > 0) {
			Ndebug("Build_file(1): Ran out of %s files before "
				"out of uptimes(%d).\n", pacct, upind);
		}
	}

	if (db_flag > 0) {
		Ndebug("Build_file(1): Total file records this pass = %d.\n",
			Total_frecs);
	}

	return(0);
}


/*
 * ============================ get_crec() ===========================
 *	Get core record and return pointer to it.
 */
struct	Crec *
get_crec(struct wkmgmtbs *wm, uint type)
{
	struct Crec *cptr;

	if (db_flag > 0) {
		Ndebug("get_crec(1) called\n");
	}
	if ((cptr = (struct Crec *)malloc(sizeof(struct Crec))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"Core-records");
	}
	cptr->cp_type = type;
	cptr->cp_wm   = wm;
	cptr->cp_next = NULL;

	return(cptr);
}



/*
 * ============================ get_frec() ===========================
 *	Get File record and return pointer to it.
 */
struct	Frec *
get_frec(off_t off, uint fi, uint type)
{
	struct Frec *fptr;

	if (db_flag > 0) {
		Ndebug("get_frec(1) called\n");
	}
	if ((fptr = (struct Frec *)malloc(sizeof(struct Frec))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"File-records");
	}
	fptr->fp_ind  = fi;
	fptr->fp_type = type;
	fptr->fp_offset = off;
	fptr->fp_next = NULL;

	return(fptr);
}


/*
 * ============================ get_segptr() ===============================
 *	Return a pointer to the correct segment structure.
 *
 *	REC_EOP records are always done first, since they are used to
 *	determine the start and stop times of the segments.  A segment's
 *	start time is the time stamp from the SOJ  record.
 *	The stop time is the time stamp from the EOJ record.
 *
 *	The time stamp of Workload Mgmt must fall within
 *	the start and stop time of the segment.  Since the stop time of a
 *	segment may be the same as the start time of the next segment,
 *	we cannot rely solely on the time stamp when processing NQS
 *	records.  In this case, we must be sure that a segment has at
 *	most 1 NQS or Workload Mgmt record associated with it.
 *
 */
struct segment *
get_segptr(int rec, int uind, uint64_t jid, time_t btime, time_t etime,
	   time_t stime)
{
	struct segment	*sp;	/* final pointer to active segment */
	struct segment	*osp;
	struct segment  *orig_sp;
	struct segment **htable;
	int hindex;

	/*
	 *  Sanity check on jid value; due to the composition of the
	 *  jid on IRIX, checking for a maxjid value isn't meaningful.
	 */
	if ((jid < 0) &&
	    !((rec == REC_WKMG) && (jid == WM_NO_JID))) {
		acct_err(ACCT_INFO,
		       _("A Job ID (0x%llx) less than zero was found for a record type %d."),
			jid, rec);
		return(NOJOBFOUND);
	}

	/*  legal uptime index?  */
	if (uind > upIndex) {
		if (db_flag > 1) {
			Ndebug("get_segptr(2): uind(%d), upIndex(%d), "
				"time(%d - %24.24s).\n",
				uind, upIndex, btime, ctime(&btime));
		}
		if (i_opt) {
			acct_err(ACCT_INFO,
			       _("An invalid time stamp value was found in record type (%d)."),
				rec);
			acct_err(ACCT_INFO, _("The record was ignored."));
			return(NOJOBFOUND);

		} else {
			acct_err(ACCT_ABORT,
			       _("An invalid time stamp value was found in record type (%d)."),
				rec);
		}
	}

	/*  Did we pass all the uptimes already? */
	if (uptime[uind].up_stop == 0) {
		acct_err(ACCT_INFO,
		       _("All of the boot times have been used.")
			);
		return(NOJOBFOUND);
	}

	if (Last_upTime != uind) {
		if (db_flag > 2) {
			Ndebug("get_segptr(3): moving to uptime %d.\n", uind);
		}
		Last_upTime = uind;
	}


	/*
	 *  If this is a workload management job with job ID == WM_NO_JID,
	 *  then this is the only record of its job.  Add it to the up_wm
	 *  list.
	 */
	if ((rec == REC_WKMG) && (jid == WM_NO_JID)) {
		if (db_flag > 1) {
			Ndebug("WM_NO_JID(%lld) record found, btime %s",
				jid, ctime(&btime));
		}
		osp = uptime[uind].up_wm;
		if ((sp = (struct segment *)malloc(sizeof(struct segment))) ==
				NULL) {
			acct_perr(ACCT_ABORT, errno,
			  _("There was insufficient memory available when allocating '%s'."),
			    "WM_NO_JID record");
		}

		/* 
		 * the up_wm list is independent of the hash table list of
		 * segment pointers.
		 */
		initialize_segment(sp, jid);

		if (osp == NULL) {
			uptime[uind].up_wm = sp;

		} else {
			for (; osp->sg_ptr != NULL; osp = osp->sg_ptr);
			osp->sg_ptr = sp;
			
		}

		return(sp);
	}

	/* Get a pointer to the initial segment structure for this job. */
	htable = uptime[uind].up_seg;
	hindex = HASH_INDEX(jid);
	if((sp = seg_hashtable(htable, hindex, jid, uind)) ==
	   (struct segment *)NULL ) {
		/* shouldn't happen; exit */
		acct_err(ACCT_ABORT,
		       _("No segment was returned from the hash table in the '%s' routine."),
			"seg_hashtable()");
	}
	
	orig_sp = sp;

	if ((rec == REC_EOP) || (rec == REC_SOJ) || (rec == REC_EOJ)) {
		/*
		 *	these records always belong to last segment 
		 */
		while((sp->sg_ptr != NULL) && (sp->sg_ptr != BADPOINTER) ) {
			sp = sp->sg_ptr;
		}

		/*
		 *  This segment's job is already done (it has an EOJ
		 *  record).  Add another segment.
		 */
		if (sp->sg_ptr == BADPOINTER) {
			if (etime) {
				acct_err(ACCT_INFO,
				       _("A pacct EOJ record was found with no other records.")
					);
				return(NOJOBFOUND);
			}
			if (db_flag > 1) {
				Ndebug("Job id %lld wrapped (pacct rec)\n",
				       jid);
			}

			osp = sp;
			if ((sp = (struct segment *)
				malloc(sizeof(struct segment))) == NULL) {
				acct_perr(ACCT_ABORT, errno,
					_("There was insufficient memory available when allocating '%s'."),
					"wrapped job");
			}
			initialize_segment(sp, jid);
			sp->sg_start = stime;

			osp->sg_ptr = sp;

			return(sp);
		}

		/*
		 *  If this segment's job is still accumulating records
		 *  and this is an eoj record, finish off this seg.
		 */
		if ((sp->sg_ptr == NULL) && etime) {
			sp->sg_stop = etime;
			sp->sg_ptr = BADPOINTER;

			return(sp);
		}

		/*
		 *  Regular pacct record.  Is the start time earlier than
		 *  those seen so far?
		 */
		if ((stime < sp->sg_start) || (sp->sg_start == 0) ) {
			sp->sg_start = stime;
		}

		return(sp);

	} else if ((etime) && (rec != REC_EOJ) ) {
		acct_err(ACCT_CAUT,
		       _("The end time (etime) was set on a non-pacct call in the '%s' routine."),
			"get_segptr()");
	}

	/*
	 *  Other record types may go in any entry on chain.
	 *  Search existing entries.
	 */
	while((sp != NULL) && (sp != BADPOINTER)) {
		/*  If currently empty, this is good enough.  */
		if ((sp->sg_start == 0) && (sp->sg_stop == 0) ) {
			return(sp);
		}

		/*
		 *	If time falls within the start and stop
		 *	time of this segment, then this is the right
		 *	segment, unless this is a workload mgmt record.
		 *	In that case, if there is already an workload
		 *	mgmt record associated with the segment (seqno != -1),
		 *	then keep looking.
		 */
		if ((rec == REC_WKMG) && (sp->sg_reqid != -1)) {
			sp = sp->sg_ptr;
			continue;
		}

		if (((sp->sg_start != 0) && (btime >= sp->sg_start)) &&
			((sp->sg_stop != 0) && (btime <= sp->sg_stop)) ) {
			return(sp);
		}

		if ((sp->sg_start == 0) && (btime <= sp->sg_stop) ) {
			return(sp);
		}

		/*
		 *	This job has not ended and we have not yet found
		 *	another correct job, so this must be it.
		 */
		if (sp->sg_stop == 0) {
			if (sp->sg_ptr != NULL) {
				acct_err(ACCT_CAUT,
				       _("A non-null segment pointer was found on a job in the '%s' routine."),
					"get_segptr()");
			}

			/* set start if appropriate */
			if ((sp->sg_start == 0) ||
					(stime && (sp->sg_start > stime)) ) {
				sp->sg_start = stime;
			}

			return(sp);
		}

		/*
		 * Try the next segment.
		 */
		sp = sp->sg_ptr;
	}  /* end of while(sp) */

	/*
	 *  We did not find a job which fit, so add another job to the
	 *  segment (this is the JIDWRAP case)
	 */
	if (db_flag > 1) {
		Ndebug("get_segptr(2): adding new segment for jid(0x%llx), "
			"type(%d).\n", jid, rec);
	}
	
	sp = orig_sp;
	if (db_flag > 3) {
		Dump_segment_rec(sp, hindex, uind);
	}

	/*  Set sp to last segment in job. */
	while((sp->sg_ptr != NULL) && (sp->sg_ptr != BADPOINTER) ) {
		sp = sp->sg_ptr;
	}

	osp = sp;
	if ((sp = (struct segment *)malloc(sizeof(struct segment))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"wrapped job");
	}
	initialize_segment(sp, jid);

	osp->sg_ptr = sp;

	return(sp);
}

/*
 * =============================== Shift_uptime() ===========================
 *	Shift uptime table once to allow for records from before first uptime.
 *	Also update the upind field in the tr_wm[] arrays.
 */
void
Shift_uptime(int tp, char *fname, int findex)
{
	static	int	Uptime_shifted = 0;
	time_t	abc;
	struct  wm_track  *wt;
	char	*name;
	int	ind;

	if (db_flag > 0) {
		Ndebug("Shift_uptime(1) called, tp(%d), name(%s), index(%d)\n",
			tp, fname, findex);
	}
	if (Uptime_shifted) {
		acct_err(ACCT_ABORT,
		       _("Another record with an earlier than expected time stamp\n   was found in file '%s%d'."),
			fname, findex);
	}
	Uptime_shifted = 1;

	switch(tp) {

	case REC_CFG:
		name = "config";
		break;

	case REC_EOP:
		name = "EoP";
		break;

	case REC_SOJ:
		name = "SoJ";
		break;

	case REC_EOJ:
		name = "EoJ";
		break;

	case REC_JOB:
		name = "Job";
		break;

	case REC_WKMG:
		name = "Wkmg";
		break;

	default:
		acct_err(ACCT_CAUT,
		       _("An unknown record type (%d) was found in file '%s%d'\n   in the '%s' routine."),
			tp, fname, findex,
			"Shift_uptime()");
	}		/* end of switch(tp) */

	if (db_flag > 0) {
		Ndebug("Shift_uptime(1): Record type '%d - %s' discovered with "
			"time stamp before first system boot time\n"
			"\tin file '%s%d'\n", tp, name, fname, findex);
	}
	if (db_flag > 5) {
		Dump_uptime_tbl("Old uptime[] table");
	}
	if (db_flag > 3) {
		Ndebug("Shift_uptime(4): Backing up to new first uptime.\n");
	}

	/*  Shift uptimes forward.  */
	for(ind = Max_upIndex; ind > 0; ind--) {
		uptime[ind].up_start = uptime[ind-1].up_start;
		uptime[ind].up_stop  = uptime[ind-1].up_stop;
		uptime[ind].up_size  = uptime[ind-1].up_size;
		uptime[ind].up_seg   = uptime[ind-1].up_seg;
		uptime[ind].up_wm    = uptime[ind-1].up_wm;
		uptime[ind].up_jidhead = uptime[ind-1].up_jidhead;
		uptime[ind].up_jidlast = uptime[ind-1].up_jidlast;
	}

	/*  Fill in first uptime.  */
	uptime[0].up_start = time(&abc) - A_YEAR; 
	uptime[0].up_stop  = uptime[1].up_start - 1;
	upIndex++;
	upind = 0;
	Build_segment_hash_tbl(upind);

	if (db_flag > 5) {
		Dump_uptime_tbl("Shifted uptime[] table");
	}

	/*  Update workload management indexes.  */
	for(wt = tr_wm; wt != NULL; wt = wt->wt_next) {
		wt->wt_upind = wt->wt_upind + 1;
	}

	return;
}

/*
 *	Given a jid, find the appropriate segment or malloc one
 */
struct segment *
seg_hashtable(struct segment **htable, int hindex, uint64_t jid, int uind)
{
	struct segment *sp;
	struct segment *newseg;
	struct segment *segchain;

	if (db_flag > 0) {
		Ndebug("seg_hashtable(1) called, hindex(%d) jid(0x%x)"
			" uind(%d)\n",
			hindex, jid, uind);
	}
	sp = htable[hindex];
	if (sp == NULL) {
		if ((newseg = (struct segment *)malloc(sizeof(struct segment)))
				== (struct segment *)NULL) {
			/* this exits */
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when allocating '%s'."),
				"segment");
		}
		initialize_segment(newseg, jid);
		uptime[uind].up_size++;
		if (uptime[uind].up_jidhead == NULL) {
			uptime[uind].up_jidhead = newseg;
			uptime[uind].up_jidlast = newseg;
		} else {
			(uptime[uind].up_jidlast)->sg_jidlistnext = newseg;
			uptime[uind].up_jidlast = newseg;
		}
		htable[hindex] = newseg;
		return (newseg);
	} else {
		/* add to hash slot chain and jid chain if not already there */
		segchain = htable[hindex];
		for (sp = segchain; sp; sp = sp->sg_hashnext) {		
			if (sp->sg_jid == jid) {
				/* found it */
				return (sp);
			}
			if (sp->sg_hashnext == NULL) {
				/* at the end; add it to hash index chain */
				if ((newseg = (struct segment *)malloc(
				     (sizeof(struct segment)))) ==
				     (struct segment *)NULL) {
					/* this exits */
					acct_perr(ACCT_ABORT, errno,
						_("There was insufficient memory available when allocating '%s'."),
						"segment");
				}
				initialize_segment(newseg, jid);
				sp->sg_hashnext = newseg;
				(uptime[uind].up_jidlast)->sg_jidlistnext =
					newseg;
				uptime[uind].up_jidlast = newseg;
				uptime[uind].up_size++;
				return (newseg);
			}	
		}
	}
	/* shouldn't get here */
	return (struct segment *)NULL;
}

void
initialize_segment(struct segment *newseg, uint64_t jid)
{
	if (db_flag > 0) {
		Ndebug("initialize_segment(1) called, jid(0x%x)\n", jid);
	}
	memset(newseg, 0, sizeof(struct segment));
	newseg->sg_jid = jid;
	newseg->sg_seqno = -1;
	newseg->sg_mid = -1;
	newseg->sg_reqid = -1;
	newseg->sg_arrayid = -1;
	newseg->sg_lproc = &(newseg->sg_proc);
	newseg->sg_lwm = &(newseg->sg_wm);	
}
