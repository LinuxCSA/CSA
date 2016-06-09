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

#include <grp.h>
#include <malloc.h>
#include <math.h>
#include <memory.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_REGCOMP
#ifdef REG_EXTENDED
#define REGCOMP_FLAG    REG_EXTENDED
#else	/* REG_EXTENDED */
#define REGCOMP_FLAG    0
#endif	/* REG_EXTENDED */
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
char *regex();
extern char *__loc1;
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */

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

#define TIMEBUF_MAX	50

int	headerdone = FALSE;	/* header already printed? */

/*	Memory counters */
uint64_t max_corehimem = 0; /* maximum core memory used (Mbytes) */
uint64_t max_cmdchimem = 0; /* maximum CMD core memory used (Mbytes) */

uint64_t max_virthimem = 0; /* maximum virtual memory used (Mbytes) */
uint64_t max_cmdvhimem = 0; /* maximum CMD virtual memory used (Mbytes) */

double	t_coremem = 0.0; /* total core memory integral [Mbytes*secs] */
double	t_cmdcmem = 0.0; /* total CMD core memory integral [Mbytes*secs] */

double	t_virtmem = 0.0; /* total virtual memory integral [Mbytes*secs] */
double	t_cmdvmem = 0.0; /* total CMD virtual memory integral [Mbytes*secs] */

int	t_pgswaps = 0;	/* total # of pages swapped */
int	t_cmdpgswaps = 0; /*total # of CMD pages swapped */

int	t_cmdswaps = 0;	/* total # of CMD times swapped */

int	t_minflt = 0;	/* total # of minor page faults */

int	t_majflt = 0;	/* total # of major page faults */

/*	Time counters */
double	t_utime = 0.0;	/* total user CPU time (seconds) */
double	t_cmdutime = 0.0; /* total CMD user CPU time (secs) */

double	t_wutime = 0.0;	/* total weighted user CPU time (seconds) */
double	t_cmdwutime = 0.0; /* total weighted user CPU time (seconds) */

double	t_stime = 0.0;	/* total system CPU time [seconds] */
double	t_cmdstime = 0.0; /* total CMD system CPU time [secs] */

/*	I/O counters */
int64_t	t_chr = 0;	/* total # of characters read (Mbytes) */
int64_t	t_cmdchr = 0;	/* total # of CMD characters read (Mbytes) */

int64_t t_chw = 0;	/* total # of characters written (Mbytes) */
int64_t t_cmdchw = 0;	/* total # of CMD characters written (Mbytes) */

int64_t	t_scr = 0;	/* total # of logical read I/O requests */
int64_t	t_cmdscr = 0;	/* total # of CMD logical read I/O requests */

int64_t t_scw = 0;	/* total # of logical write I/O requests */
int64_t t_cmdscw = 0;	/* total # of CMD logical write I/O requests */

/*	Delay counters */
uint64_t t_cpuCount = 0;	 /* total # of CPU delay values recorded */
double	 t_cpuDelay = 0.0;	 /* total CPU delay [seconds] */

uint64_t t_blkCount = 0;	 /* total # of block I/O delay values recorded */
double	 t_blkDelay = 0.0;	 /* total block I/O delay [seconds] */

uint64_t t_swpCount = 0;	 /* total # of swap in delay values recorded */
double	 t_swpDelay = 0.0;	 /* total swap in delay [seconds] */

double	 t_runReal = 0.0;	 /* total CPU "wall-clock" running time [seconds] */
double	 t_runVirt = 0.0;	 /* total CPU "virtual" running time [seconds] */

/*	SBU counters */
double	t_sbu = 0.0;	/* total system billing units */
double	t_cmdsbu = 0.0;	/* total CMD system billing units */


/*	Command counters */
int	t_Ncmd = 0;	 /* total # of CSA commands processed */
int	t_Ncmdmem = 0;	 /* total # of CSA Memory records processed */
int	t_Ncmdio = 0;	 /* total # of CSA Input/Output records processed */
int	t_Ncmddelay = 0; /* total # of CSA Delay records processed */
int	t_Ncmdcmd = 0;	 /* total # of CMD commands processed */

