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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/times.h>
#include "csaacct.h"

#include <grp.h>
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
 *	process_eop() - Process an End of process record 
 *	Returns TRUE if no error in record.
 */
int
process_eop(int fd, int findex, int mark)
{
	int	type;
	time_t 	btime;			/* process beginning time [seconds] */
	struct	segment	*sp;		/* active segment pointer */
	struct	Frec	*Fptr;		/* new File record pointer */
	uint64_t jobid;			/* Job IDentifier */

	/*
	 *  See if the kernel had problems writing to the accounting file.
	 *  If the record is not now a CSA base record then what is it?
	 */
	type = acctent.prime->ah_type;

	if (!acctent.csa) {
		if (db_flag) {
			fprintf(stderr,
			      "process_eop(1): ERROR(%5o): An unknown record type detected.\n",
				type);
		}
		acct_err(ACCT_WARN,
		       _("An unknown record type (%4o) was found in the '%s' routine."),
			0, "process_eop()");
		return(FALSE);
	}

	/*
	 *	Ensure first time is earliest.
	 */
	btime = acctent.csa->ac_btime;
	if ((btime < uptime[0].up_start) &&
	    ((btime + PACCT_FUDGE) < uptime[0].up_start) ) {
	    if (btime < EARLY_FUDGE) {
		char btime_str[26];
		time_t	time = EARLY_FUDGE;

		strcpy(btime_str, ctime(&btime));
		Nerror("REC_EOP: EARLY_FUDGE error (%d - %24.24s) ",
			btime, btime_str);
		Nerror("< (%d - %24.24s).\n", time, ctime(&time));

		acct_err(ACCT_WARN,
			_("An earlier than expected time stamp was discovered in the file '%s%d' near offset (%lld).  Examine the data set."),
			pacct, findex, Pacct_offset);
		if (i_opt) {
			acct_err(ACCT_INFO, _("The record was ignored."));
			if (db_flag > 2) {
				Dump_process_rec(acctent.csa);
			}
			return(RC_BAD);
		}
	    }
	    if (upind != 0) {
			if (db_flag > 2) {
				Dump_process_rec(acctent.csa);
			}
			acct_err(ACCT_ABORT,
			       _("An earlier than expected time stamp was discovered in the file '%s%d' near offset (%lld).  Examine the data set."),
				pacct, findex, Pacct_offset);

	    } else {
			Shift_uptime(REC_EOP, pacct, findex);
	    }
	}

	/*  Check for time to go to next uptable entry. */
  	while(uptime[upind].up_stop < btime) {
		/* seek backwards so we will reread
		 * this record on return.
		 */
		if (db_flag > 3) {
			Ndebug("process_eop(4): Past stop time(%d), "
				"going to next process uptime(%d) - %s%d.\n",
				uptime[upind].up_stop, upind+1, pacct, findex);
		}
		upind++;
		if (upind >= upIndex) {
			return(RC_UPIND);
		}
	}

	jobid = acctent.csa->ac_jid;
	if ((sp = get_segptr(REC_EOP, upind, jobid, btime, 0L, btime)) ==
			NOJOBFOUND) {
		return(RC_NOJOB);
	}

	/*  Insert File record at end of process chain. */
	Total_frecs++;
	Fptr = get_frec(Pacct_offset, mark, REC_EOP);
	*(sp->sg_lproc) = Fptr;
	sp->sg_lproc = &(Fptr->fp_next);

	return(TRUE);
}
