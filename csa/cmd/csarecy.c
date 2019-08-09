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

/*
 * csarecy.c:
 *		Program to save job record data which needs to be
 *		propagated across accounting runs. We check each job
 *		for either a workload mgmt end of request record, or if 
 *		the job is not a workload mgmt request, for an accteoj
 *		record. If the job does not contain these end of job indicators,
 *		the job header record is broken into its component pieces and
 *		written to pacct0.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <limits.h>

#include <sys/stat.h>
#include <time.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csaacct.h"

typedef struct UpEnt {
	time_t	starttime;	/* uptime start time */
	time_t	stoptime;	/* uptime stop time */
	int	used;		/* FALSE = don't recycle, TRUE = recycle */
} UpEnt;

typedef struct UpTbl {
	UpEnt	*first;
	UpEnt	*curr;
	int	curr_ind;
	int	nused;		/* # of table entries in use */
	int	size;		/* total # of table entries allocated */
} UpTbl;

extern int	db_flag;

char	*Prg_name = "";
int	total_uptimes = 0;
int	total_jobs = 0;
int	saved_uptimes = 0;
int	saved_jobs = 0;

int	A_opt = FALSE;		/* interactive ask on job recycling option */
int	r_opt = FALSE;		/* report option */
int	R_opt = FALSE;		/* report only option - no job recycling */

int	once = TRUE;

int	pfd0 = -1;		/* recycled pacct file descriptor */
int	sfd = -1;		/* sorted pacct file descriptor */
int	ufd0 = -1;		/* recycled upfile file descriptor */

char	path[ACCT_PATH+1];
char	*pacct0 = WPACCT;	/* recycled pacct file */
char	*spacct = SPACCT;	/* input sorted pacct file */
char	*uptime0 = "uptimes0";	/* recycled uptime file */

time_t	uptime_start = 0;	/* current uptime start time */
time_t	uptime_stop = 0;	/* current uptime stop time */
UpTbl	*Up;			/* up table */


typedef	enum {
	REC_EOP,
	REC_JOB,
	REC_WKMG,
	REC_MAX
} ac_rec;

#define UP_SIZ		20	/* # up table entries to allocate at 1 time */

char	acctentbuf[ACCTENTSIZ];		/* accounting record buffer */
struct	acctent   acctent = {acctentbuf};

struct	ac_utsname	cfg_sysname;	/* config system name */
struct	ac_utsname	*cfg_sn = &cfg_sysname;

struct	acctcfg	Up_hdr;			/* Uptime header structure */

#define MAXTIME ((long)(1L<<30))
time_t	btime = MAXTIME;

uint64_t jobid = -1;
uid_t	user = -1;
uint64_t project = -1;

static	void	get_uptimes();
static	void	print_report(int, struct acctent *);
static	int	preserve_job(struct acctjob *, int64_t);
static	int	preserve_rec(struct acctjob *, int, char *);
static	int	query();
static	void	scan_spacct();
static	void	Uptime_record(int);

/*
 *	usage() - program usage message.
 */
static void
usage() {
	acct_err(ACCT_ABORT,
	       "Command Usage:\n%s\t[-r] [-s spacct] [-A] [-D debug] [-P pacct] [-R]",
		Prg_name);
}


/*
 *	Dump_acct_hdr() - Display an accounting record header.
 */
void
Dump_acct_hdr(struct achead *hdr) 
{
	Ndebug(" header revision(%#o), type(%#o), flag(%#o), size(%d).\n",
		hdr->ah_revision, hdr->ah_type, hdr->ah_flag, hdr->ah_size);
	return;
}

/*
 *	main() - main program including parameter processing and main loop.
 */
