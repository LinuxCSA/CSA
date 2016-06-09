/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2006-2008 Silicon Graphics, Inc. All Rights Reserved.
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
#include <sys/fcntl.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>

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
#include "csaacct.h"

#include "csabuild.h"
#include "extern_build.h"

extern	int	files_open;		/* number of open files */
extern	struct	ac_utsname *cfg_sn;	/* config system name */

int	jobh_cnt = 0;		/* number of job headers written */
int	prc_cnt = 0;		/* number of process records written */
int	job_cnt = 0;		/* number of job state records written */
int	tot_cnt = 0;		/* total number of job records written */

int	wm_cnt = 0;		/* total #  workload management recs written */

int64_t	nbytes = 0;		/* # bytes written in 1 job record */

int	sfd;			/* Sorted pacct file descriptor */

struct	acctjob	Job_hdr;	/* Current job header structure */

static int jid_compare(const void *, const void *);

/*
 * ============================== Create_srec() =========================
 *	Create Sorted pacct record file - this routine traverses the tables
 *	build earlier and writes uptime configuration header records,
 *	and workload mgmt "no jid" records, and job records.
 */
void
Create_srec() 
{
	int	ind;
	int	size;
	uint64_t  jid;
	int	notused, i;
	struct	segment *sp;
	struct segment *jidlist;
	int	tblsize = 0;
	struct segment **sorttbl;
	int	old_file = 0;
	int64_t	uptime_hdr_off;
	struct	acctcfg	uphdr;
	struct	stat	statbuf;

	jobh_cnt = prc_cnt = job_cnt = tot_cnt = 0;
	if (db_flag > 0) {
		Ndebug("Create_srec(1): Writing spacct file.\n");
	}

	/*
	 *	Open S-record file.
	 */
	if (stat(spacct, &statbuf) == 0) {
		old_file++;
	}
	if ((sfd = openacct(spacct, "w")) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			spacct);
	}
	if (old_file && (db_flag > 0)) {
		Ndebug("Create_srec(1): file '%s' is being overwritten.\n",
			spacct);
	}
	if (seekacct(sfd, 0, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s' %s."),
			spacct,
			"Sorted Pacct to 0");
	}

	/*  For each uptime.  */
	for(ind = 0; ind < upIndex; ind++) {
		if (uptime[ind].up_stop == 0 ) {
			/* all done */
			break;
		}
		if (db_flag > 1) {
			Ndebug("Create_srec(2): Writing uptime %d jobs.\n",
				ind);
		}

		tblsize = (uptime[ind].up_size + 1) * sizeof(struct segment *);
		if ((sorttbl = (struct segment **)malloc(tblsize)) == NULL) {
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when allocating '%s'."),
				"segment sort table");
		}
		memset(sorttbl, 0, tblsize);

		/*  Seek to the end of file and remember position.  */
		if ((uptime_hdr_off = seekacct(sfd, 0, SEEK_END)) < 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the positioning of file '%s' %s."),
				spacct,
				"to the End of File");
		}

		if (create_hdr1(&uphdr.ac_hdr, ACCT_KERNEL_CFG) < 0) {
			acct_err(ACCT_WARN,
			       _("An error was returned from the call to the '%s' routine."),
				"header()");
		}

		uphdr.ac_kdmask  = 0;
		uphdr.ac_rmask  = 0;
		uphdr.ac_uptimelen = 0;
		uphdr.ac_event = AC_CONFCHG_BOOT;

		uphdr.ac_boottime = uptime[ind].up_start;
		uphdr.ac_curtime  = uptime[ind].up_stop;

		strncpy(uphdr.ac_uname.sysname, cfg_sn->sysname,  25);
		strncpy(uphdr.ac_uname.nodename, cfg_sn->nodename, 25);
		strncpy(uphdr.ac_uname.release, cfg_sn->release,  41);
		strncpy(uphdr.ac_uname.version, cfg_sn->version,  41);
		strncpy(uphdr.ac_uname.machine, cfg_sn->machine,  25);

		/*  Write initial version of the uptime (config) record. */
		size =  sizeof(struct acctcfg);
		if (db_flag > 0) {
			Ndebug("Create_srec(1): Writing Config record(%d).\n",
				size);
		}
		if (writeacct(sfd, (char *)&uphdr, size) != size) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the writing of file '%s' %s."),
				spacct,
				"uphdr");
		}

		if (db_flag > 8) {
			Ndebug("Create_srec(9) : writing records for "
				"uptime[%d]\n", ind);
		}

		/*  For each WM_NO_JID workload management entry.  */
		for (sp = uptime[ind].up_wm; sp != NULL; sp = sp->sg_ptr) {
			if (db_flag > 1) {
				Ndebug("Create_srec(2): Writing WM_NO_JID "
					"job.\n");
			}
			jobh_cnt++;
			tot_cnt += Write_Job_recs(-1, sp, ind);
			uphdr.ac_uptimelen +=
				(sizeof(struct acctjob) + nbytes);
		}

		/* 
		 * Sort the jobs for this uptime period in ascending order
		 * and then write them to the sorted pacct file. 
		 */
		jidlist = uptime[ind].up_jidhead;
		for (i = 0; i < uptime[ind].up_size; i++) {
			sorttbl[i] = jidlist;
			jidlist = jidlist->sg_jidlistnext;
		}
		qsort(sorttbl, uptime[ind].up_size, sizeof(struct segment *),
			jid_compare);
		notused = 0;
		for(i = 0; i < uptime[ind].up_size; i++) {
			/*  If there is any data.  */
			sp = sorttbl[i];
			if (sp->sg_proc != NULL) {
				jid = sp->sg_jid;
				if (db_flag > 1) {
					Ndebug( "Create_srec(2): jid(0x%llx), "
						"ptr(%d).\n", jid, sp);
				}
				jobh_cnt++;
				tot_cnt += Write_Job_recs(jid, sp, ind);
				uphdr.ac_uptimelen += (sizeof(struct acctjob)
					+ nbytes);
				/*
				 *	JIDWRAP case (yeach!)
				 */
				sp = sp->sg_ptr;
				while((sp != NULL) && (sp != BADPOINTER) ) {
					jid = sp->sg_jid;
					if (db_flag > 1) {
						Ndebug( "Create_srec(2): jid "
						       "wrap of jid = 0x%llx\n",
							jid);
					}
					jobh_cnt++;
					tot_cnt += Write_Job_recs(jid, sp, ind);
					uphdr.ac_uptimelen +=
					    (sizeof(struct acctjob) + nbytes);
					sp = sp->sg_ptr;
				}

			} else {
				notused++;
			}
		}

		/*  Write the final version of the up time header.  */
		if (seekacct(sfd, uptime_hdr_off, SEEK_SET) < 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the positioning of file '%s' %s."),
				spacct,
				"for uphdr offset");
		}
		size =  sizeof(struct acctcfg);
		if (db_flag > 0) {
			Ndebug("Create_srec(1): Rewriting Config record(%d).\n",
				size);
		}
		if (writeacct(sfd, (char *)&uphdr, size) != size) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the writing of file '%s' %s."),
				spacct,
				"rewrite uphdr");
		}

		if (db_flag > 2) {
			if (notused) {
				Ndebug("Create_srec(3): Number of segment for "
					"uptime %d not used = %d.\n",
					ind, notused);

			} else {
				Ndebug("\nCreate_srec(3): Number of segments "
					"for uptime %d = %d.\n", ind,
					uptime[ind].up_size);
			}
		}

		free(sorttbl);
	}

	closeacct(sfd);

	/*
	 *	Some simple stats.
	 */
	if (db_flag > 1) {
		Ndebug("\nNumber of Job header  records written %8d.\n",
			jobh_cnt);
		Ndebug("Number of End-of-Proc records written %8d.\n",
			prc_cnt);
		Ndebug("Number of Job-state   records written %8d.\n",
			job_cnt);
		Ndebug("Number of Workld Mgmt records written %8d.\n",
			wm_cnt);
		Ndebug("Total number of Job records written   %8d.\n",
			tot_cnt);
	}

	return;
}


