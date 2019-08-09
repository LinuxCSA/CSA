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
 * csajrep.c - Generate job reports from sorted pacct files.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#include "csa.h"
#include "csaacct.h"

#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_PROJ_H
#include <proj.h>
#endif	/* HAVE_PROJ_H */

#ifndef MAXPROJNAMELEN
#define MAXPROJNAMELEN 32
#endif


#include "acctdef.h"
#include "acctmsg.h"

#define	DEFAULT_JID	-2
#define	DEFAULT_PRID	-2
#define	DEFAULT_SEQNO	-2
#define	SUMMARY		1
#define	SUMMARY_ONLY	2

int	Newjob;
char	*Prg_name = "";
int	sfd = -1;
char	*spacct = SPACCT;
time_t	Sstart;
time_t	Sstop;

#define	REC_PROCESS	1
#define	REC_WKMG	2
#define	MAX_REC		3

int	head_printed[MAX_REC + 1];

typedef enum
{
    S_record,  /* Does this record match the selection criteria? */
    S_job      /* Does this job have record matching selection criteria? */
} Sel_scope;

struct sbutype
{
    double  sbu_wt;  /* workload management sbu multiplier */
    uint64_t   jid;     /* jid of this workload management segment */
};

struct sbutype		*mult;
#define SBUSIZE		10  /* init max # workload mgmt segments per job */
int	max_sbusize = SBUSIZE;
uint64_t Jid	= -1;
uint64_t Prid	= -1;
uid_t	Uid	= -1;
int64_t	Seqno	= -1;
int	Alljobs		= 0;
int	Fullreport	= 0;
int	No_Zero		= 0;
int	exclusive	= 0;
int	form_feed	= 0;
int	suppress_head	= 0;
int	total_cmd_only	= 0;
int	f_J 	= 0;
int	f_N	= 0;
int	f_S	= 0;
int	f_V	= 0;
int	f_b 	= 0;
int	f_c 	= 0;
int	f_q 	= 0;
int	f_st 	= 0;
int	f_w 	= 0;
int	f_x 	= 0;

int	MEMINT;
int	MTASK_LO;	/* low mtask report bucket */
int	MTASK_HI;	/* high mtask report bucket */

char            acctentbuf[ACCTENTSIZ];
struct acctent  acctrec = {acctentbuf};

struct sumcmd
{
    uid_t	uid;	/* uid from 1st process record */
    uint64_t	prid;	/* prid from 1st process record */
    uint64_t	ash;	/* ash from 1st process record */
    u_int64_t	utime;
    u_int64_t	stime;
    u_int64_t	qwtime;
    u_int64_t	corehimem;
    u_int64_t	virthimem;
    u_int64_t	chr;
    u_int64_t	chw;
    u_int64_t	scr;
    u_int64_t	scw;
    u_int64_t	pgswap;
    u_int64_t	minflt;
    u_int64_t	majflt;
    u_int64_t   cpuDelayTime;
    u_int64_t   blkDelayTime;
    u_int64_t   swpDelayTime;
    u_int64_t   runRealTime;
    u_int64_t   runVirtTime;
    double	sbu;
};

struct sumbatch
{
    int64_t	utime;		/* shepherd user cpu time */
    int64_t	stime;		/* shepherd system cpu time */
};

struct sumcmd	cmt;
struct sumbatch	*batchtot = NULL;

struct reqent
{
    uid_t	 	uid;
    uint64_t		prid;
    u_int64_t		utime;		/* user cpu time (usec) */
    u_int64_t		stime;		/* system cpu time (usec) */
    u_int64_t		cpuDelayTime;	/* cpu delay time (nsec) */
    u_int64_t		blkDelayTime;	/* block i/o delay time (nsec)*/
    u_int64_t		swpDelayTime;	/* swap in delay time (nsec)*/
    struct reqent	*next;		/* diff uid/prid for this jid */
};

struct reqseg
{
    char		qname[16];	/* queue name */
    uint64_t		jid;		/* job id */
    short		init_stat;	/* NQ_INIT/WM_INIT subtype */
    short		term_stat;	/* NQ_TERM/WM_INIT subtype */
    time_t		start_time;	/* start time of this segment (sec) */
    u_int64_t		corehimem;	/* job hiwater core memory (Kbytes) */
    u_int64_t		virthimem;	/* job hiwater virtual mem (Kbytes) */
    time_t		end_time;	/* end time of this segment
                                           (from eoj)*/
    int64_t		qwait;		/* queue wait time (sec) */
    struct reqseg	*prev;		/* previous segment for this request */
    struct reqseg	*next;		/* next segment for this request */
    struct reqent	*list;		/* usage by uid/prid for this jid */
};

struct reqhdr
{
    int64_t		seqno;		/* request sequence number/ID */
    int			arrayid;	/* Workload mgmt request ID */
    char		reqname[16];	/* request name */
    time_t		wall_start;	/* wall-clock start time (sec) */
    time_t		wall_end;	/* wall-clock end time (sec) */
    struct reqseg	*first;		/* first request segment */
    struct reqseg	*curr;		/* current request segment */
    struct reqent	*ent;		/* curr jid/uid/prid entry */
};

struct rseg_hdr
{
    struct reqseg	*first;		/* first allocation block */
    struct reqseg	*last;		/* last allocation block */
    struct reqseg	*curr;		/* current allocation block */
    int			navail;		/* # free entries in this alloc blk */
};

struct rent_hdr
{
    struct reqent	*first;		/* first allocation block */
    struct reqent	*last;		/* last allocation block */
    struct reqent	*curr;		/* current allocation block */
    int			navail;		/* # free entries in this alloc blk */
};

struct reqhdr		*req;
struct rent_hdr		*rent;
struct rseg_hdr		*rseg;
#define RSEG_INC	25		/* # reqseg entries to alloc at 1x */
#define RENT_INC	75		/* # reqent entries to alloc at 1x */
#define INIT_MSG	0
#define TERM_MSG	0
#define NO_NEXT_JID	-99

char *WM_Init_statname[] =	/* names for workload management init_stat */
{
    "Unknown",	/* WM_NO_INIT = -1 --> 0 */
    "Start",	/* WM_INIT_START = 1 */
    "Restart",	/* WM_INIT_RESTART = 2 */
    "Rerun",	/* WM_INIT_RERUN = 3 */
    "Spool"	/* WM_SPOOL_INIT = 4 */
};

char *WM_Term_statname[] =	/* names for workload management term_stat */
{
    "Unknown",	/* WM_NO_TERM = -2 --> 0 */
    "Exit",	/* WM_TERM_EXIT = 1 */
    "Requeue",	/* WM_TERM_REQUEUE = 2 */
    "Hold",	/* WM_TERM_HOLD = 3 */
    "Rerun",	/* WM_TERM_RERUN = 4 */
    "Migrate",	/* WM_TERM_MIGRATE = 5 */
    "Spool"	/* WM_SPOOL_TERM = 6 */
};

/*
 * Action codes for find_wm() and add_wm().
 */
typedef enum
{
    FA_seg_before,	/* add a new reqseg before the current one */
    FA_seg_after,	/* add a new reqseg after the previous one */
    FA_find_ent		/* find the reqent entry for this prid/uid */
} FAaction;

void	add_wm(struct reqseg *, FAaction);
int64_t	argtoi(char *, char);
void	clean_wm();
void	do_job(struct acctjob *, int64_t, Sel_scope);
void	do_wm(struct acctjob *, int64_t);
void	find_wm(uint64_t, uint64_t, uid_t, time_t, time_t);
struct reqseg	*get_reqseg();
struct reqent	*get_reqent();
void	init_wm();
void	wm_process(int);
void	print_heading(int);
void	print_daemon(int);
void	print_wm(int);
void	print_rec(int);
void	print_sum();
void	scan_spacct();
int	select_job(int, Sel_scope);
void	sum_daemon(int);

void usage()
{
    acct_err(ACCT_ABORT,
           "Command Usage:\n%s\t[-ABbcFhJLqS] [-s spacct] [-TtVwxZ]\n%s\t[-ABbcehJ] [-j jid] [-L] [-r reqid] [-p project] [-qS]\n\t\t[-s spacct] [-Tt] [-u login] [-VwxZ]\n%s\t-N [-A] [-s spacct]",
    	Prg_name, Prg_name, Prg_name);
}