int
main(int argc, char **argv)
{
	char		ch;
	int		c;
	extern char	*optarg;

	Prg_name = argv[0];
	while((c = getopt(argc, argv, "AD:P:Rrs:")) != EOF) {
		ch = (char)c;

		switch(ch) {

		case 'A':		/* ask option */
			r_opt = A_opt = TRUE;
			break;

		case 'D':		/* debug level option */
			db_flag = atoi(optarg);
			if ((db_flag < 0) || (db_flag > 2)) {
				acct_err(ACCT_FATAL,
				       _("The (-%s) option's argument, '%s', is invalid."),
					"D", optarg);
				Nerror("Option -D valid values are 1 and 2\n");
				usage();
			}
				
			setvbuf(stderr, NULL, _IONBF, 0);
			Ndebug("Debugging option set to level %d.\n", db_flag);
			break;
		case 'P':
			pacct0 = optarg;
			break;

		case 'R':		/* report, no recycle option */
			R_opt = 1;
		case 'r':		/* report option */
			r_opt = 1;
			break;

		case 's':		/* sorted pacct file */
			spacct = optarg;
			break;

		default:
			acct_err(ACCT_FATAL,
			       _("An unknown option '%c' was specified."),
				optopt);
			usage();

		}		/* end of switch(ch) */
	}		/* end of while(getopt) */

	if  (A_opt) {
		FILE *in = stdin;               /* input file */
		int	iactive;

		iactive = isatty(fileno(in));
		if  (!iactive) {
			acct_err(ACCT_WARN,
			       _("The (-%s) option is only valid in interactive mode."),
				"A");
		}

		if (R_opt) {
			acct_err(ACCT_FATAL,
			       _("The (-%s) and the (-%s) options are mutually exclusive."),
				"A", "R");
			usage();
		}
	}

	strcpy(path, pacct0);
	strcat(path, "0");	
	pacct0 = path;

	/*  Open the accounting files. */
	if (db_flag > 0) {
		Ndebug("main(1): Open accounting files.\n");
	}

	if ((sfd = openacct(spacct, "r")) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			spacct);
	}

	if ((pfd0 = openacct(pacct0, "w")) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			pacct0);
	}

	/*  Process the spacct file. */
	get_uptimes();
	scan_spacct();

	closeacct(pfd0);

	if (db_flag > 0) {
		fprintf(stdout, " file '%s' uptimes (%d), jobs(%d)\n",
			spacct, total_uptimes, total_jobs);
		fprintf(stdout, " file '%s' uptimes (%d), jobs(%d)\n",
			pacct0, saved_uptimes, saved_jobs);
	}

	exit(0);
}


/*
 *	scan_spacct() - process the information from a sorted pacct file.
 *
 *	for(each up period) {
 *		for(each job record) {
 *			read header;
 *			if ( !interested ) continue
 *			read records;
 *			write records;
 *		}
 *	}
 */
static void
scan_spacct() 
{
	struct	acctcfg uphdr;
	struct	acctjob	jobhdr;
	int64_t	job_offset;
	int	nbytes = 0;
	int	size;
	int	upsize;
	int64_t	up_offset;
	int	want;
	int	uptime_num = 0;
	int64_t	uptime_size;

	if (db_flag > 0) {
		Ndebug("scan_spacct(1): Read file '%s'.\n", spacct);
	}

	/*
	 *	Read each uptime record.
	 */
	total_uptimes = 0;
	upsize = sizeof(struct acctcfg);
	while(readacct(sfd, (char *)&uphdr, upsize) == upsize) {
		up_offset = seekacct(sfd, 0, SEEK_CUR);
		total_uptimes++;

		if (db_flag > 1) {
			Ndebug("scan_spacct(2): UpTime record(%d), "
				"offset(%lld, %#llo):",
				total_uptimes, up_offset, up_offset);
			Dump_acct_hdr((struct achead *)&uphdr);
		}

		Up->curr = Up->first + uptime_num;
		Up->curr_ind = uptime_num;

		/*
		 *	Remember size of uptime.
		 */
		uptime_size  = uphdr.ac_uptimelen;
		uptime_start = uphdr.ac_boottime;	/* uptime start time */
		once = TRUE;

		/*
		 *	While still in this uptime, read the Job header 
		 *	records.
		 */
		size = sizeof(struct acctjob);
		while(uptime_size && (readacct(sfd, (char *)&jobhdr, size) ==
				size) ) {
			/*
			 *	Remember Start of Job record header.
			 */
			job_offset = seekacct(sfd, 0, SEEK_CUR);
			total_jobs++;

			if (db_flag > 1) {
				Ndebug("scan_spacct(2): Job record(%d), "
					"offset(%lld, %#llo), flag(%#o):",
					total_jobs, job_offset, job_offset,
					jobhdr.aj_flags);
				Dump_acct_hdr((struct achead *)&jobhdr);
			}

			/*
			 *	Has this job terminated?  We only
			 *	recycle active jobs.
			 */
			want = TRUE;
			if (jobhdr.aj_flags & END_OF_JOB_B) {
				want = FALSE;
			}
			if (jobhdr.aj_flags & END_OF_JOB_C) {
				want = FALSE;
			}
			if (jobhdr.aj_flags & END_OF_JOB_T) {
				want = FALSE;
			}
			if (!(jobhdr.aj_flags & WKMG_IN_JOB) &&
			     (jobhdr.aj_flags & END_OF_JOB_I) ) {
				want = FALSE;
			}
			if (want) {
				if ((nbytes = preserve_job(&jobhdr,
							   job_offset)) >= 0) {
					Up->curr->used = TRUE;
					Up_hdr.ac_uptimelen += nbytes;
					saved_jobs++;
				}
			}

			/*
			 *	Insure position at next Job record.
			 */
			uptime_size -= sizeof(struct acctjob) + jobhdr.aj_len;
			job_offset = job_offset + jobhdr.aj_len;
			if (seekacct(sfd, job_offset, SEEK_SET) < 0) {
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred on the seek to the next '%s' record."),
					"job header");
			}
		} /* end while jobs */

		/*
		 *	Insure we are at next uptime.
		 */
		if (seekacct(sfd, 0, SEEK_CUR) != uphdr.ac_uptimelen + up_offset) {
			acct_err(ACCT_WARN,
			       _("The position in file '%s' is wrong for the next %s record.\n   Examine the data file for correct version of the file.\n   (Match the binary with the record structure.)"),
				spacct, "uptime");
		}
		uptime_num++;

	} /* end while uptime */

	/*  Rewrite the final Uptime header. */
	Uptime_record(FALSE);

	return;
} 