/*
 * =============================== Read_record() =========================
 *	Read a record from a pacct or sorted pacct file.
 *	Accounting record is read to global acctent structure.
 */
static	void
Read_record(struct Frec *fptr, uint64_t jid, int num) 
{
	static	int	fd;
	int	err;
	char	string[128];

	if (db_flag > 0) {
		Ndebug("Read_record(1) called, jid(0x%x)\n", jid);
	}
	/*
	 *	Check if the file is open?
	 */
	if (file_tbl[fptr->fp_ind].status != F_OPEN) {
		/*
		 *	Make sure we won't have too many open files.
		 */
		if (files_open >= sysconf(_SC_OPEN_MAX)-1) {
			close_all_input_files();
		}
		if ((fd = open_file(file_tbl[fptr->fp_ind].name,
				F_READ)) < 0) {
			acct_perr(ACCT_ABORT, errno,
				_("Too many open files (%d) occurred in the '%s' routine."),
				files_open, "Write_Job_recs()");
		}

	} else {
		fd = file_tbl[fptr->fp_ind].fd;
	}

	if (fd < 0) {
		acct_err(ACCT_ABORT,
		       _("An invalid file descriptor was found in the '%s' routine."),
			"Write_Job_recs()");
	}

	/*  Seek to correct offset.  */
	if (seekacct(fd, fptr->fp_offset, SEEK_SET) < 0) {
		sprintf(string, "to offset %d, %#o",
			fptr->fp_offset, fptr->fp_offset);
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s' %s."),
			file_tbl[fptr->fp_ind].name, string);
	}

	/*  Read record.  */
	if (db_flag > 0) {
		Ndebug("Read_record(1): Read pacct entry(%d), "
			"offset(%d, %#o).\n",
			num, fptr->fp_offset, fptr->fp_offset);
	}
	if ((err = readacctent(fd, &acctent, FORWARD)) <= 0) {
		sprintf(string, "from offset %d, %#o",
			fptr->fp_offset, fptr->fp_offset);
		Ndebug("error(%d), %s\n", err, string);
		acct_err(ACCT_ABORT,
		       _("An error occurred during the reading of file '%s' %s."),
			file_tbl[fptr->fp_ind].name, string);
	}

	return;
}