char	*optstring = "ABbceFhJj:LNp:qr:Ss:Ttu:VwxZ";

int
main(int argc, char **argv)
{
    char	ch;
    int		c;
    int		N_arg = 0;	/* # options used except -s */
    int		no_selection;
    int		user_select = 0;
    int		siz;
    extern char	*optarg;
    extern int	optind;

    Prg_name = argv[0];
    no_selection = Prid + Jid + Seqno + Uid;

    while ((c = getopt(argc, argv, optstring)) != EOF)
    {
        ch = (char) c;
        N_arg++;
        switch (ch)
        {
        case 'A':
            Alljobs = 1;
            break;
        case 'B':
            f_st = 1;
            break;
        case 'b':
            f_b = 1;
            break;
        case 'c':
            f_c = 1;
            break;
        case 'e':
            exclusive = 1;
            break;
        case 'F':
            Fullreport = 1;
            break;
        case 'h':
            suppress_head = 1;
            break;
        case 'J':
            f_J = 1;
            break;
        case 'j':
            sscanf(optarg, "%llx", &Jid);
            break;
        case 'L':
            form_feed = 1;
            break;
        case 'N':
            f_N = 1;
            break;
	case 'r':
            sscanf(optarg, "%lld", &Seqno);
            break;
        case 'p':
            Prid = argtoi(optarg, ch);
            break;
        case 'q':
            f_q = 1;
            break;
        case 'S':
            f_S = 1;
            break;
        case 's':
            spacct = optarg;
            N_arg--;
            break;
        case 'T':
            total_cmd_only = SUMMARY_ONLY;
            break;
        case 't':
            total_cmd_only = SUMMARY;
            break;
        case 'u':
            Uid = argtoi(optarg, ch);
            break;
	case 'V':
	    f_V = 1;
	    break;
        case 'w':
            f_w = 1;
            break;
        case 'x':
            f_x = 1;
            break;
        case 'Z':
            No_Zero = 1;
            break;
        default:
            usage();
        } /* end switch */
    } /* end while getopt */

    if (optind != argc)
    {
        acct_err(ACCT_FATAL,
               _("Extra arguments were found on the command line.")
        	);
        usage();
    }

    /*
     *	If -N was specified, check the other options.
     */
    if (f_N)
    {
        switch (N_arg)
        {
        case 1:		/* -N or -Ns specified */
            break;

        case 2:		/* -NA or -NAs specified */
            if (!Alljobs)
            {
                acct_err(ACCT_FATAL,
                       _("The (-%s) option can be used with only one of the (%s) options."),
                	"N", "{A s}");
                usage();
            }
            break;

        default:
            acct_err(ACCT_FATAL,
                   _("Some invalid options were used with the (-%s) option."),
            	"N");
            usage();
        }

        /*
         *	Allocate memory for Workload management strucures.
         */
        siz = sizeof(struct reqhdr);
        if ((req = (struct reqhdr *) malloc(siz)) == NULL)
            acct_perr(ACCT_ABORT, errno,
             _("There was insufficient memory available when allocating '%s'."),
            	"Workload management request header");
        memset((char *) req, '\0', siz);

        siz = sizeof(struct rent_hdr);
        if ((rent = (struct rent_hdr *) malloc(siz)) == NULL)
            acct_perr(ACCT_ABORT, errno,
            	_("There was insufficient memory available when allocating '%s'."),
            	"reqent header");
        memset((char *) rent, '\0', siz);

        siz = sizeof(struct rseg_hdr);
        if ((rseg = (struct rseg_hdr *) malloc(siz)) == NULL)
            acct_perr(ACCT_ABORT, errno,
            	_("There was insufficient memory available when allocating '%s'."),
            	"reqseg header");
        memset((char *) rseg, '\0', siz);
    }

    /*
     *	Check to see that: (1) -F is not used with -jnpu
     *	(2) -e is used with -jnpu.
     */
    user_select = Prid + Jid + Seqno + Uid;
    if (Fullreport && (user_select > no_selection))
    {
        acct_err(ACCT_FATAL,
               _("The (-%s) and the (-%s) options are mutually exclusive."),
        	"F", "{j n p u}");
        usage();
    }

    if (exclusive && (user_select == no_selection))
    {
        acct_err(ACCT_FATAL,
               _("The (-%s) option must be used with the (-%s) option."),
        	"e", "{j n p u}");
        usage();
    }

    if ((sfd = openacct(spacct, "r")) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the opening of file '%s'."),
        	spacct);

    /*
     *	Allocate memory for various tables
     */
    siz = max_sbusize * sizeof(struct sbutype);
    if ((mult = (struct sbutype *) malloc(siz)) == NULL)
        acct_perr(ACCT_ABORT, errno,
        	_("There was insufficient memory available when allocating '%s'."),
        	"sbu table");

    init_wm();

    /*
     *	Scan the sorted pacct file for input data.
     */
    scan_spacct();
    closeacct(sfd);

    exit(0);
}

/*
 *	scan_spacct() - scan the sorted pacct records.
 *
 *	for (each up period)
 *	{
 *		for (each job master record)
 *		{
 *			read job header;
 *			if ( !interested ) continue
 *			read records;
 *			write records;
 *		}
 *	}
 */
void scan_spacct() 
{
    struct acctcfg  uphdr;
    struct acctjob  jobhdr;
    int             wanted = 0;
    int64_t	    offset;
    int64_t	    uoffset;
    int64_t         uptime_size;

    cmt.uid = cmt.prid = cmt.ash = -1;

    /*
     *	Read each uptime record.
     */
    while (readacct(sfd, (char *) &uphdr, sizeof(struct acctcfg)) ==
           sizeof(struct acctcfg))
    {
        uoffset = seekacct(sfd, 0, SEEK_CUR);
        /*
         *	Remember size of uptime.
         */
        uptime_size = uphdr.ac_uptimelen;
        
        /*
         *	Save times.
         */
        Sstart = uphdr.ac_boottime;
        Sstop = uphdr.ac_curtime;

        /*
         *	While still in this uptime, read the job header records.
         */
        while (uptime_size &&
               readacct(sfd, (char *) &jobhdr, sizeof(struct acctjob)) ==
               sizeof(struct acctjob))
        {
            /*  Remember end of the job header record. */
            offset = seekacct(sfd, 0, SEEK_CUR);
            /*
             *	See if we're interested in this job.  If -A, then process
             *	all jobs.  Otherwise, look only at terminated jobs.  If -N,
             *  examine only Workload management requests.
             */
            wanted = 0;
            switch (f_N)
            {
            case 0:	/* look at all jobs, not just Workload Mgmt */
                if (Alljobs)
                    wanted = 1;
                if (jobhdr.aj_flags & END_OF_JOB_B)
                    wanted = 1;
                if (jobhdr.aj_flags & END_OF_JOB_C)
                    wanted = 1;
                if (jobhdr.aj_flags & END_OF_JOB_T)
                    wanted = 1;
                if ((!(jobhdr.aj_flags & WKMG_IN_JOB)) &&
                    (jobhdr.aj_flags & END_OF_JOB_I))
                {
                    wanted = 1;
                }
                if (wanted)
                {
                    wanted = 0;
                    do_job(&jobhdr, offset, S_job);
                }
                break;

            default:	/* look only at Workload management requests */
                if (jobhdr.aj_wkmg_num) {
                    if (Alljobs)
                        wanted = 1;
                    else if (jobhdr.aj_flags & END_OF_JOB_B)
                        wanted = 1;
                }
                if (wanted)
                    do_wm(&jobhdr, offset);
                break;
            }

            /*
             *	Ensure position at next job header record.
             */
            uptime_size -= sizeof(struct acctjob) + jobhdr.aj_len;
            offset = offset + jobhdr.aj_len;
            if (seekacct(sfd, offset, SEEK_SET) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	spacct);
        }

        /*
         *	Ensure we are at next uptime.
         */
        if (seekacct(sfd, 0, SEEK_CUR) != uphdr.ac_uptimelen + uoffset)
            acct_err(ACCT_WARN,
                   _("The position in file '%s' is wrong for the next %s record.\n   Examine the data file for correct version of the file.\n   (Match the binary with the record structure.)"),
            	spacct, "uptime");
    } /* end while uptime */
    
    return;
} 

