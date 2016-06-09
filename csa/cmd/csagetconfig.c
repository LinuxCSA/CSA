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
 *
 *	csagetconfig.c - command interface to the config() library
 *			 routine for scripts.
 *
 *	This program serves as a command interface for scripts to 
 *	the library routine to search the configuration file for a
 *	string which matches the argument.  If found, the associated
 *	string is written to stdout and a status of 0 is returned.
 *	If not found then a status of one is returned.
 *
 *	COMMAND LINE
 *	
 *	usage: csagetconfig arg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>

#include "acctdef.h"
#include "acctmsg.h"

char		*Prg_name;

void usage()
{
	acct_err(ACCT_ABORT,
	       _("Command Usage:\n%s\tlabel_name"),
		Prg_name);
}

main(int argc, char **argv)
{
	char	cname[4096];
	char	*response;

	Prg_name = argv[0];

	if (argc != 2)
		usage();

	strcpy(cname, argv[1]);

	if ((response = config(cname)) == NULL)
		exit(1);
	else
	{
		fprintf(stdout, "%s", response);
		exit(0);
	}
}
