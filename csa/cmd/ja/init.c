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

#include <grp.h>
#include <malloc.h>
#include <memory.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>

#ifdef HAVE_REGCOMP
#ifdef REG_EXTENDED
#define REGCOMP_FLAG    REG_EXTENDED
#else	/* REG_EXTENDED */
#define REGCOMP_FLAG    0
#endif	/* REG_EXTENDED */
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
char *regcmp();
extern char *__loc1;
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */


#include <job.h>
#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csaacct.h"
#include "csaja.h"
#include "extern_ja.h"

extern	int	optind;
extern	char	*optarg;

char	*fn;			/* pointer to file name */
char	fname[MAX_FNAME];	/* job accounting file name */
int	temp_file = FALSE;	/* temp file used if TRUE */
int	**marks;		/* file position markers */

#ifdef HAVE_REGCOMP
regex_t **names;
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
char	**names;
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */

int	MEMINT;			/* memory integral index */

int	a_opt = 0;	/* a option: select by array session ID */
int	c_opt = 0;	/* c option: command report */
int	d_opt = 0;	/* d option: daemon report */
int	D_opt = 0;	/* D option: debug option */
int	e_opt = 0;	/* e option: extended summary report */
int	f_opt = 0;	/* f option: command flow report */
int	g_opt = 0;	/* g option: select by group ID */
int	h_opt = 0;	/* h option: report hiwater mark */
int	j_opt = 0;	/* j option: select by job ID */
int	J_opt = 0;	/* J option: select all special pacct records */
int	l_opt = 0;	/* l option: long report */
int	m_opt = 0;	/* m option: mark position */
int	M_opt = 0;	/* M option: select by mark position */
int	n_opt = 0;	/* n option: select by command name */
int	o_opt = 0;	/* o option: other command report */
int	p_opt = 0;	/* p option: select by project ID */
int	r_opt = 0;	/* r option: raw mode */
int	s_opt = 0;	/* s option: summary report */
int	t_opt = 0;	/* t option: terminate job accounting */
int	u_opt = 0;	/* u option: select by user ID */

uint64_t s_ash = -1;	/* array session ID to select by */
gid_t	s_gid = -1;	/* group ID to select by */
uint64_t s_jid = -1;	/* job ID to select by */
uint64_t s_prid = -1;	/* project ID to select by */
uid_t	s_uid = -1;	/* user ID to select by */

int	M_optargc;	/* -M option argument count */
char	**M_optargv;	/* -M option argument Mark list */
int	n_optargc;	/* -n option argument count */
char	**n_optargv;	/* -n option argument Name list */
 
/*
 * Output the new format usage message.
 */
static void
usage()
{
	acct_err(ACCT_ABORT,
	       "Command %s Usage:\n"
	       "%s\t-c [[-f] [-s] [-t]] [[-a ash] [-g gid] [-j jid]\n"
               "\t[-M marks] [-n names] [-p prid] [-u uid]]\n"
	       "\t[[-l [-h]] [-d] [-e] [-J] [-r]] [-D lvl]\n"
	       "\t[file] (command report)\n"
	       "%s\t-f [[-c] [-s] [-t]] [[-a ash] [-g gid] [-j jid]\n"
	       "\t[-M marks] [-n names] [-p prid] [-u uid]]\n"
	       "\t[[-l [-h]] [-d] [-e] [-J] [-r]] [-D lvl]\n"
	       "\t[file] (command flow report)\n"
	       "%s\t-o [-s] [-t] [[-a ash] [-g gid] [-j jid]\n"
	       "\t[-M marks] [-n names] [-p prid] [-u uid]]\n"
	       "\t[[-d] [-e] [-r]] [-D lvl]\n"
	       "\t[file] (command other report)\n"
	       "%s\t-s [[-c] [-f] [-t]] [[-a ash] [-g gid] [-j jid]\n"
	       "\t[-M marks] [-n names] [-p prid] [-u uid]]\n"
	       "\t[[-l [-h]] [-d] [-e] [-r]] [-D lvl]\n"
	       "\t[file] (summary report)\n"
	       "%s\t-m [-D lvl] [file] (mark position request)",
		Prg_name,
		Prg_name, Prg_name, Prg_name,
		Prg_name, Prg_name, Prg_name);

	return;
}
 

/*
 *	lastacctent() - Return the offset to the last account entry.
 */
