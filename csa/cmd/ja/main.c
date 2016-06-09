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
 *	ja - user job accounting for current job
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

#include "csa_api.h"
#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csa.h"
#include "csaacct.h"
#include "csaja.h"
#include "extern_ja.h"

char		acctentbuf[ACCTENTSIZ];
struct acctent	acctrec = {acctentbuf};

struct	achead		hdr;		/* accounting record header */
uint64_t		jid;		/* jid for caller of ja */

char	*Prg_name;
int	range[2];
int	*defmarks[] = {range, NULL};
int	jafd;		/* file descriptor for file I/O */

extern	char	*fn;		/* pointer to file name */
extern	int	**marks;	/* file position markers */
int	eof_mark;		/* end of file position */

extern	int	M_optargc;	/* -M option argument count */
extern	char	**M_optargv;	/* -M option argument Mark list */

int	t_Nsite = 0;		/* number of SITE records found */

static	int	getacctent(int ***);
static	void	init_config();


/*
 *	ja(main) routine.
 */
main(int argc, char **argv)
{
	int	file_exists;
	int	get_ret = TRUE;
	int	prt_ret = TRUE;
	struct	actctl	act;
	struct	stat	stbuf;
	struct	stat	*sb = &stbuf;
	char	temp[40];
	int	type;
	struct csa_job_req job_req;

	Prg_name = argv[0];

	/*  Process the command options. */
	if ((file_exists = init(argc, argv)) < 0) {
		exit(1);
	}

	/*  See if the job accounting file exists. */
	if (stat(fn, sb) == -1) {
		if (errno != ENOENT) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred getting the status of file '%s'."),
				fn);
		}
		file_exists = eof_mark = 0;

	} else {
		eof_mark = sb->st_size;
	}

	/*  If no report was requested */
	if (!c_opt && !f_opt && !o_opt && !s_opt){
		bzero((char *)&job_req, sizeof(job_req));
		/* turn off job accounting */
		if (t_opt) {
			act.ac_sttnum = 0;
			strncpy(job_req.job_path, fn, ACCT_PATH);
			if (csa_jastop(&job_req)) {
				acct_perr(ACCT_ABORT, errno,
					_("An error was returned from the call to the '%s' routine."),
					"acctctl()");
			}
			/* remove file if a temp file name was used */
			if (temp_file && unlink(fn)) {
				acct_perr(ACCT_WARN, errno,
					_("An error occurred during the removal of file '%s'."),
					fn);
			}
			exit(0);
		}

		/* Turn on job accounting. */
		if (!file_exists && creat(fn, 0644) == -1) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred creating file '%s'."),
				fn);
		}

		strncpy(job_req.job_path, fn, ACCT_PATH);
		if (csa_jastart(&job_req) == -1) {
			acct_perr(ACCT_ABORT, errno,
				_("An error was returned from the call to the '%s' routine."),
				"acctctl()");
		}

		/* Return mark if requested. */
		if (m_opt) {
			printf("%d", eof_mark);
		}

		exit(0);
	}

	/*
 	 * A report was requested, but first check for an empty or nonexistent
 	 * job accounting file.
 	 */
	if (eof_mark == 0) {
		acct_err(ACCT_ABORT,
		       _("The job accounting file is empty or does not exist.")
			);
	}

	/*  Open the job accounting file. */
	if ((jafd = openacct(fn, "r")) < 0) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			fn);
	}

	/*  Determine the file's positioning marks. */
	if (M_opt) {
		marks = getmarks(M_optargc, M_optargv);
		free((char *)M_optargv);
	} else {
		defmarks[0][0] = 0;
		defmarks[0][1] = eof_mark;
		marks = defmarks;
	}

	/*
	 *  Gather up the accounting stats and if requested, issue the command
	 *  report.
	 */
	while (prt_ret && (get_ret = getacctent(&marks)) ) {
		type = acctrec.prime->ah_type;
		if (db_flag > 3) {
			fprintf(stderr, "main(6): Record header "
				"magic(%#o), revision(%#o), type(%#o), "
				"flag(%#o), size(%d).\n",
				acctrec.prime->ah_magic,
				acctrec.prime->ah_revision, type, 
				acctrec.prime->ah_flag, acctrec.prime->ah_size);
		}
		switch(type) {

		case (ACCT_KERNEL_CSA):
			prt_ret = pr_process();
			break;

		case (ACCT_KERNEL_CFG):
		case (ACCT_KERNEL_EOJ):
		case (ACCT_KERNEL_SOJ):
			prt_ret = pr_special();
			break;
#ifdef WKMG_HERE
		case (ACCT_DAEMON_NQS):
			bld_nqs();
			break;
#endif
		case (ACCT_KERNEL_SITE0):
		case (ACCT_KERNEL_SITE1):
		case (ACCT_DAEMON_SITE0):
		case (ACCT_DAEMON_SITE1):
			t_Nsite++;
			acct_err(ACCT_INFO,
			       _("A reserved SITE type(%d) record was found and ignored."),
				type);
			fprintf(stdout, "# SITE   (%4o) - SITE reserved "
				"record.\n", type);
			break;

		default:
			acct_err(ACCT_WARN,
			       _("An invalid daemon identifier (%d) was found in the header field."),
				type);
		}

	}	/* while getacctent() */

	/*  Generate the requested reports. */
	rpt_process();
#ifdef WKMG_HERE
	if (d_opt) {
		rpt_nqs();
	}
#endif

	/*  Turn off job accounting if requested. */
	if (t_opt) {
		strncpy(job_req.job_path, fn, ACCT_PATH);
		if (csa_jastop(&job_req) == -1) {
			acct_perr(ACCT_ABORT, errno,
				_("An error was returned from the call to the '%s' routine."),
				"acctctl()");
		}

	/*  Only remove the file if a temporary file name was used. */
		if (temp_file && unlink(fn)) {
			acct_perr(ACCT_WARN, errno,
				_("An error occurred during the removal of file '%s'."),
				fn);
		}
	}

	exit(0);
}


/*
 *	getacctent() - Get the next job accounting entry.
 *		return the number of bytes read.
 */
off_t	nextpos = 0;
off_t	lastpos = 0;
static	int
getacctent(int ***marks)
{
	int	nbytes;
	char	temp[40];
	int	type;

	if (M_opt && (nextpos >= lastpos) ) {
		if (**marks == NULL) {
			return(FALSE);
		}
		nextpos = (**marks)[0];
		lastpos = (**marks)[1];
		(*marks)++;
		if (seekacct(jafd, nextpos, SEEK_SET) < 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the positioning of file '%s'."),
				fn);
		}
	}

	/*  Read the next record. */
	nbytes = readacctent(jafd, &acctrec, FORWARD);
	if (nbytes < 0) {
		fprintf(stderr, "%s: readacctent(%s) error(%d).\n",
			Prg_name, fn, nbytes);
		fflush(stdout);
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the reading of file '%s' %s."),
			fn,
			", the job accounting file");
	}
	nextpos += nbytes;

	return(nbytes ? TRUE : FALSE);
}
