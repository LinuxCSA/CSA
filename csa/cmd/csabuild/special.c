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

#include "csabuild.h"
#include "extern_build.h"

#define TIMEBUF_MAX	50

int	Max_upIndex = 100;	/* Maximum number of uptimes */
int	upIndex = -1;		/* Current maximum uptime index */
struct	Uptime	*uptime;	/* boot and reboot table from cfg records */

int	t_Ncfg = 0;	/* total # of CFG records processed */
int	t_NcfgBoot = 0;	/* total # of CFG Boot records processed */
int	t_NcfgFile = 0;	/* total # of CFG File records processed */
int	t_NcfgOn = 0;	/* total # of CFG On recrds processed */
int	t_NcfgOff = 0;	/* total # of CFG Off records processed */
int	t_NcfgUnk = 0;	/* total # of CFG Unknown records processed */

int	t_Nsoj = 0;	/* total # of SOJ records processed */
int	t_Neoj = 0;	/* total # of EOJ records processed */

struct	ac_utsname	cfg_sysname;		/* config system name storage */
struct	ac_utsname	*cfg_sn = &cfg_sysname;	/* config system name */

/*
 *	now() - get the current time in seconds since 1 Jan 1970.
 */
time_t
now()
{
	time_t	abc;

	return(time(&abc));
}

/*
 *	process_special() - Process a special record from the pacct file and
 *		accumulate totals.
 *	Returns TRUE if no error in pacct record, otherwise return FALSE.
 */
