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
#endif  /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include "acctdef.h"
#include "csa_conf.h"
#include "sbu.h"
#include "acctmsg.h"

#define FUNC_NAME	"init_wm_sbu()"	/* library function name */

/*
 *	Get the system parameter values from the configuration file
 *	and create run-time variables for the workload management sbu
 *	values and termination codes.
 */
void init_wm_sbu()
{
	char	*ptr;
	char	token[25];
	char	*valptr;
	int	db_flag = 0;
	int	ind;
	int	siz;

	__WKMG_NO_CHARGE = 0;
	/*
	 *	Get the number of workload management queues and machines.
	 */
	__WKMG_NUM_MACHINES = init_int(WKMG_NUM_MACHINES, 2, TRUE);

	__WKMG_NUM_QUEUES = init_int(WKMG_NUM_QUEUES, 3, TRUE);

	/*
	 *	Get the memory for the queue and machine sbu arrays.
	 */
	if (__WKMG_NUM_QUEUES > 0) {
		siz = sizeof(struct wm_sbu_qtype) * __WKMG_NUM_QUEUES;
		if ((wm_qsbu = (struct wm_sbu_qtype *)malloc(siz)) == NULL) {
			acct_perr(ACCT_ABORT, errno,
			  _("%s: There was insufficient memory available when allocating '%s'"),
				  FUNC_NAME,"workload management queue SBU's"); 
		}
		memset((char *)wm_qsbu, '\0', siz);
	} else {
		wm_qsbu = NULL;
	}


	if (__WKMG_NUM_MACHINES > 0) {
		siz = sizeof(struct wm_sbu_mtype) * __WKMG_NUM_MACHINES;
		if ((wm_msbu = (struct wm_sbu_mtype *)malloc(siz)) == NULL) {
			acct_perr(ACCT_ABORT, errno,
			  _("%s: There was insufficient memory available when allocating '%s'"),
				 FUNC_NAME,"workload management machine SBU's"); 
		}
		memset((char *)wm_msbu, '\0', siz);
	} else {
		wm_msbu = NULL;
	}

	/*
	 *	Initialize the queue sbu array using values from the
	 *	configuration file.
	 */
	for (ind = 0; ind < __WKMG_NUM_QUEUES; ind++) {
		sprintf(token, "%s%d", WKMG_QUEUE, ind);
		ptr = init_char(token, "1.0	b_name", TRUE);

		if ((valptr = strtok(ptr, __SEPARATORS__)) != NULLCHARPTR) {
			wm_qsbu[ind].qsbu = atof(valptr);
		} else {
			acct_err(ACCT_ABORT,
			   _("%s: The '%s' %s value is missing from configuration file.\n    Please add the '%s' %s value to the configuration file."),
				 FUNC_NAME, token, "sbu", token, "sbu");
		}
		ptr = NULLCHARPTR;

		if ((valptr = strtok(ptr, __SEPARATORS__)) != NULLCHARPTR) {
			strcpy(wm_qsbu[ind].quename, valptr);
		} else {
			acct_err(ACCT_ABORT,
			  _("%s: The '%s' %s name is missing from configuration file.\n  Please add the '%s' %s name to the configuration file."),
				 FUNC_NAME, token, "queue", token, "queue");
		}

		if (db_flag > 2) {
			Ndebug("init_wm_sbu(): wm_qsbu[%d] - sbu = %f, ",
			       ind, wm_qsbu[ind].qsbu);
			Ndebug("queue = <%s>\n", wm_qsbu[ind].quename);
		}
	}

	/*
	 *	Initialize the machine sbu array using values from the
	 *	configuration file.
	 */
	for (ind = 0; ind < __WKMG_NUM_MACHINES; ind++) {
		sprintf(token, "%s%d", WKMG_MACHINE, ind);
		ptr = init_char(token, "1.0	snumber", TRUE);

		if ((valptr = strtok(ptr, __SEPARATORS__)) != NULLCHARPTR) {
			wm_msbu[ind].msbu = atof(valptr);
		} else {
			acct_err(ACCT_ABORT,
			  _("%s: The '%s' %s value is missing from configuration file.\n  Please add the '%s' %s value to the configuration file."),
				 FUNC_NAME, token, "sbu" , token, "sbu" );
		}
		ptr = NULLCHARPTR;

		if ((valptr = strtok(ptr, __SEPARATORS__)) != NULLCHARPTR) {
			strcpy(wm_msbu[ind].machname, valptr);
		} else {
			acct_err(ACCT_ABORT,
			  _("%s: The '%s' %s name is missing from configuration file.\n  Please add the '%s' %s name to the configuration file."),
				 FUNC_NAME, token, "queue" , token, "queue" );
		}

		if (db_flag > 2) {
			Ndebug("init_wm_sbu():  wm_msbu[%d] - sbu = %f",
			       ind, wm_msbu[ind].msbu);
			Ndebug("queue = <%s>\n", wm_msbu[ind].machname);
		}
	}
	/*
	 *	See which job termination codes are to be billed.
	 */
	__WKMG_TERM_EXIT = init_int(WKMG_TERM_EXIT, 1, TRUE);

	if (__WKMG_TERM_EXIT == 0) {
		__WKMG_NO_CHARGE = 1;
	}
	if (db_flag > 2) {
		Ndebug("init_wm_sbu(): __WKMG_TERM_EXIT = %d\n",
		       __WKMG_TERM_EXIT);
	}

	__WKMG_TERM_REQUEUE = init_int(WKMG_TERM_REQUEUE, 1, TRUE);

	if (__WKMG_TERM_REQUEUE == 0) {
		__WKMG_NO_CHARGE = 1;
	}
	if (db_flag > 2) {
		Ndebug("init_wm_sbu(): __WKMG_TERM_REQUEUE = %d\n",
		       __WKMG_TERM_REQUEUE);
	}

	__WKMG_TERM_HOLD = init_int(WKMG_TERM_HOLD, 1, TRUE);

	if (__WKMG_TERM_HOLD == 0) {
		__WKMG_NO_CHARGE = 1;
	}
	if (db_flag > 2) {
		Ndebug("init_wm_sbu(): __WKMG_TERM_HOLD = %d\n",
		       __WKMG_TERM_HOLD);
	}

	__WKMG_TERM_RERUN = init_int(WKMG_TERM_RERUN, 1, TRUE);

	if (__WKMG_TERM_RERUN == 0) {
		__WKMG_NO_CHARGE = 1;
	}
	if (db_flag > 2) {
		Ndebug("init_wm_sbu(): __WKMG_TERM_RERUN = %d\n",
		       __WKMG_TERM_RERUN);
	}

	__WKMG_TERM_MIGRATE = init_int(WKMG_TERM_MIGRATE, 1, TRUE);

	if (__WKMG_TERM_MIGRATE == 0) {
		__WKMG_NO_CHARGE = 1;
	}
	if (db_flag > 2) {
		Ndebug("init_wm_sbu(): __WKMG_TERM_MIGRATE = %d\n",
		       __WKMG_TERM_MIGRATE);
		Ndebug("   __WKMG_NO_CHARGE = %d\n", __WKMG_NO_CHARGE);
	}

	return;
}
#undef	FUNC_NAME
