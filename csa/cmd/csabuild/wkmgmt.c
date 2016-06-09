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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include "csaacct.h"

#include <grp.h>
#include <malloc.h>
#include <memory.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csabuild.h"
#include "extern_build.h"

/*
 *	wkmgmt.c:
 *		Routines to preprocess the workload management accounting
 *		data.  One record is written for each segment of a request.
 *		Requests are uniquely identified by their request ID/array ID
 *		pair.  Array ID is used by LSF.
 *
 *		This code is pretty much a copy of the csabuild/nqs.c file.
 *
 *		Currently, LSF is the only workload management product that
 *		we are dealing with.  And currently, LSF only generates two
 *		types of accounting records:  WM_INIT and WM_TERM.
 *
 *		For simplicity and portability, the "infrastructure" (data
 *		structures and functionality of the code) is not modified
 *		even though some of it does not apply to LSF.  The code to
 *		process the WM_RECV and WM_SPOOL records remains pretty much
 *		untouched since LSF does not generate those.  In the future,
 *		if LSF generates more record types or if we have other
 *		workload management products, this file will need to be
 *		revisited.
 */

/*
 *	Initial value for variable
 */
#define WM_NO_LTYPE	-1

/*
 *	Structures defining the linked list.
 *	Information is kept by request ID and array ID for all the RECV/INIT
 *	to TERM periods.  There is a separate wkmgmtbs node for each such
 *	period.
 */
/*
 *	Segments merely point to a request's wkmgmtbs list.
 */
struct	wmseg {
	int		upind;		/* uptime index for this record */
	int		findex;		/* pacct file index for this rec */
	int64_t		pacct_offset;	/* pacct offset for this record */
	struct	wkmgmtbs *riptr;	/* wkmgmtbs(c) nodes for this reqid */
	struct	wmseg	*nptr;		/* next node for this reqid */
};

/*
 *	Structures containing running info about each workload management
 *	request.  There is 1 structure per requestID.arrayID pair.
 */
struct	wmreq {
	int64_t		reqid;		/* request ID */
	int		arrayid;	/* array ID - for LSF */
	time_t		stime;		/* RECV or INIT time stamp */
	short		ltype;		/* previous rec type */
	short		lsubtype;	/* previous rec subtype */
	struct	wmseg	*sfirst;	/* 1st workload management segment */
	struct	wmseg	*slast;		/* last workload management segment */
	struct	wmreq 	*bptr;		/* previous workload mgnt request */
	struct	wmseg	*ppfirst;	/* 1st spooling segment */
	struct	wmseg	*pplast;	/* last spooling segment */
	struct	wmreq	*nptr;		/* next workload management request */
};

/*
 *	Pointers to the request list
 */
struct req_head {
	struct	wmreq	*first;		/* 1st node in the list */
	struct	wmreq	*last;		/* last node in the list */
};

/*
 *	Global variables
 */
static int	nout_rec = 0;		/* # output recs (riptr nodes) */
static int	cur_findex = 0;
static struct	req_head  *rhead;	/* pointer to the request list */
struct	wm_track *tr_wm;	/* global wkld mgnt request tracking struct */

static char *Type_statname[] = {	/* names for WM_type */
	"Unknown",	/* WM_NONE = -2 --> 0 */
	"Info",		/* WM_INFO = 1 */
	"Recv",		/* WM_RECV = 2 */
	"Init",		/* WM_INIT = 3 */
	"Spool",	/* WM_SPOOL = 4 */
	"Term",		/* WM_TERM = 5 */
	"Con"		/* WM_CON = 6 */
};

static char *Info_statname[] = {	/* names for WM_INFO */
	"Unknown",	/* WM__NO_xxxx = -2 --> 0 */
	"AcctOn",	/* WM_INFO_ACCTON = 1 */
	"AcctOff"	/* WM_INFO_ACCTOFF = 2 */
};

static char *Init_statname[] = {	/* names for WM_INIT */
	"Unknown",	/* WM_NO_INIT = -1 --> 0 */
	"Start",	/* WM_INIT_START = 1 */
	"Restart",	/* WM_INIT_RESTART = 2 */
	"Rerun"		/* WM_INIT_RERUN = 3 */
};

static char *Recv_statname[] = {	/* names for WM_RECV */
	"Unknown",	/* WM_NO_xxxx = -2 --> 0 */
	"New"		/* WM_RECV_NEW = 1 */
};

static char *Term_statname[] = {	/* names for WM_TERM */
	"Unknown",	/* WM_NO_TERM = -2 --> 0 */
	"Exit",		/* WM_TERM_EXIT = 1 */
	"Requeue",	/* WM_TERM_REQUEUE = 2 */
	"Hold",		/* WM_TERM_HOLD = 3 */
	"Rerun",	/* WM_TERM_RERUN = 4 */
	"Migrate"	/* WM_TERM_MIGRATE = 5 */
};

/*
 *	Routine prototypes.
 */
static	void	Consolidate_wm(struct segment *, int64_t, int, int);
static	void	dump_wm_tbl();
static	struct	wmreq	*find_request(int64_t, int);
static	void	init_wkmgmtbs(struct wkmgmtbs *, struct wkmgmtbs *);
static  int	jidcmp(const void *, const void *);
static	void	make_request(int);
static	struct	wmseg	*make_segment(struct wmreq *, int);
static	void	wm_con(struct wkmgmtbs *);
static	void	wm_init(struct wkmgmtbs *);
static	void	wm_recv(struct wkmgmtbs *);
static	void	wm_spool(struct wkmgmtbs *);
static	void	wm_term(struct wkmgmtbs *);
static	void	prc_wm(struct wkmgmtbs *);
static	int	reqcmp(const void *, const void *);
static	void	trackwm(struct wkmgmtbs *, int);


/*
 *	process_wm() - process workload management daemon record.
 */