int	t_NIcmd = 0;	/* total # of Incremental records encountered */
int	t_NIcmdcmd = 0;	/* total # of Incremental CMD commands processed */

time_t	t_begin = 0;	/* beginning time (seconds) of first cmd */
time_t	t_end= 0;	/* ending time (seconds) of last command */

/*
 *	matchnames() - See if we have a match for the current command
 *	with any of the selection names.
 */
int
#ifdef HAVE_REGCOMP
matchnames(char *comm, regex_t **names)
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
matchnames(char *comm, char **names)
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */
{
	char	command[17];
	int	ind;
#ifdef HAVE_REGCOMP
	regmatch_t rm;
#endif

	for(ind = 0; ind < 16 && comm[ind] != ' ' && comm[ind] != '\0'; ind++) {
		command[ind] = comm[ind];
	}
	command[ind] = '\0';

	while (*names) {
#ifdef HAVE_REGCOMP
		if (regexec(*names++, command, 1, &rm, REGCOMP_FLAG)
			!= REG_NOMATCH) {
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
		if (regex(*names++, command)) {
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */
			return(TRUE);
		}
	}

	return(FALSE);
}


/*
 *	printcmdhdr() - Print header for the command report.
 */
void
printcmdhdr(int l_opt)
{
	printf("\n\n");
	printf("Job Accounting - Command Report\n");
	printf("===============================\n\n");

	/* print first line of header */
	printf("%-15.15s", "    Command   ");
	printf("  %8.8s", " Started");
	printf("  %10.10s", "  Elapsed ");
	printf("  %10.10s", " User CPU ");
	printf("  %10.10s", " Sys CPU  ");
	if (c_opt) {
		printf("  %-10.10s", "   CPU   ");
		printf("  %-10.10s", "Block I/O");
		printf("  %-10.10s", " Swap In ");
	}
	if (l_opt) {
		printf("  %10.10s", "  CPU MEM ");
		printf("  %20.20s", "     Characters     ");
		printf("  %18.18s", "    Logical I/O   ");

		if (h_opt) {
			printf("  %8.8s", " CoreMem");
			printf("  %8.8s", " VirtMem");
		}
		printf("  %3.3s", " Ex");
	}
	printf("\n");

	/* print second line of header */
	printf("%-15.15s", "     Name    ");		/* Command */
	printf("  %8.8s", "   At   ");			/* Started At */
	printf("  %10.10s", "  Seconds ");		/* Elapsed */
	printf("  %10.10s", " Seconds  ");		/* User CPU */
	printf("  %10.10s", " Seconds  ");		/* Sys CPU */
	if (c_opt) {
		printf("  %10.10s", "Delay Secs");	/* CPU Delay */
		printf("  %10.10s", "Delay Secs");	/* Block I/O Delay */
		printf("  %10.10s", "Delay Secs");	/* Swap In Delay */
	}
	if (l_opt) {
		printf("  %10.10s", "Avg Mbytes");	/* CPU Mem */
		printf("  %9.9s", "   Read  ");		/* Chars */
		printf("  %9.9s", " Written ");
		printf("  %8.8s", "  Read  ");		/* Log I/O */
		printf("  %8.8s", "  Write ");

		if (h_opt) {
			printf("  %8.8s", " HiValue");	/* Memory */
			printf("  %8.8s", " HiValue");
		}
		printf("  %3.3s", " St");		/* Exit status */
		printf("  %3.3s", " Ni");
		printf("  %2.2s", "Fl");
	}
	if (c_opt) {
		printf("  %7.7s\n", " SBU's ");
	} else {
		printf("\n");
	}

	/* print last line of header - the separators */
	printf("%-15.15s", "===============");		/* Command Name */
	printf("  %8.8s", "========");			/* Started At */
	printf("  %10.10s", "==========");		/* Elapsed Time */
	printf("  %10.10s", "==========");		/* User CPU Seconds */
	printf("  %10.10s", "==========");		/* Sys CPU Seconds */
	if (c_opt) {
		printf("  %10.10s", "==========");	/* CPU Delay Seconds */
		printf("  %10.10s", "==========");	/* Block I/O Delay Seconds */
		printf("  %10.10s", "==========");	/* Swap In Delay Seconds */
	}
	if (l_opt) {
		printf("  %10.10s", "==========");	/* CPU Mem Avg Mwds */
		printf("  %9.9s", "=========");		/* Chars read */
		printf("  %9.9s", "=========");		/* Chars written */
		printf("  %8.8s", "========");		/* Log I/O Requests */
		printf("  %8.8s", "========");

		if (h_opt) {
			printf("  %8.8s", "========");	/* Memory */
			printf("  %8.8s", "========");
		}
		printf("  %3.3s", "===");		/* Exit St */
		printf("  %3.3s", "===");		/* Ni */
		printf("  %2.2s", "==");		/* Fl */
	}
	if (c_opt) {
		printf("  %7.7s\n", "=======");		/* SBU's */
	} else {
		printf("\n");
	}

	return;
}

/*
 *	printohdr() - Print header for the other command report.
 */
void
printohdr()
{
	printf("\n\n");
	printf("Job Accounting - Other (Alternate) Command Report\n");
	printf("=================================================\n\n");

	/* print first line of header */
	printf("%-15.15s", "    Command    ");
	printf("  %8.8s", " Started");
	printf("  %10.10s", "  Elapsed ");
	printf("  %6.6s", " Proc ");
	printf("  %6.6s", "Parent");
	printf("  %8.8s", " CoreMem ");
	printf("  %8.8s", " VirtMem ");
	printf("  %11.11s", "    CPU     ");
	printf("  %11.11s", " Block I/O  ");
	printf("  %11.11s", "  Swap In   ");
	printf("  %-9.9s", "Real Run");
	printf("  %-9.9s", "Virt Run");
	printf("\n");

	/* print second line of header */
	printf("%-15.15s", "     Name      ");
	printf("  %8.8s", "   At   ");
	printf("  %10.10s", "  Seconds ");
	printf("  %6.6s", "  ID  ");
	printf("  %6.6s", "ProcID");
	printf("  %8.8s", " HiWater");
	printf("  %8.8s", " HiWater");
	printf("  %11.11s", "Delay Count");
	printf("  %11.11s", "Delay Count");
	printf("  %11.11s", "Delay Count");
	printf("  %9.9s", "Time Secs");
	printf("  %9.9s", "Time Secs");
	printf("\n");

	/* print last line of header -- the separators */
	printf("%-15.15s", "===============");
	printf("  %8.8s", "========");
	printf("  %10.10s", "==========");
	printf("  %6.6s", "======");
	printf("  %6.6s", "======");
	printf("  %-8.8s", "========");
	printf("  %-8.8s", "========");
	printf("  %-11.11s", "===========");
	printf("  %-11.11s", "===========");
	printf("  %-11.11s", "===========");
	printf("  %9.9s", "=========");
	printf("  %9.9s", "=========");
	printf("\n");

	return;
}



/*
 *	pr_process() - Process a pacct record and accumulate totals.
 *	Returns TRUE if no error in pacct record, otherwise return FALSE.
 */
int
pr_process()
{
	int		ind;

	time_t	btime;			/* process beginning time [seconds] */
	double	etime;			/* elapsed time [seconds] */
	uint64_t  elaptime;		/* elapsed time [usecs] */
	uint64_t  systime;		/* system time [usecs] */
	double	stime;			/* system CPU time [seconds] */
	uint64_t  usertime;		/* user time [usecs] */
	double	utime;			/* user CPU time [seconds] */

	double	mem = 0.0;		/* memory integral [Mbyte-sec] */
	double	coremem = 0.0;		/* core memory integral [Mbyte-sec] */
	double	virtmem = 0.0;		/* virt memory integral [Mbyte-sec] */
	uint64_t corehimem = 0;		/* core memory hiVal [Mbytes] */
	uint64_t virthimem = 0;		/* virtual memory hiVal [Mbytes] */

	int64_t chr = 0;		/* # of characters read */
	int64_t	chw = 0;		/* # of characters written */
	int64_t	scr = 0;		/* # of logical I/O read requests */
	int64_t scw = 0;		/* # of logical I/O write requests */
	int	pgswaps = 0;		/* # of pages swapped */
	int	minflt = 0;		/* # of minor page faults */
	int	majflt = 0;		/* # of major page faults */

	uint64_t cpuCount = 0;		/* # of CPU delay values recorded */
	double	 cpuDelay = 0.0;	/* CPU delay [seconds] */
	uint64_t blkCount = 0;		/* # of block I/O delay values recorded */
	double	 blkDelay = 0.0;	/* block I/O delay [seconds] */
	uint64_t swpCount = 0;		/* # of swap in delay values recorded */
	double	 swpDelay = 0.0;	/* swap in delay [seconds] */
	double	 runReal = 0.0;		/* CPU "wall-clock" running time [seconds] */
	double	 runVirt = 0.0;		/* CPU "virtual" running time [seconds] */

	double	sbu = 0.0;		/* system billing units */
	char	time_buf[TIMEBUF_MAX];	/* time string buffer */

	/*
	 *  See if the kernel had problems writing to the job accounting file.
	 *  If the record is not now a CSA base record, then what is it?
	 */
	if (!acctrec.csa) {
		int	type = acctrec.prime->ah_type;

		if (db_flag) {
			fprintf(stderr,
			      "# ERROR  (%5o): An unexpected record type was detected as a primary type.\n",
				type);
		}
		acct_err(ACCT_WARN,
		       _("An unexpected record type (%d) was detected as a primary type in the\n   '%s' routine."),
			type, "pr_process()");
		return(TRUE);
	}

	/*
	 * Reject this entry if the user is being selective and does
	 * not want to see this one.
	 */
	if (g_opt && (acctrec.csa->ac_gid != s_gid) ||
	    j_opt && (acctrec.csa->ac_jid != s_jid) ||
	    p_opt && (acctrec.csa->ac_prid!= s_prid) ||
	    u_opt && (acctrec.csa->ac_uid != s_uid) ||
	    a_opt && (acctrec.csa->ac_ash != s_ash) ||
	    n_opt && !matchnames(acctrec.csa->ac_comm, names)) {

		if (db_flag > 3) {
		    fprintf(stderr,
		      "%s: record deselected - uid(%d, %d), gid(%d, %d), prid(%lld, %lld), ash(0x%llx, 0x%llx), jid(0x%llx, 0x%llx).\n",
			Prg_name,
			acctrec.csa->ac_uid, s_uid,
			acctrec.csa->ac_gid, s_gid,
			acctrec.csa->ac_prid, s_prid,
			acctrec.csa->ac_ash, s_ash,
			acctrec.csa->ac_jid, s_jid);
		}
		return(TRUE);
	}

	/*
	 * Convert the stats into something meaningful.
	 */
	elaptime = acctrec.csa->ac_etime;
	etime = TIME_2_SECS(elaptime);
	usertime = acctrec.csa->ac_utime;
	utime    = TIME_2_SECS(usertime);
	if (db_flag > 1) {
		fprintf(stderr, "%s: utime data - USEC(%llu), SEC(%10.2f)\n",
			"pr_process()",usertime, utime);
	}
	systime  = acctrec.csa->ac_stime;
	stime    = TIME_2_SECS(systime);

	if (db_flag > 1) {
		fprintf(stderr, "process ID information.\n");
	}
	if (s_ash == -1) {
		s_ash = acctrec.csa->ac_ash;

	} else if (acctrec.csa->ac_ash != s_ash) {
		s_ash = -2;
	}

	if (s_prid == -1) {
		s_prid = acctrec.csa->ac_prid;

	} else if (acctrec.csa->ac_prid != s_prid) {
		s_prid = -2;
	}

	if (s_gid == -1) {
		s_gid = acctrec.csa->ac_gid;

	} else if (acctrec.csa->ac_gid != s_gid) {
		s_gid = -2;
	}

	if (s_jid == -1) {
		s_jid = acctrec.csa->ac_jid;

	} else if (acctrec.csa->ac_jid != s_jid) {
		s_jid = -2;
	}

	if (s_uid == -1) {
		s_uid = acctrec.csa->ac_uid;

	} else if (acctrec.csa->ac_uid != s_uid) {
		s_uid = -2;
	}

	/*  Process for a Memory record. */
	if (acctrec.mem) {
		t_Ncmdmem++;
		if (db_flag > 1) {
			fprintf(stderr, "process Memory information.\n");
		}
		corehimem  = acctrec.mem->ac_core.himem;
		if (corehimem > max_corehimem) {
			max_corehimem = corehimem;
		}
		virthimem  = acctrec.mem->ac_virt.himem;
		if (virthimem > max_virthimem) {
			max_virthimem = virthimem;
		}
		pgswaps = acctrec.mem->ac_pgswap;
		minflt = acctrec.mem->ac_minflt;
		majflt = acctrec.mem->ac_majflt;

		switch(MEMINT) {

		case 1:
			coremem = TIME_2_SECS(acctrec.mem->ac_core.mem1);
			virtmem = TIME_2_SECS(acctrec.mem->ac_virt.mem1);
			mem  = coremem + virtmem;
			break;

		case 2:
			coremem = TIME_2_SECS(acctrec.mem->ac_core.mem2);
			virtmem = TIME_2_SECS(acctrec.mem->ac_virt.mem2);
			mem  = coremem + virtmem;
			break;

		case 3:
			coremem = TIME_2_SECS(acctrec.mem->ac_core.mem3);
			virtmem = TIME_2_SECS(acctrec.mem->ac_virt.mem3);
			mem  = coremem + virtmem;
			break;

		default:
			acct_err(ACCT_ABORT,
			       _("An invalid value for MEMINT (%d) was found in the configuration file.\n  MEMINT must be between 1 and 3."),
				MEMINT);
		}
	}

	/*  Process for a I/O record. */
	if (acctrec.io) {
		t_Ncmdio++;
		if (db_flag > 1) {
			fprintf(stderr, "process I/O information.\n");
		}

		chr = acctrec.io->ac_chr;
		chw = acctrec.io->ac_chw;
		scr = acctrec.io->ac_scr;
		scw = acctrec.io->ac_scw;
	}

	/*  Process for a Delay record. */
	if (acctrec.delay) {
		t_Ncmddelay++;
		if (db_flag > 1) {
			fprintf(stderr, "process Delay information.\n");
		}

		cpuCount = acctrec.delay->ac_cpu_count;
		cpuDelay = NSEC_2_SECS(acctrec.delay->ac_cpu_delay_total);
		blkCount = acctrec.delay->ac_blkio_count;
		blkDelay = NSEC_2_SECS(acctrec.delay->ac_blkio_delay_total);
		swpCount = acctrec.delay->ac_swapin_count;
		swpDelay = NSEC_2_SECS(acctrec.delay->ac_swapin_delay_total);
		runReal = NSEC_2_SECS(acctrec.delay->ac_cpu_run_real_total);
		runVirt = NSEC_2_SECS(acctrec.delay->ac_cpu_run_virtual_total);
	}

	if (db_flag > 1) {
		fprintf(stderr, "Compute the SBUs.\n");
	}
  	sbu = process_sbu(&acctrec);

	/* Accumulate totals. */
	if (db_flag > 1) {
		fprintf(stderr, "accumulate totals.\n");
	}
	t_Ncmd++;
	t_utime += utime;
	t_stime += stime;

	/* XXX when have multitasking records, uncomment this 
	t_wutime += TIME_2_SECS(ucputime(&acctrec));
	*/
	t_wutime += utime;

	t_Ncmdcmd++;
	/* time information */
	t_cmdutime += utime;
	t_cmdstime += stime;

	/* memory information */
	if (corehimem > max_cmdchimem) {
		max_cmdchimem = corehimem;
	}
	if (virthimem > max_cmdvhimem) {
		max_cmdvhimem = virthimem;
	}
	t_cmdcmem += coremem;
	t_cmdvmem += virtmem;

	/* I/O information */
	t_cmdchr += chr;
	t_cmdchw += chw;
	t_cmdscr += scr;
	t_cmdscw += scw;

	t_cmdsbu += sbu;

	t_coremem += coremem;
	t_virtmem += virtmem;

	t_chr += chr;
	t_chw += chw;
	t_scr += scr;
	t_scw += scw;

	t_sbu += sbu;
	t_pgswaps += pgswaps;
	t_minflt += minflt;
	t_majflt += majflt;

	/* delay information */
	t_cpuCount += cpuCount;
	t_cpuDelay += cpuDelay;
	t_blkCount += blkCount;
	t_blkDelay += blkDelay;
	t_swpCount += swpCount;
	t_swpDelay += swpDelay;
	t_runReal += runReal;
	t_runVirt += runVirt;
	/* end Accumulate Totals */

	if (db_flag > 1) {
		fprintf(stderr, "Process time information.\n");
	}
	btime = acctrec.csa->ac_btime;
	if (!t_begin) {
		t_begin = btime;

	} else if (btime < t_begin) {
		t_begin = btime;
	}

	if (btime + etime > t_end) {
		t_end = btime + etime;
	}

	/* Add this process to the command flow tree. */
	if (f_opt) {
		makeflwt(acctrec.csa);
	}

	/* Issue command report. */
	if (c_opt) {
		if (db_flag > 1) {
			fprintf(stderr, "generate command report.\n");
		}
		if (!r_opt && !headerdone) {
			printcmdhdr(l_opt);
			headerdone = TRUE;
		}

		printf("%-15.15s", acctrec.csa->ac_comm);

		strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT1,
			localtime(&acctrec.csa->ac_btime));
		printf(" %s", time_buf);

		printf("  %10.2f", etime);
		printf("  %10.2f", utime);
		printf("  %10.2f", stime);

		printf("  %10.2f", cpuDelay);
		printf("  %10.2f", blkDelay);
		printf("  %10.2f", swpDelay);
		if (l_opt) {
			printf("  %10.2f", (utime + stime) ?
				mem / (utime + stime) : 0.0);
			printf("  %9.3f", (double)chr / (1024 * 1024));
			printf("  %9.3f", (double)chw / (1024 * 1024));
			printf("  %8lld", scr);
			printf("  %8lld", scw);
			if (h_opt) {
				printf("  %8lld", corehimem);
				printf("  %8lld", virthimem);
			}

			/*
			 * ac_stat is only 8 bits, so can't split
			 * into separate code and data portions to display
			*/
			printf("  %3d", acctrec.csa->ac_stat);
			printf("  %3d", acctrec.csa->ac_nice);
			printf("  %c", (acctrec.csa->ac_hdr1.ah_flag & AFORK) ?
					'F' : ' ');
			printf("%c", (acctrec.csa->ac_hdr1.ah_flag & ASU) ?
					'S' : ' ');
		}	/* if (l_opt) */

		printf("  %7.2f\n", sbu);
	}

	/* issue other command report */
	if (o_opt) {
		if (db_flag > 1) {
	 		fprintf(stderr, "generate OTHER command report.\n");
		}
		if (!r_opt && t_Ncmd == 1) {
			printohdr();
		}

		printf("%-15.15s", acctrec.csa->ac_comm);

		strftime(time_buf, TIMEBUF_MAX, TIME_FORMAT1,
			localtime(&acctrec.csa->ac_btime));
		printf(" %s", time_buf);

		printf("  %10.2f", etime);
		printf("  %6d", acctrec.csa->ac_pid);
		printf("  %6d", acctrec.csa->ac_ppid);

		printf("  %8lld", corehimem);
		printf("  %8lld", virthimem);

		printf("  %11lld", cpuCount);
		printf("  %11lld", blkCount);
		printf("  %11lld", swpCount);

		printf("  %9.2f", runReal);
		printf("  %9.2f", runVirt);

		printf("\n");
	}

	return(TRUE);
}
