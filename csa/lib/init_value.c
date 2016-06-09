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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>

#include "acctdef.h"
#include "csa_conf.h"
#include "sbu.h"
#include "acctmsg.h"


/*
 *	Return a pointers to the character value of a system parameter
 *	which is set in the configuration file.  If the label was not
 *	found in the configuration file, use the default value passed.
 */
#define	FUNC_NAME	"init_char()"	/* library function name */
char *
init_char(char *label, char *value, int flag)
{
	char		*resp;		/* return value from config() */

	if ((resp = config(label)) == NULL) {
		if (flag) {
			acct_err(ACCT_CAUT,
			       _("%s: The '%s' parameter is missing from configuration file.\n    Please add '%s' to the configuration file."),
				FUNC_NAME,
				label, label);
		}
		resp = value;
	}

	return(resp);
}
#undef	FUNC_NAME


/*
 *	Return the real value of a system parameter which is set
 *	in the configuration file.  If the label was not found in the
 *	configuration file, use the default value passed.
 */
#define	FUNC_NAME	"init_real()"	/* library function name */
double
init_real(char *label, double value, int flag)
{
	char		*resp;		/* return value from config() */

	if ((resp = config(label)) == NULL) {
		if (flag) {
			acct_err(ACCT_CAUT,
			       _("%s: The '%s' parameter is missing from configuration file.\n    Please add '%s' to the configuration file."),
				FUNC_NAME,
				label, label);
		}
		return(value);
	}

	return(atof(resp));
}
#undef	FUNC_NAME


/*
 *	Return the integer value of a system parameter which is set
 *	in the configuration file.  If the label was not found in the
 *	configuration file, use the default value passed.
 */
#define	FUNC_NAME	"init_int()"	/* library function name */
int
init_int(char *label, int value, int flag)
{
	char		*resp;		/* return value from config() */

	if ((resp = config(label)) == NULL) {
		if (flag) {
			acct_err(ACCT_CAUT,
			       _("%s: The '%s' parameter is missing from configuration file.\n    Please add '%s' to the configuration file."),
				FUNC_NAME,
				label, label);
		}
		return(value);
	}

	return(atoi(resp));
}
#undef	FUNC_NAME