/*
 * do_job():
 *	Determine if this job contains a record which matches the selection
 *	criteria as specified by -j, -n, -p, and -u.  If such a record is
 *	found, recursively call do_job() to output the record and accumulate
 *	totals.
 *
 *	When do_job is called recursively, scan = S_record.  If -e is
 *	specified, then only those records which exactly match the selection
 *	criteria are printed and summed.  If -Z is specified, then print and
 *	sum records only if jid != 0.  Otherwise, all of the records in the
 *	job are reported and accumulated.
 *
 *	Totals for the process records are accumulated in print_rec().
 *	Totals for the daemon accounting records are accumulated in
 *	sum_daemon().
 *
 *  Parameters:	*jobhdr =  job header record
 *		offset  =  file offset to the end of the job header record
 *		scan    =  S_job
 *				We are trying to determine if this job
 *				has a record which matches the selection
 *				criteria.
 *		       =  S_record
 *				We have selected this job and are now
 *				looking for records to output and sum.
 */
void do_job(struct acctjob *jobhdr, int64_t offset, Sel_scope scan)
{
    struct acctcsa   *csa;
    int               i, j;
    int               numseg;
    int               ppr = 0;
    uint64_t          prev_jid;
    double            sbu_wt;
    int               size;
    struct sbutype  *tmult;
    int               want_this;
    struct wkmgmtbs  *wmb;
    int               wm_done = 0;

    /*
     *	If printing, clear heading printed array.
     */
    if (scan == S_record)
    {
        for (i = 0; i <= MAX_REC; i++)
        {
            if (head_printed[i] != 0)
            {
                /*
                 * Print trailer.
                 */
                if (form_feed)
                    fprintf(stdout, "%c", '\014');
                else
                    fprintf(stdout, "\n\n\n\n");
            }
            head_printed[i] = 0;
        }
        Newjob = 1;
    }

    /*
     *	Workload mgmt records - must be done first because of sbu multiplier
     */

    /*
     *	Reallocate space for the multiplier array if necessary.
     */
    numseg = jobhdr->aj_wkmg_num;
    if (numseg > max_sbusize) {
        max_sbusize += (numseg - max_sbusize);
        size = max_sbusize * sizeof(struct sbutype);

        if ((tmult = (struct sbutype *) realloc(mult, size)) == NULL)
            acct_perr(ACCT_ABORT, errno,
            	_("There was insufficient memory available when reallocating '%s'."),
            	"sbu table");
        mult = tmult;
    }

    /*
     *	Workload management records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_wkmg_start, SEEK_SET) < 0) {
	acct_perr(ACCT_ABORT, errno,
		_("An error occurred on the seek to the first '%s' record."),
		"workload management");
    }

    for (i = 0, want_this = 1; i < jobhdr->aj_wkmg_num; i++)
    {
        if (readacctent(sfd, &acctrec, FORWARD) <= 0) {
	    acct_perr(ACCT_ABORT, errno,
		_("An error occurred during the reading of file '%s' %s."),
		spacct, "workload management record");
        }
        if (acctrec.wmbs == NULL) {
	    acct_err(ACCT_ABORT,
		 _("An error occurred during the reading of file '%s' %s."),
		spacct, "workload management record");
        }
        wmb = acctrec.wmbs;
        if (scan == S_job)
        {
            /*
             * See if there is a workload management record which matches
             * the selection criteria.
             */
            if (select_job(REC_WKMG, scan))
            {
                do_job(jobhdr, offset, S_record);
                return;
            }
        }
        else
        {
            /*
             * Print and sum the workload management records of interest.
             */
            if (exclusive || No_Zero)
                want_this = select_job(REC_WKMG, scan);
            if (want_this)
            {
                mult[i].sbu_wt = wmb->sbu_wt;
                mult[i].jid = wmb->jid;
                print_rec(REC_WKMG);
                ppr = 1;
                sum_daemon(REC_WKMG);
                if (wmb->term_subtype != WM_NO_TERM)
                    wm_done = 1;
            }
        }
    }
    if (wm_done)
        fprintf(stdout, "\tWORKLOAD MANAGEMENT REQUEST COMPLETED\n");

    if (ppr == 1)
    {
        ppr = 0;
        print_daemon(REC_WKMG);		/* output totals */
    }

    /*
     *	End-of-process records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_eop_start, SEEK_SET) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred on the seek to the first '%s' record."),
        	"eop");
    prev_jid = DEFAULT_JID;
    for (i = 0, want_this = 1; i < jobhdr->aj_eop_num; i++)
    {
        if (readacctent(sfd, &acctrec, FORWARD) <= 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the reading of file '%s' %s."),
            	spacct, "eop record");
        if (acctrec.csa == NULL)
            acct_err(ACCT_ABORT,
                   _("An error occurred during the reading of file '%s' %s."),
            	spacct, "eop record");
        csa = acctrec.csa;
        if (scan == S_job)
        {
            if (select_job(REC_PROCESS, scan))
            {
                do_job(jobhdr, offset, S_record);
                return;
            }
        }
        else
        {
            if (exclusive || No_Zero)
                want_this = select_job(REC_PROCESS, scan);
            if (want_this)
            {
                if (numseg)
                {
                    if (csa->ac_jid != prev_jid)
                    {
                        sbu_wt = 1.0;
                        for (j = 0; j < numseg; j++)
                        {
                            if (mult[j].jid == csa->ac_jid)
                            {
                                sbu_wt = mult[j].sbu_wt;
                                break;
                            }
                        }
                        prev_jid = csa->ac_jid;
                    }
                    csa->ac_sbu *= sbu_wt;
                }
                print_rec(REC_PROCESS);
                ppr = 1;
            }
        }
    }
    /*
     *	Start-of-job, end-of-job, and configuration records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_job_start, SEEK_SET) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred on the seek to the first '%s' record."),
        	"soj, eoj, or cfg");
    for (i = 0, want_this = 1; i < jobhdr->aj_job_num; i++)
    {
        if (readacctent(sfd, &acctrec, FORWARD) <= 0)
        {
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the reading of file '%s' %s."),
            	spacct,
                      "soj, eoj, or cfg record");
        }
        if (acctrec.soj == NULL &&
            acctrec.eoj == NULL &&
            acctrec.cfg == NULL)
        {
            acct_err(ACCT_ABORT,
                   _("An error occurred during the reading of file '%s' %s."),
            	spacct, "soj, eoj, or cfg record");
        }
        if (scan == S_job)
        {
            if (select_job(REC_PROCESS, scan))
            {
                do_job(jobhdr, offset, S_record);
                return;
            }
        }
        else
        {
            if (exclusive || No_Zero)
                want_this = select_job(REC_PROCESS, scan);
            if (want_this)
                print_rec(REC_PROCESS);
        }
    }
    if (ppr == 1)
    {
        ppr = 0;
        print_sum();		/* output totals */
    }

}

/*
 *	Print heading for this record type.
 */
