/*
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
 * This is to verify CSA API calls
 *
 * ChangeLog
 * * 02/15/2006	Jay Lan <jlan@sgi.com>
 * - initial creation
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa.h"
#include "csa_api.h"
#include "csaacct.h"
#include "job.h"


#define MAX_FNAME	128	/* was in extern_ja.h */

extern int db_flag;		/* debug flag */

char acct_path[ACCT_PATH];
char	*fn;			/* pointer to file name */
char	fname[MAX_FNAME];	/* job accounting file name */

/*
 * The following structure defines an entry in the name_list[] array below:
 *
 *      Field           Meaning
 *	----------	------------------------------------------------------
 *	id		An indentifier used in the kernel to identify this
 *			accounting method.
 *	flag		A flag byte... See below.
 *	name		A character string which identifies this accounting
 *			method, specified with the -n option.
 *	req_state	The request state of this accounting method.
 *	req_param	The request parameter (i.e., threshold) of this
 *			accounting method.
 *	config_name	Name of the accounting method in the csa.conf file.
 */
struct acct_method
{
	ac_kdrcd    id;           /* Identifier of accounting method */
	int         flag;	  /* Flags...See below */
	char       *name;         /* Name of accounting method */
	int         req_state;    /* Request state */
	int64_t     req_param;    /* Request parameter (i.e., threshold) */
	char       *config_name;  /* Name in the csa.conf file */
};

/*
 * STAT_FLAG - Used to indicate which accounting methods are actually
 *             "active."  Only the ones that are active will be displayed on
 *             a "status" request and searched for in the csa.conf file.
 */
#define	STAT_FLAG	010

#define NOT_SET  -1

/*
 * The list of accounting methods in this array must match the order of the
 * list in the ac_kdrcd enum in <linux/csa_kern.h>.
 */
struct acct_method name_list[] =
{
    /*  id,              flag, name,	  req_state, p,  config_name */
    { ACCT_KERN_CSA,      010, "csa",	  NOT_SET,   0,  CSA_START   },
    { ACCT_KERN_JOB_PROC, 000, "job",	  NOT_SET,   0,  NULL        },
    { ACCT_DMD_WKMG,      010, "wkmg",	  NOT_SET,   0,  WKMG_START  },
    { ACCT_DMD_SITE1,     000, "site1",	  NOT_SET,   0,  NULL        },
    { ACCT_DMD_SITE2,     000, "site2",	  NOT_SET,   0,  NULL        },
    { ACCT_MAXKDS,        000, "",	  NOT_SET,   0,  NULL        },

    { ACCT_RCD_MEM,       010, "mem",	  NOT_SET,   0,  MEM_START   },
    { ACCT_RCD_IO,        010, "io",	  NOT_SET,   0,  IO_START    },
    { ACCT_RCD_DELAY,     010, "delay",	  NOT_SET,   0,  DELAY_START },
    { ACCT_THD_MEM,       010, "memt",	  NOT_SET,   0,  MEMT_START  },
    { ACCT_THD_TIME,      010, "time",	  NOT_SET,   0,  TIME_START  },
    { ACCT_RCD_SITE1,     000, "rsite1",  NOT_SET,   0,  NULL        },
    { ACCT_RCD_SITE2,     000, "rsite2",  NOT_SET,   0,  NULL        },
    { ACCT_MAXRCDS,       000, "",	  NOT_SET,   0,  NULL        },
};

char *status_list[] = { "Off", "Error", "On" };



