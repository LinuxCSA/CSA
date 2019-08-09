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
 * csacon.c:
 *	Condense job records for periodic accounting.  Records can
 *	be collapsed by a combination of uid/prid/jid/gid.
 *
 *	If the records are not collapsed by a particular field, that field
 *	is set to CACCT_NO_ID.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/sysmacros.h>

#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "csaacct.h"
#include "cacct.h"
#include "sbu.h"


extern	int	db_flag;	/* debug flag */
extern char	*optarg;

char	*Prg_name;
int	sfd = -1;
#define	I_PRIME	0
#define	I_NONPRIME	1

char	*spacct	= SPACCT;

#define	REC_EOP		1<<0
#define	REC_JOB		1<<1
#define REC_WKMG	1<<2

struct	map	{
	uint64_t	prid;
	uid_t		uid;
	uint64_t	jid;
	gid_t		group;
	int		ijob;
	struct	cacct	*acp;
};

struct	map	*acmap	= NULL;
#define	MAPSIZE		500		/* initial map size */
#define	MAPINC		200		/* map increment */

struct sbutype {
	double		sbu_wt;	/* workload mgmt sbu multiplier */
	uint64_t	jid;	/* jid of this workload mgmt segment */
};

struct sbutype		*mult;
#define SEGINC		10	/* workload mgmt segments per job incr */
int	max_sbusize	= SEGINC;

int	MEMINT	= 0;

int	mapsize	= 0;
int	A_flg = 0;
int	g_flg = 0;
int	j_flg = 0;
int	p_flg = 0;
int	u_flg = 0;

static	void	add_map(uid_t, uint64_t, uint64_t, gid_t, int);
static	void	proc_job(struct acctjob *, int64_t);
static	struct	cacct	*find_acd(uid_t, uint64_t, gid_t, uint64_t);
static	void	init_config();
static	void	scan_srecs();
static	void	sum_rec(int, struct acctent *, double, int);
static	void	wr_output();

static void
usage() {
	acct_err(ACCT_ABORT,
	       "Command Usage:\n%s\t[-A] [-jpu [-g]] [-D level] [-s spacct]",
		Prg_name);
}

int
main(int argc, char **argv)
{
	char	ch;
	int	c;
	int	size;

	Prg_name = argv[0];
	while((c = getopt(argc, argv, "AD:gjps:u")) != EOF) {
		ch = (char)c;
		switch(ch) {

		case 'A':		/* All job option */
			A_flg = 1;
			break;
		case 'D':		/* debug option */
			db_flag = atoi(optarg);
			if ((db_flag < 0) || (db_flag > 6)) {
				acct_err(ACCT_FATAL,
				       _("The (-%s) option's argument, '%s', is invalid."),
					"D", optarg);
				Nerror("Option -D valid values are 0 thru 6\n");
				usage();
			}	
			break;
		case 'g':		/* consolidate by group ID */
			g_flg = 1;
			break;
		case 'j':		/* consolidate by job ID */
			j_flg = 1;
			break;
		case 'p':		/* consolidate by project ID */
			p_flg = 1;
			break;
		case 's':		/* sorted pacct file */
			spacct = optarg;
			break;
		case 'u':		/* consolidate by user ID */
			u_flg = 1;
			break;
		default:
			acct_err(ACCT_FATAL,
			       _("An unknown option '%c' was specified."),
				optopt);
			usage();
		} /* end switch */
	} /* end while getopt */

	/*
	 *	If no consolidate criteria specified use prid/user pair
	 */
	if ((p_flg + j_flg + u_flg + g_flg) == 0) {
		p_flg = u_flg = 1;
	}

	/*
	 *	If group only send warning and exit
	 *	since not all records have groups.
	 */
	if ((p_flg + j_flg + u_flg) == 0) {
		usage();
	}

	/*
	 *	Get the system parameters from the configuration file.
	 */
	init_config();

	if ((sfd = openacct(spacct, "r")) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			spacct);
	}

	size = max_sbusize * sizeof(struct sbutype);
	if ((mult = (struct sbutype *)malloc(size)) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"SBU table");
	}
		
	/*
	 *	Scan the job record file for input data.
	 */
	scan_srecs();

	/*
	 *	Write the output data
	 */
	wr_output();

	exit(0);
}

