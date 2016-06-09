/*
 * Copyright (c) 2000-2004 Silicon Graphics, Inc and LANL  All Rights Reserved.
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdarg.h>
#include <errno.h>
#include "csaacct.h"

#include "sbu.h"
#include "acctmsg.h"
#include "csa.h"

#define DEVNM_SIZ	16	/* max # char in tape device names */

/*
 *	This file contains the user defined sbu routines.
 *	If a site wishes to use the distributed sbu routines, these 
 *	routines will return -1 and the sbus will be calculated using
 *	the SGI distributed routines.  If sites want to modify the
 *	sbu routines, make the necessary changes in this file.  The
 *	modified user supplied routines should return the sbu value 
 *	(which must not be -1).
 */

/*
 *	User defined NQS sbu routine.
 *
 *	By default, this routine returns -1.0, which indicates to the
 *	calling routine that the SGI distributed NQS sbu routine 
 *	should be used to calculate the NQS sbu value.
 *
 *	Note: subtype codes are defined in /usr/include/csaacct.h
 *
 *	The following configuration file parameters are available
 *	for use by this routine:
 *
 *	 Variable			Configuration parameter
 *	 ========			=======================
 *	 int __NQS_NUM_QUEUES		NQS_NUM_QUEUES
 *	 char nq_qsbu[i].quename[16]	NQS_QUEUEi.queue_name
 *	 double nq_qsbu[i].qsbu		NQS_QUEUEi.sbu
 *		where i = 0 to NQS_NUM_QUEUES
 *
 *	 int __NQS_NUM_MACHINES		NQS_NUM_MACHINES
 *	 char nq_msbu[i].machname[16]	NQS_MACHINEi.machine_name
 *	 double nq_msbu[i].msbu		NQS_MACHINEi.sbu
 *		where i = 0 to NQS_NUM_MACHINES
 *	
 *	The default sbu calculation is:
 *	  sbu = nq_qsbu[x].qsbu * nq_msbu[y].msbu
 *		where x is the index when nq_qsbu[x].quename == quename
 *		where y is the index when nq_msbu[y].machname == machname
 */
#define	FUNC_NAME	"__user_nqs_sbu()"
double
__user_nqs_sbu(const char *args, ...)
{
	char		machname[16];	/* originating machine name */
	char		reqname[16];	/* request name */
	char		quename[16];	/* NQS queue name */
	double		stime;		/* shepherd system cpu time (secs) */
	double		utime;		/* shepherd user cpu time (secs) */
	int		narg;		/* number of arguments passed */
	int		qtype;		/* queue type */
	int64_t		qwtime;		/* queue wait time (secs) */
	int64_t		code;		/* completion code */
	int64_t		mid;		/* originating machine id */
	short		init_subtype;	/* initialization subtype (NQ_INIT) */
	short		term_subtype;	/* termination subtype (NQ_TERM) */
	short		disp_subtype;	/* dispose subtype (NQ_DISP) */
	double		sbu = 1.0;
	struct	nqsbs	*rb;
	va_list		ap;

	/*
	 *	Use the SGI distributed sbu routine.
	 * SiTeChAnGe
	 *	Change the #ifndef to #ifdef if a site specific
	 *	NQS sbu calculation is to be performed.
	 */
#ifndef NEVER_DEFINED
	return(-1.0);
#else

	/*
	 *	The following code shows how to retrieve the passed
	 *	parameters.  The order of the retrieval must NOT
	 *	be changed!
	 */
	va_start(ap, args);
	narg = va_arg(ap, int);
	if (narg != NQS_NARG) {
		acct_err(ACCT_ABORT,
		       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
			FUNC_NAME, NQS_NARG, narg);
	}

	init_subtype = va_arg(ap, short);	/* NQ_INIT subtype */
	term_subtype = va_arg(ap, short);	/* NQ_TERM subtype */
	disp_subtype = va_arg(ap, short);	/* NQ_DISP subtype */
	mid = va_arg(ap, int64_t);		/* machine id */
	strncpy(machname, va_arg(ap, char *), 16); /* machine name */
	strncpy(reqname, va_arg(ap, char *), 16);  /* request name */
	strncpy(quename, va_arg(ap, char *), 16);  /* queue name */
	qtype = va_arg(ap, int);		/* queue type */
	qwtime = va_arg(ap, int64_t);		/* queue wait time (secs) */
	code = va_arg(ap, int64_t);		/* completion code */
	utime = va_arg(ap, double);		/* shep user cpu time (secs) */
	stime = va_arg(ap, double);		/* shep sys cpu time (secs) */
	
	va_end(ap);

	/*
	 * SiTeChAnGe
	 *	Add the site specific NQS sbu calculation here.
	 *	sbu = <site_sbu>
	 */
	return(sbu);
#endif     /* NEVER_DEFINED */
}
#undef	FUNC_NAME