/*****************************************************************************
 *
 * NAME
 *	get_config	- Get information from the csa.conf file.
 *
 * SYNOPSIS
 *	get_config();
 *
 * DESCRIPTION
 *	This routine gets information from the csa.conf file as to what
 *	should be turned on, along with the thresholds.  It sets the requested
 *	state (on) in the name_list[] structure.
 *
 *	It is expected that at least one accounting method is found to be on
 *	in csa.conf.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
void get_config() {
	int	any_on = 0;
	char	*config_str;
	int	i;
	int	optargc;
	char	**optargv;
	int64_t threshold;

	if (db_flag > 2)
		printf("get_config(3):\n");

	for (i = 0; i < ACCT_MAXRCDS; i++) {
		if (! (name_list[i].flag & STAT_FLAG))
			continue;

		config_str = init_char(name_list[i].config_name, NULL, TRUE);
		if (config_str == NULL)
			continue;

		if (db_flag > 2)
			Ndebug("    %s\t'%s'\n", name_list[i].config_name,
				config_str);
        
		/* Parse the config string. */
		if ((optargc = getoptlst(config_str, &optargv)) < 0)
			acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
				"optargv");

		if (! strcasecmp(optargv[0], "on")) {
			if (name_list[i].id == ACCT_THD_MEM ||
			    name_list[i].id == ACCT_THD_TIME) {
				if (optargc == 1) {
					acct_err(ACCT_CAUT,
					    _("A threshold value is required for %s in the csa.conf file."),
					    name_list[i].config_name);
					continue;
				}
				threshold = atoll(optargv[1]);

				if (threshold < 0) {
					acct_err(ACCT_CAUT,
					    _("Threshold value for %s in the csa.conf file must be positive."),
					    name_list[i].config_name);
					continue;
				}
				name_list[i].req_param = threshold;
			}
			name_list[i].req_state = ACS_ON;
			any_on++;

		} else
			continue;
	}

	/* Must have something on. */
	if (any_on == 0)
		acct_err(ACCT_ABORT,
		    _("None of the accounting methods in the csa.conf file is on."));

	return;
}



/*****************************************************************************
 *
 * NAME
 *	do_start	- start accounting method(s) based on /etc/csa.conf
 *
 * SYNOPSIS
 *	int do_start();
 *
 * DESCRIPTION
 *	If a linked record is being turned on, the CSA base record ("csa") is
 *	also turned on since there can be no linked records without the base
 *	record.  The same happens when a threshold is being set since
 *	thresholds will not have any effect if csa isn't running.
 *
 * RETURNS
 *      0       - If the indicated accounting method(s) were turned on.
 *      -1      - If failed.
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
int do_start(int method)
{
	int		any_on = FALSE;
	struct csa_start_req	start_req;
	int		i, j;

	if ((method >= 0) && (method < ACCT_MAXRCDS))
		name_list[method].req_state = ACS_ON;

	/* Can't have the linked records or thresholds w/o the base record. */
	if (name_list[ACCT_RCD_MEM].req_state == ACS_ON ||
	    name_list[ACCT_RCD_IO].req_state == ACS_ON ||
	    name_list[ACCT_RCD_DELAY].req_state == ACS_ON ||
	    name_list[ACCT_THD_MEM].req_state == ACS_ON ||
	    name_list[ACCT_THD_TIME].req_state == ACS_ON)
		name_list[ACCT_KERN_CSA].req_state = ACS_ON;

	memset((void *) &start_req, 0, sizeof(start_req));
    
	for (i = 0, j = 0; i < ACCT_MAXRCDS; i++) {
		if (i == ACCT_MAXKDS)
			continue;
		if (name_list[i].req_state == ACS_ON) {
			start_req.sr_method[j].sr_id = name_list[i].id;
			start_req.sr_method[j].param = name_list[i].req_param;
			if (db_flag > 1) {
				Ndebug("        id = %d, name = %s, parameter = %lld\n",
				    name_list[i].id, name_list[i].name,
				    start_req.sr_method[j].param);
			}
			j++;
		}
	}
	start_req.sr_num = j;
	strcpy(start_req.sr_path, acct_path);

	if (csa_start(&start_req) != 0)
		acct_perr(ACCT_ABORT, errno,
			_("Unable to %s accounting."),
			"enable");

	return(0);
}



/*****************************************************************************
 *
 * NAME
 *	do_stop		- Turn "off" the indicated accounting method(s).
 *
 * SYNOPSIS
 *	do_stop();
 *
 * DESCRIPTION
 *	If the CSA base record ("csa") is being turned off, then all the
 *	linked records are also turned off since there can be no linked
 *	records without the base record.  Thresholds are also turned off
 *	since thresholds will not have any effect if csa isn't running.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
int do_stop(int method)
{
	struct csa_stop_req	stop_req;
	int			i, j, retval;

    /* Can't have the linked records or thresholds without the base record. */
    	if (method != ACCT_KERN_CSA) {
		name_list[method].req_state = ACS_OFF;
	} else {
		name_list[ACCT_KERN_CSA].req_state == ACS_OFF;
		name_list[ACCT_RCD_MEM].req_state = ACS_OFF;
		name_list[ACCT_RCD_IO].req_state = ACS_OFF;
		name_list[ACCT_RCD_DELAY].req_state = ACS_OFF;
		name_list[ACCT_THD_MEM].req_state = ACS_OFF;
		name_list[ACCT_THD_TIME].req_state = ACS_OFF;
	}

	if (db_flag > 1) {
		Ndebug("do_stop(2):\n");
	}
    
	memset((void *) &stop_req, 0, sizeof(stop_req));
    
	for (i = 0, j = 0; i < ACCT_MAXRCDS; i++) {
		if (i == ACCT_MAXKDS)
			continue;
		if (name_list[i].req_state == ACS_OFF) {
			stop_req.pr_id[j] = name_list[i].id;
			j++;
			if (db_flag > 1) {
				Ndebug("        id = %d, name = %s\n",
				name_list[i].id, name_list[i].name);
			}
		}
	}
	stop_req.pr_num = j;

	retval = csa_stop(&stop_req);
	if (retval != 0)
		acct_perr(ACCT_ABORT, errno,
			_("Unable to %s accounting."),
			"disable");
	return retval;
}



