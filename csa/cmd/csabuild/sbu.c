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
#include <sys/utsname.h>
#include "csaacct.h"

#include <grp.h>
#include <malloc.h>
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
 *	Check the termination subtype to see if we should charge for this 
 *	segment of a workload management request.  Return the SBU multiplier.
 *	0.0 should be returned if this segment should not be charged for.
 *	Otherwise, return the value from wm_sbu().
 *
 */
static double
wm_check_charge(struct wkmgmtbs *wmptr)
{
	/*
	 *	If all portions of the request are to be billed,
	 *	call the SBU routine.
	 */
	if (__WKMG_NO_CHARGE == 0)
		return(wm_sbu(wmptr));

	/*
	 *	See if this is a segment that we should bill.
	 */
	if (wmptr->term_subtype == WM_TERM_EXIT && __WKMG_TERM_EXIT == 0)
		return(0.0);
	if (wmptr->term_subtype == WM_TERM_REQUEUE &&
	    __WKMG_TERM_REQUEUE == 0) {
		return(0.0);
	}
	if (wmptr->term_subtype == WM_TERM_HOLD && __WKMG_TERM_HOLD == 0)
		return(0.0);
	if (wmptr->term_subtype == WM_TERM_RERUN && __WKMG_TERM_RERUN == 0)
		return(0.0);
	if (wmptr->term_subtype == WM_TERM_MIGRATE &&
	    __WKMG_TERM_MIGRATE == 0) {
		return(0.0);
	}

	return(wm_sbu(wmptr));
}

/*
 * ============================ compute_SBU() ============================
 *	Call the correct SBU calculation routine
 */
double
compute_SBU(int type, struct acctent *buf)
{
	double	sbu = 0.0;

	if (db_flag > 3) {
		Ndebug("compute_SBU(4): process type (%d).\n", type);
	}

	switch(type) {

	case ACCT_KERNEL_MEM:
	case ACCT_KERNEL_IO:
		break;

	case ACCT_KERNEL_CSA:
		buf->csa->ac_sbu = sbu = process_sbu(buf);
		break;

	case ACCT_KERNEL_CFG:
	case ACCT_KERNEL_SOJ:
		break;

	case ACCT_KERNEL_EOJ:
		buf->eoj->ac_sbu = sbu = process_sbu(buf);
		break;

	case ACCT_DAEMON_WKMG:
		buf->wmbs->sbu_wt = sbu = wm_check_charge(buf->wmbs);
		break;

	default:
		acct_err(ACCT_WARN,
		       _("Bad daemon identifier (%d) in header field."),
			type);
	}

	return(sbu);
}
