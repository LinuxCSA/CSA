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
 * csadrep:
 *	Condense job records and generate a report on the
 *	use of the daemons (workload mgmt)
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "csa.h"
#include "csaacct.h"

#include <ctype.h>
#include <fcntl.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"

#define	KBYTE		1024	/* bytes per kbyte */

extern	int	db_flag;		/* debug flag */

/*
 *	workload management data information.
 */
struct mf
{
    char	name[16];	/* originating machine name */
    int		numjobs;	/* # of jobs from this machine */
    struct mf	*next;
};

#define	MAX_QUEUE	250	/* maximum number of queues */
				/* WARNING: changing this number invalidates */
				/* any binary files you may have already */
				/* created */

struct drep_batch
{
    u_int64_t	stime;		/* system time (usecs) */
    u_int64_t	utime;		/* user time */
    struct {
        u_int64_t    stime;	/* system time */
        u_int64_t    utime;	/* user time */
        char        qname[16];	/* queue name */
        int         numjobs;	/* number of requests */
        int64_t     quwait;	/* total queue wait time (secs) */
        struct mf  *mfp;	/* originating mainframe specific info*/
        u_int64_t    corehimem;		/* job core memory hiwater */
        u_int64_t    hi_corehimem;	/* hi job core memory hiwater */
        u_int64_t    lo_corehimem;	/* low job core memory hiwater */
        u_int64_t    virthimem;		/* job virtual memory hiwater */
        u_int64_t    hi_virthimem;	/* hi job virtual memory hiwater */
        u_int64_t    lo_virthimem;	/* low job virtual memory hiwater */
        u_int64_t    lotime;	/* low CPU time use */
        u_int64_t    hitime;	/* hi CPU time use */
    } queues[MAX_QUEUE];
};

/*
 *	workload management very verbose per request data.
 */
struct request
{
    uid_t	uid;		/* user id - who is it */
    char	reqname[16];	/* request name */
    int		arrayid;	/* workload management array ID */
    time_t	startime;	/* start time */
    u_int64_t	stime;		/* system CPU time */
    u_int64_t	utime;		/* user CPU time */
    u_int64_t	corehimem;	/* job core memory hiwater */
    u_int64_t	virthimem;	/* job virtual memory hiwater */
    u_int64_t	cpu_delay;	/* CPU delay */
    u_int64_t	blkio_delay;	/* block I/O delay */
    u_int64_t	swapin_delay;	/* swap in delay */
    char	machine[16];	/* originating machine name */
    int64_t	seq;		/* workload mgmt request id */
    int		flag;		/* internal flag */
    struct	request	*next;	/* next request */
};

/*
 *	workload management information from a binary input file.
 */
struct rd_request
{
    struct request	*req;	/* list of requests for this queue */
    int			ind;	/* requests[] index */
};

/*
 *	workload management header information written out to binary file.
 */
struct wm_hdr
{
    char	name[8];
    int		count;
};

/*
 *	Time/delay information.
 */
struct drep_times
{
    u_int64_t	stime;		/* system cpu time */
    u_int64_t	utime;		/* user cpu time */
    u_int64_t	cpu_delay;	/* CPU delay */
    u_int64_t	blkio_delay;	/* block I/O delay */
    u_int64_t	swapin_delay;	/* swap in delay */
};

/*
 *	Job data.
 */
struct drep_jobs
{
    int	total;
};

/*
 *	Job types.
 */
#define	BATCH_JOB	01
#define	INTER_JOB	02

/*
 *	Record types.
 */
#define	REC_PROCESS	01
#define	REC_WKMG	010

/*
 *	Globals.
 */
int	Alljobs		= 0;
int	verbose		= 0;
int	repflag		= 0;
char	*Prg_name	= "";
int	sfd		= -1;
char	*spacct		= SPACCT;
int	sflag		= 0;
int	ofile		= 0;		/* output file descriptor */
int	ifile		= 0;		/* input file descriptor */
int	erflag		= 0;
int	noascii		= 0;

/*
 *	Data chains for each daemon.
 */

struct	drep_jobs	inter, batch;
struct	drep_times	itimes, btimes;
struct	drep_batch	qinfo;

struct	request		*requests[MAX_QUEUE];

/*
 *	Accumulators for an workload management job
 */
u_int64_t	Q_utime;	/* total user cpu time */
u_int64_t	Q_stime;	/* total system cpu time */
u_int64_t	Q_corehimem;	/* hiwater core memory mark */
u_int64_t	Q_virthimem;	/* hiwater virtual memory mark */
u_int64_t	Q_cpu_delay;	/* total CPU delay */
u_int64_t	Q_blkio_delay;	/* total block I/O delay */
u_int64_t	Q_swapin_delay;	/* total swap in delay */


/*
 *	Reporting flags.
 */
#define	REP_WKMG	01	/* report on workload mgmt flag */
#define	REP_BI		02	/* report on batch vs. interactive flag */

#define REP_ALL		(REP_WKMG|REP_BI)

char            acctentbuf[ACCTENTSIZ];
struct acctent  acctrec = {acctentbuf};

/*
 *	Function prototypes.
 */
void	add_wm(struct drep_batch *,struct drep_batch *,struct rd_request *);
char	*alloc(int);
void	birep();
void	dealloc(char *);
void	dojob(struct acctjob *, int64_t);
double	fperc(double, double);
void	wm_rep();
float	perc(int, int);
void	rd_ofile(char *);
void	report();
void	scan_spacct();
void	Sumit(int, int, int, int);
void	sum_wm(int, int, int);
void	wr_ofile();

void usage()
{
    acct_err(ACCT_ABORT,
           "Command Usage:\n%s\t[-Aajn] [-D level] [-V level] [-o ofile] [-s spacct]\n%s\t[-A] [-D level] [-o ofile [-N]] [-s spacct]\n%s\t[-ajn] [-D level] [-V level] [-o ofile] files\n%s\t[-D level] [-o ofile [-N]] files",
    	Prg_name, Prg_name, Prg_name, Prg_name);
}