/*
 *	for(each up period) {
 *		for(each job record) {
 *			read header;
 *			if ( !intrested ) continue
 *			read records;
 *			write records;
 *		}
 *	}
 */
static void
scan_srecs() 
{
	struct	acctcfg uphdr;
	struct	acctjob	srhdr;
	int64_t	offset = 0;
	int64_t	uoffset;
	int	wanted = 0;
	int64_t	uptime_size;

	/*  Read each uptime record. */
	while(readacct(sfd, (char *)&uphdr, sizeof(struct acctcfg)) ==
			sizeof(struct acctcfg)) {
		uoffset = seekacct(sfd, 0, SEEK_CUR);
		/*  Remember size of uptime. */
		uptime_size = uphdr.ac_uptimelen;
		if (db_flag > 2) {
			fprintf(stderr, "scan_srecs(): Uptime record "
				"boottime (%24.24s), len (%lld)\n",
				ctime(&uphdr.ac_boottime), uphdr.ac_uptimelen);
		}
		/*
		 *	While still in this uptime, read the Job record 
		 *	Headers.
		 */
		while(uptime_size &&
		    readacct(sfd, (char *)&srhdr, sizeof(struct acctjob)) ==
				sizeof(struct acctjob)) {
			/*  Remember Start of Job record header. */
			offset = seekacct(sfd, 0, SEEK_CUR);
			if (db_flag > 2) {
				fprintf(stderr,"scan_srecs(): Job header record"
					" jid (0x%llx), uid (%d),"
					" btime(%24.24s)\n",
					srhdr.aj_jid, srhdr.aj_uid,
					ctime(&srhdr.aj_btime));
			}
			/*
			 *	Check job header for eoj indicator.
			 *	(Has this job terminated?)
			 */
			wanted = 0;
			if (A_flg) wanted = 1;
			if (srhdr.aj_flags & END_OF_JOB_B) wanted = 1;
			if (srhdr.aj_flags & END_OF_JOB_C) wanted = 1;
			if (srhdr.aj_flags & END_OF_JOB_T) wanted = 1;
			if ((!(srhdr.aj_flags & WKMG_IN_JOB)) &&
				(srhdr.aj_flags & END_OF_JOB_I) ) {
				wanted = 1;
			}
			if (wanted) {
				wanted = 0;
				proc_job(&srhdr, offset);
			}

			/*
			 *	Insure position at next Job record.
			 */
			uptime_size -= sizeof(struct acctjob) + srhdr.aj_len;
			offset = offset + srhdr.aj_len;
			if (seekacct(sfd, offset, SEEK_SET) < 0) {
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred during the positioning of file '%s'."),
					spacct);
			}
		}

		/*
		 *	Insure we are at next uptime.
		 */
		if (seekacct(sfd, 0, SEEK_CUR) != uphdr.ac_uptimelen + uoffset) {
			acct_err(ACCT_WARN,
			       _("The position in file '%s' is wrong for the next %s record.\n   Examine the data file for correct version of the file.\n   (Match the binary with the record structure.)"),
				spacct, "uptime");
		}
	} /* end while uptime */

	return;
} 


/* 
 *	Static data definitions for proc_job.
 */
char	acctentbuf[ACCTENTSIZ];		/* accounting record buffer */
static struct acctent   acctrec = {acctentbuf};
int		numseg;		/* # workload mgmt segments for this job */
uint64_t	prev_jid;

/*
 *	proc_job() called for all jobs which ended.
 *		Reads each record in the job.
 *		Summaries are done by uid/prid pairs
 */
static int TMASK = END_OF_JOB_I | END_OF_JOB_C | END_OF_JOB_T;

