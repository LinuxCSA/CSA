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
 *	Valid.c : library routines to check record validity.
 *		Return value:
 *			INV_NO_ERROR if the record is valid.
 *			HEADER_ERROR if the record is invalid due to a header
 *				error.
 *			OTHER_ERROR if the record is invalid due to a
 *				non-header error.
 *			Note:  Error is just the first bad thing we find.
 *
 *		Arguments:
 *			Pointer to the record.
 *			Pointer to the ascii reason for the record being
 *				judged invalid.
 *
 *	Typical use:
 *		char *failure;
 *		if (invalid_pacct(acctent_pointer, failure) != INV_NO_ERROR)
 *		{
 *			fprintf(stderr, "program: %s\n", failure);
 *			exit(1);
 *		}
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "csaacct.h"

#include "acctmsg.h"
#include "acctdef.h"

#ifndef PID_MAX_LIMIT
#define PID_MAX_LIMIT 0x400000
#endif

#ifndef MAXUID
#define MAXUID	UINT_MAX
#endif

typedef enum
{
	WKMG,
	CWKMG,
	THE_REST
} record_type;

extern int    rev_tab[];

/*
 *	error() - Process error.
 */
static void error(record_type type, char *failure, char *rec, char *fail)
{
	switch (type)
	{
	case WKMG:
		strcpy(failure, " Invalid wkmg record - ");
		break;
	case CWKMG:
		strcpy(failure, " Invalid consolidated wkmg record - ");
		break;
	default:  /* THE_REST */
		strcpy(failure, " Invalid record - ");
		strcat(failure, rec);
		break;
	}
	strcat(failure, fail);
	
	return;
}


/*
 *	invalid_common_wm() - Verify some of the common fields in the raw and
 *	                      consolidated workload management daemon records.
 */