static	int
lastacctent()
{
	int	nbytes;

	if (seekacct(jafd, 0, SEEK_END) < 0 || (nbytes =
			readacctent(jafd, &acctrec, BACKWARD)) <= 0) {
		acct_perr(ACCT_ABORT, errno,
		    _("An error occurred during the positioning of file '%s'."),
			fn);
	}

	return(eof_mark - nbytes);
}


/*
 *	getnames() - Get the names from the option argument list.
 */
#ifdef HAVE_REGCOMP
static	regex_t **
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
static	char **
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */
getnames(int optargc, char **optargv)
{
	int	ind;

#ifdef HAVE_REGCOMP
	regex_t	**n;
	if ((n = (regex_t **)malloc((optargc + 1) * sizeof(regex_t *))) == NULL) {
	    acct_perr(ACCT_ABORT, errno,
	     _("There was insufficient memory available when allocating '%s'."),
	    	"selection by name\n");
	}
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
	char	**n;
	if ((n = (char **)malloc((optargc + 1) * sizeof(char *))) == NULL) {
	    acct_perr(ACCT_ABORT, errno,
	     _("There was insufficient memory available when allocating '%s'."),
	    	"selection by name\n");
	}
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */

	/*
	 * Convert each item in the option argument list into a compiled form
	 * of a regular expression.
	 */
	for (ind = 0; ind < optargc; ind++) {
#ifdef HAVE_REGCOMP
		regex_t *s = (regex_t *) calloc(1, sizeof(regex_t));
		if (regcomp(s, optargv[ind], REGCOMP_FLAG)) {
			free(s);
			acct_err(ACCT_ABORT,
			  _("An invalid regular expression was supplied for selection by name.")
				);
		}
		n[ind] = s;
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
		if ((n[ind] = regcmp(optargv[ind], (char *)0)) == NULL) {
			acct_err(ACCT_ABORT,
			  _("An invalid regular expression was supplied for selection by name.")
				);
		}
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */
	}
	n[optargc] = NULL;

	return(n);
}


/*
 *	getrange() - Get the range values from the option argument list item.
 */
static	void
getrange(char *optarg, int *r)
{
	int	ind;
	int	base;
	int	digit;
	int	saved_r0;
	char	c;
	char	buf[128];

	ind = 0;
	r[1] = eof_mark;
	strncpy(buf, optarg, sizeof(buf));
	c = *optarg++;
	if (c == '.') {
		r[0] = lastacctent();
		c = *optarg++;

	} else {
		do {
			if (c == '0') {
				base = 8;
			} else {
				base = 10;
			}
			r[ind] = 0;
			while (c >= '0' && c <= '9') {
				if ((digit = c - '0') >= base) {
					break;
				}
				r[ind] = r[ind] * base + digit;
				c = *optarg++;
			}
			if (c != ':' || ind++ > 0) {
				break;
			}

		} while ((c = *optarg++) >= '0' && c <= '9');

		saved_r0 = r[0];
		if ((r[0] = skipja(r[0])) < 0) {
			fprintf(stderr,
			      "%s:  getrange() invalid beginning mark %d, eof %d\n",
				Prg_name, saved_r0, eof_mark);
			sprintf(buf, "%d", saved_r0);
			acct_err(ACCT_ABORT,
			    _("The (-%s) option's argument, '%s', is invalid."),
				"M", buf);
		}
		if (ind == 0) {
			r[1] = r[0] + sizeof(struct acctcsa);
		}
	}

	if (c != '\0') {
		acct_err(ACCT_ABORT,
		       _("The (-%s) option's argument, '%s', is invalid."),
			"M", buf);
	}

	if (r[0] < 0 || r[0] >= r[1] || r[1] > eof_mark) {
		fprintf(stderr, "%s:  getrange adjusted begin-mark %d\n",
			Prg_name, r[0]);	
		fprintf(stderr,
		   "%s:  getrange(0 < begin-mark %d < end-mark %d < eof %d)\n",
		   Prg_name, saved_r0, r[1], eof_mark);
		sprintf(buf, "%d:%d", saved_r0, r[1]);
		acct_err(ACCT_ABORT,
		       _("The (-%s) option's argument, '%s', is invalid."),
			"M", buf);
	}

	return;
}

/*
 *	getmarks() - Get the mark values from the option argument list.
 */