/*
 *	User defined workload management sbu routine.
 *
 *	By default, this routine returns -1.0, which indicates to the
 *	calling routine that the SGI distributed workload management sbu 
 *	routine should be used to calculate the workload management sbu value.
 *
 *	Note: subtype codes are defined in /usr/include/csaacct.h
 *
 *	The following configuration file parameters are available
 *	for use by this routine:
 *
 *	 Variable			Configuration parameter
 *	 ========			=======================
 *	 int __WKMG_NUM_QUEUES		WKMG_NUM_QUEUES
 *	 char wm_qsbu[i].quename[16]	WKMG_QUEUEi.queue_name
 *	 double wm_qsbu[i].qsbu		WKMG_QUEUEi.sbu
 *		where i = 0 to (WKMG_NUM_QUEUES - 1)
 *
 *	 int __WKMG_NUM_MACHINES	WKMG_NUM_MACHINES
 *	 char wm_msbu[i].machname[16]	WKMG_MACHINEi.machine_name
 *	 double wm_msbu[i].msbu		WKMG_MACHINEi.sbu
 *		where i = 0 to (WKMG_NUM_MACHINES - 1)
 *	
 *	The default sbu calculation is:
 *	  sbu = wm_qsbu[x].qsbu * wm_msbu[y].msbu
 *		where x is the index when wm_qsbu[x].quename == quename
 *		where y is the index when wm_msbu[y].machname == machname
 */
#define	FUNC_NAME	"__user_wm_sbu()"
double
__user_wm_sbu(const char *args, ...)
{
	char		machname[16];	/* originating machine name */
	char		reqname[16];	/* request name */
	char		quename[16];	/* workload management queue name */
	char		serv_provider[16]; /* service provider */
	double		stime;		/* shepherd system cpu time (secs) */
	double		utime;		/* shepherd user cpu time (secs) */
	int		narg;		/* number of arguments passed */
	int		qtype;		/* queue type */
	int64_t		qwtime;		/* queue wait time (secs) */
	int64_t		code;		/* completion code */
	short		init_subtype;	/* initialization subtype (WM_INIT) */
	short		term_subtype;	/* termination subtype (WM_TERM) */
	double		sbu = 1.0;
	struct	wkmgmtbs *rb;
	va_list		ap;

	/*
	 *	Use the SGI distributed sbu routine.
	 * SiTeChAnGe
	 *	Change the #ifndef to #ifdef if a site specific
	 *	workload management sbu calculation is to be performed.
	 */
#ifndef NEVER_DEFINED
	return(-1.0);
#else

	/*
	 *	The following code shows how to retrieve the passed
	 *	parameters.  The order of the retrieval must NOT
	 *	be changed!
	 */
	va_start(ap, args);
	narg = va_arg(ap, int);
	if (narg != WKMG_NARG) {
		acct_err(LB_NUMARGS, ACCT_ABORT, FUNC_NAME, WKMG_NARG, narg);
	}

	strncpy(serv_provider, va_arg(ap, char *), 16); /* service provider */
	init_subtype = va_arg(ap, short);	/* WM_INIT subtype */
	term_subtype = va_arg(ap, short);	/* WM_TERM subtype */
	strncpy(machname, va_arg(ap, char *), 16); /* machine name */
	strncpy(reqname, va_arg(ap, char *), 16);  /* request name */
	strncpy(quename, va_arg(ap, char *), 16);  /* queue name */
	qtype = va_arg(ap, int);		/* queue type */
	qwtime = va_arg(ap, int64_t);		/* queue wait time (secs) */
	code = va_arg(ap, int64_t);		/* completion code */
	utime = va_arg(ap, double);		/* shep user cpu time (secs) */
	stime = va_arg(ap, double);		/* shep sys cpu time (secs) */
	
	va_end(ap);

	/*
	 * SiTeChAnGe
	 *	Add the site specific workload management sbu calculation
	 *	here.
	 *	sbu = <site_sbu>
	 */
	return(sbu);
#endif     /* NEVER_DEFINED */
}
#undef	FUNC_NAME

