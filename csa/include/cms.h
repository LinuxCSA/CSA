/*
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

#ifndef	_CMS_H
#define	_CMS_H

struct acctinfo {
	int64_t		procCount;	  /* Number of processes */
	double		cpuTime;	  /* Cpu time (user+sys), in minutes */
	double		systemTime;	  /* System time, in minutes */
	double		realTime;	  /* Real time consumed, in minutes */
	double		kcoreTime;	  /* Kcore-minutes, in (kbytes * min) */
	double		kvirtualTime;	  /* Kvirtual-minutes, in (kbyptes * min) */
	double		charsRead;	  /* Characters read, in bytes */
	double		charsWritten;	  /* Characters written, in bytes */
	double		readSysCalls;	  /* Read system calls */
	double		writeSysCalls;	  /* Write system calls */
	double		highWaterCoreMem;  /* High water core memory, in kbytes */
	double		highWaterVirtMem;  /* High water virutal memory,in kbytes */
	double		majorFaultCount;  /* Number of major faults */
	double		minorFaultCount;  /* Number of minor faults */
	double		cpuDelayTime;	  /* CPU delay */
	double		blkDelayTime;	  /* Block I/O delay */
	double		swpDelayTime;	  /* Swap in delay */
	double		siteReserved1;	  /* Site reserved 1 */
	double		siteReserved2;	  /* Site reserved 2 */
};


struct cms {
	struct achead	cmsAcctHdr;	/* acct record header */
	char 		cmsCmdName[16];	/* name of command being billed for */
	struct acctinfo	prime;		/* prime time accounting info */
	struct acctinfo	nonprime;	/* non-prime-time accounting info */ 
};

#endif  /* !_CMS_H */
