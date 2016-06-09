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
 *	Calculate system billing info for the various types of
 *	accounting records.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include "csaacct.h"

#include "acctdef.h"
#include "sbu.h"
#include "acctmsg.h"

extern	int	MEMINT;

/*
 *	Variable and pointer definitions used in the SBU calculations.
 *	Extern declarations are done in sbu.h, which must be kept up-to-date.
 */

/*
 ***************************************************************************
 *	Multitasking CPU time SBUs.
 ***************************************************************************
 */
struct mt_weights	*mttime_weight;

/*
 ***************************************************************************
 *	NQS SBUs.
 ***************************************************************************
 */
int	__NQS_NO_CHARGE;
int	__NQS_TERM_EXIT;
int	__NQS_TERM_REQUEUE;
int	__NQS_TERM_PREEMPT;
int	__NQS_TERM_HOLD;
int	__NQS_TERM_OPRERUN;
int	__NQS_TERM_RERUN;
int	__NQS_NUM_MACHINES;	/* # machines for which SBUs are specified */
int	__NQS_NUM_QUEUES;	/* # queues for which SBUs are specified */

struct nq_sbu_qtype	*nq_qsbu;	/* queue sbu array */
struct nq_sbu_mtype	*nq_msbu;	/* originating machine SBU array */

/*
 ***************************************************************************
 *      Workload Management SBUs.
 ***************************************************************************
 */
int     __WKMG_NO_CHARGE;
int     __WKMG_TERM_EXIT;
int     __WKMG_TERM_REQUEUE;
int     __WKMG_TERM_HOLD;
int     __WKMG_TERM_RERUN;
int     __WKMG_TERM_MIGRATE;
int     __WKMG_NUM_MACHINES;    /* # machines for which SBUs are specified */
int     __WKMG_NUM_QUEUES;      /* # queues for which SBUs are specified */

struct wm_sbu_qtype     *wm_qsbu;       /* queue SBU array */
struct wm_sbu_mtype     *wm_msbu;       /* originating machine SBU array */


/*
 ***************************************************************************
 *	Process (kernel) SBUs.
 ***************************************************************************
 *		Prime time weighting factors.
 */
double	P_BASIC;	/* Basic prime time weighting factor */

double	P_TIME;		/* General time weighting factor */
double	P_STIME;	/*   System CPU time weighting factor */
double	P_UTIME;	/*   User CPU time weighting factor */
double	P_QTIME;	/*   Run queue wait time weighting factor */

double	P_MEM;		/* General memory weighting factor */
double	P_XMEM;		/*   CPU time core memory weighting factor */
double	P_VMEM;		/*   CPU time virtual memory weighting factor */

double	P_IO;		/* General I/O weighting factor */
double	P_CIO;		/*   I/O chars xfer weighting factor */
double	P_LIO;		/*   Logical I/O weighting factor */

double	P_DELAY;	/* General delay weighting factor */
double	P_CPUDELAY;	/*   CPU delay weighting factor */
double	P_BLKDELAY;	/*   Block I/O delay weighting factor */
double	P_SWPDELAY;	/*   Swap in delay weighting factor */

double	P_MPP;		/* MPP weighting factor */
double	P_MPPPE;	/*   MPP PE's used weighting factor */
double	P_MPPBESU;	/*   MPP Barrier/Eureka weighting factor */
double	P_MPPTIME;	/*   MPP Time weighting factor */

/*
 *		Non-prime time weighting factors.
 */
double	NP_BASIC;	/* Basic non-prime time weighting factor */

double	NP_TIME;	/* General time weighting factor */
double	NP_STIME;	/*   System CPU time weighting factor */
double	NP_UTIME;	/*   User CPU time weighting factor */
double	NP_QTIME;	/*   Run queue wait time weighting factor */

double	NP_MEM;		/* General memory weighting factor */
double	NP_XMEM;	/*   CPU time core memory weighting factor */
double	NP_VMEM;	/*   CPU time virtual memory weighting factor */

double	NP_IO;		/* General I/O weighting factor */
double	NP_CIO;		/*   I/O chars xfer weighting factor */
double	NP_LIO;		/*   Logical I/O weighting factor */

double	NP_DELAY;	/* General delay weighting factor */
double	NP_CPUDELAY;	/*   CPU delay weighting factor */
double	NP_BLKDELAY;	/*   Block I/O delay weighting factor */
double	NP_SWPDELAY;	/*   Swap in delay weighting factor */