/*
 *	User defined process sbu routine.
 *
 *	By default, this routine returns -1.0, which indicates to the
 *	calling routine that the SGI distributed process sbu routine
 *	should be used to calculate the sbu value.
 *
 *	NOTE: If any portion of __user_process_sbu() does not return -1.0,
 *	then none of the default process sbu calculations will be used.
 *	This means that if you change any part of this routine, you
 *	must supply a sbu calculation for each type of record which
 *	contains information which is to be billed.  The record types are:
 *		1) base
 *		2) memory
 *		3) I/O
 *		4) multitasking	
 *			The MTTIME_WEIGHT values from the configuration
 *			file have already been factored into the user
 *			cpu time (utime in the BASE record) on all machines.
 *		5) MPP
 *		6) start-of-job
 *		7) end-of-job 
 *
 *	The following configuration file parameters are available
 *	for use by this routine:
 *
 *	 Variable			Configuration parameter
 *	 ========			=======================
 *	 double P_BASIC			P_BASIC
 *	 double P_TIME			P_TIME
 *	 double P_STIME			P_STIME
 *	 double P_UTIME			P_UTIME
 *	 double P_MEM			P_MEM
 *	 double P_XMEM			P_XMEM
 *	 double P_VMEM			P_VMEM
 *	 double P_IO			P_IO
 *	 double P_CIO			P_CIO
 *	 double P_LIO			P_LIO
 *	 double P_DELAY			P_DELAY
 *	 double P_CPUDELAY		P_CPUDELAY
 *	 double P_BLKDELAY		P_BLKDELAY
 *	 double P_SWPDELAY		P_SWPDELAY
 *
 *	 double NP_BASIC		NP_BASIC
 *	 double NP_TIME			NP_TIME
 *	 double NP_STIME		NP_STIME
 *	 double NP_UTIME		NP_UTIME
 *	 double NP_MEM			NP_MEM
 *	 double NP_XMEM			NP_XMEM
 *	 double NP_VMEM			NP_VMEM
 *	 double NP_IO			NP_IO
 *	 double NP_CIO			NP_CIO
 *	 double NP_LIO			NP_LIO
 *	 double NP_DELAY		NP_DELAY
 *	 double NP_CPUDELAY		NP_CPUDELAY
 *	 double NP_BLKDELAY		NP_BLKDELAY
 *	 double NP_SWPDELAY		NP_SWPDELAY
 *
 *
 *	 double P_MPP			P_MPP
 *	 double P_MPPPE			P_MPPPE
 *	 double P_MPPBESU		P_MPPBESU
 *	 double P_MPPTIME		P_MPPTIME
 *	 double NP_MPP			NP_MPP
 *	 double NP_MPPPE		NP_MPPPE
 *	 double NP_MPPBESU		NP_MPPBESU
 *	 double NP_MPPTIME		NP_MPPTIME
 *
 *	 double mttime_weight[i].cpu	MTTIME_WEIGHTi
 *		where i = 0 to sysconf(_SC_CRAY_NCPU) - 1
 *
 *	The default sbu calculation for the base record is:
 *	  prime_sbu =	(P_TIME *	(P_STIME * stime
 *					+ P_UTIME * utime))
 *	  prime_sbu *= p_ratio
 *
 *	  nonprime_sbu =(NP_TIME *	(NP_STIME * stime
 *					+ NP_UTIME * utime))
 *	  nonprime_sbu *= np_ratio
 *
 *	  sbu = (P_BASIC * prime_sbu) + (NP_BASIC * nonprime_sbu)
 *
 *	The default sbu calculation for memory record is:
 *	  prime_sbu =	(P_MEM *	(P_XMEM * coremem
 *					+ P_VMEM * virtmem))
 *	  prime_sbu *= p_ratio
 *
 *	  nonprime_sbu =(NP_MEM *	(NP_XMEM * coremem
 *					+ NP_VMEM * virtmem))
 *	  nonprime_sbu *= np_ratio
 *
 *	  sbu += (P_BASIC * prime_sbu) + (NP_BASIC * nonprime_sbu)
 *
 *	The default sbu calculation for I/O record is:
 *	  prime_sbu = (P_IO * (P_CIO * cio + P_LIO * lio))
 *	  prime_sbu *= p_ratio
 *
 *	  nonprime_sbu = (NP_IO * (NP_CIO * cio + NP_LIO * lio))
 *	  nonprime_sbu *= np_ratio
 *
 *	  sbu += (P_BASIC * prime_sbu) + (NP_BASIC * nonprime_sbu)
 *
 *	The default sbu calculation for delay record is:
 *	  prime_sbu = (P_DELAY *	(P_CPUDELAY * cpuDelay
 *					+ P_BLKDELAY * blkDelay
 *					+ P_SWPDELAY * swpDelay))
 *	  prime_sbu *= p_ratio
 *
 *	  nonprime_sbu = (NP_DELAY *	(NP_CPUDELAY * cpuDelay
 *					+ NP_BLKDELAY * blkDelay
 *					+ NP_SWPDELAY * swpDelay))
 *	  nonprime_sbu *= np_ratio
 *
 *	  sbu += (P_BASIC * prime_sbu) + (NP_BASIC * nonprime_sbu)
 *
 *      The default sbu calculation for MPP record is:
 *        prime_sbu = (P_MPP *		(P_MPPPE * mpptime * mpppe
 *                    			+ P_MPPBESU * mpptime * mppbesu
 *                    			+ P_MPPTIME * mpptime))
 *	  prime_sbu *= p_ratio
 *
 *        nonprime_sbu = (NP_MPP *	(NP_MPPPE * mpptime * mpppe
 *                       		+ NP_MPPBESU * mpptime * mppbesu
 *                       		+ NP_MPPTIME * mpptime)
 *	  nonprime_sbu *= np_ratio
 *
 *        sbu += (P_BASIC * prime_sbu) + (NP_BASIC * nonprime_sbu)
 *
 */