static void
proc_job(struct acctjob *sh, int64_t offset)
{
	int ind;
	int size;
	struct	sbutype *tmult;
	int	eoj_by_crash = 0;

	/*
	 *	workload mgmt records - this must be done first!
	 */
	numseg = sh->aj_wkmg_num;
	while (numseg > max_sbusize) {
		max_sbusize += SEGINC;
		size = max_sbusize * sizeof(struct sbutype);

		if ((tmult = (struct sbutype *)realloc(mult, size)) == NULL){
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when reallocating '%s'."),
				"SBU table");
		}
		mult = tmult;
	}

	/*
	 *	Workload management records.
	 */
	if (db_flag > 2) {
		fprintf(stderr, "proc_job: seek workload management - "
			"offset(%lld), start(%d), num(%d).\n", offset,
			sh->aj_wkmg_start, sh->aj_wkmg_num);
	}
	if (seekacct(sfd, offset + sh->aj_wkmg_start, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
		    _("An error occurred during the positioning of file '%s'."),
		      spacct);
	}

	for (ind = 0; ind < sh->aj_wkmg_num; ind++) {
		if (readacctent(sfd, &acctrec, FORWARD) <= 0) {
			acct_perr(ACCT_ABORT, errno,
			 _("An error occurred during the reading of file %s' %s."),
			    spacct, "for workload management record");
		}
		if (db_flag > 3) {
			fprintf(stderr, "proc_job: workload management - "
				"revision(%#o), type(%#o), flag(%#o), "
				"size(%d).\n",
				acctrec.prime->ah_revision,
				acctrec.prime->ah_type,
				acctrec.prime->ah_flag,
				acctrec.prime->ah_size);
		}

		if (!acctrec.wmbs) {
			acct_err(ACCT_ABORT,
			 _("An error occurred during the reading of file '%s' %s."),
			    spacct, "for workload management record");
		}

		mult[ind].sbu_wt = acctrec.wmbs->sbu_wt;
		mult[ind].jid = acctrec.wmbs->jid;
		sum_rec(REC_WKMG, &acctrec, 0.0, 0);
	}
	if (sh->aj_flags & END_OF_JOB_C) {
		eoj_by_crash = 1;
	}

	/*  Flag if All jobs wanted and this is an active job. */
	if (A_flg && !eoj_by_crash) {
		if (!(sh->aj_flags & TMASK)) {
			eoj_by_crash = 1;
		}
	}

	/*  Process records. */
	if (db_flag > 2) {
		fprintf(stderr, "proc_job: seek Process - offset(%lld), "
			"start(%d), num(%d).\n", offset, sh->aj_eop_start,
			sh->aj_eop_num);
	}
	if (seekacct(sfd, offset + sh->aj_eop_start, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s'."),
			spacct);
	}

	prev_jid = CACCT_NO_ID;
	for(ind = 0; ind < sh->aj_eop_num; ind++) {
		if (readacctent(sfd, &acctrec, FORWARD) <= 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the reading of file '%s' %s."),
				spacct,
				"for process record");
		}
		if (db_flag > 3) {
			fprintf(stderr, "proc_job: Process - revision(%#o), "
				"type(%#o), flag(%#o), size(%d).\n",
				acctrec.prime->ah_revision,
				acctrec.prime->ah_type,
				acctrec.prime->ah_flag,
				acctrec.prime->ah_size);
		}

		if (!acctrec.csa) {
			acct_err(ACCT_ABORT,
			       _("An error occurred during the reading of file '%s' %s."),
				spacct,
				"for process record");
		}

		sum_rec(REC_EOP, &acctrec, 0.0, eoj_by_crash);
		eoj_by_crash = 0;
	}

	/*  Job records (soj, eoj, and config records).  */
	if (db_flag > 2) {
		fprintf(stderr, "proc_job: seek soj, eoj, or cfg - "
			"offset(%lld), start(%d), num(%d).\n", offset,
			sh->aj_job_start, sh->aj_job_num);
	}
	if (seekacct(sfd, offset + sh->aj_job_start, SEEK_SET) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the positioning of file '%s'."),
			spacct);
	}

	for(ind = 0; ind < sh->aj_job_num; ind++) {
		if (readacctent(sfd, &acctrec, FORWARD) <= 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the reading of file '%s' %s."),
				spacct,
				"for soj, eoj or cfg record");
		}
		if (db_flag > 3) {
			fprintf(stderr, "proc_job: soj, eoj, or cfg - "
			     "revision(%#o), type(%#o), flag(%#o), size(%d).\n",
				acctrec.prime->ah_revision,
				acctrec.prime->ah_type,
				acctrec.prime->ah_flag,
				acctrec.prime->ah_size);
		}

		sum_rec(REC_JOB, &acctrec, 0.0, eoj_by_crash);
	}

	/*
	 *	If we are consolidating by Job ID increment internal job
	 *	counter.
	 */
	if (j_flg) {
		j_flg++;
	}

	return;
}