double	NP_MPP;		/* MPP weighting factor */
double	NP_MPPPE;	/*   MPP PE's used weighting factor */
double	NP_MPPBESU;	/*   MPP Barrier/Eureka weighting factor */
double	NP_MPPTIME;	/*   MPP Time weighting factor */

/*
 ***************************************************************************
 *	Tape SBUs.
 ***************************************************************************
 */
struct tp_sbu_type	*tp_sbu;	/* tape SBU array */

/*
 *****************************************************************************
 *	End of SBU variable definitions.
 *****************************************************************************
 */

extern double    __user_process_sbu(int, ...);
extern double	 __user_wm_sbu(int, ...);

/*
 *      Workload management SBU calculation
 *		If there is no queue and machine name match, an
 *		SBU multiplier of 1.0 is returned.  A value of
 *		1.0 means that this portion of a workload
 *		management job will be billed normally.
 */
#define	FUNC_NAME	"wm_sbu()"		/* library function name */
double
wm_sbu(struct wkmgmtbs *rb)
{
	double	sbu;
	int	ind;
	static	int	user_def = 1;	/* 0 = no user defined SBU routine */
					/* 1 = user defined SBU routine */

	/*
	 *	See if there is a user defined SBU routine.
	 */
	if (user_def == 1) {
		sbu = __user_wm_sbu(
			WKMG_NARG,		/* # arguments passed */
			rb->serv_provider,	/* service provider */
			rb->init_subtype,	/* initialization subtype */
			rb->term_subtype,	/* termination subtype */
			rb->machname,		/* originating machine name */
			rb->reqname,		/* request name */
			rb->quename,		/* queue name */
			rb->qtype,		/* queue type */
			rb->qwtime,		/* queue wait time (secs) */
			rb->code,		/* completion code */
			TIME_2_SECS(rb->utime), /* shepherd user cpu time */
						/*  (secs) */
			TIME_2_SECS(rb->stime)); /* shepherd system cpu time */
						/*  (secs) */
		if (sbu != -1.0) {
			return(sbu);
		} else {
			user_def = 0;
		}
	}

	/*
	 *	Use the distributed SBU calculation.
	 */
	for (ind = 0, sbu = 1.0; ind < __WKMG_NUM_QUEUES; ind++) {
		if (!strcmp(rb->quename, wm_qsbu[ind].quename)) {
			sbu *= wm_qsbu[ind].qsbu;
			break;
		}
	}

	for (ind = 0; ind < __WKMG_NUM_MACHINES; ind++) {
		if (!strcmp(rb->machname, wm_msbu[ind].machname)) {
			sbu *= wm_msbu[ind].msbu;
			break;
		}
	}

	return(sbu);
}
#undef	FUNC_NAME