int process_wm(int fd, int findex)
{
	static	int	tso_flag = 0;
	static	time_t	lasttime = 0;
	time_t	btime;

/*
 *	Check the record time information against the uptime info.
 */
	btime = acctent.wmbs->time;
	if (lasttime) {
		if (lasttime > btime) {
			/*
			 *	If time stamps are out of order, keep on going.
			 *	Assume that the records belong in the current
			 *	or later up periods.
			 */
			if (!tso_flag) {
				tso_flag++;
				acct_err(ACCT_WARN,
				  _("The %s records were out of time order in file '%s%d'\n   near offset %lld due to a date change or an invalid file."),
					 "workload management", pacct,
					 findex, Pacct_offset);
			}
		}
	} else {
		if (btime < uptime[0].up_start) {
		    if (btime < EARLY_FUDGE) {
			char btime_str[26];
			time_t	time = EARLY_FUDGE;

			strcpy(btime_str, ctime(&btime));
			Nerror("REC_WKMG: EARLY_FUDGE error (%d - %24.24s) ",
			       btime, btime_str);
			Nerror("< (%d - %24.24s)\n", time, ctime(&time));

			acct_err(ACCT_WARN,
				_("An earlier than expected time stamp was discovered in the file '%s%d' near offset (%lld).  Examine the data set."),
				pacct, findex, Pacct_offset);
			if (i_opt) {
				acct_err(ACCT_INFO,
					_("The record was ignored."));
				if (db_flag > 2) {
					Dump_rec(acctent.wmbs,
						 sizeof(struct wkmgmtbs));
				}
				return(RC_BAD);
			}
		    }
		    if (upind != 0) {
				if (db_flag > 2) {
					Dump_rec(acctent.wmbs,
						 sizeof(struct wkmgmtbs));
				}
				acct_err(ACCT_ABORT,
				  _("An earlier than expected time stamp was discovered in the file '%s%d' near offset (%lld).  Examine the data set."),
					 pacct, findex, Pacct_offset);
		    } else {
				Shift_uptime(REC_WKMG, pacct, findex);
		    }
		}
	}

	lasttime = btime;
	while (uptime[upind].up_stop < btime) {
		if (db_flag > 3) {
			Ndebug("process_wm(4): Past stop time(%d) going to "
			       "next workload management uptime(%d) - %s%d.\n",
			       uptime[upind].up_stop, upind+1, pacct, findex);
		}
		upind++;
		if (upind > upIndex) {
			break;
		}
	}
	cur_findex = findex;

/*
 *	Process the workload management records - consolidate all associated
 *	data.  Since there aren't valid jids in some of the workload
 *	management daemon records, don't attach the consolidated record to a
 *	segment now.  This will be done later by a call to add_segs_wm() after
 *	all of the pacct records have been processed.  The workload management
 *	consolidated records at that time have accurate information.
 */
	prc_wm(acctent.wmbs);

	return(RC_GOOD);
}


/*
 *	print_wm_rec - print raw workload management requests.
 */
static void print_wm_rec(struct wkmgmtbs *wbuf)
{
	time_t	stime;

	Ndebug("   WKMG_REC: type(%d), uid(%d-%s), jid(0x%llx).\n",
	       wbuf->type, wbuf->uid, uid_to_name(wbuf->uid), wbuf->jid);

	Ndebug("   WKMG_REC: ash(0x%llx), prid(%lld-%s).\n",
	       wbuf->ash, wbuf->prid, prid_to_name(wbuf->prid));

	stime = wbuf->time;
	Ndebug("   WKMG_REC: reqid(%lld), arrayid(%d), time: %s",
	       wbuf->reqid, wbuf->arrayid, ctime(&stime));

	switch(wbuf->type) {

	case WM_INFO:
	{
		int	subtype = wbuf->subtype < 0 ? 0 : wbuf->subtype;

		Ndebug("   WM_INFO: subtype(%d - %s).\n", subtype,
		       Info_statname[subtype]);
	}
	break;

	case WM_RECV:
	{
		int	subtype = wbuf->subtype < 0 ? 0 : wbuf->subtype;

		Ndebug("   WM_RECV: subtype(%d - %s).\n", subtype,
		       Recv_statname[subtype]);
		Ndebug("   WM_RECV: machname(%s), reqname(%s), quename(%s), "
		       "qtype(%d)\n", wbuf->machname, wbuf->reqname,
		       wbuf->quename, wbuf->qtype);
	}
	break;

	case WM_INIT:
	{
		int	subtype = wbuf->subtype < 0 ? 0 : wbuf->subtype;

		Ndebug("   WM_INIT: subtype(%d - %s).\n", subtype,
		       Init_statname[subtype]);
		Ndebug("   WM_INIT: machname(%s), reqname(%s), quename(%s), "
		       "qtype(%d)\n", wbuf->machname, wbuf->reqname,
		       wbuf->quename, wbuf->qtype);
	}
	break;

	case WM_TERM:
	{
		int	subtype = wbuf->subtype < 0 ? 0 : wbuf->subtype;

		Ndebug("   WM_TERM: subtype(%d - %s).\n", subtype,
		       Term_statname[subtype]);
		Ndebug("   WM_TERM: machname(%s), reqname(%s), quename(%s), "
		       "qtype(%d)\n", wbuf->machname, wbuf->reqname,
		       wbuf->quename, wbuf->qtype);
		Ndebug("   WM_TERM: code(%lld), utime(%lld), stime(%lld)\n",
		       wbuf->code, wbuf->utime, wbuf->stime);
	}
	break;

	case WM_SPOOL:
	{
		int	subtype = wbuf->subtype < 0 ? 0 : wbuf->subtype;

		if (subtype == WM_SPOOL_INIT) {
			Ndebug("   WM_SPOOL: subtype(%d - %s).\n", subtype,
			       Init_statname[subtype]);
		} else {
			Ndebug("   WM_SPOOL: subtype(%d - %s).\n", subtype,
			       Term_statname[subtype]);
		}
		Ndebug("   WM_SPOOL: machname(%s), reqname(%s), quename(%s), "
		       "qtype(%d)\n", wbuf->machname, wbuf->reqname,
		       wbuf->quename, wbuf->qtype);
	}
	break;

	case WM_CON:
	{
		int	subtype;

		Ndebug("   WM_CON: machname(%s), reqname(%s), quename(%s), "
		       "qtype(%d)\n", wbuf->machname, wbuf->reqname,
		       wbuf->quename, wbuf->qtype);
		Ndebug("   WM_CON: code(%lld), utime(%lld), stime(%lld)\n",
		       wbuf->code, wbuf->utime, wbuf->stime);
		subtype = wbuf->subtype < 0 ? 0 : wbuf->subtype;
		Ndebug("   WM_CON: init_type(%d - %s).\n", wbuf->subtype,
		       Init_statname[subtype]);
		subtype = wbuf->term_subtype < 0 ? 0 : wbuf->term_subtype;
		Ndebug("   WM_CON: term_type(%d - %s).\n", wbuf->term_subtype,
		       Term_statname[subtype]);
		Ndebug("   WM_CON: qwtime(%lld)\n", wbuf->qwtime);
	}
	break;

	default:
		Ndebug(" Unknown workload management request type(%d)\n",
		       wbuf->type);

	}		/* end of switch(type) */

	return;
}


/*
 *	prc_wm - process the raw workload management requests.
 */