int
process_special(int fd, int findex, int mark)
{
	char	time_buf[TIMEBUF_MAX];	/* time string buffer */

	struct	segment	*sp;	/* active segment pointer */
	struct	Frec	*Fptr;	/* new File record pointer */
	uint64_t jobid;		/* Job Identifier */

	/*  Queue start-of-job records to matching (probably new) Job ID. */
	if (acctent.soj) {
		jobid = acctent.soj->ac_jid;
		t_Nsoj++;

		sp = get_segptr(REC_SOJ, upind, jobid,
			acctent.soj->ac_btime,
			0L,
			acctent.soj->ac_rstime);
		if (db_flag > 4) {
			Ndebug("process_special(5): SOJ: Job ID(0x%llx), "
				"btime(%24.24s), sp(%d).\n",
				jobid, ctime(&acctent.soj->ac_btime), sp);
			Ndebug("process_special(5): SOJ: Job ID(0x%llx), "
				"rstime(%24.24s), sp(%d).\n",
				jobid, ctime(&acctent.soj->ac_rstime), sp);
		}
		if (sp == NOJOBFOUND) {
			return(RC_NOJOB);
		}

		/*
		 *	Insert File record at end of chain.
		 *	Treat SOJ records as EOJ records in the chain.
		 */
		Total_frecs++;
		Fptr = get_frec(Pacct_offset, mark, REC_EOJ);
		*(sp->sg_lproc) = Fptr;
		sp->sg_lproc = &(Fptr->fp_next);

		return(RC_GOOD);

	/*  Queue end-of-job records to matching (existing) Job ID. */
	} else if (acctent.eoj) {
		jobid = acctent.eoj->ac_jid;
		t_Neoj++;

		sp = get_segptr(REC_EOJ, upind, jobid,
			acctent.eoj->ac_btime,
			acctent.eoj->ac_etime,
			0L);
		if (db_flag > 3) {
			Ndebug("process_special(4): EOJ: Job ID(0x%llx), "
				"btime(%24.24s), etime(%24.24s), sp(%d).\n",
				jobid, ctime(&acctent.eoj->ac_btime),
				ctime(&acctent.eoj->ac_etime), sp);
		}
		if (sp == NOJOBFOUND) {
			return(RC_NOJOB);
		}

		/*
		 *	Insert File record at end of chain.
		 */
		Total_frecs++;
		Fptr = get_frec(Pacct_offset, mark, REC_EOJ);
		*(sp->sg_lproc) = Fptr;
		sp->sg_lproc = &(Fptr->fp_next);

		return(RC_GOOD);

	/*  Queue configuration records to Job ID 0. */
	} else if (acctent.cfg) {
		struct	ac_utsname *aun;
		int	event;
		static	time_t	boot_time;

		aun = &acctent.cfg->ac_uname;
		strncpy(cfg_sn->sysname,  aun->sysname, 25);
		strncpy(cfg_sn->nodename, aun->nodename, 25);
		strncpy(cfg_sn->release,  aun->release, 41);
		strncpy(cfg_sn->version,  aun->version, 41);
		strncpy(cfg_sn->machine,  aun->machine, 25);

		event = acctent.cfg->ac_event;
		switch(event) {

		case	AC_CONFCHG_BOOT:
			strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
				localtime(&acctent.cfg->ac_boottime));
			t_Ncfg++;
			t_NcfgBoot++;
			boot_time = acctent.cfg->ac_boottime;
			break;

		case	AC_CONFCHG_FILE:
			t_Ncfg++;
			t_NcfgFile++;
			strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
				localtime(&acctent.cfg->ac_curtime));
			boot_time  = acctent.cfg->ac_boottime;
			break;

		case	AC_CONFCHG_ON:
			t_Ncfg++;
			t_NcfgOn++;
			strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
				localtime(&acctent.cfg->ac_curtime));
			boot_time  = acctent.cfg->ac_boottime;
			break;

		case	AC_CONFCHG_OFF:
			t_Ncfg++;
			t_NcfgOff++;
			strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
				localtime(&acctent.cfg->ac_curtime));
			boot_time = acctent.cfg->ac_boottime;
			break;

		default:
			t_Ncfg++;
			t_NcfgUnk++;
			strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT2,
				localtime(&acctent.cfg->ac_curtime));
			boot_time = acctent.cfg->ac_boottime;
			break;
		}  /* end of switch(event) */

		if (boot_time) {
			int	size;
			int	need_new = 1;

			/*	Create initial uptime entry. */
			if (upIndex < 0) {
				size = sizeof(struct Uptime) * (Max_upIndex +1);
				if ((uptime = (struct Uptime *)malloc(size)) ==
						 NULL) {
					acct_perr(ACCT_ABORT, errno,
						_("There was insufficient memory available when allocating '%s'."),
						"uptime table");
				}
				memset(uptime, 0, size);
				upind = 0;
				upIndex = upind +1;
				uptime[upind].up_start = boot_time;
				uptime[upind].up_stop  = now();
				uptime[upind+1].up_start = 0;
				uptime[upind+1].up_stop  = 0;
				Build_segment_hash_tbl(upind);
				if (db_flag > 3) {
					Dump_uptime_rec(upind);
				}

			/*	Create new uptime entry if needed. */
			} else if (boot_time != uptime[upind].up_start) {
				for(upind = 0; upind < upIndex; upind++) {
					if(boot_time == uptime[upind].up_start){
						need_new = 0;
						break;
					}
				}
				if (need_new) {
				    uptime[upind].up_stop = boot_time;
				    upind = upIndex++;
				    uptime[upind].up_start = boot_time;
				    uptime[upind].up_stop  = now();

				    if ((upind +1) >= Max_upIndex) {
					Max_upIndex += 100;
					size = sizeof(struct Uptime) *
						(Max_upIndex +1);
					if ((uptime = (struct Uptime *)
							realloc(uptime, size))
							== NULL) {
						acct_perr(ACCT_ABORT, errno,
							_("There was insufficient memory available when allocating '%s'."),
							"uptime table");
					}
					/* Need to expand the uptime array. */
				    }
				    uptime[upind+1].up_start = 0;
				    uptime[upind+1].up_stop  = 0;
				    Build_segment_hash_tbl(upind);
				}

			 	if (db_flag > 3) {
					Dump_uptime_rec(upind);
				}
			}

			if (db_flag > 0) {
				Ndebug("process_special(1): CFG: upind(%d), "
					"boottime(%d), uptime(%d)\n",
					upind, boot_time,
					uptime[upind].up_start);
			}
		}

		sp = get_segptr(REC_CFG, upind, 0,
			acctent.cfg->ac_curtime,
			0L, 0L);
		if (db_flag > 0) {
			Ndebug("process_special(1): CFG: upIndex(%d), "
				"boottime(%24.24s), now(%24.24s)\n",
				upIndex, ctime(&boot_time),
				ctime(&acctent.cfg->ac_curtime));
		}
		if (sp == NOJOBFOUND) {
			return(RC_NOJOB);
		}

		/*
		 *	Insert File record at end of chain.
		 */
		Total_frecs++;
		Fptr = get_frec(Pacct_offset, mark, REC_EOJ);
		*(sp->sg_lproc) = Fptr;
		sp->sg_lproc = &(Fptr->fp_next);

		return(RC_CFG);

	} else {
		acct_err(ACCT_INFO,
		       _("No Base or EOJ record was found in the file '%s%d' near offset %lld."),
			pacct, findex, Pacct_offset);
	}

	return(0);
}
