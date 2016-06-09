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
 *	This module contains error message processing routines.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <nl_types.h>
#include <string.h>

#include "acctmsg.h"

nl_catd	mcfd;		/* catalog file descriptor */
extern	char	*Prg_name;

int	db_flag = 0;		/* debug flag */

char	buf2[ACCT_BUFL];	/* vsprint error message buffer */
char	buf3[ACCT_BUFL+40];	/* catmsgfmt error message buffer */

/*
 *	msg_lvl - Set the appropriate severity level.
 */
static char	*
msg_lvl(accterr level)
{
	char	*e;	/* Error severity */

	switch(level) {

	case ACCT_INFO:
	    e = "INFORMATIONAL";
	    break;

	case ACCT_EFFY:
	    e = "EFFICIENCY";
	    break;

	case ACCT_WARN:
	    e = "WARNING";
	    break;

	case ACCT_CAUT:
	    e = "CAUTION";
	    break;

	case ACCT_FATAL:
	    e = "UNRECOVERABLE";
	    break;

	case ACCT_ABORT:
	    e = "UNRECOVERABLE";
	    break;

	default:
	    e = "UNKNOWN";
	}

	return(e);
}


/*
 *	msg_printable - Ensure that a message is printable and broken into
 *	managable lengths.
 */
static void
msg_printable(char *msg)
{
	char	*c;
	int	char_cnt, cerr_cnt, line_cnt;
	int	ind;

	if (msg == NULL) {
	    return;
	}

	c = msg;
	cerr_cnt = 0;
	char_cnt = 0;
	while (*c != (char)NULL) {
	    char_cnt++;

/*
 *	Check for a real \n, count it as a new line.
 */
	    if (*c == '\n') {
		line_cnt++;
		char_cnt = 0;

/*
 *	Check for a real \t, count it as a max 8 characters.
 */
	    } else if (*c == '\t') {
		char_cnt = ((char_cnt+7) /8) *8;

/*
 *	Check for a non-printable character, change to ':'.
 */
	    } else if (!isprint(*c) ) {
		cerr_cnt++;
		*(char *)c = ':';
	    }

/*
 *	Check the length of the line against the limit.
 */
	    if ((char_cnt) > MAX_ERR_LEN) {
		for(ind = 0; ind < MAX_ERR_LEN; ind++) {

/*
 *	Search backwards for last whitespace and make it a newline.
 */
		    if (*(char *)(c-ind) == ' ') {
			*(char *)(c-ind) = '\n';
			char_cnt = ind;
			line_cnt++;
			break;
		    }
		}
	    }

	    c++;
	}

	return;
}


/*
 *	acct_err - Process an error message from the message system.
 */
void
acct_err(accterr level, char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);

	vfprintf (stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

/*
 *     Check if the message if fatal.
 */
	if (level == ACCT_ABORT) {
	   exit(1);
	}
}


/*
 *	acct_perr - Process an error message from the message system that
 *	includes a errno message code.
 */
void
acct_perr(accterr level, int errnm, char *fmt, ...)
{
	va_list args;
	char	sysstr[200], errstr[400];

	strncpy(errstr, fmt, 200);

/*
 *	Validate the errno value and get the error message.
 */
msgerr:
	sprintf(sysstr, _("\n   System Error(%d): %s.\n"), errnm,
		strerror(errnm));

	strncat(errstr, sysstr, 200);

	va_start(args, fmt);
	vfprintf (stderr, errstr, args);
	va_end(args);

/*
 *	Check if the message if fatal.
 */
	if (level == ACCT_ABORT) {
	    exit(1);
	}

	return;
}


/*
 *	Error message routine - write a message to stderr.
 */
void
Nerror(char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}


/*
 *	Write a debug message to stderr if debugging compiled.
 */
void
Ndebug(char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	return;
}