int **
getmarks(int optargc, char **optargv)
{
	int	**mstr;
	int	*r;
	int	*s;
	int	ind;
	int	jnd;
	int	last;
	int	nbytes;

	/*
	 *  The marks array comprises two sections: the first has
	 *  (optargc + 1) elements of type (int *) while the second
	 *  has (optargc * 2) elements of type (int).  The pairs of
	 *  numbers representing the beginning and ending marks are
	 *  stored in the second section.  Pointers to these pairs
	 *  are stored in the first section; a NULL value is stored
	 *  in the last element there.
	 */
	nbytes = (optargc + 1) * sizeof(int *) + optargc * 2 * sizeof(int);
	if ((mstr = (int **)malloc(nbytes)) == NULL) {
	    acct_perr(ACCT_ABORT, errno,
	     _("There was insufficient memory available when allocating '%s'."),
	    	"positioning marks\n");
	}

	/*
	 *  Convert each item in the option argument list into a beginning
	 *  and ending mark, i.e. a range and add it to the list of marks.
	 *
	 *  To set the value of r, add (optargc + 1) * sizeof(int *) to
	 *  mstr to get to the start of the second section described above,
	 *  then add (2 * ind) * sizeof(int) to get to the specific range.
	 */
	for(ind = 0; ind < optargc; ind++) {
		r = (int *) (mstr + optargc + 1) + 2 * ind;
		getrange(optargv[ind], r);

		for(jnd = 0; jnd < ind; jnd++) {
			s = mstr[jnd];
			if (r[0] < s[0]) {
				mstr[jnd] = r;
				r = s;
			}
		}
		mstr[ind] = r;
	}
	mstr[optargc] = NULL;

	/* Combine ranges that overlap. */
	last = optargc - 1;
	for(ind = last; ind > 0; ind--) {
		r = mstr[ind - 1];
		s = mstr[ind];
		if ((r[0] <= s[1]) && (r[1] >= s[0]) ) {
			if (s[0] < r[0]) {
				r[0] = s[0];
			}
			if (s[1] > r[1]) {
				r[1] = s[1];
			}
			for(jnd = ind; jnd <= last; jnd++) {
				mstr[jnd] = mstr[jnd + 1];
			}
			last--;
		}
	}

	return(mstr);
}


/*
 *	skipja() - Return the position of the first non-ja entry
 *	following the mark.
 */
int
skipja(int mark)
{
	int	nbytes;

	if (seekacct(jafd, mark, SEEK_SET) < 0) {
		acct_perr(ACCT_WARN, errno,
		   _("An error occurred during the positioning of file '%s'."),
			fn);
		return(-1);
	}

	if ((nbytes = readacctent(jafd, &acctrec, FORWARD)) <= 0 ) {
		acct_perr(ACCT_WARN, errno,
		     _("An error occurred during the reading of file '%s' %s."),
			fn,
			"job accounting file\n");
		return(-1);
	}

	if ((acctrec.csa == NULL) && (acctrec.cfg == NULL) &&
	    (acctrec.soj == NULL) && (acctrec.eoj == NULL) ) {
		acct_err(ACCT_WARN,
		       _("An unknown record type (%4o) was found in the '%s' routine."),
			0, "skipja()");
		return(mark);
	}

	if (acctrec.csa != NULL) {
		if (strcmp(acctrec.csa->ac_comm, "ja") == 0) {
			mark += nbytes;
		}
	}

	return(mark);
}


/*
 *	init_config() - Get the system parameter values from the
 *	configuration file and create run-time variables.
 */
static	void
init_config()
{

	/*
	 *	Determine which memory integral to use.
	 */
	MEMINT = init_int(MEMORY_INTEGRAL, 1, TRUE);

	/*
	 *	Initialize variables needed for multitasking and pacct sbus.
	 */
	init_pacct_sbu();

	return;
}


/*
 *	init_irix() - initialize the IRIX format argument list.
 */
