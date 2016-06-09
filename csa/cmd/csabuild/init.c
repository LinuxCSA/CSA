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

extern	int	optind;
extern	char	*optarg;

char	*spacct = SPACCT;	/* sorted pacct output file */
char	*pacct = WPACCT;	/* pacct input files */

int	a_opt = 1;		/* assume system crash/restart */
int	A_opt = 0;		/* Abort on errors option */
int	i_opt = 0;		/* Ignore bad records option */
int	n_opt = 0;		/* Supress Workload Mgmt unite option */
int	t_opt = 0;		/* Timer option */

int	MAXFILES = 200;		/* max # of files to be processed */
int	MEMINT = 0;		/* memory integral index */

time_t	cutoff = -1;		/* time cutoff value */
 
/*
 * Output the new format usage message.
 *
 *	while((c = getopt(argc, argv, "aAD:ino:P:s:S:t")) != EOF) {
 *	csabuild [-a] [-A] [-D debug] [-i] 
 *		 [-n] [-o time] [-P pacct] [-s spacct] [-t]
 *	-a		assume system crash/restart
 *	-A		Abort on errors option
 *	-D debug	Set the debug level to <debug>
 *	-i		Ignore bad records option
 *	-n		Suppress Workload Mgmt unite option
 *	-o xx		cutoff time value option
 *	-P pacct	Input pacct file
 *	-s spacct	Output sorted pacct file
 *	-t		Timer option
 */
static void
usage()
{
	acct_err(ACCT_ABORT,
	       "Command Usage:\n%s\t[-a] [-A] [-D debug] [-i] [-n]\n\t\t[-o time] [-P pacct] [-s spacct] [-t]",
		Prg_name);
}


/*
 *	init_cmd() - process the command line options.
 */
void
init_cmd(int argc, char **argv)
{
	char	ch;
	int	c;
	time_t	Now;

/*
 *	Process the command options.
 */
	while((c = getopt(argc, argv, "aAD:ino:P:s:t")) != EOF) {
		ch = (char)c;
		switch(ch) {

		case 'a':
			a_opt = 0;
			break;

		case 'A':
			A_opt++;
			break;

		case 'D':
			db_flag = atoi(optarg);
			if ((db_flag < 0) || (db_flag > 9)) {
				acct_err(ACCT_FATAL,
				       _("The (-%s) option's argument, '%s', is invalid."),
					"D", optarg);
				Nerror("Option -D valid values are 0 thru 9\n");
				usage();
			}
			Ndebug("Debugging option set to level %d.\n", db_flag);
			break;

		case 'i':
			i_opt = 1;
			break;

		case 'n':
			n_opt = 1;
			break;

		case 'o':
			if (time(&Now) == (time_t)-1) {
				acct_perr(ACCT_ABORT, errno,
					_("An error was returned from the call to the '%s' routine."),
					"time()");
			}
			cutoff = Now - (A_DAY * atol(optarg));
			break;

		case 'P':
			pacct = optarg;
			break;

		case 's':
			spacct = optarg;
			break;

		case 't':
			t_opt = 1;
			break;

		default:
			usage();
		}		/* end of switch(getopt) */
	}

	/*
	 *	Create the Workload Mgmt head pointers.
	 */
	init_wm();

	return;
}


/*
 *	init_config() - Get the system parameter values from the
 *	configuration file and create run-time variables.
 */
void
init_config()
{

	/*
	 *	Determine which memory integral to use.
	 */
	MEMINT = init_int(MEMORY_INTEGRAL, 1, TRUE);
	MAXFILES = init_int("BUILD_MAXFILES", 200, TRUE);

	/*
	 *  Initialize variables needed for SBU calculations.
	 */
	init_pacct_sbu();
	init_wm_sbu();

	return;
}