/*
 *	preserve_job() - writes job data into the pacct0 file.
 */
int	do_report;
int	first_rec;
int64_t	rcd_offset = 0;
int	write_ok;
static int
preserve_job(struct acctjob *jh, int64_t offset)
{
	int	ind, jnd;
	int	nbytes = 0;

	UpEnt	*ent;

	btime = MAXTIME;
	do_report = r_opt;
	first_rec = TRUE;
	write_ok = TRUE;
	if (R_opt) {
		write_ok = FALSE;
	}

	/*
	 *	Workload management records.
	 */
	if (seekacct(sfd, offset + jh->aj_wkmg_start, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
		 _("An error occurred on the seek to the first '%s' record."),
		   "Workload Management"); 
	}
	if ((rcd_offset = seekacct(pfd0, 0, SEEK_CUR)) < 0) {
		acct_perr(ACCT_ABORT, errno,
		 _("An error occurred during the positioning of file '%s' %s."),
		   pacct0, "Workload Management record position");
	}
	
	for (ind = 0; ind < jh->aj_wkmg_num; ind++) {
		(void)preserve_rec(jh, REC_WKMG,
				   "workload management record");
		
		if (write_ok) {
			nbytes += acctent.prime->ah_size;
		}		/* if write_ok */

	}		/* end of for(ind) */
	/*
	 *	Process records.
	 */
	if (seekacct(sfd, offset + jh->aj_eop_start, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
		  _("An error occurred on the seek to the first '%s' record."),
			" process");
	}
	if ((rcd_offset = seekacct(pfd0, 0, SEEK_CUR)) < 0) {
		acct_perr(ACCT_ABORT, errno,
		 _("An error occurred during the positioning of file '%s' %s."),
		    pacct0, "process record position");
	}

	for(ind = 0; ind < jh->aj_eop_num; ind++) {
		if (preserve_rec(jh, REC_EOP, "process record") < 0) {
			return(-1);
		}

		if (write_ok) {
			nbytes += acctent.prime->ah_size;
			nbytes += acctent.csa->ac_hdr2.ah_size;
		}
	}		/* end of for(ind) */

	/*
	 *	Job records (SOJ, EOJ, config records)
	 */
	if (seekacct(sfd, offset + jh->aj_job_start, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
		   _("An error occurred on the seek to the first '%s' record."),
			"soj, eoj and cfg");
	}
	if ((rcd_offset = seekacct(pfd0, 0, SEEK_CUR)) < 0) {
		acct_perr(ACCT_ABORT, errno,
		 _("An error occurred during the positioning of file '%s' %s."),
			pacct0, "soj, eoj and cfg record position");
	}

	for(ind = 0; ind < jh->aj_job_num; ind++) {
		(void)preserve_rec(jh, REC_JOB, "soj, eoj, or cfg record");
		
		if (write_ok) {
			nbytes += acctent.prime->ah_size;
			if (acctent.eoj) {
				nbytes += acctent.eoj->ac_hdr2.ah_size;
			}
		}
	}		/* end of for(ind) */

	return(nbytes);
} 