/*****************************************************************************
 *
 * NAME
 *	do_halt		- Turn "off" all accounting methods.
 *
 * SYNOPSIS
 *	do_halt();
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
void do_halt()
{
	if (db_flag > 1)
		Ndebug("do_halt(2): acctctl(AC_HALT)\n");
        
	if (csa_halt() != 0)
		acct_perr(ACCT_ABORT, errno,
			_("Unable to %s accounting."),
			"halt system");
	return;
}



/*****************************************************************************
 *
 * NAME
 *	do_format	- Format and display the information contained in an
 *			  "actstat" structure.
 *
 * SYNOPSIS
 *	do_format( indx, status );
 *
 *	indx		r	The index into name_list[] which identifies
 *				the accounting method to be formatted.
 *	status		r	A pointer to the "actstat" structure to
 *				format.
 *
 * DESCRIPTION
 *	This routine formats the status information into human readable text.
 *	This information is written to standard output.
 *
 *	The time threshold value is converted from microseconds to seconds
 *	before it is printed.
 *
 * RETURNS
 *	Nothing
 *
 *****************************************************************************/
void do_format(int indx, struct csa_am_stat *status)
{
	int64_t     param;
	double	time_param;
	static int  print_header = 1;
	static int  separate_output = 1;

	if (print_header) {
		time_t	today;

		(void) time(&today);
		fprintf(stdout, "#\tAccounting status for %s", ctime(&today));
		fprintf(stdout, "#\t      Name\tState\tValue\n");
		print_header = 0;

	} else if (separate_output && indx >= ACCT_RCDS) {
		fprintf(stdout, "\n");
		separate_output = 0;
	}

	if (status->am_param != 0) {
		if (status->am_id == ACCT_THD_TIME) {
			time_param = TIME_2_SECS(status->am_param);
			fprintf(stdout, "\t%10s\t%s\t%.0f\n", 
				name_list[indx].name,
				status_list[status->am_status], time_param);
		} else {
			param = status->am_param;
			fprintf(stdout, "\t%10s\t%s\t%lld\n",
				name_list[indx].name,
				status_list[status->am_status], param);
		}
	} else {
		fprintf(stdout, "\t%10s\t%s\n", name_list[indx].name,
			status_list[status->am_status]);
	}

	return;
}

/*****************************************************************************
 *
 * NAME
 *	do_status	- Perform a status of all accounting methods.
 *
 * SYNOPSIS
 *	do_status();
 *
 * DESCRIPTION
 *	This routine requests the current status of the various known
 *	accounting method(s) and formats the information returned into human
 *	readable text.  This information is written to standard output.
 *
 *	It formats and displays the information in the order defined by the
 *	name_list[] array.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
int do_status()
{
	struct csa_status_req	status_req;
	int	i;

	if (db_flag > 1) {
		Ndebug("do_status(2):\n");
		Ndebug("   csaswitch(AC_KDSTAT): # methods = %d\n", NUM_KDS);
		Ndebug("   csaswitch(AC_RCDSTAT): # methods = %d\n", NUM_RCDS);
	}

	/* Get the kernel and daemon accounting status. */
	memset((void *) &status_req, 0, sizeof(status_req));
	status_req.st_num = NUM_KDS;
	if (csa_kdstat(&status_req) != 0) {
		acct_perr(ACCT_FATAL, errno,
			_("Unable to get the Kernel and Daemon accounting"
			" status information."));
		return(-1);
	}
	printf("csa_kdstat():\tOK\n");

	if (db_flag > 1) {
		for (i = 0; i < ACCT_MAXKDS; i++) {
			if (name_list[i].flag & STAT_FLAG)
				do_format(i, &status_req.st_stat[i]);
		}
	}
    
	/* Get the record accounting status. */
	memset((void *) &status_req, 0, sizeof(status_req));
	status_req.st_num = NUM_RCDS;
	if (csa_rcdstat(&status_req) != 0) {
		acct_perr(ACCT_FATAL, errno,
			_("Unable to get the Record accounting status"
			" information."));
		return(-1);
	}
	printf("csa_rcdstat():\tOK\n");

	if (db_flag > 1) {
		for (i = ACCT_RCDS; i < ACCT_MAXRCDS; i++) {
			if (name_list[i].flag & STAT_FLAG)
				do_format(i, &status_req.st_stat[i-ACCT_RCDS]);
		}
	}
	return 0;
}