#define	FUNC_NAME	"__user_process_sbu()"
double
__user_process_sbu(const char *args, ...)
{
	double	sbu = 0.0;	/* site specific SBU value                   */
	double	p_ratio;	/* prime time ratio                          */
	double	np_ratio;	/* non-prime time ratio                      */

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
	double	cpuDelay;	/* Delay waiting for CPU		     */	
	double	blkDelay;	/* Delay waiting for block I/O		     */
	double  swpDelay;	/* Delay waiting for swap in		     */
	
	u_int64_t eoj_corehimem;/* Job high-water core memory (Kbyte)        */
	u_int64_t eoj_virthimem;/* Job high-water virt. memory (Kbyte)       */
	double	mtwtime;	/* multitasking semaphore wait (secs)        */
	int	mpppe;		/* MPP number of Processing Elements         */
	int	mppbesu;	/* MPP number of Barrier/Eureka units        */
	double	mpptime;	/* MPP elapsed time (seconds)                */

	int	narg;		/* # of arguments passed */
	int	rec_type;	/* record type */
	struct acctent	*acctrec;
	va_list		ap;

	/*
	 *	Use the SGI distributed sbu routine.
	 * SiTeChAnGe
	 *	Change the #ifndef to #ifdef if a site specific
	 *	process sbu calculation is to be performed.
	 */
#ifndef NEVER_DEFINED
	return(-1.0);
#else

	/*
	 *	The following code shows how to retrieve the passed
	 *	parameters.  The order of the retrieval must NOT
	 *	be changed!
	 */
	va_start(ap, args);
	rec_type = va_arg(ap, int);

	switch(rec_type) {
	case ACCT_KERNEL_CSA:	/* Base record */
		narg = va_arg(ap, int);
		if (narg != PACCT_CSA_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			    PACCT_CSA_NARG, narg);
		}

		utime = va_arg(ap, double);
		stime = va_arg(ap, double);
		ctime = va_arg(ap, double);
		etime = va_arg(ap, long);
		elaps[0] = va_arg(ap, long);
		elaps[1] = va_arg(ap, long);
		p_ratio = va_arg(ap, double);
		np_ratio = va_arg(ap, double);

		/*
		 * SiTeChAnGe
		 *	Add the site specific base process sbu calculation
		 *	here.
		 *
		 *	As an example, the following code sets the sbu to
		 *		P_UTIME * user_cpu_secs
		 *	P_UTIME is from the configuration file.
		 *	
		 *	sbu = P_UTIME * utime;
		 *
		 */
		break;

	case ACCT_KERNEL_MEM:	/* Memory record */
		narg = va_arg(ap, int);
		if (narg != PACCT_MEM_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			    PACCT_MEM_NARG, narg);
		}

		coremem = va_arg(ap, double);
		corehimem = va_arg(ap, u_int64_t);
		virtmem = va_arg(ap, double);
		virthimem = va_arg(ap, u_int64_t);
		swaps = va_arg(ap, u_int64_t);
		etime = va_arg(ap, long);
		elaps[0] = va_arg(ap, long);
		elaps[1] = va_arg(ap, long);
		p_ratio = va_arg(ap, double);
		np_ratio = va_arg(ap, double);

		/*
		 * SiTeChAnGe
		 *	Add the site specific memory sbu calculation here.
		 *
		 *	As an example, the following code sets the sbu to
		 *		P_XMEM * core_memory_integral
		 *	P_XMEM is from the configuration file.
		 *	
		 *	sbu = P_XMEM * coremem;
		 *
		 */
		break;

	case ACCT_KERNEL_IO:	/* I/O record */
		narg = va_arg(ap, int);
		if (narg != PACCT_IO_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			    PACCT_IO_NARG, narg);
		}

		cio = va_arg(ap, u_int64_t);
		lio = va_arg(ap, u_int64_t);
		etime = va_arg(ap, long);
		elaps[0] = va_arg(ap, long);
		elaps[1] = va_arg(ap, long);
		p_ratio = va_arg(ap, double);
		np_ratio = va_arg(ap, double);

		/*
		 * SiTeChAnGe
		 *	Add the site specific I/O sbu calculation here.
		 *
		 *	sbu = <site_sbu>
		 *
		 */
		break;

	case ACCT_KERNEL_DELAY:	/* Delay record */
		narg = va_arg(ap, int);
		if (narg != PACCT_DELAY_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			    PACCT_DELAY_NARG, narg);
		}

		cpuDelay = va_arg(ap, double);
		blkDelay = va_arg(ap, double);
		swpDelay = va_arg(ap, double);
		etime = va_arg(ap, long);
		elaps[0] = va_arg(ap, long);
		elaps[1] = va_arg(ap, long);
		p_ratio = va_arg(ap, double);
		np_ratio = va_arg(ap, double);

		/*
		 * SiTeChAnGe
		 *	Add the site specific I/O sbu calculation here.
		 *
		 *	sbu = <site_sbu>
		 *
		 */
		break;

	case ACCT_KERNEL_MPP:	/* MPP record */
		narg = va_arg(ap, int);
		if (narg != PACCT_MPP_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			    PACCT_MPP_NARG, narg);
		}

		mpppe   = va_arg(ap, int);	/* MPP # PE's */
		mppbesu = va_arg(ap, int);	/* MPP Barier/Eureka Units */
		mpptime = va_arg(ap, double);	/* MPP elapsed time (sec) */

		/*
		 * SiTeChAnGe
		 *	Add the site specific MPP sbu calculation here.
		 *
		 *	As an example, the following code sets the sbu
		 *	to P_MPPTIME * mpptime.
		 *
		 *	sbu = P_MPPTIME * mpptime;
		 */
		break;

	case ACCT_KERNEL_MT:	/* multitasking record */
		/*
		 *	Note: The MTTIME_WEIGHT values from the configuration
		 *	file have already been factored into the user cpu
		 *	time (utime in the BASE record) via the ucputime()
		 *	accounting library routine.
		 */
		narg = va_arg(ap, int);
		if (narg != PACCT_MT_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			    PACCT_MT_NARG, narg);
		}

		mtwtime = va_arg(ap, double);	/* semaphore wait (secs) */

		/*
		 * SiTeChAnGe
		 *	Add the site specific multitasking sbu calculation
		 *	here.
		 *	sbu = <site_sbu>
		 */
		break;

	case ACCT_KERNEL_EOJ:	/* end-of-job record */
		narg = va_arg(ap, int);
		if (narg != PACCT_EOJ_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			    PACCT_EOJ_NARG, narg);
		}

		eoj_corehimem = va_arg(ap, u_int64_t);
		eoj_virthimem = va_arg(ap, u_int64_t);

		/*
		 * SiTeChAnGe
		 *	Add the site specific end-of-job sbu calculation here.
		 *	sbu = <site_sbu>
		 */
		break;

	case ACCT_KERNEL_SOJ:	/* start-of-job record */
		/*
		 * deferred
		 */
		break;

	default:
		acct_err(ACCT_WARN,
		       _("%s: An invalid record type (%d) was found.\n   Returning an SBU of %f."),
			FUNC_NAME, rec_type, sbu);
		break;
	}  /* switch */

	va_end(ap);
	return(sbu);
