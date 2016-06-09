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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/times.h>
#include <sys/utsname.h>

#include <grp.h>
#include <malloc.h>
#include <math.h>
#include <memory.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csa.h"
#include "csaacct.h"
#include "csaja.h"
#include "extern_ja.h"

#ifndef	MIN
#define	MIN(A,B)	((A) < (B) ? (A) : (B))
#endif

#ifndef	MAX
#define	MAX(A,B)	((A) > (B) ? (A) : (B))
#endif

#define DATE_FMT_JA	" %m/%d/%y %H:%M:%S"

#define TIMEBUF_MAX	50

/*	Memory counters */
extern	uint64_t max_corehimem; /* maximum core memory used (Mbytes) */
extern	uint64_t max_cmdchimem; /* maximum CMD core memory used (Mbytes) */

extern	uint64_t max_virthimem; /* maximum virtual memory used (Mbytes) */
extern	uint64_t max_cmdvhimem; /* maximum CMD virtual memory used (Mbytes) */

extern	double	t_coremem; /* total core memory integral [Mbytes*secs] */
extern	double	t_cmdcmem; /* total CMD core memory integral [Mbytes*secs] */
extern	double	tb_mem;    /* total memory integral [Mbytes*secs] */

extern	double	t_virtmem; /* total virtual memory integral [Mbytes*secs] */
extern	double	t_cmdvmem; /* total CMD virtual memory integral [Mbytes*secs] */

extern	int	t_cmdswaps;	/* total # of CMD times swapped */

extern	int	t_pgswaps;	/* total # of pages swapped */
extern	int	t_cmdpgswaps;	/* total # of CMD pages swapped */

extern  int	t_minflt;	/* total # of minor page faults */

extern  int	t_majflt;	/* total # of major page faults */

/*	Time counters */
extern	double	t_utime;	/* total user CPU time (seconds) */
extern	double	t_cmdutime;	/* total CMD user CPU time (secs) */

extern	double	t_wutime;	/* total weighted user CPU time (seconds) */
extern	double	t_cmdwutime;	/* total weighted user CPU time (seconds) */

extern	double	t_stime;	/* total system CPU time [seconds] */
extern	double	t_cmdstime;	/* total CMD system CPU time [secs] */

/*	I/O counters */
extern	int64_t	t_chr;		/* total # of characters read (Mbytes) */
extern	int64_t	t_cmdchr;	/* total # of CMD characters read (Mbytes) */

extern	int64_t	t_chw;		/* total # of characters written (Mbytes) */
extern	int64_t	t_cmdchw; 	/* total # of CMD characters written (Mbytes) */

extern	int64_t	t_scr;		/* total # of logical read I/O requests */
extern	int64_t	t_cmdscr; 	/* total # of CMD logical read I/O requests */

extern	int64_t t_scw;		/* total # of logical write I/O requests */
extern	int64_t	t_cmdscw;	/* total # of CMD logical write I/O requests */

/*	Delay counters */
extern	uint64_t t_cpuCount;	/* total # of CPU delay values recorded */
extern	double	 t_cpuDelay;	/* total CPU delay [seconds] */

extern	uint64_t t_blkCount;	/* total # of block I/O delay values recorded */
extern	double	 t_blkDelay;	/* total block I/O delay [seconds] */

extern	uint64_t t_swpCount;	/* total # of swap in delay values recorded */
extern	double	 t_swpDelay;	/* total swap in delay [seconds] */

extern	double	 t_runReal;	/* total CPU "wall-clock" running time [seconds] */
extern	double	 t_runVirt;	/* total CPU "virtual" running time [seconds] */

/*	SBU counters */
extern	double	t_sbu;		/* total system billing units */
extern	double	t_cmdsbu;	/* total CMD system billing units */

/*	Command counters */
extern	int t_Ncmd;	 /* total # of CSA commands processed */
extern	int t_Ncmdmem;	 /* total # of CSA Memory records processed */
extern	int t_Ncmdio;	 /* total # of CSA Input/Output records processed */
extern	int t_Ncmddelay; /* total # of CSA Delay records processed */
extern	int t_Ncmdcmd;	 /* total # of CMD commands processed */