/*
 * =============================== Write_record() =========================
 *	Write a sorted pacct file record.
 *	Upon entrance, the Sorted pacct file is at EOF.
 */
static	int
Write_record(int type, int num) 
{
	struct  wkmgmtbs *wm;
	struct	acctcon	*ct;
	int	ignore;
	int	bytes;

	/*  Account for record in hdr. */
	if (db_flag > 0) {
		Ndebug("Write_record(1): process type (%d).\n", type);
	}
	ignore = 0;
	bytes = acctent.prime->ah_size;
	switch(type) {

	case REC_CFG:
		if (db_flag > 1) {
			Ndebug("Write_record(2): REC_CFG (%#4o).\n",
				Job_hdr.aj_flags);
		}
		break;

	case REC_EOP:
		Job_hdr.aj_eop_num++;
		if (db_flag > 1) {
			Ndebug("Write_record(2): REC_EOP (%#4o).\n",
				Job_hdr.aj_flags);
		}
		if (acctent.csa) {
			Job_hdr.aj_jid = acctent.csa->ac_jid;
			Job_hdr.aj_uid = acctent.csa->ac_uid;
			Job_hdr.aj_btime = acctent.csa->ac_btime;
			bytes += acctent.csa->ac_hdr2.ah_size;
			compute_SBU(ACCT_KERNEL_CSA, &acctent);

		} else {
			Dump_acctent(&acctent,
				"Record error - CSA expected.");
			ignore++;
			break;
		}
		prc_cnt++;
		break;

	case REC_EOJ:
		Job_hdr.aj_job_num++;
		if (acctent.eoj) {
			Job_hdr.aj_flags |= END_OF_JOB_I;
			if (db_flag > 1) {
				Ndebug("Write_record(2): REC_EOJ (%#4o).\n",
					Job_hdr.aj_flags);
			}
			Job_hdr.aj_jid = acctent.eoj->ac_jid;
			Job_hdr.aj_uid = acctent.eoj->ac_uid;
			Job_hdr.aj_btime = acctent.eoj->ac_btime;
			bytes += acctent.eoj->ac_hdr2.ah_size;
			compute_SBU(ACCT_KERNEL_EOJ, &acctent);

		} else if (acctent.soj) {
			Job_hdr.aj_jid = acctent.soj->ac_jid;
			Job_hdr.aj_uid = acctent.soj->ac_uid;
			Job_hdr.aj_btime = acctent.soj->ac_btime;
			if (db_flag > 1) {
				Ndebug("Write_record(2): REC_SOJ (%4#o).\n",
					Job_hdr.aj_flags);
			}
			break;

		} else if (acctent.cfg) {
			if (db_flag > 1) {
				Ndebug("Write_record(2): REC_CFG (0x%llx).\n",
					Job_hdr.aj_jid);
			}
			break;

		} else {
			Dump_acctent(&acctent,
				"Record error - JOB record expected.");
			ignore++;
			break;
		}
		job_cnt++;
		break;

	case REC_WKMG:
		Job_hdr.aj_wkmg_num++;
		wm = (struct wkmgmtbs *)acctent.wmbs;
		if (wm->term_subtype != WM_NO_TERM) {
			Job_hdr.aj_flags |= END_OF_JOB_B;
		}
		Job_hdr.aj_flags |= WKMG_IN_JOB;
		if (db_flag > 1) {
			Ndebug("Write_record(0): REC_WKMG (%#4o).\n",
				Job_hdr.aj_flags);
		}
		if (acctent.wmbs) {
			compute_SBU(ACCT_DAEMON_WKMG, &acctent);
		} else {
			Dump_acctent(&acctent,
				"Record error - Workload Management expected.");			ignore++;
			break;
		}
		wm_cnt++;
		break;

	default:
		ignore++;
		acct_err(ACCT_CAUT,
		       _("An unknown record type (%d) was found in the '%s' routine\n   during the header phase."),
			type, "Write_record()");

	}  /* end of switch(type) */

	/*  Check if record is to be written out. */
	if (ignore) {
		return(0);
	}

	/*  Write record.  */
	if (db_flag > 0) {
		Ndebug("Write_record(1): write spacct record (%d).\n",
			num);
	}
	if (writeacctent(sfd, &acctent) <= 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the writing of file '%s' %s."),
			spacct,
			"during write");
	}

	return(bytes);
}