#endif     /* NEVER_DEFINED */
}
#undef	FUNC_NAME


/*
 *	User defined tape sbu routine.
 *
 *	By default, this routine returns -1.0, which indicates to the
 *	calling routine that the SGI distributed tape sbu routine 
 *	should be used to calculate the tape sbu value.
 *
 *	Note: Generally, the only valid device group names are
 *	TAPE and CART, unless your site has configured these differently.
 *
 *	The following configuration file parameters are available
 *	for use by this routine:
 *	 Variable			Configuration parameter
 *	 ========			=======================
 *	 char tp_sbu[i].tp_devgrp[16]	TAPE_SBUi.device_group 
 *	 double tp_sbu[i].tp_mount	TAPE_SBUi.mount
 *	 double tp_sbu[i].tp_reserv	TAPE_SBUi.reserve
 *	 double tp_sbu[i].tp_read	TAPE_SBUi.read
 *	 double tp_sbu[i].tp_write	TAPE_SBUi.write
 *		where i = 0 to (TP_MAXDEVGRPS - 1)
 *
 *	The default sbu calculation is:
 *	  if (rec_type == TAPE_BASE)
 *		sbu =	(tp_sbu[i].tp_read * nread)
 *			+ (tp_sbu[i].tp_write * nwrite)
 *			+ tp_sbu[i].tp_mount
 *	  if (rec_type == TAPE_RSV)
 *		sbu = tp_sbu[i].tp_reserv * rtime
 *	  where i is the index when tp_sbu[i].tp_devgrp == devgrp
 *
 */
