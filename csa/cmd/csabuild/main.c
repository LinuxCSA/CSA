/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2007 Silicon Graphics, Inc  All Rights Reserved.
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
 *	csabuild - Program to create sorted pacct file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "csaacct.h"

#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csabuild.h"
#include "extern_build.h"

char	*Prg_name;

/*
 *	csabuild (main) routine.
 */
main(int argc, char **argv)
{
	long	hz;
	struct	tms	timbuf1;
	struct	tms	timbuf2;

	Prg_name = argv[0];
	hz = sysconf(_SC_CLK_TCK);

	/*  Process the command options. */
	init_cmd(argc, argv);

	if (db_flag > 2) {
		Ndebug("main(3): cutoff(%d)  -  %s", cutoff, ctime(&cutoff));
	}
	if (db_flag > 7) {
		Ndebug("sizeof(struct acctcfg):  %d\n", sizeof(struct acctcfg));
		Ndebug("sizeof(struct acctjob):  %d\n", sizeof(struct acctjob));
		Ndebug("sizeof(struct acctcsa):  %d\n", sizeof(struct acctcsa));
		Ndebug("sizeof(struct acctsoj):  %d\n", sizeof(struct acctsoj));
		Ndebug("sizeof(struct accteoj):  %d\n", sizeof(struct accteoj));
		Ndebug("sizeof(struct nqsbs):    %d\n", sizeof(struct nqsbs));
		Ndebug("sizeof(struct wkmgmtbs): %d\n",sizeof(struct wkmgmtbs));
		Ndebug("sizeof(struct tapebs):   %d\n", sizeof(struct tapebs));
	}

	/*  Get the parameters from the system configuration file. */
	init_config();

	/*  Build the file table structure. */
	CHECKADDRS(1);
	Build_filetable();
	CHECKADDRS(2);

	/*  Build internal parse tables. */
	if (t_opt) {
		times(&timbuf1);
	}
	CHECKADDRS(3);
	Build_tables();
	CHECKADDRS(4);
	if (t_opt) {
		times(&timbuf2);
		acct_err(ACCT_INFO,
		       _("Times required to build tables: user %f seconds, sys %f seconds."),
			(((double)(timbuf2.tms_utime-timbuf1.tms_utime))/((float)hz)),
		(((double)(timbuf2.tms_stime-timbuf1.tms_stime))/((float)hz)));
	}
	CHECKADDRS(5);

	if (db_flag >= 9) {
		Dump_segment_tbl();
	}
	CHECKADDRS(6);

	/*  Now write the Sorted pacct file. */
	if (t_opt) {
		times(&timbuf1);
	}
	CHECKADDRS(7);
	Create_srec();
	CHECKADDRS(8);
	if (t_opt) {
		times(&timbuf2);
		acct_err(ACCT_INFO,
		       _("Times required to build tables: user %f seconds, sys %f seconds."),
			(((double)(timbuf2.tms_utime-timbuf1.tms_utime))/((float)hz)),
		(((double)(timbuf2.tms_stime-timbuf1.tms_stime))/((float)hz)));
	}
	CHECKADDRS(9);

	exit(0);
}