/*****************************************************************************
 *
 * NAME
 *      do_check        - Perform a status on the indicated accounting method.
 *
 * SYNOPSIS
 *	int = do_check(indx);
 *
 *	indx		r	The index into name_list[] which identifies
 *				the accounting method to be checked/formatted.
 *
 * RETURNS
 *      1       - If the given accounting method is currently "on".
 *      0       - If the status of the given accounting method is currently
 *                not "on".
 *	-1	- If csa_check() call failed.
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
int do_check(int indx)
{
	struct csa_check_req	check_req;

	if (db_flag > 1) {
		Ndebug("do_check(2): acctctl(AC_CHECK): id = %d, name = %s\n",
			name_list[indx].id, name_list[indx].name);
	}
    
	check_req.ck_stat.am_id = name_list[indx].id;
	check_req.ck_stat.am_status = ACS_OFF;
	check_req.ck_stat.am_param = 0;

	if (csa_check(&check_req) != 0) {
		acct_perr(ACCT_FATAL, errno,
			_("Unable to get the accounting status for '%s'."),
			name_list[indx].name);
		return(-1);
	}
	if (db_flag > 1)
		do_format(indx, &check_req.ck_stat);

	if (check_req.ck_stat.am_status == ACS_ON)
		return 1;
	else
		return 0;
}


int do_job()
{
	struct csa_job_req job_req;
	jid_t	myjid=0;	/* job ID of job ja is running in */
	pid_t	pid;		/* PID of job */
	int	fnl;		/* length of file name (bytes) */
	char	*dir;		/* pointer to directory name for job file */
	char	tmp[] = "/tmp";
	struct	stat	stbuf;
	struct	stat	*sb = &stbuf;

	/* Construct the job accounting file name */
	/* Make sure this process is part of a job by fetching it's job ID. */
	pid = getpid();
	myjid = job_getjid(pid);

	if (myjid == 0 || myjid == (jid_t)-1) {
		acct_err(ACCT_FATAL,
			_("The process running does not have a valid Job ID."));
		return(-1);
	}

	fn = fname;
	if ((dir = getenv(ACCT_TMPDIR)) == NULL &&
	    (dir = getenv("TMPDIR")) == NULL) {
		dir = tmp;
	}

	if ((fnl = sprintf(fn, "%s/.jacct%llx", dir, myjid)) < 0) {
		acct_err(ACCT_FATAL,
			_("Cannot build the file name from the TMPDIR environment variable and the Job ID."));
		return(-1);
	}

	if (fnl >= MAX_FNAME) {
		acct_err(ACCT_FATAL,
			_("The file name exceeds the maximum length of 128 characters."));
		return(-1);
	}

	if (stat(fn, sb) == -1) {
		if (errno != ENOENT) {
			acct_perr(ACCT_FATAL, errno,
				_("An error occurred getting the status of file '%s'."),fn);
			return(-1);
		}
		
		if (creat(fn, 0644) == -1) {
			acct_perr(ACCT_FATAL, errno,
				_("An error occurred creating file '%s'."),
				fn);
			return(-1);
		}
	}

	strncpy(job_req.job_path, fn, ACCT_PATH);
	if (csa_jastart(&job_req) == -1) {
		acct_perr(ACCT_FATAL, errno,
			_("An error was returned from the call to the '%s' routine."),
			"acctctl()");
		return(-1);
	}
	printf("csa_jastart():\tOK\n");

	if (csa_jastop(&job_req)) {
		acct_perr(ACCT_FATAL, errno,
			_("An error was returned from the call to the '%s' routine."),
			"acctctl()");
		return(-1);
	}
	printf("csa_jastop():\tOK\n");
	return(0);
}