static void prc_wm(struct wkmgmtbs *wbuf)
{
	static	int	nrec = 0;

	nrec++;
	if (db_flag > 8) {
		print_wm_rec(wbuf);
	}

	switch(wbuf->type) {

		/*
		 *	Informational record 
		 */
	case WM_INFO:
		if (db_flag > 8) {
			Ndebug("prc_wm(9): INFO.\n");
		}
		break;		/* ignore this record?? */

		/*
		 * 	A request has been received.
		 *	Add a request node to the end of the linked 
		 *	list and then initialize the node.
		 */
	case WM_RECV:
		wm_recv(wbuf);
		break;

		/*
		 *	A request has been initiated.
		 *	Add a new segment node to the end of this 
		 *	request's segment list.
		 */
	case WM_INIT:
		wm_init(wbuf);
		break;

		/*
		 *	A request has been terminated.
		 *	Add the termination code to the wkmgmtbs node.
		 */
	case WM_TERM:
		wm_term(wbuf);
		break;

		/*
		 *	A request's output has been spooled (netclient).
		 *	If necessary, add a request node to the end of
		 *	the linked list and then initialize the node.
		 */
	case WM_SPOOL:
		wm_spool(wbuf);
		break;

		/*
		 *	A recycled consolidated request has been found.
		 *	Save all data in consolidated structure.
		 */
	case WM_CON:
		wm_con(wbuf);
		break;

	default:
		acct_err(ACCT_CAUT,
		  _("An unknown record type (%4o) was found in the '%s' routine."),
		    wbuf->type, "prc_wm()");

	}		/* end of switch(type) */

	if (db_flag > 8) {
		dump_wm_tbl();
	}

	return;
}


/*
 *	Find the request node that matches both the request ID and
 *	array ID by searching backwards through the request list.
 *
 *	Return the pointer to the request node, if found.  Otherwise,
 *	return NULL.
 */
static struct wmreq *find_request(int64_t reqid, int arrayid)
{
	struct	wmreq	*req_ptr;

	req_ptr = rhead->last;
	while (req_ptr != NULL) {
		if ((req_ptr->reqid == reqid) &&
		    (req_ptr->arrayid == arrayid)) {
			return(req_ptr);	/* return request node */

		} else {
			req_ptr = req_ptr->bptr;
		}
	}

	return(NULL);		/* request node not found */
}


/*
 *	Create the head pointer to the request list.
 */
void init_wm()
{
	if ((rhead = (struct req_head *)malloc(sizeof(struct req_head)))
	    == NULL) {
		acct_perr(ACCT_ABORT, errno,
		  _("There was insufficient memory available when allocating '%s'."),
		    "request head");
	}
	rhead->first = rhead->last = NULL;

	return;
}


/*
 *	Initialize a wkmgmtbs(consolidated) node using information from the
 *	current input record if we're sure it's really there.
 */
static void init_wkmgmtbs(struct wkmgmtbs *riptr, struct wkmgmtbs *wbuf)
{
	riptr->start_time = wbuf->time;
	riptr->uid = wbuf->uid;
	if (db_flag > 7) {
		Ndebug("init_wkmgmtbs(8): wbuf->uid(%d), riptr->uid(%d).\n",
		       wbuf->uid, riptr->uid);
	}
	riptr->type	= WM_CON;
	riptr->prid	= wbuf->prid;
	riptr->ash	= wbuf->ash;
	riptr->jid	= wbuf->jid;
	riptr->reqid	= wbuf->reqid;
	riptr->arrayid	= wbuf->arrayid;
	riptr->qtype	= wbuf->qtype;
	riptr->code	= 0;
	riptr->utime	= wbuf->utime;
	riptr->stime	= wbuf->stime;

	riptr->init_subtype = WM_NO_INIT;
	riptr->term_subtype = WM_NO_TERM;

	riptr->qwtime	= DEFAULT_QWTIME;
	riptr->sbu_wt	= 1.0;

	strcpy(riptr->serv_provider, wbuf->serv_provider);
	strcpy(riptr->machname, wbuf->machname);
	strcpy(riptr->quename, wbuf->quename);
	strcpy(riptr->reqname, wbuf->reqname);

	return;
}

/*
 *	Add a request node (and its associated segment and wkmgmtbs(c) nodes)
 *	to the end of the linked list
 */
static void make_request(int typ)
{
	struct	wmreq	*nw_req;

	/*
	 *	Allocate space for the new request node and zero it out
	 */
	if ((nw_req = (struct wmreq *)malloc(sizeof(struct wmreq))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
		  _("There was insufficient memory available when allocating '%s'."),
		    "wmreq node");
	}
	memset((char *)nw_req, '\0', sizeof(struct wmreq));

	/*
	 *	Link the new node to the end of the list
	 */

	nw_req->nptr = NULL;
	if (rhead->last != NULL) {
		rhead->last->nptr = nw_req;
		nw_req->bptr = rhead->last;
		rhead->last = nw_req;
	} else {
		rhead->first = rhead->last = nw_req;
		nw_req->bptr = NULL;
	}

	/*
	 *	Now add the segment and wkmgmtbs nodes
	 */
	nw_req->sfirst = nw_req->slast = NULL;
	nw_req->ppfirst = nw_req->pplast = NULL;
	make_segment(nw_req, typ);
	nw_req->ltype = WM_NO_LTYPE;

	return;
}

/*
 *	Add a segment node and its associated wkmgmtbs node to
 *	the end of a request's segment list.  Return a pointer
 *	to the new segment node.
 */
static struct wmseg *make_segment(struct wmreq *req_ptr, int typ)
{
	struct	wkmgmtbs *nw_wm;
	struct	wmseg	*nw_seg;

	/*
	 *	Allocate memory for the new nodes and initialize fields
	 */
	if ((nw_wm = (struct wkmgmtbs *)malloc(sizeof(struct wkmgmtbs))) ==
	    NULL) {
		acct_perr(ACCT_ABORT, errno,
		  _("There was insufficient memory available when allocating '%s'."),
		    "wkmgmtbs(c) node");
	}
	memset((char *)nw_wm, '\0', sizeof(struct wkmgmtbs));
	if (create_hdr1(&nw_wm->hdr, ACCT_DAEMON_WKMG)) {
		acct_err(ACCT_WARN,
		  _("An error was returned from the call to the '%s' routine."),
		    "create_hdr()");
	}
	nout_rec++;
	if (db_flag > 7) {
		Ndebug("make_segment(8): nout_rec(%d).\n", nout_rec);
	}
	if ((nw_seg = (struct wmseg *)malloc(sizeof(struct wmseg))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
		  _("There was insufficient memory available when allocating '%s'."),
		    "segment node");
	}

	/*
	 *	Add the node to the linked list
	 */
	if (typ == NORM_REQ) {
		if (req_ptr->sfirst == NULL) {
			req_ptr->sfirst = req_ptr->slast = nw_seg;
		} else {
			req_ptr->slast->nptr = nw_seg;
			req_ptr->slast = nw_seg;
		}
	} else {	/* typ == AUX_REQ */
		if (req_ptr->ppfirst == NULL) {
			req_ptr->ppfirst = req_ptr->pplast = nw_seg;
		} else {
			req_ptr->pplast->nptr = nw_seg;
			req_ptr->pplast = nw_seg;
		}
	}

	/*
	 *	Initialize the new nodes with default values and
	 *	some info for the current location of this record in
	 *	the pacct file for possible error handling later
	 */
	nw_seg->upind = upind;
	nw_seg->findex = cur_findex;
	nw_seg->pacct_offset = Pacct_offset;
	nw_seg->nptr = NULL;
	nw_seg->riptr = nw_wm;