/*
 *	preserve_rec() - writes record into the pacct0 file.
 */
static int
preserve_rec(struct acctjob *jh, int type, char *name)
{
	/*
	 *	Read the next record.
	 */
	if (readacctent(sfd, &acctent, FORWARD) <= 0) {
		acct_err(ACCT_ABORT,
		       _("An error occurred during the reading of file '%s' %s."),
			spacct, name);
	}

	if (acctent.csa && (acctent.csa->ac_jid == 0) ) {
		return(-1);
	}

	if (db_flag > 1) {
		Ndebug("preserve_rec(4): %s record, offset(%d, %#o), type %d:",
			name, rcd_offset, rcd_offset, type);
		Dump_acct_hdr(acctent.prime);
	}

	if (do_report) {
		print_report(type, &acctent);
		do_report = 0;
		if (A_opt) {
			write_ok = query();
		}
	}

	if (write_ok) {
		if (first_rec) {
		/*
		 *	Create and write an uptime record if needed.
		 */
			if (jh->aj_btime > uptime_stop) {
				Uptime_record(TRUE);
			}

			if ((rcd_offset = seekacct(pfd0, 0, SEEK_CUR)) < 0) {
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred during the positioning of file '%s' %s."),
					pacct0,
					"for the job entry");
			}
			first_rec = FALSE;
		}

		if (writeacctent(pfd0, &acctent) <= 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the writing of file '%s'."),
				pacct0);
		}
	}

	return(0);
}


/*
 *	query()  - ask user if job should be preserved?
 *		(Y)es return TRUE, (n)o return FALSE, else retry.
 */
static int
query() {
	char buf[80];

	for(; 1;) {
		fprintf(stdout, _("Preserve this Job? (Y/n) "));
		if (fgets(buf, 80, stdin) == NULL) {
			return(TRUE);
		}
		if (buf[0] == 'y' || buf[0] == 'Y') {
			return(TRUE);
		} 
		if (buf[0] == 'n' || buf[0] == 'N') {
			return(FALSE);
		}
		fprintf(stdout, _("Please reply either Y(es) or n(o).\n"));
	}
}


/*
 *	print_report() - output a record for the report request.
 */