/*
 * =============================== Write_Job_recs() =========================
 *	Write a sorted pacct record file.
 *	Upon entrance, the Sorted pacct file is at EOF.
 */
int
Write_Job_recs(uint64_t jid, struct segment *sptr, int index) 
{
	int64_t	absol_off[REC_MAX+1];	/* abs offset of start of rec */
	struct Crec	*cptr, *first_cptr;
	struct Frec	*fptr, *first_fptr;
	int64_t	data_off;  /* offset of end of job hdr */
	int64_t	hdr_off;   /* offset to start of job hdr */
	int	ind;
	int	ncore, nread, nwrite;
	char	*save_buf;
	int	size;
	char	string[128];

	if (db_flag > 0) {
		Ndebug("Write_Job_recs(1): Write Job ID(0x%llx) information.\n",
			jid);
	}

	/*
	 *	Initialize Job header record.
	 */
	if (create_hdr1(&Job_hdr.aj_hdr, ACCT_JOB_HEADER) < 0) {
		acct_err(ACCT_WARN,
		       _("An error was returned from the call to the '%s' routine."),
			"header()");
	}
	Job_hdr.aj_flags = 0;		/* completion status flags */
	Job_hdr.aj_uid = 0;		/* User ID */
	Job_hdr.aj_jid = 0;		/* Job ID */
	Job_hdr.aj_btime = 0;		/* Beginning time */
	Job_hdr.aj_eop_num    = 0;	/* acctcsa */
	Job_hdr.aj_eop_start  = 0;
	Job_hdr.aj_job_num    = 0;	/* acctsoj, accteoj */
	Job_hdr.aj_job_start  = 0;
	Job_hdr.aj_wkmg_num   = 0;	/* workload management */
	Job_hdr.aj_wkmg_start = 0;
	Job_hdr.aj_len = 0;		/* bytes from end of this job */
					/* header to next job header */

	/*  Initialize offset array  */
	for(ind = 0; ind < REC_MAX+1; ind++) {
		absol_off[ind] = -1;
	}

	/*  Remember where to write hdr record.  */
	if ((hdr_off = seekacct(sfd, 0, SEEK_CUR)) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s' %s."),
			spacct,
			"for the header check");
	}

	/*  Write initial empty header. */
	size = sizeof(struct acctjob);
	if (db_flag > 0) {
		Ndebug("Write_Job_recs(1): Writing initial Job header for "
			"Job ID(0x%llx).\n", jid);
	}
	if (writeacct(sfd, (char *)&Job_hdr, size) != size) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the writing of file '%s' %s."),
			spacct,
			"writing the Job hdr");
	}

	if ((data_off = seekacct(sfd, 0, SEEK_CUR)) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s' %s."),
			spacct,
			"for the start data check");
	}
	
	nbytes = ncore = nread = nwrite = 0;
	
	/*  Process each process file record in the process chain. */
	first_fptr = sptr->sg_proc;
	for(fptr = first_fptr; fptr != NULL; fptr = fptr->fp_next) {
		/*  Read the next record from process list.  */
		Read_record(fptr, jid, nread);
		nread++;
		if (db_flag > 3) {
			Ndebug("Write_Job_recs(4): Record(%d, %d), "
				"offset(%d)", nread, nwrite, fptr->fp_offset);
			Dump_acct_hdr(acctent.prime);
		}

		if (acctent.prime->ah_type != ACCT_KERNEL_CSA) {
			continue;
		}

		/*
		 *  Save the absolute offset for the first record of
		 *  each type.
		 */
		if (absol_off[fptr->fp_type] == -1) {
			if ((absol_off[fptr->fp_type] =
					seekacct(sfd, 0, SEEK_CUR)) == -1) {
				sprintf(string, "to record type %d",
					fptr->fp_type);
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred during the positioning of file '%s' %s."),
					file_tbl[fptr->fp_ind].name,
					string);
			}
		}

		/*  Write the record to the Sorted pacct file. */
		size = Write_record(fptr->fp_type, nwrite);
		if (size) {
			nbytes += size;
			nwrite++;
		}

	}  /* end of for(fptr) */
	
	/*
	 *  Process each non-process file record in the process chain.
	 *  cfg, soj and eoj records
	 */
 	first_fptr = sptr->sg_proc;
	for(fptr = first_fptr; fptr != NULL; fptr = fptr->fp_next) {
		/*  Read the next record from process list.  */
		Read_record(fptr, jid, nread);
		nread++;
		if (db_flag > 3) {
			Ndebug("Write_Job_recs(4): Record(%d, %d), "
				"offset(%d)", nread, nwrite, fptr->fp_offset);
			Dump_acct_hdr(acctent.prime);
		}

		if (acctent.prime->ah_type == ACCT_KERNEL_CSA) {
			continue;
		}

		/*
		 *  Save the absolute offset for the first record of
		 *  each type.
		 */
		if (absol_off[fptr->fp_type] == -1) {
			if ((absol_off[fptr->fp_type] =
					seekacct(sfd, 0, SEEK_CUR)) == -1) {
				sprintf(string, "to record type %d",
					fptr->fp_type);
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred during the positioning of file '%s' %s."),
					file_tbl[fptr->fp_ind].name,
					string);
			}
		}

	 	/*  Write the record to the Sorted pacct file. */
		size = Write_record(fptr->fp_type, nwrite);
		if (size) {
			nbytes += size;
			nwrite++;
		}

	}  /* end of for(fptr) */

	/*  For each core record in the workload management chain, write a
	 *  consolidated record.
	 */
        acctent.csa = (struct acctcsa *)NULL;
        acctent.mem = (struct acctmem *)NULL;
        acctent.io = (struct acctio *)NULL;
        acctent.delay = (struct acctdelay *)NULL;
	acctent.soj = (struct acctsoj *)NULL;
	acctent.eoj = (struct accteoj *)NULL;
	acctent.cfg = (struct acctcfg *)NULL;
	acctent.job = (struct acctjob *)NULL;
	acctent.wmbs = (struct wkmgmtbs *)NULL;
	acctent.other = (struct achead *)NULL;

	save_buf   = acctent.ent;
	first_cptr = sptr->sg_wm;
	for(cptr = first_cptr; cptr != NULL; cptr = cptr->cp_next) {
		/*  Get the next workload management record. */
		acctent.ent   = (char *)cptr->cp_wm;
		acctent.prime = (struct achead *)acctent.ent;
		acctent.wmbs  = (struct wkmgmtbs *)acctent.ent;
		ncore++;
		if (db_flag > 3) {
			Ndebug("Write_Job_recs(4): Workload Management "
			       "Record(%d, %d) ", ncore, nwrite);
			Dump_acct_hdr(acctent.prime);
		}

		/*
		 *  Save the absolute offset for the first record of
		 *  each type.
		 */
		if (absol_off[cptr->cp_type] == -1) {
			if ((absol_off[cptr->cp_type] =
					seekacct(sfd, 0, SEEK_CUR)) < 0) {
				sprintf(string, "to record type %d",
					cptr->cp_type);
				acct_perr(ACCT_ABORT, errno,
				  _("An error occurred during the positioning of file '%s' %s."),
					spacct, string);
			}
		}

		/*  Write the workload management record to the Sorted pacct
		    file. */
		size = Write_record(cptr->cp_type, nwrite);
		if (size) {
			nbytes += size;
			nwrite++;
		}

	}  /* end of for(cptr) */

	/*
	 *  Did this record die when the system crashed?  If not
	 *  workload mgmt, assume it is dead, unless options told us not to.
	 */
	acctent.ent = save_buf;
	if (db_flag > 3) {
		Ndebug("Write_Job_recs(4): flags(%#4o).\n", Job_hdr.aj_flags); 
	}
	if ( !(Job_hdr.aj_flags & WKMG_IN_JOB) ) {
		/*  If no option and not last uptime. */
		if (a_opt && (index+1 < upIndex) ) {
			if ((Job_hdr.aj_flags & END_OF_JOB_I) == 0) {
			    if (db_flag > 0) {
				Ndebug("Write_Job_recs(0): terminate as "
					"crashed jid(0x%llx), flag(%#4o), "
					"index(%d, %d)\n", Job_hdr.aj_jid,
					Job_hdr.aj_flags, index, upIndex);
			    }
				Job_hdr.aj_flags |= END_OF_JOB_C;
			}
		}
	}

	/*
	 *  Flag this job if it's older than the cutoff time.
	 */
	if (sptr->sg_start && (sptr->sg_start <= cutoff)) {
		Job_hdr.aj_flags |= END_OF_JOB_T;
		if (db_flag > 4) {
			Ndebug("Write_Job_recs(5): Old job - upind(%d), "
				"start(%d) - %s\n", index, sptr->sg_start,
				ctime(&sptr->sg_start));
		}
	}

	/*  Write header record - code is not order dependent */
	for(ind = 0; ind < REC_MAX+1; ind++) {
		/*
		 *  Don't have negative filler offsets
		 *  because other programs don't like them
		 */
		if (absol_off[ind] == -1) {
			absol_off[ind] = data_off;
		}
	}

	Job_hdr.aj_eop_start  = absol_off[REC_EOP] - data_off;
	Job_hdr.aj_job_start  = absol_off[REC_EOJ] - data_off;
	Job_hdr.aj_wkmg_start = absol_off[REC_WKMG] - data_off;
	Job_hdr.aj_len = nbytes;

	if (Job_hdr.aj_flags & WKMG_IN_JOB) {
		/* set the correct jid from the segment pointer */
		Job_hdr.aj_jid = sptr->sg_jid;	
	}

	if (seekacct(sfd, hdr_off, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s' %s."),
			spacct,
			"for the header offset");
	}

	size = sizeof(struct acctjob);
	if (db_flag > 0) {
		Ndebug("Write_Job_recs(1): Rewriting Job Header record for "
			"Job ID(0x%llx).\n", jid);
	}
	if (writeacct(sfd, (char *)&Job_hdr, size) != size) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the writing of file '%s' %s."),
			spacct,
			"rewriting the Job header");
	}

	/*  Seek to the end of file */
	if (seekacct(sfd, 0, SEEK_END) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s' %s."),
			spacct,
			"for the End of File");
	}

	return(nwrite);
}

/*
 * ============================ jid_compare() ===============================
 *	Sort procedure used with qsort to order the segments by jid in
 *	ascending order
 */
static int
jid_compare(const void *seg1, const void *seg2)
{
	struct segment **h1;
	struct segment **h2;
	struct segment *s1;
	struct segment *s2;

	h1 = (struct segment **)seg1;
	h2 = (struct segment **)seg2;

	s1 = *h1;
	s2 = *h2;

	if ((s1 == NULL) || (s2 == NULL)) {
		/* bad news; shouldn't happen; bail out */
		acct_err(ACCT_ABORT,
		       _("A null segment pointer was found during sorting in the '%s' routine."),
			"jid_compare()");
	}
	if (db_flag > 4) {
		Ndebug("jid_compare called, seg1 jid(0x%x), seg2 jid(0x%x)\n",
			(int)s1->sg_jid, (int)s2->sg_jid);
	}

	if (s1->sg_jid < s2->sg_jid) {
		return -1;
	} else if (s1->sg_jid > s2->sg_jid) {
		return 1;
	} else {
		return 0;
	}
}
