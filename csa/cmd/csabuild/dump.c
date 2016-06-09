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
 *	Dump structures.
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


/*
 * ========================= Dump_acct_hdr() ===========================
 *	Display an acctent record.
 */
void
Dump_acct_hdr(struct achead *hdr) 
{
  Ndebug(" header magic(%#o), revision(%#o), type(%#o), flag(%#o), size(%d).\n",
		hdr->ah_magic, hdr->ah_revision, hdr->ah_type,
		hdr->ah_flag, hdr->ah_size);
  return;
}


/*
 * ========================= Dump_acctent() ===========================
 *	Display an acctent record.
 */
void
Dump_acctent(struct acctent *ac, char * str) 
{
	Ndebug("\n Dump_acctent(): %s\n\n", str);
	if (ac->ent)
		Ndebug(" Acctent.ent = %d\n", ac->ent);
	if (ac->prime) {
		Ndebug(" Acctent.prime ");
		Dump_acct_hdr(ac->prime);
	}
	if (ac->csa) {
		Ndebug(" Acctent.csa = %d\n", ac->csa);
	}
	if (ac->mem) {
		Ndebug(" Acctent.mem = %d\n", ac->mem);
	}
	if (ac->io) {
		Ndebug(" Acctent.io  = %d\n", ac->io);
	}
	if (ac->delay) {
		Ndebug(" Acctent.delay  = %d\n", ac->delay);
	}
	if (ac->soj) {
		Ndebug(" Acctent.soj = %d\n", ac->soj);
	}
	if (ac->eoj) {
		Ndebug(" Acctent.eoj = %d\n", ac->eoj);
	}
	if (ac->cfg) {
		Ndebug(" Acctent.cfg = %d\n", ac->cfg);
	}
	if (ac->job) {
		Ndebug(" Acctent.job= %d\n", ac->job);
	}
	if (ac->wmbs) {
		Ndebug(" Acctent.wmbs = %d\n", ac->wmbs);
	}	
	if (ac->other) {
		Ndebug(" Acctent.other = %d\n", ac->other);
	}

	return;
}


/*
 * ========================= Dump_uptime_rec() ===========================
 *	Display an uptime table entry.
 */
void
Dump_uptime_rec(int ind) 
{
	char T1[26], T2[26];

	strncpy(T1, ctime(&uptime[ind].up_start), 24);
	strncpy(T2, ctime(&uptime[ind].up_stop), 24);
	T1[25] = '\0';
	T2[25] = '\0';
	Ndebug(" Uptime(%d)\t%24.24s (%10ld)\t\t%24.24s (%10ld)\n",
		ind, T1, uptime[ind].up_start, T2, uptime[ind].up_stop);

	return;
}


/*
 * ========================= Dump_uptime_tbl() ===========================
 *	Display the entire uptime table.
 */
void
Dump_uptime_tbl(char *str) 
{
	int	ind;

	Ndebug("\nUptime table - %s.\n", str);
	Ndebug("Segment #\tStart Time\t\t\t\t\tStop time\n");
	for(ind = 0; ind <= upIndex; ind++) {
		if (uptime[ind].up_stop) {
			Dump_uptime_rec(ind);
		}
	}
	Ndebug("\n");

	return;
}

/* =========================== Dump_rec() =============================== */
void
Dump_rec(void *rec, int bytes)
{
	int	ind;
	int	*reci;

	Ndebug("Dump of ignored record.\n");
	reci = (int *)rec;
	for(ind = 0; ind < bytes/8; ind++) {
		Ndebug("Word %d = %24.24o %x\n",
			ind, reci[ind], reci[ind]);
	}

	return;
}

/* ====================== Dump_process_rec() ============================= */
void
Dump_process_rec(struct acctcsa *rec)
{
	Ndebug("Dump of Process record.\n");
	Dump_rec(rec, sizeof(struct acctcsa));

	return;
}


