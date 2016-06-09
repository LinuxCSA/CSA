/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2006-2007 Silicon Graphics, Inc.  All Rights Reserved.
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
 *  The routines in this file convert pacct records.  Fields which did not
 *  exist previously are zeroed out.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "csaacct.h"

#include "acctdef.h"
#include "acctmsg.h"
#include "convert.h"
#include "csa.h"

/*
 *  Convert pacct CSA records to the newest format.
 */
int
convert_pacct_csa(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_CSA, flag);

	return (sizeof (struct acctcsa));
}

/*
 *  Convert pacct Memory records to the newest format.
 */
int
convert_pacct_mem(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_MEM, flag);

	return (sizeof (struct acctmem));
}

/*
 *  Convert pacct Input/Output records to the newest format.
 */
int
convert_pacct_io(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_IO, flag);

	return (sizeof (struct acctio));
}

/*
 *  Convert pacct Delay records to the newest format.
 */
int
convert_pacct_delay(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_DELAY, flag);

	return (sizeof (struct acctdelay));
}


/*
 *  Convert pacct SOJ records to the newest format.
 */
int
convert_pacct_soj(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_SOJ, flag);

	return (sizeof (struct acctsoj));
}

/*
 *  Convert pacct EOJ records to the newest format.
 */
int
convert_pacct_eoj(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_EOJ, flag);

	return (sizeof (struct accteoj));
}

/*
 *  Convert pacct CFG records to the newest format.
 */
int
convert_pacct_cfg(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_CFG, flag);

	return (sizeof (struct acctcfg));
}

/*
 * Convert pacct Workload Management records to the newest format.
 */
int
convert_pacct_wkmg(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_WKMG, flag);

	return (sizeof (struct wkmgmtbs));
}

/*
 * Convert spacct Job header records to the newest format.
 */
int
convert_pacct_job(void *buf, int flag)
{
	convert_hdr((struct achead *)buf, REC_JOB, flag);

	return (sizeof (struct acctjob));
}

/*
 *  Convert pacct/spacct records to the newest format.
 *
 *  Returns 0 if error.
 */
#define FUNC_NAME     "convert_pacct()"
int
convert_pacct(void *buff, int flag)
{
	struct	achead	*h;

	h = (struct achead *)buff;

	CHECK_REV (h);

	switch (h->ah_type) {

	case	ACCT_KERNEL_CSA:
		return (convert_pacct_csa (buff, 0));

	case	ACCT_KERNEL_MEM:
		return (convert_pacct_mem (buff, 0));

	case	ACCT_KERNEL_IO:
		return (convert_pacct_io (buff, 0));

	case	ACCT_KERNEL_DELAY:
		return (convert_pacct_delay (buff, 0));

	case	ACCT_KERNEL_SOJ:
		return (convert_pacct_soj (buff, 0));

	case	ACCT_KERNEL_EOJ:
		return (convert_pacct_eoj (buff, 0));

	case	ACCT_KERNEL_CFG:
		return (convert_pacct_cfg (buff, 0));

	case	ACCT_DAEMON_WKMG:
		return (convert_pacct_wkmg (buff, 0));

	case	ACCT_JOB_HEADER:
		return (convert_pacct_job (buff, 0));

	default:
		acct_err(ACCT_CAUT,
		       _("%s: An error was returned from converting acct type %d."),
			FUNC_NAME, h->ah_type);
		return (0);
	}

}
#undef  FUNC_NAME

/*
 *  Determine if this pacct/spacct file has been converted
 *  Return:    -1 - if the file is not a pacct/spacct file.
 *		0 - if the file HAS NOT been converted.
 *		1 - if the file HAS been converted.
(*/
#define FUNC_NAME       "verify_pacct()"
int
verify_pacct(FILE *in)
{
	int	retval = 0;
	struct	achead	buf;
	int	type;
	
/*
 *	Read first record in order to check record header.
 */
	if (fread((char *)&buf, sizeof(struct achead), 1, in) != 1) {
		acct_err(ACCT_ABORT,
		       _("%s: An error was returned from the '%s' routine."),
			FUNC_NAME, "fread()");
	}

/*
 *	Check the record header.
 */
	type = buf.ah_type;
	/* Question - Should this check include the following types:
	   ACCT_KERNEL_SITE* and ACCT_DAEMON_SITE*? */
	if ((type != ACCT_KERNEL_CSA) ||
	    (type != ACCT_KERNEL_MEM) || (type != ACCT_KERNEL_IO)  ||
	    (type != ACCT_KERNEL_SOJ) || (type != ACCT_KERNEL_EOJ) ||
	    (type != ACCT_KERNEL_CFG) ||
	    (type != ACCT_DAEMON_WKMG) || (type != ACCT_JOB_HEADER)) {
		retval = -1;

	} else if ((buf.ah_magic == ACCT_MAGIC) &&
	    (buf.ah_revision == rev_tab[buf.ah_type])) {
		retval = 1;
	}

/*
 *	Rewind the file for conversion processing.
 */
	rewind(in);

	return(retval);
}
#undef  FUNC_NAME
