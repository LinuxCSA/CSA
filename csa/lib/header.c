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
#include <sys/types.h>
#include <sys/param.h>
#include "csaacct.h"

#include "acctdef.h"
#include "acctmsg.h"
#include "cacct.h"
#include "cms.h"
#include "convert.h"
#include "csa.h"

/*
 *  Create the general accounting header.
 *
 *  Return 0 if successful, -1 if not. 
 */
int
#define	FUNC_NAME	"create_hdr1()"
create_hdr1(struct achead *hdr, int type)
{
	hdr->ah_magic = ACCT_MAGIC;
	hdr->ah_type = type;
	hdr->ah_flag  = 0;

	switch(type) {

	case ACCT_KERNEL_CSA:
		hdr->ah_size     = sizeof (struct acctcsa);
		break;

	case ACCT_KERNEL_MEM:
		hdr->ah_size     = sizeof (struct acctmem);
		break;

	case ACCT_KERNEL_IO:
		hdr->ah_size     = sizeof (struct acctio);
		break;

	case ACCT_KERNEL_SOJ:
		hdr->ah_size     = sizeof (struct acctsoj);
		break;

	case ACCT_KERNEL_EOJ:
		hdr->ah_size     = sizeof (struct accteoj);
		break;

	case ACCT_KERNEL_CFG:
		hdr->ah_size     = sizeof (struct acctcfg);
		break;

	case ACCT_DAEMON_WKMG:
		hdr->ah_size	 = sizeof(struct wkmgmtbs);
		break;
		
	case ACCT_JOB_HEADER:
		hdr->ah_size	 = sizeof(struct acctjob);
		break;

	case ACCT_CACCT:
		hdr->ah_size	 = sizeof(struct cacct);
		break;

	case ACCT_CMS:
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