/* =========================== Dump_segment_rec() ======================== */
void
Dump_segment_rec(struct segment *sptr, int ind, int index)
{
	struct	Frec	*fptr;
	struct	Crec	*cptr;

	if (sptr->sg_proc != NULL) {
		Ndebug("\nDump_segment_rec:\n");
		Ndebug("\nJob ID 0x%x, (%d) htable[%d], sptr(%d) - sg_proc(%d),"
			" sg_lproc(%d).\n", sptr->sg_jid, index, ind,
			sptr, sptr->sg_proc, sptr->sg_lproc);
		Ndebug("================================================="
		       "=============\n");
		if (db_flag > 9) {
			for(fptr = sptr->sg_proc;
					fptr != NULL; fptr = fptr->fp_next) {
				Ndebug("&Process record %d, type(%d), "
					"offset(%d), next ptr(%d), file '%s'\n",
					fptr, fptr->fp_type, fptr->fp_offset,
					fptr->fp_next,
					file_tbl[fptr->fp_ind].name);
			}

			for(cptr = sptr->sg_wm; cptr != NULL;
					cptr = cptr->cp_next) {
				Ndebug("&Workload Mgmt record %d, type(%d), "
					"next ptr(%d)\n",
					cptr, cptr->cp_type, cptr->cp_next);
			}
		}

		if (sptr->sg_ptr != NULL) {
			if (sptr->sg_ptr == BADPOINTER) {
				Ndebug("Job closed off.\n");

			} else {
				Ndebug("Job ID EXTENDED!!!\n");
				Dump_segment_rec(sptr->sg_ptr, 0, index);
			}
		}
	}

	return;
}


/* =========================== Dump_seg_tbl() ============================= */
void
Dump_segment_tbl()
{
	int	ind;
	int	index;
	struct segment **htable;
	struct segment *sptr;

	Dump_uptime_tbl("Dump uptimes followed by segments");

	for(index = 0; uptime[index].up_stop != 0; index++) {
		htable = uptime[index].up_seg;
		for(ind = 0; ind < HASH_TBL_LEN; ind++) {
			for (sptr = htable[ind]; sptr != NULL;
			     sptr = sptr->sg_hashnext) {
				Ndebug("Uptime hash table index %d\n", ind);
				Dump_segment_rec(sptr, ind, index);
			}
		}
	}

	return;
}


/* ============================ Dump_wm_rec() ========================= */
void
Dump_wm_rec(struct wm_track *wwt)
{
	Ndebug("%-7lld  %-10d %-4d 0x%-16llx  %-4d\n", wwt->wt_reqid,
		wwt->wt_arrayid, wwt->wt_upind, wwt->wt_jid, wwt->wt_njob);

	return;
}


/* ============================ Dump_wm_tbl() ========================= */
void
Dump_wm_tbl(struct wm_track *wwt, int tablesz, char *str)
{
	int	ind;

	if (db_flag > 4) {
		Ndebug("\n%s\n", str);
		Ndebug("================================================\n");
		Ndebug("Req id   Array id upind  jid                 njob\n");
		Ndebug("-------  -------- -----  ------------------  ----\n");
		for (ind = 0; ind < tablesz; ind++) {
			Dump_wm_rec(&wwt[ind]);
		}
	}

	return;
}


/*
 * =========================== Dump_cont_wm() ==============================
 *	Dump the segments for workload management jobs which have multiple
 *	segments in the same up period (for instance, workload management jobs
 *	which have been preempted and restarted).
 */
void
Dump_cont_wm(struct wm_track *wwt, int tablesz, char *str)
{
	int	ind;
	int	njob;
	int	hindex;
	struct segment	*sptr;
	struct segment **htable;

	if (db_flag > 5) {
		Ndebug("\n%s\n", str);
                Ndebug("================================================\n");
                Ndebug("Reqid\t\tArr\t    upind   njob Date & Time\n");
                Ndebug("-----\t\t---\t    -----   ---- -----------\n");
		for (ind = 0; ind < tablesz; ind++) {
			/*
			 *	Skip entries with negative jids, so the
			 *	negative jid won't be used as an array index.
			 */
			if (wwt[ind].wt_jid < 0) {
				continue;
			}
			htable = uptime[wwt[ind].wt_upind].up_seg;
			hindex = HASH_INDEX(wwt[ind].wt_jid);
			if ((sptr = seg_hashtable(htable, hindex,
						  wwt[ind].wt_jid,
						  wwt[ind].wt_upind))
			    == (struct segment *)NULL) {
				/* shouldn't happen; exit */
				acct_err(ACCT_ABORT,
				  _("No segment was returned from the hash table in the '%s' routine."),
				    "seg_hashtable() from Dump_cont_wm()");
			} else {
			    Ndebug("%lld              %d     %d\n",
				wwt[ind].wt_reqid, wwt[ind].wt_arrayid,
				wwt[ind].wt_upind);
			    njob = 0;
			    while ((sptr != NULL) && (sptr != BADPOINTER)) {
				Ndebug( "%lld              %d     "
					"%d       %d    %s",
					sptr->sg_reqid, sptr->sg_arrayid,
					wwt[ind].wt_upind, ++njob,
					ctime(&(sptr->sg_start)));
				sptr = sptr->sg_ptr;
			    }
			}
		}		/* end of for(ind) */
	}

	return;
}