static	int
init_irix(int argc, char **argv)
{
	int	c;
	char	*optstring = "a:D:g:j:p:u:M:n:cdefhJlmorst";

	/*  Process options. */
	while ((c = getopt(argc, argv, optstring)) != EOF) {
		switch (c) {

	 	/*  Process the report selection options. */
		case 'c':		/* command report */
			c_opt++;
			break;
		case 'f':		/* command flow report */
			f_opt++;
			break;
		case 'o':		/* other command report */
			o_opt++;
			break;
		case 's':		/* summary report */
			s_opt++;
			break;

	 	/*  Process the record selection options. */
		case 'a':		/* select by array session ID */
			a_opt++;
			sscanf(optarg, "%llx", &s_ash);
			break;

		case 'g':		/* select by group ID */
			g_opt++;
			if (optarg[0] >= '0' && optarg[0] <= '9') {
				sscanf(optarg, "%d", &s_gid);

			} else {	/* ASCII */
				if ((s_gid = name_to_gid(optarg)) == -1) {
					acct_err(ACCT_ABORT,
					       _("An unknown Group name '%s' was given on the -g parameter."),
						optarg);
				}
			}
			break;

		case 'j':		/* select by job ID */
			j_opt++;
			sscanf(optarg, "%llx", &s_jid);
			break;

		case 'M':			/* select by positioning */
			M_opt++;
			if ((M_optargc = getoptlst(optarg, &M_optargv))
					== -1) {
				acct_err(ACCT_ABORT,
				       _("An error was returned from routine '%s' for the (-%s) option '%s'."),
					"getoptlst()",
					"M", optarg);
			}
			break;

		case 'n':		/* select by command names */
			n_opt++;
			if ((n_optargc = getoptlst(optarg, &n_optargv))
					== -1) {
				acct_err(ACCT_ABORT,
				       _("An error was returned from routine '%s' for the (-%s) option '%s'."),
					"getoptlst()",
					"n", optarg);
			}
			names = getnames(n_optargc, n_optargv);
			free(n_optargv);
			break;

		case 'p':		/* select by project ID */
			p_opt++;
			if (optarg[0] >= '0' && optarg[0] <= '9') {
				sscanf(optarg, "%lld", &s_prid);

			} else {	/* ASCII */
				if ((s_prid = name_to_prid(optarg)) == -1) {
					acct_err(ACCT_ABORT,
					       _("An unknown Project name '%s' was given on the -p parameter."),
						optarg);
				}
			}
			break;

		case 'u':		/* select by user ID */
			u_opt++;
			if (optarg[0] >= '0' && optarg[0] <= '9') {
				sscanf(optarg, "%d", &s_uid); /* numeric */

			} else {	/* ASCII */
				if ((s_uid = name_to_uid(optarg)) == -1) {
					acct_err(ACCT_ABORT,
					       _("An unknown User name '%s' was given on the -u parameter."),
						optarg);
				}
			}
			break;

	 	/*  Process the report modification options. */
		case 'd':		/* daemon report */
			d_opt++;
			break;
		case 'e':		/* extended summary report */
			e_opt++;
			break;
		case 'h':		/* hiwater report */
			h_opt++;
			break;
		case 'J':
			J_opt++;	/* select all special pacct */
			break;
		case 'l':		/* long report (-{B c f} only) */
			l_opt++;
			break;

	 	/*  Process the remaining options. */
		case 'D':		/* debug option */
			D_opt++;
			db_flag = atoi(optarg);
			if ((db_flag < 1) || (db_flag > 4)) {
				acct_err(ACCT_FATAL,
				       _("The (-%s) option's argument, '%s', is invalid."),
					"D", optarg);
				Nerror("Option -D valid values are 1 to 4\n");
				usage();
			}
			Ndebug("Debugging option set to level %d\n", db_flag);
			break;
		case 'm':		/* mark position */
			m_opt++;
			break;
		case 'r':		/* raw mode */
			r_opt++;
			break;
		case 't':		/* terminate job accounting */
			t_opt++;
			break;

		default:
			usage();
		}
	}

	/*  Verify the selected options - must be used with report options. */
	if (!c_opt && !f_opt && !o_opt && !s_opt){
		if (a_opt) {
		    acct_err(ACCT_ABORT,
		      _("The (-%s) option must be used with the (-%s) option."),
			 "a", "{c f o s}");
		}
		if (g_opt) {
                    acct_err(ACCT_ABORT,
                      _("The (-%s) option must be used with the (-%s) option."),
			 "g", "{c f o s}");
                }
		if (j_opt) {
                    acct_err(ACCT_ABORT,
                      _("The (-%s) option must be used with the (-%s) option."),
			 "j", "{c f o s}");
                }
		if (n_opt) {
                    acct_err(ACCT_ABORT,
                      _("The (-%s) option must be used with the (-%s) option."),
			 "n", "{c f o s}");
                }
		if (p_opt) {
                    acct_err(ACCT_ABORT,
                      _("The (-%s) option must be used with the (-%s) option."),
			 "p", "{c f o s}");
                }
		if (u_opt) {
                    acct_err(ACCT_ABORT,
                      _("The (-%s) option must be used with the (-%s) option."),
			 "u", "{c f o s}");
                }
		if (d_opt) {
                    acct_err(ACCT_ABORT,
                      _("The (-%s) option must be used with the (-%s) option."),
			 "d", "{c f o s}");
                }
		if (r_opt) {
                    acct_err(ACCT_ABORT,
                      _("The (-%s) option must be used with the (-%s) option."),
			 "r", "{c f o s}");
                }

	/*  and -m is NOT used with the report options. */
	} else if (m_opt) {
	    acct_err(ACCT_ABORT,
		_("The (-%s) option cannot be selected when issuing a report."),
			"m");
	}

	/*  Insure -m is NOT with t. */
	if (t_opt && m_opt) {
		acct_err(ACCT_ABORT,
		   _("The (-%s) and the (-%s) options are mutually exclusive."),
			"m", "t");
	}

	/*  Insure -o is NOT with c. */
	if (o_opt && c_opt) {
		acct_err(ACCT_ABORT,
		   _("The (-%s) and the (-%s) options are mutually exclusive."),
			"o", "c");
	}

	/*  Insure -e is with {s}. */
	if (e_opt && !(s_opt) ) {
		acct_err(ACCT_ABORT,
		      _("The (-%s) option must be used with the (-%s) option."),
			"e", "s");
	}

	/*  Insure -h is with l and c. */
	if (h_opt && !(c_opt && l_opt)) {
		acct_err(ACCT_ABORT,
		      _("The (-%s) option must be used with the (-%s) option."),
			"h", "c and l");
	}

	/*  Insure -l is with -{ c f} */
	if (l_opt && !(c_opt || f_opt)) {
		acct_err(ACCT_ABORT,
		      _("The (-%s) option must be used with the (-%s) option."),
			"l", "{c f}");
	}

	/*  Insure -M is with -{c}. */
	if (M_opt && !c_opt) {
		acct_err(ACCT_ABORT,
		      _("The (-%s) option must be used with the (-%s) option."),
			"M", "c");
	}

	return(0);
}