	nw_wm->term_subtype = WM_NO_TERM;
	nw_wm->qwtime = DEFAULT_QWTIME;

	return(nw_seg);
}


/*
 *	Process WM_CON records
 *	  If there isn't already a request node for this reqid.arrayid,
 *	  make one.
 */
static void wm_con(struct wkmgmtbs *wbuf)
{
	struct	wkmgmtbs *riptr = NULL;
	struct	wmreq	*rqptr;

	if (db_flag > 8) {
		Ndebug("wm_con(9): CON - reqid(%lld), arrayid(%d).\n",
		       wbuf->reqid, wbuf->arrayid);
	}

	if ((rqptr = find_request(wbuf->reqid, wbuf->arrayid)) == NULL) {
		/*
		 * This is a new request.  No other records for this
		 * request have been seen.
		 */
		if (db_flag > 2) {
			Ndebug("wm_con(3): Only WM_CON record "
			       "found for reqid(%lld), subtype(%d).\n",
			       wbuf->reqid, wbuf->subtype);
		}
		make_request(NORM_REQ);
		rqptr = rhead->last;
		rqptr->reqid = wbuf->reqid;
		rqptr->arrayid = wbuf->arrayid;
		rqptr->stime = wbuf->time;
		riptr = rqptr->slast->riptr;
		init_wkmgmtbs(riptr, wbuf);
	}

	return;
}


/*
 *	Process WM_INIT records
 */
static void wm_init(struct wkmgmtbs *wbuf)
{
	struct wkmgmtbs	*riptr;
	struct wmreq	*rqptr;
	struct wmseg	*sptr;

	if (db_flag > 8) {
		Ndebug("wm_init(9): INIT - reqid(%lld), arrayid(%d).\n",
		       wbuf->reqid, wbuf->arrayid);
	}

	/*
	 *	Ideally, this reqid should already have a request node,
	 *	but if it doesn't, we'll make one
	 */
	if ((rqptr = find_request(wbuf->reqid, wbuf->arrayid)) == NULL) {
		if (db_flag > 2) {
			Ndebug("wm_init(3): Making request node for "
			       "reqid(%lld), type WM_INIT, subtype(%d).\n",
			       wbuf->reqid, wbuf->subtype);
		}
		make_request(NORM_REQ);
		rqptr = rhead->last;
		rqptr->reqid = wbuf->reqid;
		rqptr->arrayid = wbuf->arrayid;

	} else if (rqptr->slast == NULL) {
		make_segment(rqptr, NORM_REQ);
	}

	/*
	 *	If the previous record type for this reqid is not
	 *	WM_RECV, we need to add a segment node to the end
	 *	of this reqid's list.
	 */
	if (rqptr->ltype != WM_RECV && rqptr->ltype != WM_NO_LTYPE) {
		sptr = make_segment(rqptr, NORM_REQ);
		riptr = sptr->riptr;
	} else {
		riptr = rqptr->slast->riptr;
	}

	/*
	 *	Initialize the wkmgmtbs(c) node with values from the
	 *	input record.
	 */
	init_wkmgmtbs(riptr, wbuf);
	riptr->init_subtype = wbuf->subtype;
	if (wbuf->enter_time > 0) {
		riptr->qwtime = wbuf->time - wbuf->enter_time;
	}

	rqptr->ltype    = wbuf->type;
	rqptr->lsubtype = wbuf->subtype;

	/*
	 *	Time stamp on output record is always the time stamp
	 *	of the WM_INIT record, if it exists.
	 */
	rqptr->stime = riptr->start_time = wbuf->time;

	return;
}


/*
 *	Process WM_RECV records
 *	  If a request node does not exist for the request ID,
 *	  add one to the end of the request linked list.  Update
 *	  necessary fields.
 *
 *	  If there are multiple WM_RECV records (request moves from pipe
 *	  to batch queue), the start queue wait time (sqtime) is set to the
 *	  time of the first record (usually pipe queue).  Also update the
 *	  queue type and name so we always have the most current type and
 *	  name.
 *
 *	  The initial time stamp for this segment is set to the time of
 *	  the most recent WM_RECV record.
 *
 *	  NOTE: WM_RECV records do not contain valid jids.
 */
static void wm_recv(struct wkmgmtbs *wbuf)
{
	struct wkmgmtbs	*riptr;
	struct wmreq	*rqptr;

	if (db_flag > 8) {
		Ndebug("wm_recv(9): RECV - reqid(%lld), arrayid(%d).\n",
		       wbuf->reqid, wbuf->arrayid);
	}

	if ((rqptr = find_request(wbuf->reqid, wbuf->arrayid)) == NULL) {
		make_request(NORM_REQ);
		rqptr = rhead->last;	
		rqptr->reqid = wbuf->reqid;
		rqptr->arrayid = wbuf->arrayid;
		rqptr->stime = wbuf->time;
		riptr = rqptr->slast->riptr;
		init_wkmgmtbs(riptr, wbuf);
		riptr->jid = WM_NO_JID;

	} else if (rqptr->slast == NULL) {
		rqptr->stime = wbuf->time;	/* update */
		make_segment(rqptr, NORM_REQ);
		riptr = rqptr->slast->riptr;
		init_wkmgmtbs(riptr, wbuf);
		riptr->jid = WM_NO_JID;

	} else {
		riptr = rqptr->slast->riptr;

	}

	riptr->qtype = wbuf->qtype;
	strcpy(riptr->quename, wbuf->quename);

	rqptr->ltype    = wbuf->type;
	rqptr->lsubtype = wbuf->subtype;

	return;
}


/*
 *	Process WM_TERM records
 *	  Add the termination code to the reqinfo node.
 */
static void wm_term(struct wkmgmtbs *wbuf)
{
	struct	wkmgmtbs *riptr;
	struct	wmreq	*rqptr;

	if (db_flag > 8) {
		Ndebug("wm_term(9): TERM - reqid(%lld), arrayid(%d).\n",
		       wbuf->reqid, wbuf->arrayid);
	}

	if ((rqptr = find_request(wbuf->reqid, wbuf->arrayid)) == NULL) {
		make_request(NORM_REQ);
		rqptr = rhead->last;
		rqptr->reqid = wbuf->reqid;
		rqptr->arrayid = wbuf->arrayid;
		riptr = rqptr->slast->riptr;
		init_wkmgmtbs(riptr, wbuf);

	} else if (rqptr->slast == NULL) {
		make_segment(rqptr, NORM_REQ);
		riptr = rqptr->slast->riptr;
		init_wkmgmtbs(riptr, wbuf);

	} else {
		riptr = rqptr->slast->riptr;
	}

	/*
	 *	The last node on the segment list should be
	 *	what we want.  If not, the records in the file
	 *	are out of order.
	 */
	if (riptr->term_subtype == WM_NO_TERM) {
		riptr->code = wbuf->code;
		riptr->utime = wbuf->utime;
		riptr->stime = wbuf->stime;
		riptr->term_subtype = wbuf->subtype;
	} else {
		if (db_flag > 2) {
			Ndebug("wm_term(3): reqinfo node not "
			       "at end of segment list.\n"
			       "\triptr: term_subtype(%d), jid(0x%llx), "
			       "wbuf->jid(0x%llx), rqptr->ltype(%d).\n",
			       riptr->term_subtype, riptr->jid,
			       wbuf->jid, rqptr->ltype);
		}
		return;	/* ignore this record */
	}

	rqptr->ltype    = wbuf->type;
	rqptr->lsubtype = wbuf->subtype;

	return;
}