/*
 *	Routine to summmarize records...
 *	ca_njobs:
 *              If -pu[g] is specified and the job did not end because of a
 *              crash, ca_njobs is incremented for the EOJ record.  If the
 *              job ended because of a crash, ca_njobs is incremented for the
 *              uid/prid/[group] on the first process record.
 *
 *		Thus, it is possible that some output records will have 
 *		ca_njobs = 0, because there were multiple uid/prid/[group]
 *		combinations associated with the job.
 *
 */
static void
sum_rec(int rec, struct acctent *ac, double fee, int eojbyc)
{
	struct cacct	*acp;
	int	ind, jnd, k;
	int	found, ncpu;
	uid_t	uid;
	uint64_t prid;
	double	p_load, t_fl;
	double	xmem, memk;
	double	coremem, virtmem;
	double	dtotal;
	double	stime;
	double	cstime;
	double	*sbu, sbuv;
	double	tcpu[2];
	long	etime;
	long	elaps[2];
	time_t	tstamp;		/* time stamp of current record */
	uint64_t jid;
	gid_t	group;
	static double sbu_wt;
	int	skipsbu = 0;

	/*
	 *	Get id tags of uid and prid.
	 */
	switch(rec) {

	case REC_EOP:
		if (ac->csa) {
			uid   =  ac->csa->ac_uid;
			prid  =  ac->csa->ac_prid;
			group =  ac->csa->ac_gid;
			jid   =  ac->csa->ac_jid;
			sbu   = (double *)&(ac->csa->ac_sbu);
			sbuv  =  (ac->csa->ac_sbu);
			tstamp = ac->csa->ac_btime;
			if (db_flag >= 5) {
			    (void)fprintf(stderr, "sum_rec(base): uid(%d,%s), "
				"prid(%lld,%s), jid 0x%llx\n",
				uid, uid_to_name(uid), prid, 
				prid_to_name(prid), jid);
			}

		} else {
			acct_err(ACCT_WARN,
			       _("The first record is neither a Base nor EOJ record in the '%s' file."),
				spacct);
			return;
		}
		break;

	case REC_JOB:
		if (ac->soj) {
			skipsbu = 1;
			uid  = ac->soj->ac_uid;
			prid = getdfltprojuser(uid_to_name(uid));
			jid  = ac->soj->ac_jid;
			group = CACCT_NO_ID;
			tstamp = ac->soj->ac_btime;
			if (db_flag >= 5) {
			    (void)fprintf(stderr, "sum_rec(soj): uid(%d,%s), "
				"prid(%lld,%s), jid(0x%llx)\n",
				uid, uid_to_name(uid), prid,
				prid_to_name(prid), jid);
			}

		} else if (ac->eoj) {
			uid  = ac->eoj->ac_uid;
			/*
			 * change this when EOJ ac_prid field filled in 
			 */
			prid = getdfltprojuser(uid_to_name(uid));
			jid  = ac->eoj->ac_jid;
			sbu  = (double *)&ac->eoj->ac_sbu;
			sbuv =  (ac->eoj->ac_sbu);
			group = ac->eoj->ac_gid;
			tstamp = ac->eoj->ac_etime;
			if (db_flag >= 5) {
			    (void)fprintf(stderr, "sum_rec(eoj): uid(%d,%s), "
				"prid(%lld,%s), jid(0x%llx)\n",
				uid, uid_to_name(uid), prid,
				prid_to_name(prid), jid);
			}
			if (db_flag >= 5) {
			    (void)fprintf(stderr,
				" sum_rec(JOB) - uid %d (%s), prid %lld (%s), "
				"gid %d (%s)\n",
				uid, uid_to_name(uid), prid, prid_to_name(prid),
				group, gid_to_name(group));
			}

		} else if (ac->cfg) {
			if (db_flag >= 5) {
				(void)fprintf(stderr, 
					" sum_rec(JOB) - cfg record\n");
			}
			return;
		} else {
			acct_err(ACCT_WARN,
			       _("The first record is neither a Base nor EOJ record in the '%s' file."),
				spacct);
			return;
		}
		break;

	case REC_WKMG:
	    {
		struct wkmgmtbs *wm;
		wm = ac->wmbs;

		uid  = wm->uid;
		prid = wm->prid;
		jid  = wm->jid;
		group = CACCT_NO_ID;
		tstamp = wm->start_time;
	    }
	    break;

	default:
		acct_err(ACCT_ABORT,
		       _("An unknown record type (%4o) was found in the '%s' routine."),
			rec);
	}		/* end of switch(rec) */

        /*
         *      If this is an workload management job, multiply the sbu
         *      by the appropriate workload management weight.
         */
	if ((rec != REC_WKMG) && (!skipsbu) && numseg) {
		if (jid != prev_jid) {
			sbu_wt = 1.0;
			for (k = 0; k < numseg; k++) {
				if (mult[k].jid == jid) {
					sbu_wt = mult[k].sbu_wt;
					break;
				}
			}
			prev_jid = jid;
		}
		sbuv *= sbu_wt;
		*sbu = sbuv;
	}

	/*
	 *	First find appropriate cacct record...
	 */
	if (!u_flg) {
		uid = CACCT_NO_ID;
	}
	if (!p_flg) {
		prid = CACCT_NO_ID;
	}
	if (!g_flg) {
		group = CACCT_NO_ID;
	}
	if (!j_flg) {
		jid = CACCT_NO_ID;
	}

	if (db_flag >= 5) {
		(void)fprintf(stderr, "sum_rec(): calling find_acd() with "
			"uid %d (%s), prid %lld (%s), jid 0x%llx.\n",
			uid, uid_to_name(uid), prid, prid_to_name(prid), jid);
	}
	if ((acp = find_acd(uid, prid, group, jid)) == (struct cacct *)NULL) {
		exit(-1);
	}

	/*
	 *  Fill record header.
	 */
	if (create_hdr1(&acp->ca_hdr, ACCT_CACCT) < 0) {
		acct_err(ACCT_WARN,
		       _("An error was returned from the call to the '%s' routine."),
			"header()");
	}

	/*
	 *	Now enter data.... 
	 */
	if ((rec != REC_WKMG) && (!skipsbu)) {
		acp->ca_sbu += sbuv;
	}
	acp->ca_fee = 0.0;

	if (eojbyc) { /* sys crash so no end of job record add one job to sum */
		acp->ca_njobs++;
		if (db_flag >= 5) {
			(void)fprintf(stderr, "\teojbyc: njobs = %lld\n",
				 acp->ca_njobs);
		}
	}
	switch(rec) {

	case REC_JOB:
		if (ac->cfg) {
			if (db_flag >= 3) {
			    (void)fprintf(stderr, "  CFG: event(%4o), "
				"kdmask(%6o), rmask(%6o), boottime(%24.24s), "
				"curtime(%24.24s)\n",
				ac->cfg->ac_event,
				ac->cfg->ac_kdmask, ac->cfg->ac_rmask,
				ctime(&ac->cfg->ac_boottime), 
				ctime(&ac->cfg->ac_curtime));
			}
			break;

		} else if (ac->soj) {
		    if (ac->soj->ac_type == AC_SOJ) {
			if (db_flag >= 4) {
			    (void)fprintf(stderr, "  SOJ: uid(%d,%s), "
				"jid(0x%llx), init(%d), njobs(%lld)\n",
				ac->soj->ac_uid, uid_to_name(ac->soj->ac_uid),
				ac->soj->ac_jid, ac->soj->ac_init,
				acp->ca_njobs);
			}
		    } else {
			if (db_flag >= 4) {
			    (void)fprintf(stderr, "  ROJ: uid(%d,%s), "
				"jid(0x%llx), init(%d), njobs(%lld)\n",
				ac->soj->ac_uid, uid_to_name(ac->soj->ac_uid),
				ac->soj->ac_jid, ac->soj->ac_init,
				acp->ca_njobs);
			}
		    }
		    break;

		} else if (ac->eoj) {
			acp->ca_njobs++;
			if (db_flag >= 4) {
			    (void)fprintf(stderr, "  EOJ: uid %d (%s), "
				"prid %lld (%s), njobs = %lld\n",
				uid, uid_to_name(uid), prid, prid_to_name(prid),
				acp->ca_njobs);
			}
			break;
		} 
		break;

	case REC_EOP:
		/*
		 *	process count
		 */
		acp->ca_pc++;

		/*
		 *	Ok calculate P/NP split.
		 */
		t_fl = TIME_2_SECS(acctrec.csa->ac_etime);
		etime = (long)t_fl;
		if (etime == 0) {
			etime = 1;
		}
		pop_calc(ac->csa->ac_btime, etime, elaps);
		p_load = (double)elaps[0] / etime;

		/*
		 *	Base record.
		 *	cpu time (in microseconds)
		 */
		stime = ac->csa->ac_stime;
		/* 
		 * XXX csa->ac_ctime is unimplemented
		 * cstime = ac->csa->ac_ctime + stime;
		 */
		cstime = stime;
		stime += ucputime(ac);

                tcpu[I_PRIME] = stime * p_load;
                tcpu[I_NONPRIME] = stime - tcpu[I_PRIME];
                acp->prime.cpuTime  += TIME_2_SECS(tcpu[I_PRIME]);
                acp->nonprime.cpuTime += TIME_2_SECS(tcpu[I_NONPRIME]);

		/*
		 *	Delay record.
		 */
		if (ac->delay) {
			dtotal = NSEC_2_SECS(ac->delay->ac_cpu_delay_total);
			t_fl = dtotal * p_load;
			acp->prime.cpuDelayTime += t_fl;
			acp->nonprime.cpuDelayTime += dtotal - t_fl;

			dtotal = NSEC_2_SECS(ac->delay->ac_blkio_delay_total);
			t_fl = dtotal * p_load;
			acp->prime.blkDelayTime += t_fl;
			acp->nonprime.blkDelayTime += dtotal - t_fl;

			dtotal = NSEC_2_SECS(ac->delay->ac_swapin_delay_total);
			t_fl = dtotal * p_load;
			acp->prime.swpDelayTime += t_fl;
			acp->nonprime.swpDelayTime += dtotal - t_fl;
		}

		/*
		 *	Input-Output record.
		 */
		if (ac->io) {
			/* bytes transfered */
			t_fl = ac->io->ac_chr + ac->io->ac_chw;
			acp->prime.charsXfr += FTOL(t_fl * p_load);
			acp->nonprime.charsXfr += FTOL(t_fl -(t_fl * p_load));
	
			/* logical I/O, (read and write system calls) */
			t_fl = ac->io->ac_scr + ac->io->ac_scw;
			acp->prime.logicalIoReqs += FTOL(t_fl * p_load);
			acp->nonprime.logicalIoReqs += FTOL(t_fl -(t_fl * p_load));
		}

		/*
		 *	Memory record.
		 */
		if (ac->mem) {
			/* kbyte-min */
			struct	acctmem	*mem = ac->mem;

			switch(MEMINT) {

			case 1:
				coremem = mem->ac_core.mem1;
				virtmem = mem->ac_virt.mem1;
				break;
			case 2:
				coremem = mem->ac_core.mem2;
				virtmem = mem->ac_virt.mem2;
				break;
			case 3:
				coremem = mem->ac_core.mem3;
				virtmem = mem->ac_virt.mem3;
				break;

			default:
				acct_err(ACCT_ABORT,
				       _("An invalid value for MEMINT (%d) was found in the configuration file.\n  MEMINT must be between 1 and 3."),
					MEMINT);
			}

			xmem = cstime ? (double)coremem / cstime : 0;
			memk = MB_2_KB(xmem);

			t_fl = memk * TIME_2_MINS(tcpu[I_PRIME]);
			acp->prime.kcoreTime += t_fl;

			t_fl = memk * TIME_2_MINS(tcpu[I_NONPRIME]);
			acp->nonprime.kcoreTime += t_fl;

			xmem = cstime ? (double)virtmem / cstime : 0;
			memk = MB_2_KB(xmem);

			t_fl = memk * TIME_2_MINS(tcpu[I_PRIME]);
			acp->prime.kvirtualTime += t_fl;

			t_fl = memk * TIME_2_MINS(tcpu[I_NONPRIME]);
			acp->nonprime.kvirtualTime += t_fl;

			t_fl = mem->ac_minflt;
			acp->prime.minorFaultCount += FTOL(t_fl * p_load);
			acp->nonprime.minorFaultCount += FTOL(t_fl-(t_fl*p_load));

			t_fl = mem->ac_majflt;
			acp->prime.majorFaultCount += FTOL(t_fl * p_load);
			acp->nonprime.majorFaultCount += FTOL(t_fl-(t_fl*p_load));
		}


		/* disk usage count */
		acp->ca_du = 0; /* filled in later */

		/* disk sample count */
		acp->ca_dc = 0; /* filled in later */
		
		break;

	case REC_WKMG:
	    {
		struct wkmgmtbs *wm;
		wm = ac->wmbs;

		if (wm->term_subtype != WM_SPOOL_TERM) {
			++(acp->can_nreq);
		}
		acp->can_utime += wm->utime;
		acp->can_stime += wm->stime;
	    }
	    break;

	default:
		acct_err(ACCT_ABORT,
		       _("An unknown record type (%4o) was found in the '%s' routine."),
			rec);
	}		/* end of switch(rec) */

	return;
}


