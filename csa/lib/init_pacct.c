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
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>

#include "acctdef.h"
#include "sbu.h"
#include "acctmsg.h"

#define	FUNC_NAME	"init_pacct_sbu()"	/* library function name */

/*
 *	Initialize the pacct SBU variables.  This includes the prime
 *	and non-prime time weighting factors.
 */
void
init_pacct_sbu()
{
	char		*resp;		/* return value from config() */

	/*
	 *	Get the prime time weighting factors.
	 */
	P_BASIC = init_real(SBU_P_BASIC, 0.0, TRUE);

	P_TIME = init_real(SBU_P_TIME, 0.0, TRUE);
	P_STIME = init_real(SBU_P_STIME, 0.0, TRUE);
	P_UTIME = init_real(SBU_P_UTIME, 0.0, TRUE);

	P_MEM = init_real(SBU_P_MEM, 0.0, TRUE);
	P_XMEM = init_real(SBU_P_XMEM, 0.0, TRUE);
	P_VMEM = init_real(SBU_P_VMEM, 0.0, TRUE);

	P_IO = init_real(SBU_P_IO, 0.0, TRUE);
	P_CIO = init_real(SBU_P_CIO, 0.0, TRUE);
	P_LIO = init_real(SBU_P_LIO, 0.0, TRUE);

	P_DELAY = init_real(SBU_P_DELAY, 0.0, TRUE);
	P_CPUDELAY = init_real(SBU_P_CPUDELAY, 0.0, TRUE);
	P_BLKDELAY = init_real(SBU_P_BLKDELAY, 0.0, TRUE);
	P_SWPDELAY = init_real(SBU_P_SWPDELAY, 0.0, TRUE);

	/*
	 *	Get the non-prime time weighting factors.
	 */
	NP_BASIC = init_real(SBU_NP_BASIC, 0.0, TRUE);

	NP_TIME = init_real(SBU_NP_TIME, 0.0, TRUE);
	NP_STIME = init_real(SBU_NP_STIME, 0.0, TRUE);
	NP_UTIME = init_real(SBU_NP_UTIME, 0.0, TRUE);

	NP_MEM = init_real(SBU_NP_MEM, 0.0, TRUE);
	NP_XMEM = init_real(SBU_NP_XMEM, 0.0, TRUE);
	NP_VMEM = init_real(SBU_NP_VMEM, 0.0, TRUE);

	NP_IO = init_real(SBU_NP_IO, 0.0, TRUE);
	NP_CIO = init_real(SBU_NP_CIO, 0.0, TRUE);
	NP_LIO = init_real(SBU_NP_LIO, 0.0, TRUE);

	NP_DELAY = init_real(SBU_NP_DELAY, 0.0, TRUE);
	NP_CPUDELAY = init_real(SBU_NP_CPUDELAY, 0.0, TRUE);
	NP_BLKDELAY = init_real(SBU_NP_BLKDELAY, 0.0, TRUE);
	NP_SWPDELAY = init_real(SBU_NP_SWPDELAY, 0.0, TRUE);


	return;
}
#undef	FUNC_NAME