#define	FUNC_NAME	"process_sbu()"		/* library function name */
double
process_sbu(struct acctent *acctrec)
{
	double tmpdbl;
	double	bu = 0.0;	/* total billing units                       */
	double	sbu = 0.0;	/* site specific SBU value                   */

	double	p_ratio;	/* prime time ratio                          */
	double	np_ratio;	/* non-prime time ratio                      */
	double	npbu;		/* non-prime time billing units              */
	double	pbu;		/* prime time billing units                  */

	long	etime;		/* process elapsed time (seconds)            */
	long	elaps[2];	/* prime & non-prime elapsed times (seconds) */
	double	ctime;		/* process connect time (seconds)            */
	double	utime;		/* user cpu time (seconds)                   */
	double	stime;		/* system cpu time (seconds)                 */
	double	coremem;	/* Core memory integral (Mbyte-min)          */
	double	virtmem;	/* Virtual memory integral (Mbyte-min)       */
	u_int64_t  corehimem;	/* High-water core memory (Kbyte)            */
	u_int64_t  virthimem;	/* High-water virtual memory (Kbyte)         */
	u_int64_t  swaps;	/* # of times swapped                        */
	u_int64_t  cio;		/* # of bytes transferred                    */
	u_int64_t  lio;		/* # of logical I/O requests                 */

	double	cpuDelay;	/* CPU delay (seconds)                       */
	double	blkDelay;	/* Block I/O delay (seconds)                 */
	double	swpDelay;	/* Swap in delay (seconds)                   */

	int	mpppe;		/* MPP number of Processing Elements used    */
	int	mppbesu;	/* MPP Barrier/Eureka Sync. Units used       */
	double	mpptime;	/* MPP elapsed time (seconds)                */
	
	static int	user_def = 1;	/* 0 = no user defined SBU routine */
					/* 1 = user defined SBU routine */


	/*
	 * Calculate SBU for the base level accounting record
	 */
	if (acctrec->csa) {

		/*
		 * Determine prime time and non-prime time ratios.
		 */
		tmpdbl = TIME_2_SECS(acctrec->csa->ac_etime);
		etime = (long)tmpdbl;
		if (etime == 0) {
			etime = 1;
		}
		pop_calc(acctrec->csa->ac_btime, etime, elaps);
		p_ratio = (double)elaps[0] / (double)etime;
		np_ratio = 1.0 - p_ratio;

		utime = TIME_2_SECS(ucputime(acctrec));
		stime = TIME_2_SECS(acctrec->csa->ac_stime);

		/*
		 * Extract other miscellaneous fields that the user defined
		 * SBU routine might use.
		 */

		/* ac_ctime is currently not used.
		 * csa->ac_ctime is unimplemented
		 * ctime = TIME_2_SECS(acctrec->csa->ac_ctime);
		 */
		ctime = 0;

		/*
		 * See if there is a user defined SBU routine.
		 */
		if (user_def == 1) {
			sbu = __user_process_sbu(
				ACCT_KERNEL_CSA,  /* base record */
				PACCT_CSA_NARG,   /* # arguments passed */
				
				/* Data fields; units see above. */
				utime, stime,  ctime, etime, elaps[0],
				elaps[1], p_ratio, np_ratio);

			if (sbu != -1.0) {
				bu += sbu;
			} else {
				user_def = 0;
			}
		}

		if (user_def == 0) {
			/*
			 * Use the distributed SBU calculation.
			 */
			/*
			 * Calculate the prime time charge.
			 */
			pbu = P_TIME * (  P_STIME * stime 
					+ P_UTIME * utime); 
			pbu *= p_ratio;

			/*
			 * Calculate the non-prime time charge.
			 */
			npbu = NP_TIME * (  NP_STIME * stime 
					  + NP_UTIME * utime);
			npbu *= np_ratio;

			/*
			 * Combine the prime and non-prime time charges.
			 */
			bu += P_BASIC * pbu + NP_BASIC * npbu;
		}
	}

	/*
	 *	Calculate SBUs for Memory records.
	 */
	if (acctrec->mem != NULL) {

		/*
		 * Convert memory integrals to 'Mbyte-minutes'
		 */
		switch(MEMINT) {

		case 1:
			coremem = TIME_2_MINS(acctrec->mem->ac_core.mem1);
			virtmem = TIME_2_MINS(acctrec->mem->ac_virt.mem1);
			break;

		case 2:
			coremem = TIME_2_MINS(acctrec->mem->ac_core.mem2);
			virtmem = TIME_2_MINS(acctrec->mem->ac_virt.mem2);
			break;

		case 3:
			coremem = TIME_2_MINS(acctrec->mem->ac_core.mem3);
			virtmem = TIME_2_MINS(acctrec->mem->ac_virt.mem3);
			break;

		default:
			acct_err(ACCT_ABORT,
			       _("An invalid value for MEMINT (%d) was found in the configuration file.\n  MEMINT must be between 1 and 3."),
				MEMINT);
		}
		corehimem = acctrec->mem->ac_core.himem;
		virthimem = acctrec->mem->ac_virt.himem;
		swaps = acctrec->mem->ac_pgswap;

		if (user_def == 1) {
			sbu = __user_process_sbu(
				ACCT_KERNEL_MEM,  /* memory record */
				PACCT_MEM_NARG,	  /* # arguments passed */
				
				/* Data fields; units see above. */
				coremem, corehimem, virtmem, virthimem,
				swaps, etime, elaps[0], elaps[1], p_ratio,
				np_ratio);
			if (sbu != -1.0) {
				bu += sbu;
			} else {
				user_def = 0;
			}
		}

		if (user_def == 0) {
			/*
			 * Use the distributed SBU calculation.
			 */
			/*
			 * Calculate the prime time charge.
			 */
			pbu = P_MEM * (P_XMEM * coremem + P_VMEM * virtmem);
			pbu *= p_ratio;

			/*
			 * Calculate the non-prime time charge.
			 */
			npbu = NP_MEM * (  NP_XMEM * coremem
					 + NP_VMEM * virtmem);
			npbu *= np_ratio;

			/*
			 * Combine the prime and non-prime time charges.
			 */
			bu += P_BASIC * pbu + NP_BASIC * npbu;
		}
	}

	/*
	 *	Calculate SBUs for I/O records.
	 */
	if (acctrec->io != NULL) {
		cio = acctrec->io->ac_chr + acctrec->io->ac_chw;
		lio = acctrec->io->ac_scr + acctrec->io->ac_scw;
		/*
		 * See if there is a user defined SBU routine.
		 */
		if (user_def == 1) {
			sbu = __user_process_sbu(
			  ACCT_KERNEL_IO,	/* I/O record */
			  PACCT_IO_NARG, 	/* # arguments passed */

			  /*
			   * Data fields; units see above
			   */
			  cio, lio, etime, elaps[0],
			  elaps[1], p_ratio, np_ratio);

			if (sbu != -1.0) {
				bu += sbu;
			} else {
				user_def = 0;
			}
		}

		if (user_def == 0) {
			/*
			 * Use the distributed SBU calculation.
			 */
			/*
			 * Calculate the prime time charge.
			 */
			pbu = P_IO * (  P_CIO * cio
				      + P_LIO * lio);
			pbu *= p_ratio;

			/*
			 * Calculate the non-prime time charge.
			 */
			npbu = NP_IO * (  NP_CIO * cio
					+ NP_LIO * lio);
			npbu *= np_ratio;

			/*
			 * Combine the prime and non-prime time charges.
			 */
			bu += P_BASIC * pbu + NP_BASIC * npbu;
		}
	}
	
	/*
	 *	Calculate SBUs for delay records.
	 */
	if (acctrec->delay != NULL) {
		cpuDelay = NSEC_2_SECS(acctrec->delay->ac_cpu_delay_total);
		blkDelay = NSEC_2_SECS(acctrec->delay->ac_blkio_delay_total);
		swpDelay = NSEC_2_SECS(acctrec->delay->ac_swapin_delay_total);
		/*
		 * See if there is a user defined SBU routine.
		 */
		if (user_def == 1) {
			sbu = __user_process_sbu(
			  ACCT_KERNEL_DELAY,	/* delay record */
			  PACCT_DELAY_NARG, 	/* # arguments passed */

			  /*
			   * Data fields; units see above
			   */
			  cpuDelay, blkDelay, swpDelay, etime, elaps[0],
			  elaps[1], p_ratio, np_ratio);

			if (sbu != -1.0) {
				bu += sbu;
			} else {
				user_def = 0;
			}
		}

		if (user_def == 0) {
			/*
			 * Use the distributed SBU calculation.
			 */
			/*
			 * Calculate the prime time charge.
			 */
			pbu = P_DELAY * (  P_CPUDELAY * cpuDelay
					 + P_BLKDELAY * blkDelay
					 + P_SWPDELAY * swpDelay);
			pbu *= p_ratio;

			/*
			 * Calculate the non-prime time charge.
			 */
			npbu = NP_DELAY * (  NP_CPUDELAY * cpuDelay
					   + NP_BLKDELAY * blkDelay
					   + NP_SWPDELAY * swpDelay);
			npbu *= np_ratio;

			/*
			 * Combine the prime and non-prime time charges.
			 */
			bu += P_BASIC * pbu + NP_BASIC * npbu;
		}
	}
	
	/*
	 *	Pass eoj values to the user SBU routine in case anyone
	 *	wants to bill on these.  If we haven't seen a csa base record
	 *	yet, correctly set user_def.
	 */
	if (acctrec->eoj != NULL) {
		if (user_def == 1) {
			sbu = __user_process_sbu(
				ACCT_KERNEL_EOJ,  /* end of job record */
				PACCT_EOJ_NARG,	  /* # arguments passed */
				/* High-water core memory (Kbyte) */
				acctrec->eoj->ac_corehimem,
				/* High-water virtual memory (Kbyte) */
				acctrec->eoj->ac_virthimem);
			if (sbu != -1.0) {
				bu += sbu;
			} else {
				user_def = 0;
			}
		}
	}

	return(bu);
}
#undef	FUNC_NAME