/*
 *	Find the cacct structure which matches the consolidation keys
 *	that were specified on the command line.  If no keys were specified,
 *	use uid/prid.
 */
static struct cacct *
find_acd(uid_t uid, uint64_t prid, gid_t group, uint64_t jid)
{
	int ind;
	int	size;
	int	omapsiz;

	/*
	 *	If first call allocate data struct.
	 */
	if (acmap == NULL) {
		if (db_flag >= 1) {
		    (void)fprintf(stderr, "find_acd(): allocating first map "
			"structures.\n");
		}
		size = MAPSIZE * sizeof(struct map);
		mapsize = MAPSIZE;
		if ((acmap = (struct map *)malloc(size)) == NULL) {
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when allocating '%s'."),
				"initial map");
		}
		for(ind = 0; ind < mapsize; ind++) {
			acmap[ind].uid = -1;
		}
	}

	/*
	 *	Find pair in map.
	 */
	for(ind = 0; ind < mapsize; ind++) {
		/*
		 *	If this is the end of used entries match this one.
		 */
		if (acmap[ind].uid == -1) {
			break;
		}

		/*
		 *	Look for match. If no group id match the first other
		 *	matching record. If matching by job id also check 
		 *	internal job id to insure no overlapping jids get 
		 *	summed together.
		 */
		if (((j_flg == 0) ||
		    ((j_flg != 0) && (acmap[ind].jid == jid) && 
		    (j_flg == acmap[ind].ijob))) &&
		    ((u_flg == 0) ||
		     (u_flg == 1 && (acmap[ind].uid == uid))) &&
		    ((p_flg == 0) ||
		     ((p_flg == 1) && (acmap[ind].prid == prid))) &&
		    ((g_flg == 0) ||
		     ((g_flg == 1) && ((acmap[ind].group == group) ||
		      (group == CACCT_NO_ID)))) ){

			return(acmap[ind].acp); /* got it */
		}
	}

	if (ind < mapsize) {
		if (db_flag >= 1) {
		    (void)fprintf(stderr, "find_acd(): new map struct being "
				"used - uid %d (%s), prid %lld (%s)\n",
				uid, uid_to_name(uid), 
				prid, prid_to_name(prid));
		}
		if (acmap[ind].uid == -1) {
			if (db_flag >= 3) {
			    (void)fprintf(stderr, "  adding new map node "
				"acmap[%d], uid %d (%s), prid %lld (%s), "
				"jid 0x%llx, group %d\n",
				ind, uid, uid_to_name(uid), prid,
				prid_to_name(prid), jid, group);
			}
			add_map(uid, prid, jid, group, ind);

			return(acmap[ind].acp);

		} else {
			acct_err(ACCT_ABORT,
			       _("A problem caused by a coding error %d was detected in the '%s' routine."),
				1, "find_acd()");
		}
	}

	/*
	 *	If map overflowed increase size and add to map.
	 */
	if (ind == mapsize ) {
		if (db_flag >= 1) {
		    (void)fprintf(stderr, "  map overflow.\n");
		}
		omapsiz = mapsize;
		mapsize += MAPINC;
		size = mapsize * sizeof(struct map);
		if ((acmap = (struct map *)realloc((char *)acmap, size)) ==
				NULL) {
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when reallocating '%s'."),
				"expand map");
		}
		if (db_flag >= 3) {
			(void)fprintf(stderr, "\texpanding map table from %d "
				"to %d.\n", omapsiz, mapsize);
		}
		for(ind = omapsiz; ind < mapsize; ind++) {
			acmap[ind].uid = -1;
		}
		add_map(uid, prid, jid, group, omapsiz);

		return(acmap[omapsiz].acp);
	}


	if (db_flag >= 1) {
		(void)fprintf(stderr, "find_acd(): null cacct map entry\n");
	}
	return ((struct cacct *)NULL);
}