void print_heading(int typ)
{
    int i;
    char l1[4096], l2[4096], l3[4096];
    char *start_time;

    /*
     * If suppress heading option specified, then do so.
     */
    if (suppress_head == 1)
        return;

    /*
     *	print job heading.
     */
    if (Newjob == 1)
    {
        if (Sstart)
            start_time = ctime(&Sstart);
        else
            start_time = "Unknown\n";
        fprintf(stdout, "JOB FROM SYSTEM BOOT PERIOD STARTING AT %s",
                start_time);
        fprintf(stdout, "PERIOD ENDING AT %s", ctime(&Sstop));
        Newjob = 0;
    }
    l2[0] = l3[0] = l1[0] = '\0';
    head_printed[typ] = 1;
    
    switch (typ)
    {
    case REC_PROCESS:
        fprintf(stdout, "\n");
        if (f_J)
        {
            strcat(l1, "       JOB         ");
            strcat(l2, "       ID          ");
            strcat(l3, "================== ");
        }
        
        strcat(l1, "PROJECT  LOGIN        COMMAND     ");
        strcat(l2, "NAME     NAME         NAME        ");
        strcat(l3, "======== ======== ================");
        
        if (f_S)
        {
            strcat(l1, "       ARRAY       ");
            strcat(l2, "   SESSION HANDLE  ");
            strcat(l3, " ==================");
        }
        
        if (f_st)
        {
            strcat(l1, "        START        ");
            strcat(l2, "        TIME         ");
            strcat(l3, " ====================");
        }
        
        if (f_c)
        {
            strcat(l1, " USER-TIM SYS-TIM  RUN-REAL RUN-VIRT");
            strcat(l2, " [SECS]   [SECS]   [SECS]   [SECS]  ");
            strcat(l3, " ======== ======== ======== ========");
        }

        if (f_w)
        {
            strcat(l1, "        DELAY [SECS]         HIMEM (KBYTES)   ");
            strcat(l2, "   CPU     BLOCK     SWAP    CORE    VIRTUAL  ");
            strcat(l3, " ======== ======== ======== ========= =========");
        }

        if (f_x)
        {
            strcat(l1, " K-CHARS  K-CHARS  LOGICAL  LOGICAL ");
            strcat(l2, " READ     WRITTEN  READS    WRITES  ");
            strcat(l3, " ======== ======== ======== ========");
        }

	if (f_V)
	{
	    strcat(l1, " PAGE     MINOR    MAJOR   ");
	    strcat(l2, " SWAPS    FAULTS   FAULTS  ");
	    strcat(l3, " ======== ======== ========");
	}
 
        if (f_b)
        {
            strcat(l1, " SBUs    ");
            strcat(l2, "         ");
            strcat(l3, " ========");
        }

        fprintf(stdout, "%s\n", l1);
        fprintf(stdout, "%s\n", l2);
        fprintf(stdout, "%s\n", l3);
        break;

    case REC_WKMG:
        fprintf(stdout, "\n");
        strcat(l1, "\nLOGIN    PROJECT  QUEUE            REQUEST          MACHINE          REQUEST  ARRAY ");
        strcat(l2, "NAME     NAME     NAME             NAME             NAME               ID      ID   ");
        strcat(l3, "======== ======== ================ ================ ================ ======== ===== ");

        if (f_c)
        {
            strcat(l1, "USER-TIM SYS-TIM  ");
            strcat(l2, "[SECS]   [SECS]   ");
            strcat(l3, "======== ======== ");
        }
        if (f_q)
        {
            strcat(l1, " QUEUE   QUEUE ");
            strcat(l2, "WAIT [S] TYPE  ");
            strcat(l3, "======== ===== ");
        }
        if (f_J)
        {
            strcat(l1, "       JOB         ");
            strcat(l2, "       ID          ");
            strcat(l3, "================== ");
        }
        if (f_S)
        {
            strcat(l1, "      ARRAY        ");
            strcat(l2, "  SESSION HANDLE   ");
            strcat(l3, "================== ");
        }
        if (f_st)
        {
            strcat(l1, "        START       ");
            strcat(l2, "        TIME        ");
            strcat(l3, "====================");
        }
        fprintf(stdout, "%s\n", l1);
        fprintf(stdout, "%s\n", l2);
        fprintf(stdout, "%s\n", l3);
        break;

    default:
        acct_err(ACCT_ABORT,
               _("An unknown record type (%4o) was found in the '%s' routine."),
        	typ);
    }
}

/*
 * print_rec:
 *	Output information for this record.  Also accumulate totals
 *	for process records if this was specified by the user.
 */

char *Zero = "0";
 