extern	int t_Ncfg;	/* total # of CFG records processed */
extern	int t_NcfgBoot;	/* total # of CFG Boot records processed */
extern	int t_NcfgFile;	/* total # of CFG File records processed */
extern	int t_NcfgOn;	/* total # of CFG On recrds processed */
extern	int t_NcfgOff;	/* total # of CFG Off records processed */
extern	int t_NcfgUnk;	/* total # of CFG Unknown records processed */

extern	int t_Nsoj;	/* total # of SOJ records processed */
extern	int t_Neoj;	/* total # of EOJ records processed */

extern	int t_Nsite;	/* total # of SITE records processed */

extern	int t_NIcmd;	/* total # of Incremental records encountered */
extern	int t_NIcmdcmd;	/* total # of Incremental records encountered */

extern	time_t	t_begin; /* beginning time (seconds) of first CSA cmd */
extern	time_t	t_end;	/* ending time (seconds) of last CSA command */

extern	struct	ac_utsname *cfg_sn;	/* config system name */

/*
 *	printsumhdr() - Print summary report header.
 */
void
printsumhdr(char * name)
{
	printf("\n\nJob %s Accounting - ", name);
	if (e_opt) {
		printf("Extended ");
	}
	if (s_opt) {
		printf("Summary Report\n");
	}
	printf(    "======================");
	if (e_opt) {
		printf("=========");
	}
	printf("==============\n\n");

	return;
}

/*
 *	printsum() - Output the IRIX(-s) format summary report.
 */