main(int argc, char **argv)
{
    struct stat	stbuf;
    char		ch;
    int		c;
    extern char	*optarg;
    extern int	optind;

    Prg_name = argv[0];
    while ((c = getopt(argc, argv, "ajntANo:s:D:V:")) != EOF)
    {
        ch = (char) c;
        switch (ch)
        {
        case 'a':
            repflag = REP_ALL;
            break;
        case 'j':
            repflag |= REP_BI;
            break;
        case 'n':
            repflag |= REP_WKMG;
            break;
        case 'A':
            Alljobs = 1;
            break;
        case 'N':
            noascii = 1;
            break;
        case 'o':
            if ((ofile = open(optarg, O_WRONLY|O_CREAT, 0600)) < 0)
            {
                acct_perr(ACCT_FATAL, errno,
                	_("An error occurred during the opening of file '%s'."),
                	optarg);
                usage();
            }
            break;
        case 's':
            spacct = optarg;
            sflag++;
            break;
        case 'D':
            db_flag = atoi(optarg);
            if (db_flag < 0 || db_flag > 2)
            {
                acct_err(ACCT_FATAL,
                       _("The (-%s) option's argument, '%s', is invalid."),
                	"D", optarg);
                usage();
            }
            break;
        case 'V':
            verbose = atoi(optarg);
            if (verbose < 0 || verbose > 4)
            {
                acct_err(ACCT_FATAL,
                       _("The (-%s) option's argument, '%s', is invalid."),
                	"V", optarg);
                usage();
            }
            break;

        default:
            acct_err(ACCT_FATAL,
                   _("An unknown option '%c' was specified."), optopt);
            usage();
        } /* end switch */
    } /* end while getopt */

    /*
     *	Check for valid combinations.
     */
    if (noascii && !ofile)
    {
        acct_err(ACCT_FATAL,
               _("The (-%s) option must be used with the (-%s) option."),
        	"N", "o");
        usage();
    }

    if (sflag && (argc != optind))
        usage();

    /*
     *	If nothing has been specified, generate the default report.
     */
    if (repflag == 0)
        repflag = REP_BI;

    if (argc > optind)
    {
        for ( ; optind < argc; optind++)
        {
            if ((ifile = open(argv[optind], O_RDONLY)) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the opening of file '%s'."),
                	argv[optind]);

            if (fstat(ifile,&stbuf) < 0)
            {
                acct_perr(ACCT_WARN, errno,
                	_("An error occurred getting the status of file '%s'."),
                	argv[optind]);
                erflag = 1;
                close(ifile);
                continue;
            }

            if (stbuf.st_size == 0)
            {
                acct_err(ACCT_WARN,
                       _("File '%s' has a length of zero and is being ignored."),
                	argv[optind]);
                erflag = 1;
                close(ifile);
                continue;
            }

            if (db_flag > 0)
                fprintf(stderr, "(1): processing file %s\n", argv[optind]);
            rd_ofile(argv[optind]);
            close(ifile);
        }

    } else {
        /*
         *	Open the sorted pacct file.
         */
        if ((sfd = openacct(spacct, "r")) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the opening of file '%s'."),
            	spacct);

        /*
         *	Scan the sorted pacct file for input data.
         */
        scan_spacct();
        closeacct(sfd);
    }

    /*
     *	Write the binary output data.
     */
    if (ofile)
        wr_ofile();

    /*
     *	Write the ascii output data.
     */
    if (!noascii)
        report();

    exit(erflag);
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
         *	While still in this uptime, read the job header records.
         */
        while (uptime_size &&
               readacct(sfd, (char *) &jobhdr, sizeof(struct acctjob)) ==
               sizeof(struct acctjob))
        {
            /*  Remember end of the job header record. */
            offset = seekacct(sfd, 0, SEEK_CUR);
            /*
             *	Check job header for eoj indicator.
             *	(Has this job terminated?)
             */
            wanted = 0;
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
                dojob(&jobhdr, offset);
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
 *	Process each job we are interested in.
 */
void dojob(struct acctjob *jobhdr, int64_t offset)
{
    int i;
    int once, numbatch;

    Q_stime = Q_utime = Q_corehimem = Q_virthimem = 0;
    Q_cpu_delay = Q_blkio_delay = Q_swapin_delay = 0;
    numbatch = jobhdr->aj_wkmg_num;
    /*
     *	Do the per job information
     */
    if (numbatch)
    {
        batch.total++;

    }
    else
    {
        inter.total++;
    }

    /*
     *	End-of-process records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_eop_start, SEEK_SET) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred on the seek to the first '%s' record."),
        	"eop");

    once = 1;
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

        Sumit(REC_PROCESS, numbatch, once, numbatch);
        once = 0;
    }
    /*
     *	Start-of-job, end-of-job, and configuration records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_job_start, SEEK_SET) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred on the seek to the first '%s' record."),
        	"soj, eoj, or cfg");
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
        
        Sumit(REC_PROCESS, numbatch, once, numbatch);
    }

    /*
     *	Workload management records.
     */
    if (seekacct(sfd, offset + jobhdr->aj_wkmg_start, SEEK_SET) < 0) {
	acct_perr(ACCT_ABORT, errno,
	   _("An error occurred on the seek to the first '%s' record."),
		 "Workload Mgmt");
    }
    once = 1;
    for (i = 0; i < jobhdr->aj_wkmg_num; i++)
    {
        if (readacctent(sfd, &acctrec, FORWARD) <= 0) {
	    acct_perr(ACCT_ABORT, errno,
		_("An error occurred during the reading of file '%s' %s."),
		  spacct, "Workload Mgmt record");
	}
        if (acctrec.wmbs == NULL) {
	    acct_perr(ACCT_ABORT, errno,
		_("An error occurred during the reading of file '%s' %s."),
		   spacct, "Workload Mgmt record");
	}
        Sumit(REC_WKMG, numbatch, once, numbatch);
        once = 0;
    }

    return;
}

/*
 *	Routine to summarize records...
 */
void Sumit(int typ, int type, int once, int numbatch)
{
    char      err_str[180];
    u_int64_t  tim, del;

    if (type)
        type = BATCH_JOB;
    else
        type = INTER_JOB;

    /*
     *	Sum up the data
     */
    switch (typ)
    {
    case REC_PROCESS:
        if (invalid_pacct(&acctrec, err_str) != INV_NO_ERROR)
        {
            acct_err(ACCT_WARN,
                   _("%s."),
            	err_str);
            if (db_flag > 0)
            {
                fprintf(stderr, "(1): command = <%.16s>\n",
                        acctrec.csa->ac_comm);
            }
            return;
        }

        if (acctrec.csa)
        {
            tim = acctrec.csa->ac_stime;
            if (type == BATCH_JOB)
            {
                btimes.stime += tim;
                Q_stime += tim;
            }
            else
                itimes.stime += tim;

            tim = acctrec.csa->ac_utime;
            if (type == BATCH_JOB)
            {
                btimes.utime += tim;
                Q_utime += tim;
            }
            else
                itimes.utime += tim;
        }
	if (acctrec.delay)
	{
	    del = acctrec.delay->ac_cpu_delay_total;
	    if (type == BATCH_JOB)
	    {
		btimes.cpu_delay += del;
		Q_cpu_delay += del;
	    }
	    else
		itimes.cpu_delay += del;

	    del = acctrec.delay->ac_blkio_delay_total;
	    if (type == BATCH_JOB)
	    {
		btimes.blkio_delay += del;
		Q_blkio_delay += del;
	    }
	    else
		itimes.blkio_delay += del;

	    del = acctrec.delay->ac_swapin_delay_total;
	    if (type == BATCH_JOB)
	    {
		btimes.swapin_delay += del;
		Q_swapin_delay += del;
	    }
	    else
		itimes.swapin_delay += del;
	}
        else if (acctrec.cfg)
            ;
        else if (acctrec.soj)
            ;
        else if (acctrec.eoj)
        {
            if (Q_corehimem < acctrec.eoj->ac_corehimem)
                Q_corehimem = acctrec.eoj->ac_corehimem;
            if (Q_virthimem < acctrec.eoj->ac_virthimem)
                Q_virthimem = acctrec.eoj->ac_virthimem;
        }
        else
            acct_err(ACCT_ABORT,
                   _("A corrupt pacct record was found in file '%s'."),
            	spacct);
        break;

    case REC_WKMG:
	sum_wm(once, numbatch, REC_WKMG);
	break;
 
    default:
        acct_err(ACCT_ABORT,
               _("An unknown record type (%4o) was found in the '%s' routine."),
        	typ, "Sumit()");
    }		/* end switch */
}

/*
 *	sum up the workload management data
 */
void sum_wm(int once, int numbatch, int batchtype)
{
    int            arrayid;
    char           err_str[180];
    char           machname[16];
    struct mf     *mp;
    int            not_found = 0;
    int            q;
    char           quename[16];
    int64_t        qwtime;   
    char           reqname[16];
    int64_t        seq;
    int64_t        stime;
    uint64_t       t_systime;
    uint64_t       t_utime;
    uid_t          uid;
    int64_t        utime;
    struct wkmgmtbs *wm;
    static int             nrec;
    static struct request *rp;

    wm = acctrec.wmbs;
    if (invalid_cwm(wm, err_str) != INV_NO_ERROR) {
	acct_err(ACCT_WARN, _("%s."), err_str);
        return;
    }
    strcpy(quename, wm->quename);
    strcpy(machname, wm->machname);
    strcpy(reqname, wm->reqname);
    stime = wm->stime;
    utime = wm->utime;
    qwtime = wm->qwtime;
    uid = wm->uid;
    seq = wm->reqid;
    arrayid = wm->arrayid;

    /*
     * Find q index into qinfo struct.
     */
    for (q = 0; q < MAX_QUEUE; q++)
    {
        if (qinfo.queues[q].qname[0] == '\0')
        {
            not_found = 1;
            break;
        }
        if (!strcmp(qinfo.queues[q].qname, quename))
            break;
    }
    if (q == MAX_QUEUE) {
	acct_err(ACCT_FATAL,
	  _("MAX_QUEUE exceeded. Overflow of the WKMG queue table can be corrected by recompiling with a larger MAX_QUEUE value."));
        exit(3);
    }
    if (not_found)
        strcpy(qinfo.queues[q].qname, quename);

    /*
     * Accumulate Q's new times from process records... was processed
     * before workload management!
     */
    t_systime = Q_stime + stime;
    t_utime = Q_utime + utime;
    btimes.stime += stime;
    btimes.utime += utime;

    qinfo.queues[q].stime += t_systime;
    qinfo.queues[q].utime += t_utime;
    qinfo.stime += t_systime;
    qinfo.utime += t_utime;
    qinfo.queues[q].corehimem += Q_corehimem;
    qinfo.queues[q].virthimem += Q_virthimem;
    if (qinfo.queues[q].lo_corehimem == 0 ||
        Q_corehimem < qinfo.queues[q].lo_corehimem)
    {
        if (Q_corehimem > 0)
            qinfo.queues[q].lo_corehimem = Q_corehimem;
    }
    if (qinfo.queues[q].hi_corehimem < Q_corehimem)
        qinfo.queues[q].hi_corehimem = Q_corehimem;
    if (qinfo.queues[q].lo_virthimem == 0 ||
        Q_virthimem < qinfo.queues[q].lo_virthimem)
    {
        if (Q_virthimem > 0)
            qinfo.queues[q].lo_virthimem = Q_virthimem;
    }
    if (qinfo.queues[q].hi_virthimem < Q_virthimem)
        qinfo.queues[q].hi_virthimem = Q_virthimem;

    if (once)
    {
        nrec = 0;
        qinfo.queues[q].numjobs++;
    }
    qinfo.queues[q].quwait += qwtime;
    for (mp = qinfo.queues[q].mfp; mp != NULL; mp = mp->next)
    {
        if (!strcmp(mp->name, machname))
            break;
    }
    if (mp == NULL)
    {
        mp = (struct mf *) alloc(sizeof(struct mf));
        strcpy(mp->name, machname);
        mp->next = qinfo.queues[q].mfp;
        qinfo.queues[q].mfp = mp;
    }
    if (once)
        mp->numjobs++;

    /*
     *	request pointers are by queue to ease later use.
     *
     *	If this is a new workload management request, add a new
     *	element to the appropriate chain.  Otherwise, update the
     *	cpu times to include the shepherd cpu time.
     */
    if (once)
    {
        rp = (struct request *) alloc(sizeof(struct request));
        rp->next = requests[q];
        requests[q] = rp;

        /*
         *	Set up uid and request name info
         */
        strcpy(rp->reqname, reqname);
        rp->uid = uid;

        /*
         *	Set up system/user time info.  Depends on process records
         *	being processed first.
         */
        rp->stime = t_systime;
        rp->utime = t_utime;
        rp->corehimem = Q_corehimem;
        rp->virthimem = Q_virthimem;
	rp->cpu_delay = Q_cpu_delay;
	rp->blkio_delay = Q_blkio_delay;
	rp->swapin_delay = Q_swapin_delay;
        /*
         *	Machine name, sequence number/request ID, and array ID.
         */
        rp->seq = seq;
        rp->arrayid = arrayid;
        strcpy(rp->machine, machname);
    }
    else
    {
        rp->stime += t_systime;
        rp->utime += t_utime;
    }

    /*
     *	If this is the last workload management record in this job, then
     *	see if we need to update the min and max cpu time for the queue.
     */
    nrec++;
    if (nrec == numbatch)
    {
        int	job_time;	/* total cpu time for this job */

        job_time = rp->stime + rp->utime;
        if ((qinfo.queues[q].lotime) == 0 ||
            (job_time < qinfo.queues[q].lotime))
        {
            if (job_time > 0)
                qinfo.queues[q].lotime = job_time;
        }
        if (qinfo.queues[q].hitime < job_time)
            qinfo.queues[q].hitime = job_time;
    }

    /*
     *	Zero job totals.
     */
    Q_utime = Q_stime = Q_corehimem = Q_virthimem = 0;
    Q_cpu_delay = Q_blkio_delay = Q_swapin_delay = 0;
}


/*
 *	Generate output based on option flags.
 */
void report() 
{
    if (repflag & REP_BI)
        birep();

    if (repflag & REP_WKMG)
        wm_rep();

}

/*
 *	perc and fperc calculate percentage.
 */
double fperc(double num, double tot)
{
    if (tot == 0)
        return(100.0);
    return(100.0 * (num / tot));
}

float perc(int num, int tot)
{
    if (tot == 0)
        return(100.0);
    return(100.0 * ((float) num / (float) tot));
}

/*
 *	Batch vs. Interactive report
 */
void birep() 
{
    double   f1, f2, f3, f4;
    double   p1, p2, p3;
    int      alljobs;
    double   alltime;
    char    *jtype;

    /*
     *	print heading for number of jobs summary.
     */
    fprintf(stdout, "Job Type Report\n");
    fprintf(stdout, "===============\n\n");

    fprintf(stdout, "   Job        Total Job Count  \n");
    fprintf(stdout, "   Type       (number)   (%%)    \n");
    fprintf(stdout, "-----------  ----------------  \n");

    alljobs = inter.total + batch.total;

    f1 = perc(inter.total, alljobs);
    jtype = "Interactive";
    fprintf(stdout, "%-11.11s  %10d (%3.0f)  \n",
            jtype, inter.total, f1);

    f1 = perc(batch.total, alljobs);
    jtype = "Batch";
    fprintf(stdout, "%-11.11s  %10d (%3.0f)  \n",
            jtype, batch.total, f1);

    jtype = "Grand Total";
    fprintf(stdout, "%-11.11s  %10d (%3.0f)  \n",
            jtype, alljobs, 100.0);

    /*
     *	print heading for cpu comparison.
     */
    fprintf(stdout, "\n\n");
    fprintf(stdout, "CPU Usage Report\n");
    fprintf(stdout, "================\n\n");

    fprintf(stdout, "   Job        Total CPU Time   ");
    fprintf(stdout, "System CPU Time    User CPU Time  \n");

    fprintf(stdout, "   Type        (sec)     (%%)   ");
    fprintf(stdout, "  (sec)     (%%)     (sec)     (%%) \n");

    fprintf(stdout, "-----------  ----------------  ");
    fprintf(stdout, "----------------  ----------------\n");
        
    alltime = TIME_2_SECS(btimes.stime + btimes.utime + 
                          itimes.stime + itimes.utime);
    f1 = TIME_2_SECS(itimes.stime + itimes.utime);
    f2 = TIME_2_SECS(itimes.stime);
    f3 = TIME_2_SECS(itimes.utime);
    p1 = fperc(f1, alltime);
    p2 = fperc(f2, alltime);
    p3 = fperc(f3, alltime);
    jtype = "Interactive";
    fprintf(stdout, "%-11.11s  %10.0f (%3.0f)  ", jtype, f1, p1);
    fprintf(stdout, "%10.0f (%3.0f)  %10.0f (%3.0f)\n", f2, p2, f3, p3);

    f1 = TIME_2_SECS(btimes.stime + btimes.utime);
    f2 = TIME_2_SECS(btimes.stime);
    f3 = TIME_2_SECS(btimes.utime);
    p1 = fperc(f1, alltime);
    p2 = fperc(f2, alltime);
    p3 = fperc(f3, alltime);
    jtype = "Batch";
    fprintf(stdout, "%-11.11s  %10.0f (%3.0f)  ", jtype, f1, p1);
    fprintf(stdout, "%10.0f (%3.0f)  %10.0f (%3.0f)\n", f2, p2, f3, p3);
	
    f1 = alltime;
    f2 = TIME_2_SECS(btimes.stime + itimes.stime);
    f3 = TIME_2_SECS(btimes.utime + itimes.utime);
    p1 = fperc(f1, alltime);
    p2 = fperc(f2, alltime);
    p3 = fperc(f3, alltime);
    jtype = "Grand Total";
    fprintf(stdout, "%-11.11s  %10.0f (%3.0f)  ", jtype, f1, p1);
    fprintf(stdout, "%10.0f (%3.0f)  %10.0f (%3.0f)\n", f2, p2, f3, p3);
}

int ontph;


int ondmh;

/*
 *	workload management report.
 */
void wm_rep()
{
    struct mf *mp;
    struct request *rp, *rrp;
    int        q;
    int        once = 0;
    int64_t    tqws;
    int        njobs, totjobs;
    u_int64_t   ttime, attime;
    double     p1, tqwt, avqw;
    double     totcpu, cputime;
    char       user[32];

    tqws = 0;
    for (q = 0; q < MAX_QUEUE; q++)
    {
        if (qinfo.queues[q].numjobs == 0)
            continue;
        if (once++ == 0)
        {
            fprintf(stdout, "\n\n");
            fprintf(stdout, "Batch Queue Report\n");
            fprintf(stdout, "==================\n\n");

            /*
             *	DATA_MIG:  Add column for DM to header and output
             *	lines
             */
            fprintf(stdout, "     Queue        Number     CPU Time    ");
            fprintf(stdout, "  Used   Ave Queue \n");

            fprintf(stdout, "     Name         of Jobs  (sec)    (%%)  ");
            fprintf(stdout, "  Tapes  Wait (sec)\n");

            fprintf(stdout, "---------------- -------- -------------- ");
            fprintf(stdout, "-------- ----------\n");
        }
        cputime = TIME_2_SECS(qinfo.queues[q].stime + qinfo.queues[q].utime);
        p1 = fperc(cputime, TIME_2_SECS(btimes.stime + btimes.utime));
        avqw = qinfo.queues[q].quwait / (double) qinfo.queues[q].numjobs;
        tqws += qinfo.queues[q].quwait;
        fprintf(stdout, "%-16.16s %8d %8.0f (%3.0f) ",
                qinfo.queues[q].qname, qinfo.queues[q].numjobs, cputime, p1);
        fprintf(stdout, "%8d %10.2f\n", 0, avqw);
    }
    if (once)
    {
        tqwt = tqws / (double) batch.total;
        fprintf(stdout, "%-16.16s %8d %8.0f (%3.0f) ",
                "Totals", batch.total,
                TIME_2_SECS(btimes.stime + btimes.utime), 100.0);
        fprintf(stdout, "%8d %10.2f\n", 0, tqwt);
    }
    else
        fprintf(stdout, "\n\nNo Batch usage\n");

    /*
     *	End non-verbose output.
     */
    if ((!verbose) || !once)
        return;

    for (q = 0; q < MAX_QUEUE; q++)
    {
	if (qinfo.queues[q].numjobs == 0)
            continue;
	/*
	 *	Individual queue date.
	 */
	fprintf(stdout, "\n\n");
	fprintf(stdout,
                "Queue:                                          %.16s\n",
		qinfo.queues[q].qname);

	cputime = TIME_2_SECS(qinfo.queues[q].stime + qinfo.queues[q].utime);
	cputime = cputime / (double) qinfo.queues[q].numjobs;
	fprintf(stdout,
                "Average CPU Time:                               %.3f\n",
		cputime);
	fprintf(stdout,
                "Largest CPU Time:                               %.3f\n",
		TIME_2_SECS(qinfo.queues[q].hitime));
	fprintf(stdout,
                "Smallest CPU Time:                              %.3f\n",
		TIME_2_SECS(qinfo.queues[q].lotime));
	
	p1 = (double) qinfo.queues[q].corehimem /
            (double) qinfo.queues[q].numjobs;
	fprintf(stdout,
                "Average Job Core Memory Hiwater (Kbytes)    :   %.0f\n",
                p1);
	fprintf(stdout,
                "Largest Job Core Memory Hiwater (Kbytes)    :   %lld\n",
                qinfo.queues[q].hi_corehimem);
	fprintf(stdout,
                "Smallest Job Core Memory Hiwater (Kbytes)   :   %lld\n",
                qinfo.queues[q].lo_corehimem);
        p1 = (double) qinfo.queues[q].virthimem /
            (double) qinfo.queues[q].numjobs;
	fprintf(stdout,
                "Average Job Virtual Memory Hiwater (Kbytes) :   %.0f\n",
                p1);
	fprintf(stdout,
                "Largest Job Virtual Memory Hiwater (Kbytes) :   %lld\n",
                qinfo.queues[q].hi_virthimem);
	fprintf(stdout,
                "Smallest Job Virtual Memory Hiwater (Kbytes):   %lld\n",
                qinfo.queues[q].lo_virthimem);

	for (mp = qinfo.queues[q].mfp; mp != NULL; mp = mp->next)
        {
            fprintf(stdout,
                    "Jobs submitted from %-16.16s        :   %d\n",
                    mp->name, mp->numjobs);
	}

	/*
	 *	Now for very verbose section.
	 */
	if (verbose > 2)
        {
            fprintf(stdout, "\n\n");
            fprintf(stdout, "Queue: %.16s\n", qinfo.queues[q].qname);
            fprintf(stdout, "  User         Request        CPU Time    ");
            fprintf(stdout, " Core Mem   Virt Mem  ");
	    if (verbose > 3)
		fprintf(stdout, "    CPU      Block I/O    Swap In   ");
            fprintf(stdout, "    Machine      Sequence # Array\n");

            fprintf(stdout, "  Name          Name           (sec)      ");
            fprintf(stdout, "HiWat (KB) HiWat (KB) ");
	    if (verbose > 3)
		fprintf(stdout, "Delay (sec) Delay (sec) Delay (sec) ");
            fprintf(stdout, "     Name        Request ID  ID\n");

            fprintf(stdout, "--------- ---------------- -------------- ");
            fprintf(stdout, "---------- ---------- ");
	    if (verbose > 3)
		fprintf(stdout, "----------- ----------- ----------- ");
            fprintf(stdout, "---------------- ---------- -----\n");
            for (rp = requests[q]; rp != NULL; rp = rp->next) {
                strcpy(user, uid_to_name(rp->uid));
                if (user[0] == '?')
                    sprintf(user, "%d", rp->uid);
                cputime = TIME_2_SECS(rp->stime + rp->utime);
                fprintf(stdout, "%-9.9s %-16.16s %14.3f ",
                        user, rp->reqname, cputime);
                fprintf(stdout, "%10lld %10lld ",
			rp->corehimem, rp->virthimem);
		if (verbose > 3)
		    fprintf(stdout, "%11.3f %11.3f %11.3f ",
			    NSEC_2_SECS(rp->cpu_delay),
			    NSEC_2_SECS(rp->blkio_delay),
			    NSEC_2_SECS(rp->swapin_delay));
                fprintf(stdout, "%-16.16s %10lld %5d\n",
			rp->machine, rp->seq, rp->arrayid);
            }
	}
	if (verbose > 1)
        {
            /*
             *	print a per user summary
             */
            totjobs = totcpu = 0;
            for (rp = requests[q]; rp != NULL; rp = rp->next)
                rp->flag = 0; /* zero internal flag */

            /* print heading */
            fprintf(stdout, "\n\n");
            fprintf(stdout, "  User    Number      CPU Time   \n");
            fprintf(stdout, "   ID     of Jobs      (sec)     \n");
            fprintf(stdout, "-------- --------- --------------\n");

            /* for each unique id */
            for (rp = requests[q]; rp != NULL; rp = rp->next)
            {
                if (rp->flag)
                    continue;  /* already done */
                strcpy(user, uid_to_name(rp->uid));  /* set user name */
                if (user[0] == '?')
                    sprintf(user, "%d", rp->uid);
                ttime = njobs = 0;  /* initialize counters */
                for (rrp = rp; rrp != NULL; rrp = rrp->next)
                {
                    if (rrp->uid == rp->uid)
                    {
                        njobs++;
                        ttime += rrp->stime + rrp->utime;
                        rrp->flag = 1;
                    }
                }
                cputime = TIME_2_SECS(ttime);
                fprintf(stdout, "%-8.8s %9d %14.3f\n", user, njobs, cputime);
                totjobs += njobs;
                totcpu  += cputime;
            }
            fprintf(stdout, "%-8.8s %9d %14.3f\n", "Total", totjobs, totcpu);
	}
		
    } /* end for q */

    /*
     *	Print a by user workload management use summary.
     */
    if (verbose > 1)
    {
        /*
         *	Compact the request structure, zero the flag and
         *	chain all the requests together.
         */
        for (q = 0; q < MAX_QUEUE; q++)
        {
            for (rp = requests[q]; rp != NULL && rp->next != NULL;
                 rp = rp->next)
            {
                ;
            }
            if ((rp != NULL) && (q < MAX_QUEUE - 1))
                rp->next = requests[q+1];
        }
        for (rp = requests[0]; rp != NULL; rp = rp->next)
            rp->flag = 0; /* zero internal flag */
        /*
         *	A pass to calculate total CPU time.  A double check of
         *	batch total time really since we could use that...
         */
        attime = 0;
        for (rp = requests[0]; rp != NULL; rp = rp->next)
            attime += rp->stime + rp->utime;
        if (attime != btimes.stime + btimes.utime)
        {
            if (db_flag > 1)
            {
                fprintf(stderr, "(2): Expected attime == btimes\n"
                        "  Got attime = %f,  btimes = %f\n",
                        TIME_2_SECS(attime), 
                        TIME_2_SECS(btimes.stime + btimes.utime));
            }
        }

        /*
         *	Print heading
         */
        fprintf(stdout, "\n\n");
        fprintf(stdout, "All Batch Queues:\n");
        fprintf(stdout, "  User     Number           CPU Time     \n");
        fprintf(stdout, "  Name     of Jobs       (sec)       (%%) \n");
        fprintf(stdout, "--------  ---------  --------------------\n");
        totjobs = totcpu = 0;
        for (rp = requests[0]; rp != NULL; rp = rp->next)
        {
            if (rp->flag)
                continue;  /* already done */
            strcpy(user, uid_to_name(rp->uid));  /* set user name */
            if (user[0] == '?')
                sprintf(user, "%d", rp->uid);
            ttime = njobs = 0;  /* initialize counters */
            for (rrp = rp; rrp != NULL; rrp = rrp->next)
            {
                if (rrp->uid == rp->uid)
                {
                    njobs++;
                    ttime += rrp->stime + rrp->utime;
                    rrp->flag = 1;
                }
            }
            cputime = TIME_2_SECS(ttime);
            p1 = fperc(cputime, TIME_2_SECS(attime));
            fprintf(stdout, "%-8.8s  %9d  %14.3f (%3.0f)\n",
                    user, njobs, cputime, p1);
            totjobs += njobs;
            totcpu  += cputime;
        }
        p1 = fperc(totcpu, TIME_2_SECS(attime));
        fprintf(stdout, "%-8.8s  %9d  %14.3f (%3.0f)\n",
                "Total", totjobs, totcpu, p1);
    }
}

void dealloc(char *cp)
{
    free(cp);
}

/*
 *	Alloc - allocate memory and detect errors.
 */
int  one_malloc = 0;
char *alloc(int sz)
{
    char *p;

    /*
     *	Do an initial break for a "lot" of memory then release it.
     *	The program should use this up and save itself a bunch of 
     *	system time in the process.  There are surely more
     *	optimizations within this program . . .
     */
    if (one_malloc == 0)
    {
        one_malloc = 1;
        if ((p = malloc(512 * 8 * 20)) == NULL)  /* 20 of 512-byte units */
            acct_perr(ACCT_WARN, errno,
            	_("There was insufficient memory available when allocating '%s'."),
            	"initial memory");
        else
            free(p);
    }

    if ((p = malloc(sz)) == NULL)
    {
        acct_perr(ACCT_FATAL, errno,
        	_("There was insufficient memory available when allocating '%s'."),
        	"initial memory");
        exit(2);
    }
    
    return(p);
}


/*
 *	Write a binary data file.  The output format is:
 *
 *	struct	drep_jobs	Interactive job data
 *	struct	drep_jobs	Batch job data
 *
 *	struct	drep_times	Interactive cpu timing data
 *	struct	drep_times	Batch cpu timing data
 *
 *	struct	wm_hdr
 *		1st workload management start flag
 *		int	workload management count	Not used
 *	struct	drep_batch	workload management queue info
 *
 *	(The 2nd wm_hdr tells the value of MAX_QUEUE in implementation)
 *	struct	wm_hdr
 *		2nd workload management start flag
 *		int	workload management count	MAX_QUEUE
 *
 *      (Repeat 3rd workload management record for MAX_QUEUE times)
 *	struct wm_hdr
 * 		3rd workload management start flag
 *	   	int	# originating MFs (count) in this queue
 *	struct mf	mf records	 repeat for (count) times
 *
 *	(The 4th wm_hdr tells 5th wm_hdr's are coming)
 *	struct  wm_hdr
 *		4th workload management start flag
 *		int	workload management count	Not used
 *	Request specific info by queue
 *
 *	(Repeat 5th workload management record for MAX_QUEUE times)
 *	struct wm_hdr
 *		5th workload management start flag
 *	  	int	# requests (count) in this queue
 *	struct request	request records	  repeat for (count) times
 */
void wr_ofile() 
{
    struct wm_hdr  nq_wm;
    int            q;

    /*
     *	Job data
     */
    if (write(ofile, (char *) &inter, sizeof(struct drep_jobs)) !=
        sizeof(struct drep_jobs))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(22);
    }

    if (write(ofile, (char *) &batch, sizeof(struct drep_jobs)) !=
        sizeof(struct drep_jobs))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(23);
    }

    /*
     *	Timing data
     */
    if (write(ofile, (char *) &itimes, sizeof(struct drep_times)) !=
        sizeof(struct drep_times))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(24);
    }

    if (write(ofile, (char *) &btimes, sizeof(struct drep_times)) !=
        sizeof(struct drep_times))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(25);
    }

    /*
     *	workload management queue data
     */
    strncpy(nq_wm.name, " WKMG 1 ", 8);
    nq_wm.count = 0;
    if (write(ofile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
        sizeof(struct wm_hdr)) {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(28);
    }

    if (write(ofile, (char *) &qinfo, sizeof(struct drep_batch)) !=
        sizeof(struct drep_batch))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(27);
    }

    /*
     *	workload management queue specific data by originating mainframe
     */
    strncpy(nq_wm.name, " WKMG 2 ", 8);
    nq_wm.count = MAX_QUEUE;
    if (write(ofile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
        sizeof(struct wm_hdr)) {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(28);
    }

    for (q = 0; q < MAX_QUEUE; q++)
    {
        struct mf  *mp;
        strncpy(nq_wm.name, " WKMG 3 ", 8);
        nq_wm.count = 0;	/* # MFs for this queue */
        for (mp = qinfo.queues[q].mfp; mp != NULL; mp = mp->next)
            nq_wm.count++;

        if (write(ofile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
            sizeof(struct wm_hdr)) {
            acct_perr(ACCT_FATAL, errno,
            	_("An error occurred during the writing of file '%s'."),
            	"binary output");
            exit(29);
        }

        for (mp = qinfo.queues[q].mfp; mp != NULL; mp = mp->next)
        {
            if (write(ofile, (char *) mp, sizeof(struct mf)) !=
                sizeof(struct mf))
            {
                acct_perr(ACCT_FATAL, errno,
                	_("An error occurred during the writing of file '%s'."),
                	"binary output");
                exit(30);
            }
        }
    }

    /*
     *	workload management request info by queue.
     */
    strncpy(nq_wm.name, " WKMG 4 ", 8);
    nq_wm.count = 0;
    if (write(ofile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
        sizeof(struct wm_hdr)) {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the writing of file '%s'."),
        	"binary output");
        exit(31);
    }

    for (q = 0; q < MAX_QUEUE; q++)
    {
        struct request  *rp;
        strncpy(nq_wm.name, " WKMG 5 ", 8);
        nq_wm.count = 0;		/* # of requests for this queue */
        for (rp = requests[q]; rp != NULL; rp = rp->next)
            nq_wm.count++;

        if (write(ofile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
            sizeof(struct wm_hdr)) {
            acct_perr(ACCT_FATAL, errno,
            	_("An error occurred during the writing of file '%s'."),
            	"binary output");
            exit(32);
        }

        for (rp = requests[q]; rp != NULL; rp = rp->next)
        {
            if (write(ofile, (char *) rp, sizeof(struct request)) !=
                sizeof(struct request))
            {
                acct_perr(ACCT_FATAL, errno,
                	_("An error occurred during the writing of file '%s'."),
                	"binary output");
                exit(33);
            }
        }
    }

    return;
}

/*
 *	Read binary data file: format is.... as above in wr_ofile
 */
void rd_ofile(char *filename)
{
    struct wm_hdr  i_nq_wm;
    struct wm_hdr  nq_wm;
    struct rd_request  *mrq;
    int	                k, q;

    /*
     *	Data areas which exist as globals also.  We read into these
     *	then sum with the globals.
     */
    struct drep_jobs   M_inter,                 M_batch;
    struct drep_times  M_itimes,                M_btimes;
    struct drep_batch  M_qinfo;
    struct rd_request  M_requests[MAX_QUEUE];

    /*
     *	Job data
     */
    if (read(ifile, (char *) &M_inter, sizeof(struct drep_jobs)) !=
        sizeof(struct drep_jobs))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(22);
    }

    if (read(ifile, (char *) &M_batch, sizeof(struct drep_jobs)) !=
        sizeof(struct drep_jobs))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(23);
    }

    batch.total += M_batch.total;
    inter.total += M_inter.total;

    /*
     *	Timing data
     */
    if (read(ifile, (char *) &M_itimes, sizeof(struct drep_times)) !=
        sizeof(struct drep_times))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(24);
    }

    if (read(ifile, (char *) &M_btimes, sizeof(struct drep_times)) !=
        sizeof(struct drep_times))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(25);
    }

    itimes.stime += M_itimes.stime;
    btimes.stime += M_btimes.stime;
    itimes.utime += M_itimes.utime;
    btimes.utime += M_btimes.utime;
    itimes.cpu_delay += M_itimes.cpu_delay;
    btimes.cpu_delay += M_btimes.cpu_delay;
    itimes.blkio_delay += M_itimes.blkio_delay;
    btimes.blkio_delay += M_btimes.blkio_delay;
    itimes.swapin_delay += M_itimes.swapin_delay;
    btimes.swapin_delay += M_btimes.swapin_delay;

    /*
     *	workload management data
     */
    strncpy(i_nq_wm.name, " WKMG 1 ", 8);
    i_nq_wm.count = 0;
    if (read(ifile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
        sizeof(struct wm_hdr)) {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(28);
    }

    if (strncmp(i_nq_wm.name, nq_wm.name, 8)) {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(28);
    }

    if (read(ifile, (char *) &M_qinfo, sizeof(struct drep_batch)) !=
        sizeof(struct drep_batch))
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(29);
    }

    strncpy(i_nq_wm.name, " WKMG 2 ", 8);
    i_nq_wm.count = MAX_QUEUE;
    if (read(ifile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
        sizeof(struct wm_hdr)) {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(28);
    }

    if (strncmp(i_nq_wm.name, nq_wm.name, 8))  /* check flag */
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(28);
    }

    if (i_nq_wm.count != nq_wm.count)  /* check MAX_QUEUE Definition */
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(29);
    }

    for (q = 0; q < MAX_QUEUE; q++)
        M_qinfo.queues[q].mfp = NULL; /* zero pointer NOW */

    for (q = 0; q < MAX_QUEUE; q++)
    {
        struct mf  *lmp;
        strncpy(i_nq_wm.name, " WKMG 3 ", 8);
        i_nq_wm.count = 0;
        if (read(ifile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
            sizeof(struct wm_hdr)) {
            acct_perr(ACCT_FATAL, errno,
            	_("An error occurred during the reading of file '%s' %s."),
            	filename, "binary input");
            exit(30);
        }

        if (strncmp(i_nq_wm.name, nq_wm.name, 8))  /* check flag */
        {
            acct_perr(ACCT_FATAL, errno,
            	_("An error occurred during the reading of file '%s' %s."),
            	filename, "binary input");
            exit(31);
        }

	lmp = NULL;
        for (k = 0; k < nq_wm.count; k++) {
            struct mf  *mp;
            mp = (struct mf *) alloc(sizeof(struct mf));
            if (read(ifile, (char *) mp, sizeof(struct mf)) !=
                sizeof(struct mf))
            {
                acct_perr(ACCT_FATAL, errno,
                	_("An error occurred during the reading of file '%s' %s."),
                	filename,
                          "binary input");
                exit(32);
            }

            if (M_qinfo.queues[q].mfp == NULL)
                M_qinfo.queues[q].mfp = mp;
            else
                lmp->next = mp;
            mp->next = NULL;
            lmp = mp;
        }
    }

    for (q = 0; q < MAX_QUEUE; q++)
    {
        M_requests[q].req = NULL;
        M_requests[q].ind = 0;
    }
    add_wm(&qinfo, &M_qinfo, M_requests);

    /*
     *	Individual request data.
     */
    strncpy(i_nq_wm.name, " WKMG 4 ", 8);
    i_nq_wm.count = 0;
    if (read(ifile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
        sizeof(struct wm_hdr)) {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(33);
    }

    if (strncmp(i_nq_wm.name, nq_wm.name, 8))  /* check flag */
    {
        acct_perr(ACCT_FATAL, errno,
        	_("An error occurred during the reading of file '%s' %s."),
        	filename, "binary input");
        exit(34);
    }

    for (q = 0; q < MAX_QUEUE; q++)
    {
        struct request  *rp, *lrp;
        strncpy(i_nq_wm.name, " WKMG 5 ", 8);
        i_nq_wm.count = 0;
        if (read(ifile, (char *) &nq_wm, sizeof(struct wm_hdr)) !=
            sizeof(struct wm_hdr)) {
            acct_perr(ACCT_FATAL, errno,
            	_("An error occurred during the reading of file '%s' %s."),
            	filename, "binary input");
            exit(35);
        }

        for (k = 0; k < nq_wm.count; k++) {
            rp = (struct request *) alloc(sizeof(struct request));
            if (read(ifile, (char *) rp, sizeof(struct request)) !=
                sizeof(struct request))
            {
                acct_perr(ACCT_FATAL, errno,
                	_("An error occurred during the reading of file '%s' %s."),
                	filename,
                          "binary input");
            }

            if (M_requests[q].req == NULL)
                M_requests[q].req = rp;
            else
                lrp->next = rp;

            rp->next = NULL;
            lrp = rp;
        }
    }

    /*
     *	Add the M_requests[] requests to the end of the appropriate
     *	requests[] array entry.  We have to examine the entire
     *	M_requests[] array because, there can be lists of requests
     *	after finding M_requests[n]->req == NULL.
     */
    for (q = 0; q < MAX_QUEUE; q++)
    {
        struct request  *rp;

        mrq = &M_requests[q];
        if (mrq->req == NULL)
            continue;

        if (requests[mrq->ind] == NULL)
            requests[mrq->ind] = mrq->req;
        else
        {
            for (rp = requests[mrq->ind]; rp->next != NULL; rp = rp->next)
                ;
            rp->next = mrq->req;
        }
    }

    return;
}

/*
 *	Add workload management qinfo data
 */
void add_wm(struct drep_batch *q, struct drep_batch *nq_wm,
            struct rd_request *rq)
{
    struct mf  *nnmp, *nmp, *mp;
    int  i, j;
    int  not_found;

    q->stime += nq_wm->stime;
    q->utime += nq_wm->utime;
    for (j = 0; j < MAX_QUEUE; j++)
    {
        not_found = 0;
        for (i = 0; i < MAX_QUEUE; i++)
        {
            if (q->queues[i].qname[0] == '\0')
            {
                not_found = 1;
                break;
            }
            if (!strcmp(q->queues[i].qname, nq_wm->queues[j].qname))
                break;
        }

        if (i >= MAX_QUEUE)  /* gads this is bad!!! */
        {
            acct_err(ACCT_FATAL,
                   _("MAX_QUEUE exceeded\n   Overflow of the WKMG queue table can be corrected by recompiling\n   with a larger MAX_QUEUE value.")
            	);
            exit(-1);
        }

        if (not_found == 1)
            strcpy(q->queues[i].qname, nq_wm->queues[j].qname);

        (rq+j)->ind = i;

	q->queues[i].stime += nq_wm->queues[j].stime;
	q->queues[i].utime += nq_wm->queues[j].utime;
	q->queues[i].numjobs += nq_wm->queues[j].numjobs;
	q->queues[i].quwait += nq_wm->queues[j].quwait;
	q->queues[i].corehimem += nq_wm->queues[j].corehimem;
	q->queues[i].virthimem += nq_wm->queues[j].virthimem;

	if (q->queues[i].hi_corehimem < nq_wm->queues[j].hi_corehimem)
	    q->queues[i].hi_corehimem = nq_wm->queues[j].hi_corehimem;

	if ((q->queues[i].lo_corehimem == 0) ||
	    (q->queues[i].lo_corehimem > nq_wm->queues[j].lo_corehimem))
	{
	    q->queues[i].lo_corehimem = nq_wm->queues[j].lo_corehimem;
	}

	if (q->queues[i].hi_virthimem < nq_wm->queues[j].hi_virthimem)
	    q->queues[i].hi_virthimem = nq_wm->queues[j].hi_virthimem;

	if ((q->queues[i].lo_virthimem == 0) ||
	    (q->queues[i].lo_virthimem > nq_wm->queues[j].lo_virthimem))
	{
	    q->queues[i].lo_virthimem = nq_wm->queues[j].lo_virthimem;
	}

	if ((q->queues[i].lotime == 0) ||
	    (q->queues[i].lotime > nq_wm->queues[j].lotime))
	{
	    q->queues[i].lotime = nq_wm->queues[j].lotime;
	}

	if (q->queues[i].hitime < nq_wm->queues[j].hitime)
	    q->queues[i].hitime = nq_wm->queues[j].hitime;

	/*
	 *	Add up per mainframe job stats.
	 */
	for (nmp = nq_wm->queues[j].mfp; nmp != NULL; nmp = nnmp) {
	    nnmp = nmp->next;
	    for (mp = q->queues[i].mfp; mp != NULL; mp = mp->next)
	    {
		if (!strncmp(mp->name, nmp->name, 16))
		    break;
	    }
	    if (mp == NULL)
	    {
		struct mf  *p;
		if (q->queues[i].mfp == NULL)
		    q->queues[i].mfp = nmp;
		else
		{
		    for (p = q->queues[i].mfp; p->next != NULL; p = p->next)
			;
		    p->next = nmp;
		}
		nmp->next = NULL;
	    }
	    else
	    {
		mp->numjobs += nmp->numjobs;
		dealloc((char *) nmp);
	    }
	}
    }
}