/*
 *	init() - initialize the program.
 */
int
init(int argc, char **argv)
{
	int	file_exists = 1;
	extern	int	optind;
	extern	char	*optarg;

	Prg_name = argv[0];

	/*  Process command options. */
	init_irix(argc, argv);

	/*  Process operands. */
	if (optind + 1 == argc) {
		fn = argv[optind];

	} else if (optind != argc) {
		usage();
	}

	/*  Construct the job accounting file name, if not provided. */
	if (fn == NULL) {
		uint64_t myjid = 0;	/* job ID of job ja is running in */
		pid_t	pid;		/* PID of job */
		int	fnl;		/* length of file name (bytes) */
		char	*dir;		/* pointer to directory name for */
					/* default job accounting file */
		char	tmp[] = "/tmp";

	/* Make sure this process is part of a job by fetching it's job ID. */
		pid = getpid();
		myjid = job_getjid(pid);
		if (myjid == 0) {
			acct_err(ACCT_ABORT,
			_("The process running does not have a valid Job ID."));
		}

		fn = fname;
		if ((dir = getenv(ACCT_TMPDIR)) == NULL &&
		    (dir = getenv("TMPDIR")) == NULL) {
			dir = tmp;
		}

		if ((fnl = sprintf(fn, "%s/.jacct%llx", dir, myjid)) < 0) {
			acct_err(ACCT_ABORT,
			       _("Cannot build the file name from the TMPDIR environment variable and the Job ID.")
				);
		}

		if (fnl >= MAX_FNAME) {
			acct_err(ACCT_ABORT,
			       _("The file name exceeds the maximum length of 128 characters.")
				);
		}

		temp_file = TRUE;
	}

	/*  Get the parameters from the configuration file. */
	init_config();

	/*  Initialize daemon pointers if necessary. */
#ifdef WKMG_HERE
	if (d_opt) {
		if ((nqhdr = (struct nqshdr *)malloc(sizeof(struct nqshdr))) ==
				NULL) {
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when allocating '%s'."),
				"nqshdr");
		}
		nqhdr->head = nqhdr->last = NULL;

		/* add Workload Management code here */
	}
#endif

	return(file_exists);
}