static void
print_report(int rec, struct acctent *ac)
{
	char		buf[160];
	char 		*start_time;
	char	*cp;
	char	*cp1;

	if (once) {
		if (uptime_start) {
			start_time = ctime(&uptime_start);
		} else {
			start_time = "Unknown\n";
		}

		fprintf(stdout, "\n\n");
		fprintf(stdout, "SYSTEM BOOT TIME STARTING AT %s", start_time);
		fprintf(stdout, "For %s %s %s %s %s\n", cfg_sn->sysname,
			cfg_sn->nodename, cfg_sn->release, cfg_sn->version,
			cfg_sn->machine);
		fprintf(stdout, "Preserved Accounting Jobs (Jobs which are "
			"continued).\n");
		fprintf(stdout, "=========================================="
			"==========\n\n");
		fprintf(stdout, "      JOB ID          USERS      PROJECT ID   "
			"         STARTED\n");
		fprintf(stdout, "------------------    -----    -------------  "
			"------------------------\n");
		once = FALSE;
	}

	switch (rec) {

	case REC_EOP:
		/*
		 *	Report about this preserved process.
		 */
		if (ac->csa != NULL ) {
			jobid = ac->csa->ac_jid;
			sprintf(buf, "%#18llx ", jobid);

			user = ac->csa->ac_uid;
			cp = uid_to_name(user);
			if (cp[0] == '?') {
				sprintf(&buf[19], "%8d  ", user);
			} else {
				sprintf(&buf[19], "%8s  ", cp);
			}
			project = ac->csa->ac_prid;
			if ((cp = prid_to_name(project)) == NULL) {
				cp="Unknown";
			}
			sprintf(&buf[28], "%15s  ", cp);

			if (ac->csa->ac_btime < btime) {
				btime = ac->csa->ac_btime;
			}
			fprintf(stdout, "%s", buf);
		}
		if (btime < MAXTIME) {
			fprintf(stdout, " %24s", ctime(&btime));
		} else
			fprintf(stdout, "\n");
		break;

	case REC_JOB:
	    {
		struct acctsoj *soj;
		struct accteoj *eoj;
		soj = ac->soj;
		eoj = ac->eoj;

		if (soj != NULL) {
			user = soj->ac_uid;
			cp = uid_to_name(user);
			if (cp[0] == '?') {
				sprintf(buf, "%8d  ", user);
			} else {
				sprintf(buf, "%8s  ", cp);
			}
			if (soj->ac_type == AC_SOJ) {
			    fprintf(stdout, "%#18llx %8s    SOJ record   %s",
				soj->ac_jid, buf,
				ctime(&soj->ac_btime));
			} else {
			    fprintf(stdout, "%#18llx %8s    ROJ record   %s",
				soj->ac_jid, buf,
				ctime(&soj->ac_rstime));
			}
		} else if (eoj != NULL) {
			user = eoj->ac_uid;
			cp = uid_to_name(user);
			if (cp[0] == '?') {
				sprintf(buf, "%8d  ", user);
			} else {
				sprintf(buf, "%8s  ", cp);
			}
			fprintf(stdout, "%#18llx %8s    EOJ record   %s",
				eoj->ac_jid, buf,
				ctime(&eoj->ac_btime));
		}
		/* CFG records should only be associated with jid 0 */
		if ((ac->cfg != NULL) && (ac->csa != NULL)) {
			fprintf(stdout, "CFG rec_job with jid 0x%llx \n",
				ac->csa->ac_jid);
		}
	    }
	    break;

	case REC_WKMG:
	    {
		struct wkmgmtbs *wm;
		wm = ac->wmbs;

		user = wm->uid;
		cp = uid_to_name(user);
		if (cp[0] == '?') {
			sprintf(buf, "%8d  ", user);
		} else {
			sprintf(buf, "%8s  ", cp);
		}
		if (wm->jid == WM_NO_JID) {
                        fprintf(stdout, "No job id          %8s     ", buf);
			fprintf(stdout, "%s record   %s",
				wm->serv_provider, ctime(&wm->time));
                } else {
                        fprintf(stdout, "%#18llx %8s    %s record   %s",
                                wm->jid, buf, wm->serv_provider,
				ctime(&wm->time));
                }
	    }
	    break;

	default:
		acct_err(ACCT_ABORT,
		       _("An unknown record type (%4o) was found in the '%s' routine."),
			rec, "print_report()");

	}		/* end of switch(rec) */

	return;
}


/*
 *	get_uptimes() - Extract the uptimes from the sorted pacct file.
 *		The uptimes are put in the up table.
 */
static void
get_uptimes(void)
{
	static	int	first = TRUE;
	int	size;
	int64_t	offset = 0;
	int	upsize;

	struct	acctcfg	uphdr;
	UpEnt		*ent;

	if (db_flag > 0) {
		Ndebug("get_uptimes(1): allocate uptime table.\n");
	}

	/*
	 * Allocate the up table.
	 */
	if ((Up = (UpTbl *)malloc(sizeof(UpTbl))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"UpTbl array");
	}

	size = sizeof(UpEnt) * UP_SIZ;
	if ((ent = (UpEnt *)malloc(size)) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"UpTbl entries");
	}
	memset((char *)ent, '\0', size);

	Up->first = ent;
	Up->curr  = ent;
	Up->nused = 0;
	Up->size  = UP_SIZ;

	/*
	 * Scan the file for the Up time records.
	 */
	if (db_flag > 0) {
		Ndebug("get_uptimes(1): scan file '%s' for up times.\n",
			spacct);
	}
	upsize = sizeof(struct acctcfg);
	total_uptimes = 0;
	while (readacct(sfd, (char *)&uphdr, upsize) == upsize) {
		total_uptimes++;

		if (db_flag > 1) {
			Ndebug("get_uptimes(2): CFG  record(%d), "
				"offset(%lld, %#llo):",
				total_uptimes, offset, offset);
			Dump_acct_hdr((struct achead *)&uphdr);
		}
		offset = uphdr.ac_uptimelen;	/* # bytes to next Uphdr */

		if (first) {
			strncpy(cfg_sn->sysname,  uphdr.ac_uname.sysname, 25);
			strncpy(cfg_sn->nodename, uphdr.ac_uname.nodename, 25);
			strncpy(cfg_sn->release,  uphdr.ac_uname.release, 41);
			strncpy(cfg_sn->version,  uphdr.ac_uname.version, 41);
			strncpy(cfg_sn->machine,  uphdr.ac_uname.machine, 25);
			first = FALSE;
		}

		/*
		 * Make sure that there is enough room in the Up table.
		 */
		if (Up->nused == Up->size) {
			size = (Up->size + UP_SIZ) * sizeof(UpEnt);
			if ((ent = (UpEnt *)realloc(Up->first, size)) == NULL) {
				acct_perr(ACCT_ABORT, errno,
					_("There was insufficient memory available when reallocating '%s'."),
					"UpTbl entries");
			}
			memset((char *)(ent + Up->size), '\0',
				sizeof(UpEnt) * UP_SIZ);
			Up->curr  = &ent[Up->size - 1];
			Up->first = ent;
			Up->size += UP_SIZ;
		}

		if (Up->nused > 0) {
			Up->curr++;
		}
		(Up->nused)++;
		Up->curr->starttime = uphdr.ac_boottime;
		Up->curr->stoptime  = uphdr.ac_curtime;
		Up->curr->used = FALSE;

		/* Position the file to the start of the next Up time. */
		seekacct(sfd, offset, SEEK_CUR);

	}  /* while readacct(uphdr) */

	/* Rewind the file to the beginning. */
	seekacct(sfd, 0, SEEK_SET);

	return;
}