void print_rec(int typ)
{
    struct wkmgmtbs *wm;
    int       i;
    char      linebuf[512];
    char      project[MAXPROJNAMELEN];
    char      tbuf[512];
    time_t    timeb;
    char      user[32];

    if (head_printed[typ] == 0)
        print_heading(typ);
    linebuf[0] = '\0';

    switch(typ)
    {
    case REC_PROCESS:
        /*
         *	Do the start-of-job record.
         */
        if (acctrec.soj)
        {
            if (acctrec.soj->ac_type == AC_SOJ)
                timeb = acctrec.soj->ac_btime;
            else
                timeb = acctrec.soj->ac_rstime;
            sprintf(tbuf, "\t START OF JOB AT %s", ctime(&timeb));
            strcat(linebuf, tbuf);
            break;
        }

        /*
         *	Do the end-of-job record.
         */
        if (acctrec.eoj)
        {
            timeb = acctrec.eoj->ac_etime;
            sprintf(tbuf, "\t END OF JOB AT %s", ctime(&timeb));
            strcat(linebuf, tbuf);
            break;
        }

        /*
         *	Skip the configuration record.
         */
        if (acctrec.cfg)
            break;

        strcpy(project, prid_to_name(acctrec.csa->ac_prid));
        if (project[0] == '?')
            sprintf(project, "%lld", acctrec.csa->ac_prid);
        strcpy(user, uid_to_name(acctrec.csa->ac_uid));
        if (user[0] == '?')
            sprintf(user, "%d", acctrec.csa->ac_uid);

        if (total_cmd_only == SUMMARY_ONLY)
        {
            if (cmt.prid < 0)
                cmt.prid = acctrec.csa->ac_prid;
            if (cmt.uid < 0)
                cmt.uid = acctrec.csa->ac_uid;
            if (cmt.ash < 0)
                cmt.ash = acctrec.csa->ac_ash;
        }

        if (f_J)
        {
            sprintf(tbuf, "%#18llx ", acctrec.csa->ac_jid);
            strcat(linebuf, tbuf);
        }
        
        sprintf(tbuf, "%-8s %-8s %-16s", project, user, acctrec.csa->ac_comm);
        strcat(linebuf, tbuf);
        
        if (f_S)
        {
            sprintf(tbuf, " 0x%016llx", acctrec.csa->ac_ash);
            strcat(linebuf, tbuf);
        }
        
        if (f_st)
        {
            timeb = acctrec.csa->ac_btime;
            sprintf(tbuf, " %20.20s", ctime(&timeb) + 4);
            tbuf[21] = '\0';
            strcat(linebuf, tbuf);
        }
        
        if (f_c)
        {
            if (total_cmd_only)
            {
                cmt.utime += FTOL(ucputime(&acctrec));
                cmt.stime += acctrec.csa->ac_stime;
            }
            sprintf(tbuf, " %8.3f %8.3f",
                    TIME_2_SECS(FTOL(ucputime(&acctrec))),
                    TIME_2_SECS(acctrec.csa->ac_stime));
            strcat(linebuf, tbuf);

            if (acctrec.delay != NULL)
            {
                if (total_cmd_only)
                {
                    cmt.runRealTime += acctrec.delay->ac_cpu_run_real_total;
                    cmt.runVirtTime += acctrec.delay->ac_cpu_run_virtual_total;
                }
                sprintf(tbuf, " %8.3f %8.3f",
                        NSEC_2_SECS(acctrec.delay->ac_cpu_run_real_total),
                        NSEC_2_SECS(acctrec.delay->ac_cpu_run_virtual_total));
            }
            else
                sprintf(tbuf, " %8s %8s", Zero, Zero);
            strcat(linebuf, tbuf);
        }

        if (f_w)
        {
            if (acctrec.delay != NULL)
            {
                if (total_cmd_only)
                {
                    cmt.cpuDelayTime += acctrec.delay->ac_cpu_delay_total;
                    cmt.blkDelayTime += acctrec.delay->ac_blkio_delay_total;
                    cmt.swpDelayTime += acctrec.delay->ac_swapin_delay_total;
                }
                sprintf(tbuf, " %8.2f %8.2f %8.2f",
                        NSEC_2_SECS(acctrec.delay->ac_cpu_delay_total),
                        NSEC_2_SECS(acctrec.delay->ac_blkio_delay_total),
                        NSEC_2_SECS(acctrec.delay->ac_swapin_delay_total));
            }
            else
                sprintf(tbuf, " %8s %8s %8s", Zero, Zero, Zero);
            strcat(linebuf, tbuf);
            
            if (acctrec.mem != NULL)
            {
                if (total_cmd_only)
                {
                    if (cmt.corehimem < acctrec.mem->ac_core.himem)
                        cmt.corehimem = acctrec.mem->ac_core.himem;
                    if (cmt.virthimem < acctrec.mem->ac_virt.himem)
                        cmt.virthimem = acctrec.mem->ac_virt.himem;
                }
                sprintf(tbuf, " %9lld %9lld",
                        acctrec.mem->ac_core.himem,
                        acctrec.mem->ac_virt.himem);
            }
            else
                sprintf(tbuf, " %9s %9s", Zero, Zero);
            strcat(linebuf, tbuf);
        }

        if (f_x)
        {
            if (acctrec.io != NULL)
            {
                if (total_cmd_only)
                {
                    cmt.chr += acctrec.io->ac_chr;
                    cmt.chw += acctrec.io->ac_chw;
                    cmt.scr += acctrec.io->ac_scr;
                    cmt.scw += acctrec.io->ac_scw;
                }
                sprintf(tbuf, " %8.0f %8.0f %8lld %8lld",
                        acctrec.io->ac_chr / 1024.0,
                        acctrec.io->ac_chw / 1024.0,
                        acctrec.io->ac_scr, acctrec.io->ac_scw);
            }
            else
            {
                sprintf(tbuf, " %8s %8s %8s %8s %8s %8s",
                        Zero, Zero, Zero, Zero, Zero, Zero);
            }
            strcat(linebuf, tbuf);
        }

	if (f_V)
	{
	   if (acctrec.mem != NULL) {
		if (total_cmd_only) {
		    cmt.pgswap += acctrec.mem->ac_pgswap;
		    cmt.minflt += acctrec.mem->ac_minflt;
		    cmt.majflt += acctrec.mem->ac_majflt;
		}
		sprintf(tbuf, " %8lld %8lld %8lld", acctrec.mem->ac_pgswap,
			acctrec.mem->ac_minflt, acctrec.mem->ac_majflt);
	    } else {
		sprintf(tbuf, " %8lld %8lld %8lld", Zero, Zero, Zero);
	    }
	    strcat(linebuf, tbuf);
	} 

        if (f_b)
        {
            if (total_cmd_only)
                cmt.sbu += acctrec.csa->ac_sbu;
            sprintf(tbuf, " %8.2f", acctrec.csa->ac_sbu);
            strcat(linebuf, tbuf);
        }
        strcat(linebuf, "\n");
        break;

    case REC_WKMG:
        wm = acctrec.wmbs;

        strcpy(project, prid_to_name(wm->prid));
        if (project[0] == '?')
            sprintf(project, "%lld", wm->prid);
        strcpy(user, uid_to_name(wm->uid));
        if (user[0] == '?')
            sprintf(user, "%d", wm->uid);
        
        sprintf(linebuf, "%-8s %-8s %-16s %-16s %-16s %8lld %5d",
                user, project, wm->quename, wm->reqname, wm->machname,
                wm->reqid, wm->arrayid);
        if (f_c)
        {
            sprintf(tbuf, " %8.4f %8.4f",
                    TIME_2_SECS(wm->utime), TIME_2_SECS(wm->stime));
            strcat(linebuf, tbuf);
        }
        if (f_q)
        {
            sprintf(tbuf, " %8lld %5d", wm->qwtime, wm->qtype);
            strcat(linebuf, tbuf);
        }
        if (f_J)
        {
            if (wm->jid == DEFAULT_JID)
                sprintf(tbuf, " %18s", "none");
            else
                sprintf(tbuf, " %#18llx", wm->jid);
            strcat(linebuf, tbuf);
        }
        if (f_S)
        {
            sprintf(tbuf, " 0x%016llx", wm->ash);
            strcat(linebuf, tbuf);
        }
        if (f_st)
        {
            timeb = wm->start_time;
            sprintf(tbuf, " %20.20s", ctime(&timeb) + 4);
            tbuf[21] = '\0';
            strcat(linebuf, tbuf);
        }
        strcat(linebuf, "\n");
        break;

    default:
        acct_err(ACCT_WARN,
               _("An unknown record type (%4o) was found in the '%s' routine."),
        	typ, "print_rec()");
    }
    if (total_cmd_only != SUMMARY_ONLY || typ != REC_PROCESS)
        fprintf(stdout, "%s", linebuf);
}

/*
 * select_job:
 *	Examine the input record to see if it meets the selection criteria.
 *
 *	If scan == S_record, then we want all of the records in this job
 *	unless -Z or -e was specified.  If scan == S_job, then we need to
 *	check for No_Zero and all of the keys for a match.
 *
 *	Return 1 if there is a match.  Otherwise, return 0.
 */
int select_job(int typ, Sel_scope scan)
{
    struct wkmgmtbs  *wm;
    uint64_t    jid;
    uint64_t   prid;
    int64_t  seqno;
    uid_t    uid;

    /*
     *	If -F was specified without -Z, then we report all records.
     */
    if (Fullreport && !No_Zero)
        return(1);

    /*
     *	Extract selection info from record.
     */
    switch (typ)
    {
    case REC_PROCESS:
        if (acctrec.csa)
        {
            uid = acctrec.csa->ac_uid;
            prid = acctrec.csa->ac_prid;
            jid = acctrec.csa->ac_jid;
        }
        else if (acctrec.soj)
        {
            uid = acctrec.soj->ac_uid;
            prid = DEFAULT_PRID;
            jid = acctrec.soj->ac_jid;
        }
        else if (acctrec.eoj)
        {
            uid = acctrec.eoj->ac_uid;
            prid = DEFAULT_PRID;
            jid = acctrec.eoj->ac_jid;
        }
        else  /* Ignore configuration record. */
            return(0);
        seqno = DEFAULT_SEQNO;
        break;

    case REC_WKMG:
	wm = acctrec.wmbs;

	uid = wm->uid;
	prid = wm->prid;
	jid = wm->jid;
	seqno = wm->reqid;
	break;

    default:
        acct_err(ACCT_WARN,
               _("An unknown record type (%4o) was found in the '%s' routine."),
        	typ, "select_job()");
    }

    /*
     *	If we are excluding jid zero do so.
     */
    if (No_Zero && (jid == 0))
        return(0);

    if ((scan == S_record) && !exclusive)
        return(1);

    /*
     *	Now check other selection info...
     */
    if (Uid != -1 && Uid != uid)
        return(0);
    if (Seqno != -1 && Seqno != seqno)
        return(0);
    if (Jid != -1 && Jid != jid)
        return(0);
    if (Prid != -1 && Prid != prid)
        return(0);
    
    return(1); /* ok take it */
}

/*
 *	print summary line.
 */