#define	FUNC_NAME	"__user_tape_sbu()"
double
__user_tape_sbu(const char *args, ...)
{
	short		rec_type;	/* record type (base or rsv) */
	char		devgrp[DEVNM_SIZ];  /* device group name */
	char		devname[DEVNM_SIZ]; /* device name */
	double		stime;		/* system cpu time (secs) */
	double		utime;		/* user cpu time (secs) */
	int		narg;		/* # of arguments passed */
	int64_t		nread;		/* # bytes read */
	int64_t		nwrite;		/* # bytes written */
	int		status;		/* status */
	double		rtime;		/* reservation time (secs) */
	double		sbu = 0.0;
	va_list		ap;

	/*
	 *	Use the SGI distributed sbu routine.
	 * SiTeChAnGe
	 *	Change the #ifndef to #ifdef if a site specific
	 *	tape sbu calculation is to be performed.
	 */
#ifndef NEVER_DEFINED
	return(-1.0);
#else

	/*
	 *	The following code shows how to retrieve the passed
	 *	parameters.  The order of the retrieval must NOT
	 *	be changed!
	 */
	va_start(ap, args);

	rec_type = va_arg(ap, short);
	narg = va_arg(ap, int);
	switch(rec_type) {
	case TAPE_BASE:
		if (narg != TAPE_BASE_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			   TAPE_BASE_NARG, narg);
		}
		strncpy(devgrp, va_arg(ap, char *), DEVNM_SIZ);	/* dev group */
		strncpy(devname, va_arg(ap, char *), DEVNM_SIZ);/* dev name */
		stime = va_arg(ap, double);	/* system cpu time (secs) */
		utime = va_arg(ap, double);	/* user cpu time (secs) */
		status = va_arg(ap, int);	/* status */
		nread = va_arg(ap, int64_t);	/* # bytes read */
		nwrite = va_arg(ap, int64_t);	/* # bytes written */

		/*
		 * SiTeChAnGe
		 *	Add the site specific base tape sbu calculation here.
		 *	sbu = <site_sbu>
		 */
		break;

	case TAPE_RSV:
		if (narg != TAPE_RSV_NARG) {
			acct_err(ACCT_ABORT,
			       _("%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."),
				FUNC_NAME,
			   TAPE_RSV_NARG, narg);
		}
		strncpy(devgrp, va_arg(ap, char *), DEVNM_SIZ);	/* dev group */
		strncpy(devname, va_arg(ap, char *), DEVNM_SIZ); /* dev name */
		rtime = va_arg(ap, double);	/* reservation time (secs) */

		/*
		 * SiTeChAnGe
		 *	Add the site specific tape reservation time sbu 
		 *	calculation here.
		 *	sbu = <site_sbu>
		 */
		break;

	default:
		acct_err(ACCT_WARN,
		       _("%s: An invalid record type (%d) was found.\n   Returning an SBU of %f."),
			"user_tape_sbu()",
		    rec_type, 0.0);
		break;
	}

	va_end(ap);
	return(sbu);
#endif     /* NEVER_DEFINED */
}
#undef	FUNC_NAME
