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
 * This file contains the revision table and the conversion routine to convert
 * an accounting header to the new format.
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
#include "cacct.h"
#include "convert.h"
#include "csa.h"

/*
 * The following table is used to map between a record type and the current
 * revision level of that record (indexed by ah_type field from achead).
 *
 * The list of record types indexed in this table must match with the list
 * in csa.h
 */
int	rev_tab[] = {
	-1,
	REV_CSA,				/* 0001 = ACCT_KERNEL_CSA */
	REV_MEM,				/* 0002 = ACCT_KERNEL_MEM */
	-1,
	REV_IO,					/* 0004 = ACCT_KERNEL_IO */
	-1, -1, -1,	 			/* 0005 - 0007 */
	REV_DELAY,				/* 0010 = ACCT_KERNEL_DELAY */
	-1,
	REV_SOJ,				/* 0012 = ACCT_KERNEL_SOJ */
	-1,
	REV_EOJ,				/* 0014 = ACCT_KERNEL_EOJ */
	-1, -1, -1, 				/* 0015 - 0017 */
	REV_CFG,				/* 0020 = ACCT_KERNEL_CFG */
	-1, -1, -1, -1, -1, -1, -1,		/* 0021 - 0027 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0030 - 0037 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0040 - 0047 */
	-1, -1, -1, -1, -1, -1, -1, -1,         /* 0050 - 0057 */
	-1, -1, -1, -1, -1, -1, -1, -1,         /* 0060 - 0067 */
	-1, -1, -1, -1, -1, -1, -1, -1,         /* 0070 - 0077 */
	-1, -1, -1, -1, -1, -1, -1, -1,         /* 0100 - 0107 = KERNEL_SITE */
	-1, -1, -1, -1, -1, -1, -1, -1,         /* 0110 - 0117 */
	-1, -1,					/* 0120 - 0121 */
	REV_WKMG,				/* 0122 = ACCT_DAEMON_WKMG */
	-1, -1, -1, -1, -1, -1,			/* 0123 - 0130 */
	-1, -1, -1, -1, -1, -1, -1,		/* 0131 - 0137 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0140 - 0147 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0150 - 0157 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0160 - 0167 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0170 - 0177 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0200 - 0207 = DAEMON_SITE */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* 0210 - 0217 */
	REV_JOB,				/* 0220 = ACCT_JOB_HEADER */
		-1, 
	REV_CACCT,				/* 0222 = ACCT_CACCT */
			-1,
	REV_CMS					/* 0224 = ACCT_CMS */
};


/*
 *  Convert header
 */
#define	FUNC_NAME	"convert_hdr()"
int
convert_hdr(struct achead *hdr, int type, int flag)
{
	hdr->ah_magic		= ACCT_MAGIC;

	switch(type) {

	case REC_CSA:
		hdr->ah_type     = ACCT_KERNEL_CSA;
		hdr->ah_size     = sizeof (struct acctcsa);
		break;

	case REC_MEM:
		hdr->ah_type     = ACCT_KERNEL_MEM;
		hdr->ah_size     = sizeof (struct acctmem);
		break;

	case REC_IO:
		hdr->ah_type     = ACCT_KERNEL_IO;
		hdr->ah_size     = sizeof (struct acctio);
		break;

	case REC_DELAY:
		hdr->ah_type     = ACCT_KERNEL_DELAY;
		hdr->ah_size     = sizeof (struct acctdelay);
		break;

	case REC_SOJ:
		hdr->ah_type     = ACCT_KERNEL_SOJ;
		hdr->ah_size     = sizeof (struct acctsoj);
		break;

	case REC_EOJ:
		hdr->ah_type     = ACCT_KERNEL_EOJ;
		hdr->ah_size     = sizeof (struct accteoj);
		break;

	case REC_CFG:
		hdr->ah_type     = ACCT_KERNEL_CFG;
		hdr->ah_size     = sizeof (struct acctcfg);
		break;

	case REC_WKMG:
		hdr->ah_type     = ACCT_DAEMON_WKMG;
		hdr->ah_size     = sizeof(struct wkmgmtbs);
		break;
		
	case REC_JOB:
		hdr->ah_type     = ACCT_JOB_HEADER;
		hdr->ah_size     = sizeof(struct acctjob);
		break;

	case REC_CACCT:
		hdr->ah_type     = ACCT_CACCT;
		hdr->ah_size	 = sizeof(struct cacct);
		break;

	case REC_CMS:
		hdr->ah_type     = ACCT_CMS;
		hdr->ah_size	 = sizeof(struct cms);
		break;

	default:
		acct_err(ACCT_CAUT,
		       _("%s: An unknown record type (%#o) was found."),
			FUNC_NAME, type);
		return(-1);
	}

	hdr->ah_revision = rev_tab[hdr->ah_type];
	
	return(0);
}
#undef	FUNC_NAME