void print_sum() 
{
    int    i;
    char   project[MAXPROJNAMELEN];
    char   user[32];

    if (total_cmd_only == 0)
        return;

    if (total_cmd_only == SUMMARY)
    {
        if (f_J)
        {
            fprintf(stdout, "TOTALS             ");  /* jid field */
            fprintf(stdout, "                  ");   /* project, user */
            fprintf(stdout, "                ");     /* cmd name */
        }
        else
        {
            fprintf(stdout, "TOTALS   ");  /* project field */
            fprintf(stdout, "                         "); /* user, cmd name */
        }
    }
    else if (total_cmd_only == SUMMARY_ONLY)
    {
        strcpy(user, uid_to_name(cmt.uid));
        if (user[0] == '?')
            sprintf(user, "%d", cmt.uid);
        if (f_J)
        {
            fprintf(stdout, "TOTALS             ");  /* jid field */
            strcpy(project, prid_to_name(cmt.prid));
            if (project[0] == '?')
                sprintf(project, "%lld", cmt.prid);
            fprintf(stdout, "%-8s %-8s ", project, user);
            fprintf(stdout, "                ");  /* cmd name */
        }
        else
        {
            fprintf(stdout, "TOTALS   ");   /* project field */
            fprintf(stdout, "%-8s ", user);
            fprintf(stdout, "                ");  /* cmd name */
        }
    }

    if (f_S)
    {
        if (total_cmd_only == SUMMARY_ONLY)
            fprintf(stdout, " 0x%016llx", cmt.ash);
        else
            fprintf(stdout, " %18s", " ");
    }
    if (f_st)
        fprintf(stdout, " %20.20s", " ");	/* start time */
    if (f_c)
    {
        fprintf(stdout, " %8.3f %8.3f %8.3f %8.3f",
                TIME_2_SECS(cmt.utime), TIME_2_SECS(cmt.stime),
		NSEC_2_SECS(cmt.runRealTime), NSEC_2_SECS(cmt.runVirtTime));
    }
    if (f_w)
    {
        fprintf(stdout, " %8.2f %8.2f %8.2f %9lld %9lld",
                NSEC_2_SECS(cmt.cpuDelayTime),
		NSEC_2_SECS(cmt.blkDelayTime),
                NSEC_2_SECS(cmt.swpDelayTime),
		cmt.corehimem, cmt.virthimem);
    }
    if (f_x)
    {
        fprintf(stdout, " %8.0f %8.0f %8lld %8lld",
                cmt.chr / 1024.0, cmt.chw / 1024.0, cmt.scr, cmt.scw);
    }

    if (f_V)
    {
	fprintf(stdout, " %8lld %8lld %8lld", cmt.pgswap, cmt.minflt,
		cmt.majflt);
    }

    if (f_b)
        fprintf(stdout, " %8.2f", cmt.sbu);
    fprintf(stdout, "\n");
    memset((char *)&cmt, '\0', sizeof(struct sumcmd));
    cmt.uid = cmt.prid = cmt.ash = -1;
}

/*
 *	Convert an argument to an integer ID.
 */
int64_t argtoi(char *c, char ch)
{
    int64_t  id;
    char    *str;

    if (*c >= '0' && *c <= '9')  /* Numeric */
    {
        id = atoll(optarg);
        if (id < 0)
            acct_err(ACCT_WARN,
                   _("Option '%s' requires a positive numeric argument."),
            	ch);
    }
    else  /* name? */
    {
        switch (ch)
        {
        case 'p':
            id = name_to_prid(c);
            str = "project-id";
            break;
        case 'u':
            id = name_to_uid(c);
            str = "user-id";
            break;
        default:
            acct_err(ACCT_ABORT,
                   _("An unknown option '%c' was specified."), ch);
        }
        if (id < 0)
            acct_err(ACCT_WARN,
                   _("Unable to map %s to %s in the '%s' routine."),
            	    c, str, "argtoi()");
    }
    
    return(id);
}

/*
 *	Add the current daemon accounting record to the cumulative total.
 */
void sum_daemon(int typ)
{
    int              i, j;
    struct wkmgmtbs *wm;

    switch (typ)
    {
    case REC_WKMG:
	wm = acctrec.wmbs;

	batchtot->utime += wm->utime;
	batchtot->stime += wm->stime;
	break;

    }	/* switch */
}

/*
 *	Output a daemon's total and grand total.
 */
void print_daemon(int typ)
{
    int              i, j;
    int              totline;
    struct sumbatch *wm;     /* workload management totals */

    switch (typ)
    {
    case REC_WKMG:
        wm = batchtot;
        if (f_c)
        {
            fprintf(stdout, "TOTALS  ");		/* Login */
            fprintf(stdout, "         ");		/* Project */
            fprintf(stdout, "                 ");	/* Queue */
            fprintf(stdout, "                 ");	/* Request */
            fprintf(stdout, "                 ");	/* Machine */
            fprintf(stdout, "          ");		/* Sequence/Request */
	    if (typ == REC_WKMG) {
		fprintf(stdout, "      ");              /* Array ID */
	    }
            fprintf(stdout, "% 8.4f %8.4f\n",
                    TIME_2_SECS(wm->utime), TIME_2_SECS(wm->stime));
            wm->utime = 0L;
            wm->stime = 0L;
        }
        break;

    }	/* switch */
}

/*
 *	Allocate memory for the /Workload management total array.
 */
void init_wm()
{
    if (batchtot != NULL)
        return;
    
    if ((batchtot = (struct sumbatch *)malloc(sizeof(struct sumbatch))) == NULL)
        acct_perr(ACCT_ABORT, errno,
        	_("There was insufficient memory available when allocating '%s'."),
        	"struct sumbatch");
    batchtot->utime = 0L;
    batchtot->stime = 0L;

    return;
}

/*
 * do_wm():
 *	Gather the information needed about this Workload management
 *	request and output it.
 *
 *	*jobhdr	= job header record
 *	offset	= file offset to the end of the job header record
 */