/*
 *	Process WM_SPOOL records
 *	  Update or create a wkmgmtbs(c) entry on the pp list.  These
 *	  entries contain information about the pipeclients and
 *	  netclients.
 *
 */
static void wm_spool(struct wkmgmtbs *wbuf)
{
	struct	wkmgmtbs *riptr;
	struct	wmreq	*rqptr;
	struct	wmseg	*sptr;

	if (db_flag > 8) {
		Ndebug("wm_spool(9): SPOOL - reqid(%lld), arrayid(%d).\n",
		       wbuf->reqid, wbuf->arrayid);
	}

	if ((rqptr = find_request(wbuf->reqid, wbuf->arrayid)) == NULL) {
		/*
		 * This is a new reqid. Create request, segment
		 * and wkmgmtbs(c) entries for it.
		 */
		if (db_flag > 2) {
			Ndebug("wm_spool(3): making request node for "
			       "reqid(%lld), type WM_SPOOL, subtype(%d).\n",
			       wbuf->reqid, wbuf->subtype);
		}
		make_request(AUX_REQ);
		rqptr = rhead->last;	
		rqptr->reqid = wbuf->reqid;
		rqptr->arrayid = wbuf->arrayid;
		riptr = rqptr->pplast->riptr;
		init_wkmgmtbs(riptr, wbuf);

	} else if (rqptr->pplast == NULL) {
		/*
		 * This reqid does not have any wkmgmtbs(c) entries on the
		 * pp list, so make one.
		 */
		if (db_flag > 2) {
			Ndebug("wm_spool(3): pplast == NULL.\n");
		}
		make_segment(rqptr, AUX_REQ);
		riptr = rqptr->pplast->riptr;
		init_wkmgmtbs(riptr, wbuf);

	} else {
		/*
		 * Look through the entire pp list for this jid, since
		 * there could be multiple active pipeclients and
		 * netclients.  If jid = 0 (which could happen on a
		 * WM_SPOOL_TERM record), then update the first active
		 * entry.
		 */
		int	found = 0;

		if (db_flag > 2) {
			Ndebug("wm_spool(3):  looking for a wkmgmtbs(c) "
			       "entry with jid(0x%llx).\n", wbuf->jid);
		}

		for (sptr = rqptr->ppfirst, riptr = sptr->riptr;
		     ((sptr != NULL) && (found == 0)); sptr = sptr->nptr) {
			riptr = sptr->riptr;

			switch (wbuf->jid) {
			case 0:
				if (riptr->term_subtype == WM_NO_TERM) {
					found = 1;
				}
				break;

			default:
				if (riptr->jid == wbuf->jid) {
					found = 1;
				}
				break;
			}		/* end of switch(jid) */
		}

		if (!found) {
			if (db_flag > 2) {
				Ndebug("wm_spool(3): making new "
				       "segment node.\n");
			}
			make_segment(rqptr, AUX_REQ);
			riptr = rqptr->pplast->riptr;
			init_wkmgmtbs(riptr, wbuf);
		}
	}

	/*
	 * Update the wkmgmtbs(c) entry.
	 */
	switch (wbuf->subtype) {

	case WM_SPOOL_INIT:
		riptr->init_subtype = WM_SPOOL_INIT;
		break;

	case WM_SPOOL_TERM:
		if (riptr->init_subtype != WM_SPOOL_INIT) {
			riptr->init_subtype = WM_SPOOL_INIT;
		}
		riptr->utime = wbuf->utime;
		riptr->stime = wbuf->stime;
		riptr->term_subtype = WM_SPOOL_TERM;
		break;

	default:
		acct_err(ACCT_CAUT,
		  _("An unknown NQ_SENT subtype (%d) was found in the '%s' routine."),
		    wbuf->subtype, "wm_spool()");
		break;
	}		/* end of switch(subtype) */

	return;
}


/*
 *	Dump the data structures.
 */
static void dump_wm_tbl()
{
	time_t	stime;
	struct	wkmgmtbs *riptr;
	struct	wmreq	*rqptr;
	struct	wmseg	*sptr;

	rqptr = rhead->first;

	while (rqptr != NULL) {
		int	ltype = rqptr->ltype < 0 ? 0 : rqptr->ltype;
		int	lsubtype = rqptr->lsubtype < 0 ? 0 : rqptr->lsubtype;
		char	*lst = ltype == WM_INIT ? Init_statname[lsubtype] :
			Term_statname[lsubtype];
		/*
		 *	Output the request node.
		 */
		stime  = rqptr->stime;

		Ndebug("\nRequest node:\n");
		Ndebug("\treqid %lld,  arrayid %d,  last type(%d, %s), "
		       "last subtype(%d, %s)\n\tstime:  %s\n",
		       rqptr->reqid, rqptr->arrayid,
		       rqptr->ltype, Type_statname[ltype],
		       rqptr->lsubtype, lst, ctime(&stime));

		/*
		 *	Output the request records.
		 */
		sptr = rqptr->sfirst;	/* NORM_REQ */
		Ndebug("  Normal requests\n");
		while (sptr != NULL) {
			riptr = sptr->riptr;
			print_wm_rec(riptr);
			sptr = sptr->nptr;
		}

		/*
		 *	Output the Auxiliary records.
		 */
		sptr = rqptr->ppfirst;	/* AUX_REQ */
		Ndebug("  Aux requests\n");
		while (sptr != NULL) {
			riptr = sptr->riptr;
			print_wm_rec(riptr);
			sptr = sptr->nptr;
		}

		rqptr = rqptr->nptr;
	}
	Ndebug("\n");

	return;
}


/*
 * ============================ trackwm() ============================
 *	Keep track of all workload management records so we can condense them
 *	into a single record per user for writing to the sorted pacct file.
 */