/*
 *	Uptime_record() - writes uptime record into the pacct0 file.
 */
static	void
Uptime_record(int flag)
{
	static	int64_t	uphdr_offset = -1;
	int	size;
	struct	acctcfg	*uphdr = &Up_hdr;

	/*
	 *	If initial copy of header already written.
	 */
	if (uphdr_offset >= 0) {
		/*
		 *	Write the final version of the up time header.
		 */
		if (seekacct(pfd0, uphdr_offset, SEEK_SET) < 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the positioning of file '%s' %s."),
				pacct0,
				"for Up time header offset");
		}
		size =  sizeof(struct acctcfg);
		if (db_flag > 0) {
			Ndebug("Uptime_record(1): Rewriting Boot record(%24.24s),"
				" offset(%lld), length(%lld), size(%d).\n",
				ctime(&(uphdr->ac_boottime)), uphdr_offset,
				uphdr->ac_uptimelen, size);
		}
		if (writeacct(pfd0, (char *)uphdr, size) != size) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the writing of file '%s' %s."),
				pacct0,
				"rewrite Up time header");
		}
	}

	/*  Seek to the end of file and remember position. */
	if (flag) {
		if ((uphdr_offset = seekacct(pfd0, 0, SEEK_END)) < 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the positioning of file '%s' %s."),
				pacct0,
				"to the End of File");
		}

		if (create_hdr1(&(uphdr->ac_hdr), ACCT_KERNEL_CFG) < 0) {
			acct_err(ACCT_WARN,
			       _("An error was returned from the call to the '%s' routine."),
				"header()");
		}

		uphdr->ac_kdmask  = 0;
		uphdr->ac_rmask  = 0;
		uphdr->ac_uptimelen = 0;
		uphdr->ac_event = AC_CONFCHG_BOOT;

		uphdr->ac_boottime = Up->curr->starttime;
		uphdr->ac_curtime  = Up->curr->stoptime;
		uptime_stop	   = Up->curr->stoptime;

		strncpy(uphdr->ac_uname.sysname, cfg_sn->sysname,  25);
		strncpy(uphdr->ac_uname.nodename, cfg_sn->nodename, 25);
		strncpy(uphdr->ac_uname.release, cfg_sn->release,  41);
		strncpy(uphdr->ac_uname.version, cfg_sn->version,  41);
		strncpy(uphdr->ac_uname.machine, cfg_sn->machine,  25);

		/*  Write initial version of the uptime (config) record. */
		size =  sizeof(struct acctcfg);
		if (db_flag > 0) {
			Ndebug("Uptime_record(1): Writing Boot record(%24.24s),"
				" offset(%lld), length(%lld), size(%d).\n",
				ctime(&(uphdr->ac_boottime)), uphdr_offset,
				uphdr->ac_uptimelen, size);
		}
		if (writeacct(pfd0, (char *)uphdr, size) != size) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the writing of file '%s' %s."),
				pacct0,
				"Up time header");
		}
		saved_uptimes++;
	}

	return;
} 