void do_wm(struct acctjob *jobhdr, int64_t offset)
{
    int		    batchtype;
    int             i;
    struct reqseg  *curr_seg;
    struct reqseg  *last_seg;	/* last segment in the reqseg list */
    struct reqseg  *prev;
    struct reqent  *curr_ent;
    struct wkmgmtbs *wmb;
    time_t          stime;

    if (seekacct(sfd, offset + jobhdr->aj_wkmg_start, SEEK_SET) < 0)
	acct_perr(ACCT_ABORT, errno,
	  _("An error occurred on the seek to the first '%s' record."),
            "workload management");

    /*
     * Create the list (req and req->first) which contains the information
     * about each portion of the workload management request.
     */
    for (i = 0, prev = NULL; i < jobhdr->aj_wkmg_num; i++)
    {
        if (readacctent(sfd, &acctrec, FORWARD) <= 0) {
	    acct_perr(ACCT_ABORT, errno,
		_("An error occurred during the reading of file '%s' %s."),
		spacct, "workload management record");
        }
        if (acctrec.wmbs == NULL) {
	    acct_err(ACCT_ABORT,
		_("An error occurred during the reading of file '%s' %s."),
		spacct, "workload management record");
        }
        wmb = acctrec.wmbs;

        curr_seg = get_reqseg();
        curr_ent = get_reqent();

        if (wmb->init_subtype == WM_NO_INIT)
            curr_seg->init_stat = INIT_MSG;
        else
            curr_seg->init_stat = wmb->init_subtype;
        if (wmb->term_subtype == WM_NO_TERM)
            curr_seg->term_stat = TERM_MSG;
        else
            curr_seg->term_stat = wmb->term_subtype;

        strcpy(curr_seg->qname, wmb->quename);
        curr_seg->jid = wmb->jid;
        curr_seg->start_time = curr_seg->end_time = wmb->start_time;
        curr_seg->qwait = wmb->qwtime;

        /*
         * Make sure that the reqseg list is in ascending
         * order according to start_time.
         */
        if (i == 0)
        {
            req->seqno = wmb->reqid;
            req->arrayid = wmb->arrayid;
            strcpy(req->reqname, wmb->reqname);
            req->first = curr_seg;
            req->wall_start = req->wall_end = wmb->start_time;
            batchtype = REC_WKMG;
            curr_seg->prev = NULL;
            curr_seg->next = NULL;
            last_seg = curr_seg;
        }
        else if (curr_seg->start_time >= prev->start_time)
        {
            curr_seg->prev = prev;
            curr_seg->next = NULL;
            prev->next = curr_seg;
            last_seg = curr_seg;
        }
        else
        {
            last_seg = prev;
            for (prev = prev->prev; prev != NULL; prev = prev->prev)
            {
                if (curr_seg->start_time >= prev->start_time)
                {
                    curr_seg->prev = prev;
                    curr_seg->next = prev->next;
                    curr_seg->next->prev = curr_seg;
                    prev->next = curr_seg;
                    break;
                }
            }
            if (prev == NULL)
            {
                /* 
                 * Insert at the start of the reqseg list.
                 */
                curr_seg->prev = NULL;
                curr_seg->next = req->first;
                req->first->prev = curr_seg;
                req->first = curr_seg;
            }
        }	/* if (i == 0) */

        curr_seg->list = curr_ent;

        stime = wmb->start_time;
        if (stime < req->wall_start)
            req->wall_start = stime;
        else if (stime > req->wall_end)
            req->wall_end = stime;

        curr_ent->uid = wmb->uid;
        curr_ent->prid = wmb->prid;
        curr_ent->utime = wmb->utime;
        curr_ent->stime = wmb->stime;

        prev = last_seg;
    }  /* for i */

    /*
     * Gather the usage information from the process records.  If
     * there is a new uid/prid/jid tuple in the process records, then
     * make a new entry for it.
     *
     * The process records should be in order by workload mgmt segment. 
     */
    
    /*
     *	Start-of-job, end-of-job, and configuration records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_job_start, SEEK_SET) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred on the seek to the first '%s' record."),
        	"soj, eoj, or cfg");

    req->curr = req->first;
    req->ent = req->curr->list;
    
    for (i = 0; i < jobhdr->aj_job_num; i++)
    {
        if (readacctent(sfd, &acctrec, FORWARD) <= 0)
        {
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the reading of file '%s' %s."),
            	spacct,
                      "soj, eoj, or cfg record");
        }
        if (acctrec.soj == NULL &&
            acctrec.eoj == NULL &&
            acctrec.cfg == NULL)
        {
            acct_err(ACCT_ABORT,
                   _("An error occurred during the reading of file '%s' %s."),
            	spacct, "soj, eoj, or cfg record");
        }
        wm_process(acctrec.prime->ah_type);
    }

    /*
     *	End-of-process records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_eop_start, SEEK_SET) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred on the seek to the first '%s' record."),
        	"eop");

    req->curr = req->first;
    req->ent = req->curr->list;

    for (i = 0; i < jobhdr->aj_eop_num; i++)
    {
        if (readacctent(sfd, &acctrec, FORWARD) <= 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the reading of file '%s' %s."),
            	spacct, "eop record");
        if (acctrec.csa == NULL)
            acct_err(ACCT_ABORT,
                   _("An error occurred during the reading of file '%s' %s."),
            	spacct, "eop record");
        wm_process(acctrec.prime->ah_type);
    }

    print_wm(batchtype);
    clean_wm();
}

/*
 * add_wm:
 *	If (action == FA_seg_before), add a reqseg structure before
 *	*curr.  Otherwise, add a reqseg structure after *curr.  Also
 *	create a reqent entry for the new reqseg entry.
 *
 *	Upon return, set req->curr and req->ent to point to the new entries.
 */
void add_wm(struct reqseg *curr, FAaction action)
{
    struct reqent  *new_ent;
    struct reqseg  *new_seg;

    new_ent = get_reqent();
    new_seg = get_reqseg();
    new_seg->list = new_ent;

    switch (action)
    {
    case FA_seg_before:	/* add the new segment before *seg */
        new_seg->next = curr;
        new_seg->prev = curr->prev;
        if (curr->prev != NULL)
            curr->prev->next = new_seg;
        else
            req->first = new_seg;
        curr->prev = new_seg;
        break;

    case FA_seg_after:	/* add the new segment after *seg */
        new_seg->next = curr->next;
        new_seg->prev = curr;
        if (curr->next != NULL)
            curr->next->prev = new_seg;
        curr->next = new_seg;
        break;
    }

    req->curr = new_seg;
    req->ent = new_ent;
}

/*
 * clean_wm:
 *	Zero out the workload management allocation blocks.
 */
void clean_wm()
{
    int             size;
    struct reqent  *ent;
    struct reqseg  *seg;

    size = sizeof(struct reqent) * (RENT_INC - 1);
    for (ent = rent->first; ent != NULL; ent = ent->next)
        memset((char *)(ent + 1), '\0', size);
    rent->curr = rent->first;
    rent->navail = RENT_INC - 1;

    size = sizeof(struct reqseg) * (RSEG_INC - 1);
    for (seg = rseg->first; seg != NULL; seg = seg->next)
        memset((char *)(seg + 1), '\0', size);
    rseg->curr = rseg->first;
    rseg->navail = RSEG_INC - 1;
}

/*
 * find_wm:
 *	Find the correct reqseg entry for this jid/btime combination.  If
 *	possible, use one of the existing reqseg entries.  Then find the
 *	corresponding reqent entry for the prid/uid pair.  If one does not
 *	exist, create one.
 *
 *	Note:  The start_time in the reqseg entries is initially the time
 *	stamp of the WM accounting record.  start_time could be after the
 *	start of the first process in this segment because of the timing
 *	of when WM accounting records are written and when processes
 *	(i.e., pipeclient) start.  If this is the case, we reset
 *	start_time.  For example, the pipeclient could start before the
 *	NQ_SENT_INIT record is written.
 */
void find_wm(uint64_t prid,uint64_t jid,uid_t uid,time_t btime,time_t etime)
{
    FAaction       action;
    uint64_t       next_jid;
    struct reqent  *curr_ent;
    struct reqent  *new_ent;
    struct reqent  *prev_ent;
    struct reqseg  *curr;
    struct reqseg  *prev;

    /*
     * See if we need to search the reqseg list from the start or
     * if the current location will do.
     */
    if (btime >= req->curr->start_time)
        curr = req->curr;
    else
        curr = req->first;

    for (prev = NULL, action = FA_seg_after; curr != NULL; curr = curr->next)
    {
        if (curr->next == NULL) {
            next_jid = NO_NEXT_JID;
        } else {
            next_jid = curr->next->jid;
        }

        /*
         * See if this jid/btime pair could possibly fit into this segment.
         */
        if (jid == curr->jid)
        {
            /*
             * This is it.
             */
            if (btime < curr->start_time)
                curr->start_time = btime;
            req->curr = curr;
            req->ent = curr->list;
            action = FA_find_ent;
            break;
        }
        else if (jid == next_jid)
        {
            prev = curr;
            continue;		/* the next segment is it */
        }
        else if (btime <= curr->start_time)
        {
            /*
             * If the jid doesn't match this segment or the next
             * segment and the time is before this segment's
             * start time, add a new segment.
             */
            action = FA_seg_before;
            break;
        }
		
        prev = curr;
    }

    /*
     * If we appear to be missing an workload mgmt record, fake a segment
     * just before or after the current one using the information
     * passed to this routine.
     */
    if ((action == FA_seg_before) || (action == FA_seg_after))
    {
        switch (action)
        {
        case FA_seg_before:   /* add a segment before the current one */
            add_wm(curr, FA_seg_before);
            break;

        case FA_seg_after:  /* add a segment after the previous one */
            add_wm(prev, FA_seg_after);
            break;
        }
        req->curr->jid = jid;
        req->curr->start_time = btime;
        req->curr->end_time = etime;
        req->curr->init_stat = INIT_MSG;
        req->curr->term_stat = TERM_MSG;

        req->ent->uid = uid;
        req->ent->prid = prid;
        return;
    }

    /*
     * Find the reqent entry which matches this uid/prid.  The
     * DEFAULT_PRID is from the soj or eoj record, which has no prid.
     */
    for (curr_ent = req->ent, prev_ent = NULL; curr_ent != NULL;
         curr_ent = curr_ent->next)
    {
        if (curr_ent->uid == uid)
        {
            if (curr_ent->prid == DEFAULT_PRID)
            {
                curr_ent->prid = prid;
                req->ent = curr_ent;
                return;
            }
            else if (prid == DEFAULT_PRID || curr_ent->prid == prid)
            {
                req->ent = curr_ent;
                return;
            }
        }
        prev_ent = curr_ent;
    }

    if (prev_ent == NULL)
        acct_err(ACCT_ABORT,
               _("A problem caused by a coding error %d was detected in the '%s' routine."),
        	1, "find_wm()");

    /*
     * Add a reqent entry to the end of the list.
     */
    new_ent = get_reqent();
    new_ent->next = prev_ent->next;
    prev_ent->next = new_ent;
    new_ent->uid = uid;
    new_ent->prid = prid;
    req->ent = new_ent;
}

/*
 * get_reqent:
 *	Return a pointer to the next free reqent structure.  If there
 *	aren't any, allocate RENT_INC - 1 free entries.  The first
 *	entry in each block is used for administrative info.
 */
struct reqent *get_reqent()
{
    int             size;
    struct reqent  *blk;

    /*
     * If needed, allocate a new block of structures.
     */
    if (rent->navail == 0)
    {
        if ((rent->curr == NULL) ||
            ((rent->curr != NULL) && (rent->curr->next == NULL)))
        {
            size = sizeof(struct reqent) * RENT_INC;
            if ((blk = (struct reqent *) malloc(size)) == NULL)
                acct_perr(ACCT_ABORT, errno,
                	_("There was insufficient memory available when allocating '%s'."),
                	"struct reqent");
            memset((char *)blk, '\0', size);

            if (rent->first == NULL)
                rent->first = blk;
            else
                rent->last->next = blk;
            rent->last = blk;
            rent->curr = blk;
        }
        else
            rent->curr = rent->curr->next;
        rent->navail = RENT_INC - 1;
    }
    blk = (rent->curr) + rent->navail;
    (rent->navail)--;
    return(blk);
}

/*
 * get_reqseg:
 *	Return a pointer to the next free reqseg structure.  If there
 *	aren't any, allocate RSEG_INC - 1 free entries.  The first
 *	entry in each block is used for administrative info.
 */
struct reqseg *get_reqseg()
{
    int             size;
    struct reqseg  *blk;

    /*
     * If needed, allocate a new block of structures.
     */
    if (rseg->navail == 0)
    {
        if ((rseg->curr == NULL) ||
            ((rseg->curr != NULL) && (rseg->curr->next == NULL)))
        {
            size = sizeof(struct reqseg) * RSEG_INC;
            if ((blk = (struct reqseg *) malloc(size)) == NULL)
                acct_perr(ACCT_ABORT, errno,
                	_("There was insufficient memory available when allocating '%s'."),
                	"struct reqseg");
            memset((char *)blk, '\0', size);

            if (rseg->first == NULL)
                rseg->first = blk;
            else
                rseg->last->next = blk;
            rseg->last = blk;
            rseg->curr = blk;
        }
        else
            rseg->curr = rseg->curr->next;
        rseg->navail = RSEG_INC - 1;
    }
    blk = (rseg->curr) + rseg->navail;
    (rseg->navail)--;
    return(blk);
}

/*
 * wm_process:
 *	Add the pacct information to the correct reqseg and reqent
 *	structures.
 */
void wm_process(int type)
{
    uint64_t        prid;
    uint64_t        jid;
    uid_t           uid;
    time_t          elapse;
    time_t          end_time;
    time_t          start_time;
    struct reqent  *rent;
    struct reqseg  *rseg;
    double	    tmpdbl;

    switch (type)
    {
    case ACCT_KERNEL_CSA:
        prid = acctrec.csa->ac_prid;
        jid = acctrec.csa->ac_jid;
        uid = acctrec.csa->ac_uid;
	tmpdbl = TIME_2_SECS(acctrec.csa->ac_etime);
        elapse = (time_t)tmpdbl;
        start_time = acctrec.csa->ac_btime;
        end_time = start_time + elapse;
        break;

    case ACCT_KERNEL_SOJ:
        prid = DEFAULT_PRID;
        jid = acctrec.soj->ac_jid;
        uid = acctrec.soj->ac_uid;
        if (acctrec.soj->ac_type == AC_SOJ)
            start_time = acctrec.soj->ac_btime;
        else
            start_time = acctrec.soj->ac_rstime;
        end_time = start_time;
        break;

    case ACCT_KERNEL_EOJ:
        prid = DEFAULT_PRID;
        jid = acctrec.eoj->ac_jid;
        uid = acctrec.eoj->ac_uid;
        start_time = acctrec.eoj->ac_btime;
        end_time = acctrec.eoj->ac_etime;
        break;

    default:  /* Ignore configuration record. */
        return;
    }

    if (start_time < req->wall_start)
        req->wall_start = start_time;
    else if (end_time > req->wall_end)
        req->wall_end = end_time;

    find_wm(prid, jid, uid, start_time, end_time);
    rent = req->ent;
    rseg = req->curr;

    if (type == ACCT_KERNEL_CSA)
    {
        if (rseg->end_time < end_time)
            rseg->end_time = end_time;
        rent->utime += FTOL(ucputime(&acctrec));
        rent->stime += acctrec.csa->ac_stime;
        if (acctrec.delay != NULL)
        {
            rent->cpuDelayTime += acctrec.delay->ac_cpu_delay_total;
            rent->blkDelayTime += acctrec.delay->ac_blkio_delay_total;
            rent->swpDelayTime += acctrec.delay->ac_swapin_delay_total;
        }
    }
    else if (type == ACCT_KERNEL_EOJ)
    {
        rseg->end_time = end_time;
        rseg->corehimem = acctrec.eoj->ac_corehimem;
        rseg->virthimem = acctrec.eoj->ac_virthimem;
    }

    return;
}

/*
 * print_wm():
 *	Output information about an workload mgmt request.  This report is
 *	generated when -N is specified.
 */
void print_wm(int batchtype)
{
    int             skip, only_once = 0;
    char            project[MAXPROJNAMELEN];
    char            user[32];
    time_t          wallclock;	/* secs */
    static int      do_hdr = 0;
    struct reqent  *cent;
    struct reqseg  *curr;

    if (do_hdr == 0)
    {
        do_hdr++;
        fprintf(stdout, "Request Array ");
        fprintf(stdout, "Request                 Job         "
                "Init    Term    Queue            WallClock Elapse   "
                "Qwait      Himem (kbytes)    User     Project  "
                "CPU Time        Delay (seconds)\n");

        fprintf(stdout, "Id      Id    ");
        fprintf(stdout, "Name                    Id          "
                "Status  Status  Name             (seconds) (secs)   "
                "(secs)     Core    Virtual   Name     Name     "
                "(seconds)    CPU      Block     Swap\n");

        fprintf(stdout, "======= ===== ");
        fprintf(stdout, "================ ================== "
                "======= ======= ================ ========= ======== "
                "======== ========= ========= ======== ======== "
                "========== ========= ========= =========\n");
    }

    /*
     * Calculate the wall clock time for the entire request.
     * This will be reported only once.
     */
    wallclock = req->wall_end - req->wall_start;

    /*
     * Print out information about each segment of the 
     * Workload management request.
     */
    for (curr = req->first; !only_once && curr != NULL; curr = curr->next)
    {
        /* If there is only one workload mgmt entry for this job, 
         *  curr->next will point back to curr and we get into an infinite loop.
         * If this is the case, only go through here once (only_once).
         */
        if (curr == curr->next)
            only_once = 1;
        skip = 0;
        /*
         * Print out information about the different uid/prid
         * combinations which are associated with this jid.
         */
        for (cent = curr->list; cent != NULL; cent = cent->next)
        {
            fprintf(stdout,
		"%-7lld %-5d %-16.16s %#18llx %-7.7s %-7.7s %-16.16s ",
		req->seqno, req->arrayid, req->reqname, curr->jid,
		WM_Init_statname[curr->init_stat],
		WM_Term_statname[curr->term_stat], curr->qname);

            if (skip)
                fprintf(stdout, "%-9d %-8d %-8d %-9d %-9d ", 0, 0, 0, 0, 0);
            else
            {
                skip++;
                fprintf(stdout, "%-9d %-8d %-8lld %-9lld %-9lld ",
                        wallclock, curr->end_time - curr->start_time,
                        curr->qwait, curr->corehimem, curr->virthimem);
            }

            strcpy(project, prid_to_name(cent->prid));
            if (project[0] == '?')
                sprintf(project, "%lld", cent->prid);
            strcpy(user, uid_to_name(cent->uid));
            if (user[0] == '?')
                sprintf(user, "%d", cent->uid);
            
            fprintf(stdout, "%-8.8s %-8.8s %10.3f %9.2f %9.2f %9.2f\n",
                    user, project,
                    TIME_2_SECS(cent->utime + cent->stime),
                    NSEC_2_SECS(cent->cpuDelayTime),
		    NSEC_2_SECS(cent->blkDelayTime),
		    NSEC_2_SECS(cent->swpDelayTime));
        }  /* for cent */
        wallclock = 0;	/* report wall clock time only once */
    }  /* for curr */
}