static void trackwm(struct wkmgmtbs *wm, int uind)
{
	struct wm_track *wt;
	int64_t	reqid;
	int	arrayid;
	uint64_t jid;

	/*
	 *	Get keys.
	 */
	reqid = wm->reqid;
	arrayid = wm->arrayid;
	jid = wm->jid;

	/*
	 *	Do we already have this one?  If so, keep track of
	 *	how many times we have already seen it and return.
	 */
	for(wt = tr_wm; wt != NULL; wt = wt->wt_next) {
		if ((wt->wt_reqid == reqid) && (wt->wt_arrayid == arrayid) &&
		    (wt->wt_upind == uind) && (wt->wt_jid == jid)) {
			(wt->wt_njob)++;
			if (db_flag > 4) {
				Ndebug("trackwm(5): found reqid(%lld), "
				       "arrayid(%d), jid(0x%llx), "
				       "upind(%d).\n",
				       reqid, arrayid, jid, uind);
			}
			return;
		}
	}

	/*
	 *	Guess not, so add it to the front of the list.
	 */
	if ((wt = (struct wm_track *)malloc(sizeof(struct wm_track))) ==
	    NULL) {
		acct_perr(ACCT_ABORT, errno,
		  _("There was insufficient memory available when allocating '%s'."),
		    "workload management Job");
	}

	/*
	 *	Fill it in.
	 */
	wt->wt_reqid = reqid;
	wt->wt_arrayid = arrayid;
	wt->wt_upind = uind;
	wt->wt_jid   = jid;
	wt->wt_njob  = 0;

	/*
	 *	set up links.
	 */
	wt->wt_next = tr_wm;
	tr_wm = wt;

	if (db_flag > 4) {
		Ndebug("trackwm(5): new node, - reqid(%lld), arrayid(%d), "
		       "jid(0x%llx), upind(%d).\n", reqid, arrayid, jid,
		       uind);
	}

	return;
}

/*
 * ============================ reqcmp() ============================
 *	Compare routine for sorting workload management records by 
 *	reqid/arrayid/upind/jid.
 */
static int reqcmp(const void *e1, const void *e2)
{
	struct wm_track *w1;
	struct wm_track *w2;
	int64_t tmp;

	w1 = (struct wm_track *)e1;
	w2 = (struct wm_track *)e2;
	tmp = w1->wt_reqid - w2->wt_reqid;
	if (tmp == 0) {
		tmp = w1->wt_arrayid - w2->wt_arrayid;
		if (tmp == 0) {
			tmp = w1->wt_upind - w2->wt_upind;
			if (tmp == 0) {
				return(w1->wt_jid - w2->wt_jid);

			} else {
				return(tmp);
			}

		} else {
			return(tmp);
		}

	} else {
		return(tmp);
	}
}

/*
 * ============================ jidcmp() ============================
 *	Compare routine for sorting workload management records by jid.
 */
static int jidcmp(const void *e1, const void *e2)
{
	struct wm_track *w1;
	struct wm_track *w2;

	w1 = (struct wm_track *)e1;
	w2 = (struct wm_track *)e2;
	
	if (w1->wt_jid < w2->wt_jid) {
		return(1);
	}

	return(-1);
}

struct wm_track *wwt;
/*
 * ============================ Condense_wm() ============================
 *	Put jobs together if they have common workload management records...
 *
 *	(Forget about combining records which came before first input uptime.
 *	Zap these so we do not mess up everything else.  -  this is wrong?)
 *
 *	The request ID and array ID make a unique key, so find all records
 *	with that unique key and then combine any associated jobs.
 */