void
printsum()
{
	char		*group;
	char		*name;
	char		*project;
	struct utsname	sysname;
	struct utsname	*sn = &sysname;
	char		time_buf[TIMEBUF_MAX];

	printf("Job Accounting File Name         : %s\n", fn);

	if (cfg_sn->sysname != (char *)NULL) {
		printf("Operating System                 :"
			" %s %s %s %s %s\n",
			cfg_sn->sysname, cfg_sn->nodename, cfg_sn->release,
			cfg_sn->version, cfg_sn->machine);

	} else if (uname(sn) >= 0) {
		printf("Operating System                 :"
			" %s %s %s %s %s\n",
			sn->sysname, sn->nodename, sn->release,
			sn->version, sn->machine);
	}

	if (s_uid >= 0) {
		if (name = uid_to_name(s_uid)) {
			printf("User Name (ID)                   :"
				" %s (%d)\n", name, s_uid);
		} else {
			printf("User ID                          :"
				" %d\n", s_uid);
		}
	} else if (s_uid == -2) {
		printf("User Name (ID)                   : ");
		printf("multiple user IDs selected\n");
	}

	if (s_gid >= 0) {
		if (group = gid_to_name(s_gid)) {
			printf("Group Name (ID)                  :"
				" %s (%d)\n", group, s_gid);
		} else {
			printf("Group ID                         :"
				" %d\n", s_gid);
		}

	} else if (s_gid == -2) {
		printf("Group Name (ID)                  : ");
		printf("multiple group IDs selected\n");
	}

	if (s_prid >= 0) {
		if (project = prid_to_name(s_prid)) {
			printf("Project Name (ID)                :"
				" %s (%lld)\n", project, s_prid);
		} else {
			printf("Project ID                       :"
				" %lld\n", s_prid);
		}

	} else if (s_prid == -2) {
		printf("Project Name (ID)                : ");
		printf("multiple Project IDs selected\n");
	}

/*  XXXX uncomment this code if array sessions are ported.
	if (s_ash >= 0) {
		printf("Array Session ID                 : 0x%016llx\n", s_ash);

	} else if (s_ash == -2) {
		printf("Array Session ID                 : ");
		printf("multiple Array Session IDs selected\n");
	}
*/

	if (s_jid >= 0) {
		if (name = getenv("QSUB_REQNAME")) {
			printf("Job Name (ID)                    :"
				" %s (0x%llx)\n", name, s_jid);
		} else {
			printf("Job ID                           :"
				" 0x%llx\n", s_jid);
		}
	} else if (s_jid == -2) {
		printf("Job ID                           : ");
		printf("multiple Job IDs selected\n");
	}

	/*  Time information. */
	strftime(time_buf, TIMEBUF_MAX, DATE_FMT_JA, localtime(&t_begin));
	printf("Report Starts                    :%s\n", time_buf);
	strftime(time_buf, TIMEBUF_MAX, DATE_FMT_JA, localtime(&t_end));
	printf("Report Ends                      :%s\n", time_buf);
	printf("Elapsed Time                     :"
		" %12d      Seconds\n", (t_end - t_begin));

	/*  Time summary information. */
	if (t_utime == t_wutime) {
		printf("User CPU Time                    :"
			"    %14.4f Seconds\n", t_utime);
	} else {
		printf("User CPU Time                    :"
			"    %14.4f [%14.4f] Seconds\n", t_utime, t_wutime);
	}

	printf("System CPU Time                  :"
		"    %14.4f Seconds\n", t_stime);

	/*  Memory summary information. */
	printf("CPU Time Core Memory Integral    :"
		"    %14.4f Mbyte-seconds\n", t_coremem);
	printf("CPU Time Virtual Memory Integral :"
		"    %14.4f Mbyte-seconds\n", t_virtmem);
	printf("Maximum Core Memory Used         :    %14.4f Mbytes\n",
		KB_2_MB(max_corehimem));
	printf("Maximum Virtual Memory Used      :    %14.4f Mbytes\n",
		KB_2_MB(max_virthimem));

	/*  I/O summary information. */
	printf("Characters Read                  :"
		"    %14.4f Mbytes\n", (double)t_chr/(1024 * 1024));
	printf("Characters Written               :    %14.4f Mbytes\n",
		(double)t_chw / (1024 * 1024));
	printf("Logical I/O Read Requests        : %12lld\n", t_scr);
	printf("Logical I/O Write Requests       : %12lld\n", t_scw);

	/*  Delay summary information. */
	printf("CPU Delay                        :    %14.4f Seconds\n", t_cpuDelay);
	printf("Block I/O Delay                  :    %14.4f Seconds\n", t_blkDelay);
	printf("Swap In Delay                    :    %14.4f Seconds\n", t_swpDelay);

	/*  Command summary information. */
	printf("Number of Commands               : %12d\n", t_Ncmd);
	if (J_opt) {
		printf("-------------------------------- :"
			" -----------------\n");
		printf("Number of Detail Records         :"
			" mem(%3d),  io(%3d), delay(%3d)\n",
			t_Ncmdmem, t_Ncmdio, t_Ncmddelay);
		printf("Number of Job Records            :"
			" soj(%3d), eoj(%3d)\n", t_Nsoj, t_Neoj);
		printf("Number of Config Records         : cfg(%3d), "
			"boot(%2d),"
			" file(%2d), on(%2d), off(%2d)\n",
			t_Ncfg, t_NcfgBoot,
			t_NcfgFile, t_NcfgOn, t_NcfgOff);
		if (t_Nsite) {
			printf("Number of Reserved Records       :"
				" site(%3d)\n", t_Nsite);
		}
	}
	if (t_NIcmd) {
		printf("Number of Incremental Records    :"
			" %12d\n", t_NIcmd);
	}

	if (e_opt) {
		printf("Number of Page Swaps             : %12d\n", t_pgswaps);
		printf("Number of Minor Page Faults      : %12d\n", t_minflt);
		printf("Number of Major Page Faults      : %12d\n", t_majflt);
		printf("Number of CPU Delays             :    %9lld\n", t_cpuCount);
		printf("Number of Block I/O Delays       :    %9lld\n", t_blkCount);
		printf("Number of Swap In Delays         :    %9lld\n", t_swpCount);
		printf("CPU Real Run Time                :    %14.4f Seconds\n",
			t_runReal);
		printf("CPU Virtual Run Time             :    %14.4f Seconds\n",
			t_runVirt);
	}
	printf("System Billing Units             :    %14.4f\n", t_sbu);

	return;
}


/*
 *	rpt_process() - Output process flow and/or summary reports.
 */
void
rpt_process()
{

	void	printsumhdr(char *);
	void	printsum();
	void	printBsum();
	void 	flow_rpt();

	/* If no command entries selected. */
	if ( t_Ncmd == 0 ) {
		acct_err(ACCT_ABORT,
		       _("No commands were selected for the report.")
			);
	}

	/* Issue the command flow report if requested. */
	if (f_opt && (t_Ncmd > 0) ) {
		flow_rpt();
	}

	/* Issue the summary report if requested. */
	if (s_opt) {
		if (t_Ncmd > 0) {
			if (!r_opt) {
				printsumhdr("CSA");
			}
			printsum();
		}
	}

	return;
}