int do_wracct() {
	struct csa_wra_req	wra_req;
	struct wkmgmtbs		wkmgmtbs;
	struct utsname		buf;
	jid_t			myjid = 0;

	/* Make sure this process is part of a job by fetching its job ID. */
	myjid = job_getjid(getpid());
	if (myjid == 0 || myjid == (jid_t)-1) {
		acct_err(ACCT_FATAL,
			_("The process running does not have a valid Job ID."));
		return(-1);
	}

	uname(&buf);

	memset((void *) &wkmgmtbs, 0, sizeof(wkmgmtbs));
	wkmgmtbs.hdr.ah_magic = ACCT_MAGIC;
	wkmgmtbs.hdr.ah_revision = REV_WKMG;
	wkmgmtbs.hdr.ah_type = ACCT_DAEMON_WKMG;
	wkmgmtbs.hdr.ah_flag = 0;
	wkmgmtbs.hdr.ah_size = sizeof(wkmgmtbs);

	wkmgmtbs.type = WM_INIT;
	wkmgmtbs.subtype = WM_INIT_START;
	wkmgmtbs.time = time(NULL);
	wkmgmtbs.jid = myjid;

	strncpy(wkmgmtbs.serv_provider, "csatest_api",
		sizeof(wkmgmtbs.serv_provider));
	strncpy(wkmgmtbs.machname, buf.nodename,
		sizeof(wkmgmtbs.machname));
	strncpy(wkmgmtbs.reqname, "do_wracct",
		sizeof(wkmgmtbs.reqname));
	strncpy(wkmgmtbs.quename, "queue",
		sizeof(wkmgmtbs.quename));

	memset((void *) &wra_req, 0, sizeof(wra_req));
	wra_req.wra_did = ACCT_DMD_WKMG;
	wra_req.wra_len = sizeof(wkmgmtbs);
	wra_req.wra_jid = wkmgmtbs.jid;
	wra_req.wra_buf = (char *) &wkmgmtbs; 

	return csa_wracct(&wra_req);
}



main(int argc, char **argv)
{
	int retval = 0;
	int i;

	/* test csa_auth() */
	if (csa_auth() != 0) {
		printf("csa_auth failed, errno=%d\t", errno);
		if (errno == EPERM)
			printf("You need to have proper privilege to run this test\n");
		else
			printf("CSA Job accounting is not available\n");
		exit(1);
	}
	printf("csa_auth():\tOK\n");

	/* csa rpm was installed and chkconfig enabled */

	/* get csa.conf information */
	strcpy(acct_path, init_char(PACCT_FILE, DEFPACCT, TRUE));
	get_config();

	/* csa_halt() to stop all CSA accounting methods */
	if (csa_halt() != 0) {
		printf("csa_halt():\tFAIL\terrno=%d\n", errno);
		exit(1);
	}
	printf("csa_halt():\tOK\n");

	/* Now, start up CSA again with csa_start() */
	/* Enable ACCT_DMD_WKMG to test csa_wracct() */
	if (do_start(ACCT_DMD_WKMG) != 0) {
		printf("Failed to execute csa_start\n");
		exit(1);
	}
	printf("csa_start():\tOK\n");

	/* test csa_check */
	for (i=0; i < ACCT_MAXRCDS; i++) {
		if (i == ACCT_MAXKDS)
			continue;
		if (do_check(i) < 0) {
			printf("\nAccounting method %s failed\n",
				name_list[i].name);
			exit(1);
		}
	}
	printf("csa_check():\tOK\n");

	/* Try to csa_stop() ACCT_RCD_IO */
	if (do_stop(ACCT_RCD_IO)) {
		printf("csa_stop() failure\n");
		exit(1);
	}
	printf("csa_stop():\tOK\n");

	/* Do csa_kdstat & csa_rcdstat */
	if (do_status() != 0) {
		/* printf() message is done in do_status */
		exit(1);
	}

	/* Do csa_jastart & csa_jastop:
	 *   Extensive csa_jastart/csa_jastop testing should be done with
	 *   `ja' command.
	 *   This test is to do the most basic one to make sure it works.
	 */
	setenv(ACCT_TMPDIR, "/var/tmp", 0);
	if (do_job() != 0) {
		printf("Job testing failed\n");
		exit(1);
	}

	/* Do csa_wracct */
	if (do_wracct() != 0) {
		printf("csa_wracct() failure\n");
		exit(1);
	}
	printf("csa_wracct():\tOK\n");

	printf("Done csatest_api testing:\tSUCCESS\n");
	exit(0);
}