/*
 *	Add new entry to map.
 */
static void
add_map(uid_t uid, uint64_t prid, uint64_t jid, gid_t group, int ind)
{
	struct cacct *acp;

	acmap[ind].uid = uid;
	acmap[ind].prid = prid;
	acmap[ind].jid = jid;
	acmap[ind].group = group;
	acmap[ind].ijob = j_flg;

	/*
	 *	get memory
	 */
	if ((acp = (struct cacct *) malloc(sizeof(struct cacct))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"struct cacct");
	}

	/*
	 *	Insure zero starts.
	 */
	memset((char *)acp, '\0', sizeof(struct cacct));

	/*
	 *	Initialize ID.
	 */
	acp->ca_uid = uid;
	acp->ca_prid = prid;
	acp->ca_jid = jid;
	acp->ca_gid = group;

	/*
	 *	Set pointer and return
	 */
	acmap[ind].acp = acp;

	return;
}

/*
 *	Write output data.
 */
static void
wr_output() 
{
	int ind;
	int	ofd;

	/*
	 *	open stdout
	 */
	if ((ofd = openacct (NULL, "w")) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			"stdout");
	}
	/*
	 *	For each uid,prid pair write a record.
	 *
	 */
	for(ind = 0; ind < mapsize; ind++) {
		if (acmap[ind].uid == -1) break;
	
		if (writecacct (ofd, acmap[ind].acp) != sizeof (struct cacct)) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the writing of file '%s'."),
				"stdout");
		}
	}
	closeacct(ofd);

	if (db_flag > 1) {
		(void)fprintf(stderr, "%d consolidated records written\n", ind);
	}

	return;
}

/*
 *	Get the system parameter values from the configuration file
 *	and create run-time variables.
 */
static void
init_config()
{
	MEMINT = init_int("MEMINT", 2, TRUE);

	return;
}