invalid_retval invalid_common_wm(struct wkmgmtbs *wp, char *failure,
				 record_type type)
{
	char	fail[80];

	if ((wp->uid < 0) || (wp->uid > MAXUID))
	{
		sprintf(fail, "bad User ID %d.", wp->uid);
		error(type, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (wp->prid < 0)
	{
		sprintf(fail, "bad Project ID %lld.", wp->prid);
		error(type, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (wp->ash < 0)
	{
		sprintf(fail, "bad Array\nSession Handle %lld.", wp->ash);
		error(type, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (wp->reqid < 0)
	{
		sprintf(fail, "bad request\nID %lld.", wp->reqid);
		error(type, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (wp->arrayid < 0)
	{
		sprintf(fail, "bad array\nID %d.", wp->arrayid);
		error(type, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (wp->qtype < 0)
	{
		sprintf(fail, "bad queue\ntype %d.", wp->qtype);
		error(type, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (wp->serv_provider[0] == '\0')
	{
		sprintf(fail, "bad service provider.");
		error(type, failure, NULL, fail);
		return(OTHER_ERROR);
	}

	return(INV_NO_ERROR);
}


/*
 *	invalid_wm() - Verify the workload management daemon record.
 */
invalid_retval invalid_wm(struct wkmgmtbs *wp, char *failure) 
{
	char	fail[80];
	int	nbyte = sizeof(struct wkmgmtbs);
	invalid_retval retval;

/*
 *	Validate the record header.
 */
	if (check_hdr(&(wp->hdr), fail, ACCT_DAEMON_WKMG, nbyte) !=
	    HDR_NO_ERROR)
	{
		error(WKMG, failure, NULL, fail);
		return(HEADER_ERROR);
	}

/*
 *	Validate the workload management base record fields based on the
 *	workload management record type.
 */
	if (wp->type == WM_INFO)
	{
		if ((wp->subtype != WM_INFO_ACCTON) &&
		    (wp->subtype != WM_INFO_ACCTOFF))
		{
			sprintf(fail, "unknown WM_INFO subtype %d.",
				wp->subtype);
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}

	}
	else if (wp->type == WM_RECV)
	{
		if (wp->subtype != WM_RECV_NEW)
		{
			sprintf(fail, "unknown WM_RECV subtype %d.",
				wp->subtype);
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}

	}
	else if (wp->type == WM_INIT)
	{
		if ((wp->subtype != WM_INIT_START) &&
		    (wp->subtype != WM_INIT_RESTART) &&
		    (wp->subtype != WM_INIT_RERUN))
		{
			sprintf(fail, "unknown WM_INIT subtype %d.",
				wp->subtype);
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}

	}
	else if (wp->type == WM_TERM)
	{
		if ((wp->subtype != WM_TERM_EXIT) &&
		    (wp->subtype != WM_TERM_REQUEUE) &&
		    (wp->subtype != WM_TERM_HOLD) &&
		    (wp->subtype != WM_TERM_RERUN) &&
		    (wp->subtype != WM_TERM_MIGRATE))
		{
			sprintf(fail, "unknown WM_TERM subtype %d.",
				wp->subtype);
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}

	}
	else if (wp->type == WM_SPOOL)
	{
		if ((wp->subtype != WM_SPOOL_INIT) &&
		    (wp->subtype != WM_SPOOL_TERM))
		{
			sprintf(fail, "unknown WM_SPOOL subtype %d.",
				wp->subtype);
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}

	}
	else if (wp->type == WM_CON)
	{
		return(invalid_cwm(wp, failure));
	}
	else
	{
		sprintf(fail, "unknown record type %d.", wp->type);
		error(WKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}

	if (wp->time <= 0)
	{
		sprintf(fail, "bad time.");
		error(WKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if ((retval = invalid_common_wm(wp, failure, WKMG)) != INV_NO_ERROR)
		return(retval);

	if (wp->type != WM_INFO)
	{
		if (wp->machname[0] == '\0')
		{
			sprintf(fail, "bad machine name.");
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}
		if (wp->reqname[0] == '\0')
		{
			sprintf(fail, "bad request name.");
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}
		if (wp->quename[0] == '\0')
		{
			sprintf(fail, "bad queue name.");
			error(WKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}
	}
	if (wp->jid < 0)
	{
		sprintf(fail, "bad Job ID %lld.", wp->jid);
		error(WKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}

	strcpy(failure, "");
	return(INV_NO_ERROR);
}


/*
 *	invalid_cwm() - Verify the consolidated workload management daemon
 *	                record.
 */
invalid_retval invalid_cwm(struct wkmgmtbs *cwp, char *failure)
{
	char	fail[80];
	int	nbyte = sizeof(struct wkmgmtbs);
	invalid_retval retval;

/*
 *	Validate the record header.
 */
	if (check_hdr(&(cwp->hdr), fail, ACCT_DAEMON_WKMG, nbyte) !=
	    HDR_NO_ERROR)
	{
		error(CWKMG, failure, NULL, fail);
		return(HEADER_ERROR);
	}

/*
 *	Validate the consolidated workload management base record fields.
 */
	if (cwp->jid < 0)
	{
		/*
		 *	Job id WM_NO_JID shows up when the job has yet 
		 *	to get a real job id assigned.
		 */
		if (cwp->jid != WM_NO_JID)
		{
			sprintf(fail, "bad Job ID %lld.", cwp->jid);
			error(CWKMG, failure, NULL, fail);
			return(OTHER_ERROR);
		}
	}
	if (cwp->start_time <= 0)
	{
		sprintf(fail, "bad start time.");
		error(CWKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if ((retval = invalid_common_wm(cwp, failure, CWKMG)) != INV_NO_ERROR)
		return(retval);
	if (cwp->machname[0] == '\0')
	{
		sprintf(fail, "bad machine name.");
		error(CWKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (cwp->reqname[0] == '\0')
	{
		sprintf(fail, "bad request name.");
		error(CWKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (cwp->quename[0] == '\0')
	{
		sprintf(fail, "bad queue name.");
		error(CWKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}
	if (cwp->qwtime < 0)
	{
		sprintf(fail, "bad queue\nwait time.");
		error(CWKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}

/*
 *	Verify the sub-type information.
 */
	if (cwp->init_subtype != WM_INIT_START &&	/* 1 */
	    cwp->init_subtype != WM_INIT_RESTART &&	/* 2 */
	    cwp->init_subtype != WM_INIT_RERUN &&	/* 3 */
	    cwp->init_subtype != WM_SPOOL_INIT &&	/* 4 */
	    cwp->init_subtype != WM_NO_INIT)		/* -1 */
	{
		sprintf(fail, "unknown\nWM_INIT subtype %d.",
			cwp->init_subtype);
		error(CWKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}

	if (cwp->term_subtype != WM_TERM_EXIT &&	/* 1 */
	    cwp->term_subtype != WM_TERM_REQUEUE &&	/* 2 */
	    cwp->term_subtype != WM_TERM_HOLD &&	/* 3 */
	    cwp->term_subtype != WM_TERM_RERUN &&	/* 4 */
	    cwp->term_subtype != WM_TERM_MIGRATE &&	/* 5 */
	    cwp->term_subtype != WM_SPOOL_TERM &&	/* 6 */
	    cwp->term_subtype != WM_NO_TERM)		/* -2 */
	{
		sprintf(fail, "unknown\nWM_TERM subtype %d.",
			cwp->term_subtype);
		error(CWKMG, failure, NULL, fail);
		return(OTHER_ERROR);
	}

	strcpy(failure, "");
	return(INV_NO_ERROR);
}

/*
 *	invalid_linked() - Verify linked records -- applies to csa only.
 */
static int invalid_linked(struct acctent *ap, char **rec, char *fail,
			  int *size)
{
	struct	achead	*hdr;
	int	nbyte;
	int	err;
	
	if (ap->mem)
	{
		*rec = "(Mem): ";
		if (check_hdr(&(ap->mem->ac_hdr), fail, ACCT_KERNEL_MEM,
			      sizeof(struct acctmem)) != HDR_NO_ERROR)
		{
			return (1);
		}
		*size += ap->mem->ac_hdr.ah_size;
	}

	if (ap->io)
	{
		*rec = "(I/O): ";
		if (check_hdr(&(ap->io->ac_hdr), fail, ACCT_KERNEL_IO,
			      sizeof(struct acctio)) != HDR_NO_ERROR)
		{
			return (1);
		}
		*size += ap->io->ac_hdr.ah_size;
	}

	if (ap->delay)
	{
		*rec = "(Delay): ";
		if (check_hdr(&(ap->delay->ac_hdr), fail, ACCT_KERNEL_DELAY,
			      sizeof(struct acctdelay)) != HDR_NO_ERROR)
		{
			return (1);
		}
		*size += ap->delay->ac_hdr.ah_size;
	}

	return (0);
}
		
/*
 *	invalid_pacct() - Verify a pacct record.
 */
invalid_retval invalid_pacct(struct acctent *ap, char *failure)
{
	char		fail[80];
	char		*rec;
	invalid_retval	retval;

	if (ap->eoj)
	{
		int size = 0;
		int ac_init;
		
		rec = "(EOJ): ";
		if (check_hdr(&(ap->eoj->ac_hdr1), fail, ACCT_KERNEL_EOJ,
				sizeof(struct accteoj)) != HDR_NO_ERROR)
		{
			error(THE_REST, failure, rec, fail);
			return(HEADER_ERROR);
		}
		
		ac_init = ap->eoj->ac_init;
/*
		if ((ap->eoj->ac_init < AC_INIT_LOGIN) ||
		    (ap->eoj->ac_init >= AC_INIT_MAX))
*/
		if ((ac_init < AC_INIT_LOGIN) || (ac_init >= AC_INIT_MAX))
		{
			sprintf(fail, _("bad initiator type %d."),
				ap->eoj->ac_init);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->eoj->ac_jid < 0)
		{
			sprintf(fail, _("bad Job ID %lld."), ap->eoj->ac_jid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->eoj->ac_prid < 0)
		{
			sprintf(fail, _("bad Project ID %lld."),
				ap->eoj->ac_prid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if (ap->eoj->ac_ash < 0)
		{
			sprintf(fail, "bad Array Session Handle %lld.",
				ap->eoj->ac_ash);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->eoj->ac_uid < 0 || ap->eoj->ac_uid > MAXUID)
		{
			sprintf(fail, _("bad User ID %d."), ap->eoj->ac_uid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->eoj->ac_gid < 0 || ap->eoj->ac_gid > MAXUID)
		{
			sprintf(fail, _("bad Group ID %d."), ap->eoj->ac_gid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->eoj->ac_btime <= 0)
		{
			sprintf(fail, _("bad start time."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->eoj->ac_etime <= 0)
		{
			sprintf(fail, _("bad end time."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
	}

	if (ap->soj)
	{
		int ac_init;

		rec = "(SOJ): ";
		if (check_hdr(&(ap->soj->ac_hdr), fail, ACCT_KERNEL_SOJ,
			      sizeof(struct acctsoj)) != HDR_NO_ERROR)
		{
			error(THE_REST, failure, rec, fail);
			return(HEADER_ERROR);
		}
		
		if (ap->soj->ac_type != AC_SOJ && ap->soj->ac_type != AC_ROJ)
		{
			sprintf(fail, _("bad type %d."), ap->soj->ac_type);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		ac_init = ap->soj->ac_init;
		if ((ac_init < AC_INIT_LOGIN) || (ac_init >= AC_INIT_MAX))
		{
			sprintf(fail, _("bad initiator type %d."),
				ap->soj->ac_init);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->soj->ac_jid < 0)
		{
			sprintf(fail, _("bad Job ID %lld."), ap->soj->ac_jid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->soj->ac_uid < 0 || ap->soj->ac_uid > MAXUID)
		{
			sprintf(fail, _("bad User ID %d."), ap->soj->ac_uid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->soj->ac_btime <= 0)
		{
			sprintf(fail, _("bad start time."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->soj->ac_type == AC_ROJ && ap->soj->ac_rstime <= 0)
		{
			sprintf(fail, _("bad restart time."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
	}

	if (ap->cfg)
	{
		int ac_event;

		rec = "(CFG): ";
		if (check_hdr(&(ap->cfg->ac_hdr), fail, ACCT_KERNEL_CFG,
				sizeof(struct acctcfg)) != HDR_NO_ERROR)
		{
			error(THE_REST, failure, rec, fail);
			return(HEADER_ERROR);
		}
		
		if (ap->cfg->ac_boottime <= 0)
		{
			sprintf(fail, _("bad boot time."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->cfg->ac_curtime <= 0)
		{
			sprintf(fail, _("bad current time."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		ac_event = ap->cfg->ac_event;
		if((ac_event < AC_CONFCHG_BOOT) || (ac_event >= AC_CONFCHG_MAX))
		{
			sprintf(fail, _("bad event type %d."),
					ap->cfg->ac_event);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
	}

	/*
	 *	A CSA base level record and the possible associated records.
	 */
	if (ap->csa)
	{
		struct	achead	*hdr;
		struct	achead	*hdr2;
		int	size = 0;

		rec = "(CSA): ";
		hdr = &(ap->csa->ac_hdr1);
		hdr2 = &(ap->csa->ac_hdr2);
		if (check_hdr(hdr, fail, ACCT_KERNEL_CSA,
			      sizeof(struct acctcsa)) != HDR_NO_ERROR)
		{
			error(THE_REST, failure, rec, fail);
			return(HEADER_ERROR);
		}

		if ((ap->csa->ac_uid < 0) || (ap->csa->ac_uid > MAXUID))
		{
			sprintf(fail, _("bad User ID %d."), ap->csa->ac_uid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if ((ap->csa->ac_gid < 0) || (ap->csa->ac_gid > MAXUID))
		{
			sprintf(fail, _("bad Group ID %d."), ap->csa->ac_gid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if (ap->csa->ac_prid < 0)
		{
			sprintf(fail, _("bad Project ID %lld."),
				ap->csa->ac_prid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if (ap->csa->ac_ash < 0)
		{
			sprintf(fail, "bad Array Session Handle %lld.",
				ap->csa->ac_ash);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}	

		if (ap->csa->ac_btime <= 0)
		{
			sprintf(fail, _("bad beginning time."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if ((ap->csa->ac_pid < 0) || (ap->csa->ac_pid > PID_MAX_LIMIT))
		{
			sprintf(fail, _("bad process ID %d."),
				ap->csa->ac_pid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if ((ap->csa->ac_ppid  < 0) || (ap->csa->ac_ppid > PID_MAX_LIMIT))
		{
			sprintf(fail, _("bad parent process ID %d."),
				ap->csa->ac_pid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if (ap->csa->ac_jid < 0)
		{
			sprintf(fail, _("bad Job ID %lld."), ap->csa->ac_jid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		if (ap->csa->ac_comm[0] == '\0')
		{
			sprintf(fail, _("bad command name."));
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		/*
		 *	Check possible associated records.
		 */
		if (invalid_linked(ap, &rec, fail, &size))
		{
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}

		/*
		 *	Check the 2nd header now that we know the size.
		 */
		rec = "(CSA): ";
		if (check_hdr2(hdr2, fail, ACCT_KERNEL_CSA, size) !=
		    HDR_NO_ERROR)
		{
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
	}

	if (ap->job)
	{
		rec = "(Job-hdr): ";
		if (check_hdr(&(ap->job->aj_hdr), fail, ACCT_JOB_HEADER,
			      sizeof(struct acctjob)) != HDR_NO_ERROR)
		{
			error(THE_REST, failure, rec, fail);
			return(HEADER_ERROR);
		}
		
		if (ap->job->aj_uid < 0 || ap->job->aj_uid > MAXUID)
		{
			sprintf(fail, _("bad User ID %d."), ap->job->aj_uid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->job->aj_jid < 0)
		{
			sprintf(fail, _("bad Job ID %lld."), ap->job->aj_jid);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->job->aj_eop_num < 0)
		{
			sprintf(fail, _("bad number of process records %d."),
				ap->job->aj_eop_num);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->job->aj_job_num < 0)
		{
			sprintf(fail, _("bad number of job records %d."),
				ap->job->aj_job_num);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->job->aj_wkmg_num < 0)
		{
			sprintf(fail,
			      _("bad number of workload management records %d."),
				ap->job->aj_wkmg_num);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
		
		if (ap->job->aj_len <= 0)
		{
			sprintf(fail, _("bad length (in bytes) %d."),
				ap->job->aj_len);
			error(THE_REST, failure, rec, fail);
			return(OTHER_ERROR);
		}
	}

	if (ap->wmbs)
	{
		if ((retval = invalid_wm(ap->wmbs, failure)) != INV_NO_ERROR)				return(retval);
	}

	if (ap->other)
	{
		struct achead *hdr = ap->other;
	/*
	 *	if (hdr->ah_type == ACCT_KERNEL_SGI1 ||
	 *	    hdr->ah_type == ACCT_KERNEL_SGI2 ||
	 *	    hdr->ah_type == ACCT_DAEMON_SGI1 ||
	 *	    hdr->ah_type == ACCT_DAEMON_SGI2)
	 *	{
	 *		if (hdr->ah_revision != rev_tab[hdr->ah_type])
	 *		{
	 *			rec = "(SGI): ";
	 *			sprintf(fail, "bad revision %#o.",
	 *				hdr->ah_revision);
	 *			error(THE_REST, failure, rec, fail);
	 *			return(HEADER_ERROR);
	 *		}
	 *	}
	 */
	}

	strcpy(failure, "");
	return(INV_NO_ERROR);
}


/*
 *	check_hdr() - Check the record header in the accounting record.
 */
hdr_retval check_hdr(struct achead *hdr, char *fail, int type, int nbyte)
{
	/* NOTE - 10/5/99 - TSL
	   (The following comments only apply to record types other than
	   ACCT_KERNEL_SITE* and ACCT_DAEMON_SITE*.  Generally, this
	   function is not called for the above-listed record types
	   anyway.) For the records that are read in by readacctent() in
	   acctio.c, the first three checks in this function are not really
	   needed.  The ah_magic test is already done in VLEN_READ in
	   acctio.c.  If the ah_type is not recognizable, checktype() in
	   acctio.c would have returned an error.  If the ah_revision is
	   not correct, VLEN_READ would have performed on-the-fly
	   conversion.  If the ah_revision is so screwed up such that
	   on-the-fly conversion could not be performed, VLEN_READ would
	   have returned a header error.
	*/

	if (hdr->ah_magic != ACCT_MAGIC)
	{
		sprintf(fail, _("bad magic value %#o."), hdr->ah_magic);
		return(BAD_MAGIC);
	}

	if (hdr->ah_type != type)
	{
		sprintf(fail, _("bad header type ID\n%#o, expected %#o."),
			hdr->ah_type, type);
		return(BAD_TYPE);
	}

	if (hdr->ah_revision != rev_tab[hdr->ah_type])
	{
		sprintf(fail, _("bad revision %#o."), hdr->ah_revision);
		return(BAD_REVISION);
	}
	
	if (hdr->ah_size != nbyte)
	{
		sprintf(fail,
		      _("bad record size of\n%d bytes, expected %d bytes."),
			hdr->ah_size, nbyte);
		return(BAD_SIZE);
	}

	strcpy(fail, "");
	return(HDR_NO_ERROR);
}


/*
 *	check_hdr2() - Check the 2nd record header in the csa base
 *	               accounting record.
 */
hdr_retval check_hdr2(struct achead *hdr, char *fail, int type, int nbyte)
{
	int	magic = ~ACCT_MAGIC & 0177777;	/* a 16-bit mask */
	
	if (hdr->ah_magic != magic)
	{
		sprintf(fail, _("bad hdr2 magic value\n%#o, expected %#o."),
			hdr->ah_magic, magic);
		return(BAD_MAGIC);
	}

	if (hdr->ah_revision != rev_tab[type])
	{
		sprintf(fail, _("bad hdr2 revision %#o."), hdr->ah_revision);
		return(BAD_REVISION);
	}
	
	if (hdr->ah_size != nbyte)
	{
		sprintf(fail,
		      _("bad hdr2 record size\nof %d bytes, expected %d bytes."),
			hdr->ah_size, nbyte);
		return(BAD_SIZE);
	}

	strcpy(fail, "");
	return(HDR_NO_ERROR);
}
