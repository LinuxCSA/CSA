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
#include "csa.h"

#include <grp.h>
#include <malloc.h>
#include <math.h>
#include <memory.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csaja.h"
#include "extern_ja.h"

#ifndef	MIN
#define	MIN(A,B)	((A) < (B) ? (A) : (B))
#endif

#ifndef	MAX
#define	MAX(A,B)	((A) > (B) ? (A) : (B))
#endif

#define TIMEBUF_MAX	50

extern	int headerdone;	/* report header already printed? */

int	t_Ncfg = 0;	/* total # of CFG records processed */
int	t_NcfgBoot = 0;	/* total # of CFG Boot records processed */
int	t_NcfgFile = 0;	/* total # of CFG File records processed */
int	t_NcfgOn = 0;	/* total # of CFG On recrds processed */
int	t_NcfgOff = 0;	/* total # of CFG Off records processed */
int	t_NcfgUnk = 0;	/* total # of CFG Unknown records processed */

int	t_Nsoj = 0;	/* total # of SOJ records processed */
int	t_Neoj = 0;	/* total # of EOJ records processed */

struct	ac_utsname	cfg_sysname;	/* config system name storage */
struct	ac_utsname	*cfg_sn = &cfg_sysname;	/* config system name */

static	char	*cfg_state[] = {
	"Boot", "File", "  ON", " OFF",
	"Delta", "Event",
	"Max" };

/*
 *	pr_pacct() - Process a SOJ/EOJ/CFG record and accumulate totals.
 *	Returns TRUE if no error in pacct record, otherwise return FALSE.
 */
int
pr_special()
{
	char	time_buf[TIMEBUF_MAX];		/* time string buffer */
	struct	ac_utsname	*aun;

	/*
	 * Process records (SOJ, EOJ, CFG) which are found in pacct files
	 * but not in the normal file ja processes.
	 */
	if (acctrec.soj) {
		t_Nsoj++;
		if (c_opt) {
			uint64_t jobid;

			if (!r_opt && !headerdone) {
				printcmdhdr(l_opt);
				headerdone = TRUE;
			}

			jobid = acctrec.soj->ac_jid;
			if (J_opt || (j_opt && jobid == s_jid) ||
			    (u_opt && acctrec.soj->ac_uid == s_uid) ) {
				if (acctrec.soj->ac_type == AC_SOJ) {
					strftime(time_buf, TIMEBUF_MAX,
						TIME_FORMAT2, localtime(
						&acctrec.soj->ac_btime));
					printf("# JOB    (%#18llx) %s - "
						"Start of Job\n",
						jobid, time_buf);

				} else {
					strftime(time_buf, TIMEBUF_MAX,
						TIME_FORMAT2, localtime(
						&acctrec.soj->ac_rstime));
					printf("# JOB    (%#18llx) %s - "
					       "Restart of Job\n",
						jobid, time_buf);
				} 
			} else if ((j_opt && jobid != s_jid) ||
			    (u_opt && acctrec.soj->ac_uid != s_uid)) {
				if (db_flag > 3) {
				    fprintf(stderr, "%s: SOJ record "
					"deselected - jid(0x%llx, 0x%llx), "
					"uid(%d, %d).\n", Prg_name,
					jobid, s_jid, acctrec.soj->ac_uid,
					s_uid);
				}
			}
		}

		return(TRUE);

	} else if (acctrec.eoj) {
		t_Neoj++;
		if (c_opt) {
			uint64_t jobid;

			if (!r_opt && !headerdone) {
				printcmdhdr(l_opt);
				headerdone = TRUE;
			}

			jobid = acctrec.eoj->ac_jid;
			if (J_opt || (j_opt && jobid == s_jid) ||
			    (u_opt && acctrec.eoj->ac_uid == s_uid) ) {
				strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
					localtime(&acctrec.eoj->ac_etime));
				printf("# JOB    (%#18llx) %s - End of Job\n"
					,jobid, time_buf);
			} else if ((j_opt && jobid != s_jid) ||
			    (u_opt && acctrec.eoj->ac_uid != s_uid)) {
				if (db_flag > 3) {
				    fprintf(stderr, "%s: EOJ record "
					"deselected - jid(0x%llx, 0x%llx), "
					"uid(%d, %d).\n", Prg_name,
					jobid, s_jid, acctrec.eoj->ac_uid,
					s_uid);
				}
			}
		}
 
		return(TRUE);

	} else if (acctrec.cfg) {
		int	event;

		aun = &acctrec.cfg->ac_uname;
		strncpy(cfg_sn->sysname,  aun->sysname, 25);
		strncpy(cfg_sn->nodename, aun->nodename, 25);
		strncpy(cfg_sn->release,  aun->release, 41);
		strncpy(cfg_sn->version,  aun->version, 41);
		strncpy(cfg_sn->machine,  aun->machine, 25);

		event = acctrec.cfg->ac_event;
		switch(event) {
		case AC_CONFCHG_BOOT:
			t_Ncfg++;
			t_NcfgBoot++;
			break;
		case AC_CONFCHG_FILE:
			t_Ncfg++;
			t_NcfgFile++;
			break;
		case AC_CONFCHG_ON:
			t_Ncfg++;
			t_NcfgOn++;
			break;
		case AC_CONFCHG_OFF:
			t_Ncfg++;
			t_NcfgOff++;
			break;
		default:
			t_Ncfg++;
			t_NcfgUnk++;
			break;
		}
		if (c_opt) {
			if (!r_opt && !headerdone) {
				printcmdhdr(l_opt);
				headerdone = TRUE;
			}

			switch(event) {

			case	AC_CONFCHG_BOOT:
				strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
					localtime(&acctrec.cfg->ac_boottime));
				t_Ncfg++;
				t_NcfgBoot++;
				break;

			case	AC_CONFCHG_FILE:
				t_Ncfg++;
				t_NcfgFile++;
				strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
					localtime(&acctrec.cfg->ac_curtime));
				break;

			case	AC_CONFCHG_ON:
				t_Ncfg++;
				t_NcfgOn++;
				strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
					localtime(&acctrec.cfg->ac_curtime));
				break;

			case	AC_CONFCHG_OFF:
				t_Ncfg++;
				t_NcfgOff++;
				strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
					localtime(&acctrec.cfg->ac_curtime));
				break;

			default:
				t_Ncfg++;
				t_NcfgUnk++;
				strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
					localtime(&acctrec.cfg->ac_curtime));
				break;
			}
			if (cfg_state[event] == (char *)NULL) {
				printf("# CFG %5d (%5o) (%5o) %s", event,
					acctrec.cfg->ac_kdmask,
					acctrec.cfg->ac_rmask, time_buf);

			} else {
				printf("# CFG %s(%5o) (%5o) %s",
					cfg_state[event],
					acctrec.cfg->ac_kdmask,
					acctrec.cfg->ac_rmask, time_buf);
			}

			printf("  System:  %s %s %s %s %s\n",
				aun->sysname, aun->nodename, aun->release,
				aun->version, aun->machine);
		}

		return(TRUE);
	}

	return(TRUE);
}
