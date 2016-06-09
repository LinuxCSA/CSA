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

#ifndef	_SBU_H
#define	_SBU_H

/*
 * sbu.h:
 *	Contains the definitions of all the SBU structures.
 *
 *	Contains the extern declarations for the variables
 *	and pointers used in the sbu calculations.  Definitions 
 *	are found in sbu.c.  This file must be updated when the
 *	definitions are changed.
 *
 *	Contains the constants used with the user SBU routines.
 */

/*
 *		Definitions of all the SBU structures.
 */

/*
 ***************************************************************************
 *      Workload Management sbus.
 ***************************************************************************
 */
struct wm_sbu_qtype {
	double  qsbu;           /* weighting factor */
	char    quename[16];    /* queue name */
};

struct wm_sbu_mtype {
	double  msbu;           /* weighting factor */
	char    machname[16];   /* originating machine name */
};


/*
 *			Declarations of all sbu variables
 */

/*
 ***************************************************************************
 *      Workload Management sbus.
 ***************************************************************************
 */
extern  int     __WKMG_NO_CHARGE;

extern  int     __WKMG_TERM_EXIT;
extern  int     __WKMG_TERM_REQUEUE;
extern  int     __WKMG_TERM_HOLD;
extern  int     __WKMG_TERM_RERUN;
extern  int     __WKMG_TERM_MIGRATE;

extern  int     __WKMG_NUM_MACHINES;    /* # machines with sbus specified */
extern  int     __WKMG_NUM_QUEUES;      /* # queues with sbus specified */

extern  struct wm_sbu_qtype     *wm_qsbu;       /* queue sbu array */
extern  struct wm_sbu_mtype     *wm_msbu;       /* originating machine array */


/*
 ***************************************************************************
 *	Pacct (kernel) sbus.
 ***************************************************************************
 *		Prime time weighting factors.
 */

extern double	P_BASIC;	/* Basic prime time weighting factor */

extern double	P_TIME;		/* General time weighting factor */
extern double	P_STIME;	/*   System CPU time weighting factor */
extern double	P_UTIME;	/*   User CPU time weighting factor */

extern double	P_MEM;		/* General memory weighting factor */
extern double	P_XMEM;		/*   CPU time core memory weighting factor */
extern double	P_VMEM;		/*   CPU time virtual mem weighting factor */

extern double	P_IO;		/* General I/O weighting factor */
extern double	P_BIO;		/*   I/O blocks xfer weighting factor */
extern double	P_CIO;		/*   I/O chars xfer weighting factor */
extern double	P_LIO;		/*   Logical I/O weighting factor */

extern double	P_DELAY;	/* General delay weighting factor */
extern double	P_CPUDELAY;	/*   CPU delay weighting factor */
extern double	P_BLKDELAY;	/*   Block I/O delay weighting factor */
extern double	P_SWPDELAY;	/*   Swap in delay weighting factor */

/*
 *		Non-prime time weighting factors.
 */
extern double	NP_BASIC;	/* Basic non-prime time weighting factor */

extern double	NP_TIME;	/* General time weighting factor */
extern double	NP_STIME;	/*   System CPU time weighting factor */
extern double	NP_UTIME;	/*   User CPU time weighting factor */
extern double	NP_BWTIME;	/*   Block I/O wait time weighting factor */
extern double	NP_RWTIME;	/*   Raw I/O wait time weighting factor */

extern double	NP_MEM;		/* General memory weighting factor */
extern double	NP_XMEM;	/*   CPU time core memory weighting factor */
extern double	NP_VMEM;	/*   CPU time virtual mem weighting factor */

extern double	NP_IO;		/* General I/O weighting factor */
extern double	NP_BIO;		/*   I/O blocks xfer weighting factor */
extern double	NP_CIO;		/*   I/O chars xfer weighting factor */
extern double	NP_LIO;		/*   Logical I/O weighting factor */

extern double	NP_DELAY;	/* General delay weighting factor */
extern double	NP_CPUDELAY;	/*   CPU delay weighting factor */
extern double	NP_BLKDELAY;	/*   Block I/O delay weighting factor */
extern double	NP_SWPDELAY;	/*   Swap in delay weighting factor */

/*
 *****************************************************************************
 *	End of sbu variable declarations.
 *****************************************************************************
 */

#define PACCT_CSA_NARG	 11	/* # args to __user_process_sbu() if csa base
				   rec */
#define	PACCT_MEM_NARG	 12	/* # args to __user_process_sbu() if memory
				   rec */
#define PACCT_IO_NARG	 9	/* # args to __user_process_sbu() if I/O rec */
#define PACCT_DELAY_NARG 10	/* # args to __user_process_sbu() if delay
				   rec */
#define PACCT_EOJ_NARG	 4	/* # args to __user_process_sbu() if eoj rec */
#define WKMG_NARG        12     /* # args to __user_wm_sbu() if wkmg rec */

#endif	/* _SBU_H */