void Condense_wm() 
{
	struct wm_track *wt;
	int tablesz;
	int ind, jnd;
	uint64_t jid;
	int upind, keepit, beginrec, size;
	int64_t reqid;
	int arrayid;
	int hindex;
	struct segment **htable;
	struct segment *first_sptr, *next_sptr, *sptr;

	if (tr_wm == NULL) {
		if (db_flag > 0) {
			Ndebug("Condense_wm(1): No workload management "
			       "jobs found.\n");
		}
		return;
	}

	/*
	 *	First, sort the workload management records by requestid,
	 *	arrayid, and upind.  Start by building array wwt[] from the
	 *	workload management linked list.
	 */
	for (ind = 0, wt = tr_wm; wt != NULL; ind++, wt = wt->wt_next);
	tablesz = ind;

	size = sizeof(struct wm_track) * tablesz;
	if ((wwt = (struct wm_track *)malloc(size)) == NULL) {
		acct_perr(ACCT_ABORT, errno,
		  _("There was insufficient memory available when allocating '%s'."),
		    "final workload management table");
	}

	/*
	 *	Transfer data from the linked list to table wwt[]
	 */
	for (ind = 0, wt = tr_wm; wt != NULL; ind++, wt = wt->wt_next) {
		wwt[ind].wt_reqid = wt->wt_reqid;
		wwt[ind].wt_arrayid = wt->wt_arrayid;
		wwt[ind].wt_upind = wt->wt_upind;
		wwt[ind].wt_jid   = wt->wt_jid;
		wwt[ind].wt_njob  = wt->wt_njob;
		/*
		 *	Get rid of all records prior to uptime 0(?)
		 */
/*
 *		if ((Uptime_shifted) && (wwt[ind].wt_upind == 0)) {
 *			ind--;
 *		}
 */
	}
	tablesz = ind;
	Dump_wm_tbl(wwt, tablesz,
		    "DUMP workload management table before sort (wwt)");

	/*
	 *	Sort the table by reqid, arrayid, and upind
	 */
	qsort((char *)wwt, tablesz, sizeof(struct wm_track), reqcmp);

	Dump_wm_tbl(wwt, tablesz,
		    "DUMP workload management table after sorting by reqid, "
		    "arrayid, upind");

	/*
	 *	Now search the table and zap records which do not matter
	 */
	for (ind = 0; ind < tablesz;) {
		reqid = wwt[ind].wt_reqid;
		arrayid = wwt[ind].wt_arrayid;
		jid = wwt[ind].wt_jid;
		upind = wwt[ind].wt_upind;
		/*
		 *	Keep only the records which have (1) the same
		 *	reqid/arrayid and different upind or jid
		 *	or (2) multiple segments for the same
		 *	jid/reqid/arrayid/upind.  Zap everything else.
		 */
		keepit = 0;
		beginrec = ind;
		while (reqid == wwt[ind].wt_reqid &&
		       arrayid == wwt[ind].wt_arrayid) {
			if ((upind != wwt[ind].wt_upind) ||
			    (wwt[ind].wt_njob > 0) ||
			    (jid != wwt[ind].wt_jid)) {
				keepit = 1;
			}
			if (++ind == tablesz) {
				break;
			}
		}
		if (keepit == 0) {
			for (; beginrec < ind; beginrec++) {
				wwt[beginrec].wt_jid = ZAPJID;
			}
		}
	}

	/*
	 *	Now get rid of all entries which were zapped (jid = ZAPJID).
	 *	Sort wwt[] so zapped entries are at the end of the table
	 *	and then shorten the table.
	 */
	Dump_wm_tbl(wwt, tablesz,
		    "DUMP workload management table after zapping entries");
	qsort((char *)wwt, tablesz, sizeof(struct wm_track), jidcmp);
	Dump_wm_tbl(wwt, tablesz,
		    "DUMP workload management table after sorting "
		    "jid == ZAPJID to end");

	for (ind = 0; ind < tablesz && wwt[ind].wt_jid != ZAPJID; ind++);
	if (db_flag > 2) {
		Ndebug("Condense_wm(3): workload management culling reduced "
		       "records from %d to %d.\n", tablesz, ind);
	}
	tablesz = ind;
	if (tablesz == 0) {
		if (db_flag > 2) {
			Ndebug("Condense_wm(3): No workload management jobs "
			       "to be joined - returning.\n");
		}
		return;
	}

	/*
	 *	Put back into key sort order (reqid/arrayid/upind)
	 */
	Dump_wm_tbl(wwt, tablesz,
		    "DUMP workload management table before resort into key "
		    "order");
	qsort((char *)wwt, tablesz, sizeof(struct wm_track), reqcmp);

	Dump_wm_tbl(wwt, tablesz, "DUMP workload management final table");
	Dump_cont_wm(wwt, tablesz,
		     "DUMP continued workload management segments");

	/*
	 *	Ok, now we are ready to combine workload management jobs
	 *	(finally)
	 */
	for (ind = 0; ind < tablesz; ind = jnd) {
		/*
		 *	Get a pointer to the first part of the workload
		 *	management request.  Handle the case where
		 *	jid == WM_NO_JID.  This can occur if we have a
		 *	rejected or queued job, or are missing workload
		 *	management accounting data.
		 */
		if (wwt[ind].wt_jid != WM_NO_JID) {
			htable = uptime[wwt[ind].wt_upind].up_seg;
			hindex = HASH_INDEX(wwt[ind].wt_jid);
			if ((first_sptr = seg_hashtable(htable, hindex, 
							wwt[ind].wt_jid,
							wwt[ind].wt_upind))
			    == (struct segment *)NULL ) {
				/* shouldn't happen; exit */
				acct_err(ACCT_ABORT,
				  _("No segment was returned from the hash table in the '%s' routine."),
				    "Condense_wm()");
			}
		} else {
			first_sptr = uptime[wwt[ind].wt_upind].up_wm;
		}
		
		/*
		 *	If job IDs wrapped, it may not be this sptr...
		 *	Or, if this is an WM_NO_JID part, we may not
		 *	be pointing to the right segment.
		 */
		sptr = first_sptr;
		reqid = wwt[ind].wt_reqid;
		arrayid = wwt[ind].wt_arrayid;
		if (db_flag > 3) {
			Ndebug("Condense_wm(4): Search for reqid(%lld), "
			       "arrayid(%d).\n", reqid, arrayid);
		}
		while (sptr != NULL && sptr != BADPOINTER) {
			if (db_flag > 3) {
				Ndebug("Condense_wm(4): Sptr sg_reqid(%lld), "
				       "sg_arrayid(%d).\n", sptr->sg_reqid,
				       sptr->sg_arrayid);
			}
			if ((sptr->sg_reqid == reqid) &&
			    (sptr->sg_arrayid == arrayid)) {
				break; /* this is it */
			}
			sptr = sptr->sg_ptr;
		}
		if (sptr == NULL || sptr == BADPOINTER) {
			acct_err(ACCT_ABORT,
			  _("Could not find workload management request ID (reqid) %lld, array ID (arrayid) %d, Job ID (job) 0x%llx, uptime index (upind) %d."),
			    reqid, arrayid,
			    wwt[ind].wt_jid, wwt[ind].wt_upind);
		}
		if (db_flag > 3) {
			Ndebug("Condense_wm(4): Found reqid(%lld), "
			       "arrayid(%d), jid(0x%llx).\n",
			       sptr->sg_reqid, sptr->sg_arrayid,
			       wwt[ind].wt_jid);
		}
		first_sptr = sptr;

		/*
		 *	Combine all portions of an workload management job
		 *	which have the same jid/reqid/arrayid/upind.
		 */
		Consolidate_wm(first_sptr, reqid, arrayid, ind);

		/*
		 *	Now, link any other entries which have the same
		 *	reqid/arrayid.
		 */
		for (jnd = ind+1; jnd < tablesz; jnd++) {
			if ((reqid != wwt[jnd].wt_reqid) ||
			    (arrayid != wwt[jnd].wt_arrayid)) {
				break;
			}
			if (db_flag > 4) {
				Ndebug("Condense_wm(5): looking for another "
				       "part to reqid(%lld), arrayid(%d), "
				       "jid(0x%llx)\n",
				       reqid, arrayid, wwt[jnd].wt_jid);
			}

			/*
			 *	Get the pointer to the next part of this
			 *	workload management job.  Handle the case
			 *	where jid == WM_NO_JID.
			 */
			if (wwt[jnd].wt_jid != WM_NO_JID) {
				htable = uptime[wwt[jnd].wt_upind].up_seg;
				hindex = HASH_INDEX(wwt[jnd].wt_jid);
				if ((next_sptr = seg_hashtable(htable, hindex,
							       wwt[jnd].wt_jid,
							       wwt[jnd].wt_upind))
				    == (struct segment *)NULL) {
					/* shouldn't happen; exit */
					acct_err(ACCT_ABORT,
					  _("No segment was returned from the hash table in the '%s' routine."),
					    "Condense_wm/seg_hastable()");
				}
			} else {
				next_sptr = uptime[wwt[jnd].wt_upind].up_wm;
			}


			/*
			 * Again, if job IDs wrapped it may not be this sptr...
			 */
			sptr = next_sptr;
			if (db_flag > 3) {
				Ndebug("Condense_wm(4): Search for "
				       "reqid(%lld), arrayid(%d).\n", reqid,
				       arrayid);
			}
			while ((sptr != NULL) && (sptr != BADPOINTER)) {
				if ((sptr->sg_reqid == reqid) &&
				    (sptr->sg_arrayid == arrayid) ) {
					if (db_flag > 3) {
						Ndebug("Condense_wm(4): Found "
						       "reqid(%lld), "
						       "arrayid(%d), "
						       "jid(0x%llx).\n",
						       sptr->sg_reqid,
						       sptr->sg_arrayid,
						       wwt[jnd].wt_jid);
					}
					break; /* this is it */
				}
				sptr = sptr->sg_ptr;
			}
			if ((sptr == NULL) || (sptr == BADPOINTER) ) {
				acct_err(ACCT_ABORT,
				  _("Could not find workload management request ID (reqid) %lld, array ID (arrayid) %d, Job ID (job) 0x%llx, uptime index (upind) %d."),
				      reqid, arrayid,
				      wwt[ind].wt_jid, wwt[ind].wt_upind);
			}
			next_sptr = sptr;

			/*
			 *	Make sure all the portions of this
			 *	reqid/jid/upind request are combined together
			 */
			Consolidate_wm(next_sptr, reqid, arrayid, jnd);

			/*
			 *	Insert second chain on end of first chain.
			 */
			*(first_sptr->sg_lproc) = next_sptr->sg_proc;
			first_sptr->sg_lproc = next_sptr->sg_lproc;
			*(first_sptr->sg_lwm) = next_sptr->sg_wm;
			first_sptr->sg_lwm = next_sptr->sg_lwm;

			/*
			 *	Delete "next" chain 
			 *
			 *	NOTE - Why do we have this 'if' statement?
			 *	Why don't we just free next_sptr?  The
			 *	assignment done in the 'if' part does not make
			 *	any sense.  We should be deleting next_sptr,
			 *	but we are deleting the one after that
			 *	instead.  I hesitate to change anything since
			 *	this code has been like this on the Cray
			 *	system for years.
			 *	If we do free next_sptr, we need to take care
			 *	of the sg_hashnext and sg_jidlistnext pointers
			 *	in addition to sg_ptr.
			 *	9/6/2000
			 */
			if ((next_sptr->sg_ptr != NULL) &&
			    (next_sptr->sg_ptr != BADPOINTER)) {
				next_sptr->sg_ptr = next_sptr->sg_ptr->sg_ptr;
			} else {
				next_sptr->sg_ptr   = NULL;
				next_sptr->sg_seqno = -1;
				next_sptr->sg_mid   = -1;
				next_sptr->sg_reqid = -1;
				next_sptr->sg_arrayid = -1;
				next_sptr->sg_stop  = 0;
				next_sptr->sg_start = 0;
				next_sptr->sg_proc  = NULL;
				next_sptr->sg_lproc = &(next_sptr->sg_proc);
				next_sptr->sg_wm    = NULL;
				next_sptr->sg_lwm   = &(next_sptr->sg_wm);
			}
		}		/* end of for(jnd) */
	}		/* end of for(ind) */

	if (db_flag > 0) {
		Ndebug("Condense_wm(1): Done condensing workload management "
		       "records.\n");
	}

	return;
}


/*
 * =========================== Consolidate_wm() ===========================
 *	Combine all portions of the specified workload management job which
 *	have the same jid/reqid/arrayid/upind.
 */
static void Consolidate_wm(struct segment *first_sptr, int64_t reqid,
			   int arrayid, int ind)
{
	struct segment	*next_sptr;
	struct segment	*prev_sptr;
	int		njob = 0;

	if (wwt[ind].wt_njob == 0) {
		return;
	}

	prev_sptr = first_sptr;
	next_sptr = first_sptr->sg_ptr;
	while ((next_sptr != NULL) && (next_sptr != BADPOINTER) ) {
		if ((next_sptr->sg_reqid == reqid) &&
		    (next_sptr->sg_arrayid == arrayid)) {
			njob++;

			/*
			 *	Insert second chain on end of first chain.
			 */
			*(first_sptr->sg_lproc) = next_sptr->sg_proc;
			first_sptr->sg_lproc = next_sptr->sg_lproc;
			*(first_sptr->sg_lwm) = next_sptr->sg_wm;
			first_sptr->sg_lwm = next_sptr->sg_lwm;
			
			prev_sptr->sg_ptr = next_sptr->sg_ptr;
			free((char *)next_sptr);
			next_sptr = prev_sptr->sg_ptr;

		} else {
			prev_sptr = next_sptr;
			next_sptr = next_sptr->sg_ptr;
		}
	}
 
	if (db_flag > 4) {
		Ndebug("Consolidate_wm(5): reqid(%lld), arrayid(%d), "
		       "njob(%d), jid(0x%llx).\n",
		       reqid, arrayid, njob, wwt[ind].wt_jid);
	}

	if (njob != wwt[ind].wt_njob) {
		if (db_flag > 4) {
			Ndebug("Consolidate_wm(5): Didn't find all portions "
			       "of workload management reqid(%lld), "
			       "arrayid(%d), upind(%d)!!\n" 
			       "\t njob(%d) != wt_njob(%d)\n", 
			       reqid, arrayid, wwt[ind].wt_upind,
			       njob, wwt[ind].wt_njob);
		}
	} 

	return;
}

/*
 *	add_segs_wm() - go through the list of consolidated records
 *	and attach these to an appropriate segment; needed to wait
 *	until now to do this because some workload management record
 *	types don't contain valid jids.
 */
void add_segs_wm()
{
	struct wkmgmtbs *riptr;
	struct wmreq *rqptr;
	struct wmseg *wmptr;
	struct segment *sp;
	struct Crec *Cptr;

	rqptr = rhead->first;
	while (rqptr != NULL) {
		wmptr = rqptr->sfirst;
		while (wmptr != NULL) {	
			riptr = wmptr->riptr;
			sp = get_segptr(REC_WKMG, wmptr->upind, riptr->jid,
					riptr->time, 0L, 0L);
			if (sp == NOJOBFOUND) {
				/* skip over this one. */ 
			} else {
				/* Insert record at end of the workload
				   management chain. */
				Total_crecs++;
				Cptr = get_crec(riptr, REC_WKMG);
				*(sp->sg_lwm) = Cptr;
				sp->sg_lwm = &(Cptr->cp_next);
				trackwm(riptr, wmptr->upind);
				if ((sp->sg_reqid != -1) && 
				    (sp->sg_reqid != riptr->reqid)) {
					acct_err(ACCT_ABORT,
					  _("An invalid (or multiple) request ID, array ID, or Job ID occurred in the file '%s%d'\n  near offset (%lld).  Job ID 0x%llx is associated with workload management request ID's %lld and %lld."),
						 pacct, wmptr->findex,
						 wmptr->pacct_offset,
						 riptr->jid, riptr->reqid,
						 sp->sg_reqid);
				} else {
					sp->sg_reqid = riptr->reqid;
					sp->sg_arrayid = riptr->arrayid;
				}
			}
			wmptr = wmptr->nptr;
		}
		wmptr = rqptr->ppfirst;
		while (wmptr != NULL) {	
			riptr = wmptr->riptr;
			sp = get_segptr(REC_WKMG, wmptr->upind, riptr->jid,
					riptr->time, 0L, 0L);
			if (sp == NOJOBFOUND) {
				/* skip over this one. */ 
			} else {
				/* Insert record at end of the workload
				   management chain */
				Total_crecs++;
				Cptr = get_crec(riptr, REC_WKMG);
				*(sp->sg_lwm) = Cptr;
				sp->sg_lwm = &(Cptr->cp_next);
				trackwm(riptr, wmptr->upind);
				if ((sp->sg_reqid != -1) && 
				    (sp->sg_reqid != riptr->reqid)) {
					acct_err(ACCT_ABORT,
					 _("An invalid (or multiple) request ID, array ID, or Job ID occurred in the file '%s%d'\n  near offset (%lld).  Job ID 0x%llx is associated with workload management request ID's %lld and %lld."),
						 pacct, wmptr->findex,
						 wmptr->pacct_offset,
						 riptr->jid, riptr->reqid,
						 sp->sg_reqid);
				} else {
					sp->sg_reqid = riptr->reqid;
					sp->sg_arrayid = riptr->arrayid;
				}
			}
			wmptr = wmptr->nptr;
		}
		rqptr = rqptr->nptr;
	}
}
