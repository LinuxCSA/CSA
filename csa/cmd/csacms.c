/*
 * Copyright (c) 2004-2007 Silicon Graphics, Inc. All Rights Reserved.
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


#include <stdio.h>

#include <getopt.h> // use GNU's version of getopt 

#include <linux/acct.h>
#include <ctype.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "cms.h"

/* getopt options */
struct option long_options[] = {
	{ "ascii", 0, NULL, 'a' },
	{ "prime", 0, NULL, 'p' },
	{ "non-prime", 0, NULL, 'o' },
	{ "extended-report", 0, NULL, 'e' },
	{ "sort-by-cpu", 0, NULL, 'c' },
	{ "sort-by-number", 0, NULL, 'n' },
	{ "combine-commands", 0, NULL, 'j' },
	{ "all-jobs", 0, NULL, 'A' },

	{ 0, 0, 0, 0 }
};

typedef struct _AppData {

	struct _args {
		int ascii;
		int prime;
		int nonprime;
		int extended;
		int sort_cpu;
		int sort_number;
		int join;
		int internal_fmt;
		int sorted_fmt;
		int all_jobs;
	} args;

	struct {

		int mem_integral;

	} config;

	struct cms *records;
	int nrecords;

	struct cms totals;

} AppData;

AppData thisApp = {0};


#define MAXCSACMS	5001	/* max # distinct commands for csacms */

#define SUM(a,b)	((a)->prime.b + (a)->nonprime.b)
#define MAX(a,b) ((a)>(b)? -1 : ((a)==(b)) ? 0 : 1 )	

#define MERGE_MARKER "***other"

/* internal function prototypes */

static int parse_and_validate_args(int argc, char **argv);
static void summarize_pacct_file(struct acctent *acct);

static int save_record_data(struct cms *record);

static void compress_hash(void);

static int sort_cpu_time(const void *ptr1, const void *ptr2);
static int sort_kcore(const void *ptr1, const void *ptr2);
static int sort_number(const void *ptr1, const void *ptr2);

static void print_record(struct cms *,int);
static void add_cms_records(struct cms *, struct cms *);

int
main(int argc, char **argv)
{

	int argv_files;

	int acct_fd=0;

	/* initialize all application data */
	memset(&thisApp, 0, sizeof(thisApp));

	/* set config parameters */
	thisApp.config.mem_integral = init_int(MEMORY_INTEGRAL, 1, TRUE );



	thisApp.records = (struct cms *) calloc(MAXCSACMS, sizeof(struct cms));
	thisApp.nrecords = MAXCSACMS;

	if ( thisApp.records == NULL ) {
		acct_err(ACCT_FATAL, "Memory allocation failed\n");
	}
	
	argv_files = parse_and_validate_args(argc, argv);

	/* loop through all files supplied on cmd line */
	while ( argv_files < argc ) {

		if ( acct_fd )
			closeacct(acct_fd);

		acct_fd = openacct(argv[argv_files], "r");

		if ( acct_fd < 0 ) {

			acct_err(ACCT_FATAL, "Failed to open '%s'\n",argv[argv_files]);
		}

		if ( thisApp.args.internal_fmt ) {

			struct cms record;

			while ( readcms(acct_fd, &record) == sizeof(record)) {
				
				save_record_data(&record);
			}

		}
		else if ( thisApp.args.sorted_fmt ) { 

			struct acctcfg upt;
			struct acctjob job = {0};
			long upt_where, upt_size, where;

			while ( readacct(acct_fd, (char *) &upt, 
						sizeof(struct acctcfg) ) == sizeof(struct acctcfg)) {
				
				upt_where=seekacct(acct_fd, 0, SEEK_CUR);

				upt_size = upt.ac_uptimelen;

				while ( upt_size && 
						readacct(acct_fd, (char *) &job, 
							sizeof(struct acctjob)) == sizeof(struct acctjob) ) {

					where = seekacct(acct_fd, 0, SEEK_CUR);

					/* which jobs should we consider ? */
					if ( thisApp.args.all_jobs ||
							job.aj_flags & END_OF_JOB_B ||
							job.aj_flags & END_OF_JOB_C ||
							job.aj_flags & END_OF_JOB_T ||
							( (! (job.aj_flags & WKMG_IN_JOB)) && 
							  (  (job.aj_flags & END_OF_JOB_I)) ) ) {

			
						char acctentbuf[ACCTENTSIZ];
						struct acctent	acct;
						int i;

						acct.ent = acctentbuf;

						if ( seekacct(acct_fd, where + job.aj_eop_start, SEEK_SET ) < 0 )
							acct_perr(ACCT_ABORT, errno,
									"Error occured positioning file '%s'",argv[argv_files]);

						for (i=0; i < job.aj_eop_num; i++ ) { 
						   	if ( readacctent(acct_fd, &acct, FORWARD ) <=0 ) {
							
								acct_perr(ACCT_ABORT, errno,
										"Error occured reading file '%s'",
										argv[argv_files]);
							}

							summarize_pacct_file(&acct);

						}

					}	

					upt_size -= sizeof(struct acctjob) + job.aj_len;

					where = where + job.aj_len;

					if ( seekacct(acct_fd, where, SEEK_SET ) < 0 ) {

						acct_perr(ACCT_ABORT,errno,
								"Error occured positioning file '%s'",argv[argv_files]);
					}
				}

				if ( seekacct(acct_fd, 0, SEEK_CUR ) != upt.ac_uptimelen + upt_where ) {

					acct_err(ACCT_WARN, "The position of the file '%s' is incorrect for the next record. Possible a difference in program and file versions\n", argv[argv_files]);
				}

			}

		}
		else {

			char acctentbuf[ACCTENTSIZ];

			struct acctent	acct;

			acct.ent = acctentbuf;

			while ( readacctent(acct_fd, &acct, FORWARD ) > 0 ) {

				summarize_pacct_file(&acct);

			}

		}

		argv_files++;
	}


	/* merge all single usage commands */
	if ( thisApp.args.join ) {
		struct cms record;
		int i, j;

		memset((char *)&record, '\0', sizeof(struct cms));
		strcpy(record.cmsCmdName, MERGE_MARKER);

		j = save_record_data(&record);
		for (i = 0; i < MAXCSACMS; i++) {
			if ((i != j) && 
			    (thisApp.records[i].cmsCmdName[0] != '\0') &&
			    ((thisApp.records[i].prime.procCount +
			      thisApp.records[i].nonprime.procCount) <= 1) ) {
				add_cms_records(&thisApp.records[i],
						&thisApp.records[j]);
				thisApp.records[i].cmsCmdName[0] = '\0';
			}
		}
		
	}
	
	/* compress the hash table */
	compress_hash();
	
	/* sort records depending on user preferences... */

	qsort(thisApp.records, thisApp.nrecords, sizeof(struct cms),
			(thisApp.args.sort_cpu ? sort_cpu_time :
			 (thisApp.args.sort_number ? sort_number : sort_kcore)) );

	
	/* start writing the output ... */

	if ( thisApp.args.ascii ) {

		int idx=0;
		int i;

		if ( thisApp.args.prime && thisApp.args.nonprime ) {
			printf("                                   TOTAL COMMAND SUMMARY");
			if (thisApp.args.extended) {
				printf("                                   EXTENDED REPORT");
			}
			printf("\n\n");
		}
		else if (thisApp.args.prime) {
			printf("                                   PRIME TIME COMMAND SUMMARY\n\n");
			idx = PRIME;
		}
		else if (thisApp.args.nonprime) {
			printf("                                   NON-PRIME TIME COMMAND SUMMARY\n\n");
			idx=NONPRIME;
		}
		
		if ( thisApp.args.prime || thisApp.args.nonprime ) {
			printf("%-16.16s", "COMMAND");
			printf(" %8s", "NUMBER");
			printf(" %12s", "TOTAL");
			printf(" %12s", "TOTAL");
			printf(" %10s", "TOTAL");
			printf(" %12s", "TOTAL");
			printf(" %9s", "MEAN");
	   		printf(" %9s", "MEAN");
	   		printf(" %8s", "MEAN");
	   		printf(" %6s", "HOG");
	   		printf(" %11s", "K-CHARS");
	   		printf(" %11s", "K-CHARS");
			if (thisApp.args.extended) {
				printf(" %11s", "HI WATER");
				printf(" %11s", "HI WATER");
				printf(" %9s", "LOGICAL");
				printf(" %9s", "LOGICAL");
				printf(" %10s", "SYSTEM");
				printf(" %9s", "MINOR");
				printf(" %9s", "MAJOR");
				printf(" %11s", "CPU");
				printf(" %11s", "BLOCK I/O");
				printf(" %11s", "SWAP IN");

			}
	   		printf("\n");

			printf("%-16.16s", "NAME");
			printf(" %8s", "OF");
			printf(" %12s", "(KCORE- ");
			printf(" %12s", "(KVIRT- ");
			printf(" %10s", "CPU");
			printf(" %12s", "REAL");
			printf(" %9s", "SIZE");
	   		printf(" %9s", "SIZE");
	   		printf(" %8s", "CPU");
	   		printf(" %6s", "FACTOR");
	   		printf(" %11s", "READ");
	   		printf(" %11s", "WRITTEN");
			if (thisApp.args.extended) {
				printf(" %11s", "CORE MEMORY");
				printf(" %11s", "VIRT MEMORY");
				printf(" %9s", "READS");
				printf(" %9s", "WRITES");
				printf(" %10s", "TIME");
				printf(" %9s", "FAULTS");
				printf(" %9s", "FAULTS");
				printf(" %11s", "DELAY");
				printf(" %11s", "DELAY");
				printf(" %11s", "DELAY");

			}
	   		printf("\n");

			printf("%-16.16s", "");
			printf(" %8s", "COMMANDS");
			printf(" %12s", "MINUTES)");
			printf(" %12s", "MINUTES)");
			printf(" %10s", "(MINUTES)");
			printf(" %12s", "(MINUTES)");
			printf(" %9s", "KCORE");
	   		printf(" %9s", "KVIRT");
	   		printf(" %8s", "(MINUTES)");
	   		printf(" %6s", "");
	   		printf(" %11s", "");
	   		printf(" %11s", "");
			if (thisApp.args.extended) {
				printf(" %11s", "(KBYTES)");
				printf(" %11s", "(KBYTES)");
				printf(" %9s", "");
				printf(" %9s", "");
				printf(" %10s", "(MINUTES)");
				printf(" %9s", "");
				printf(" %9s", "");
				printf(" %11s", "(MINUTES)");
				printf(" %11s", "(MINUTES)");
				printf(" %11s", "(MINUTES)");

			}
	   		printf("\n");
			
			
		}

		/* build up total record */
		for (i=0; i< thisApp.nrecords; i++ ) {
			
			thisApp.totals.prime.procCount += thisApp.records[i].prime.procCount;
			thisApp.totals.nonprime.procCount += thisApp.records[i].nonprime.procCount;

			thisApp.totals.prime.cpuTime += thisApp.records[i].prime.cpuTime;
			thisApp.totals.nonprime.cpuTime += thisApp.records[i].nonprime.cpuTime;

			thisApp.totals.prime.systemTime += thisApp.records[i].prime.systemTime;
			thisApp.totals.nonprime.systemTime += thisApp.records[i].nonprime.systemTime;

			thisApp.totals.prime.realTime += thisApp.records[i].prime.realTime;
			thisApp.totals.nonprime.realTime += thisApp.records[i].nonprime.realTime;

			thisApp.totals.prime.kcoreTime += thisApp.records[i].prime.kcoreTime;
			thisApp.totals.nonprime.kcoreTime += thisApp.records[i].nonprime.kcoreTime;

			thisApp.totals.prime.kvirtualTime += thisApp.records[i].prime.kvirtualTime;
			thisApp.totals.nonprime.kvirtualTime += thisApp.records[i].nonprime.kvirtualTime;

			thisApp.totals.prime.highWaterCoreMem += thisApp.records[i].prime.highWaterCoreMem;
			thisApp.totals.nonprime.highWaterCoreMem += thisApp.records[i].nonprime.highWaterCoreMem;

			thisApp.totals.prime.highWaterVirtMem += thisApp.records[i].prime.highWaterVirtMem;
			thisApp.totals.nonprime.highWaterVirtMem += thisApp.records[i].nonprime.highWaterVirtMem;


			thisApp.totals.prime.minorFaultCount += thisApp.records[i].prime.minorFaultCount;
			thisApp.totals.nonprime.minorFaultCount += thisApp.records[i].nonprime.minorFaultCount;

			thisApp.totals.prime.majorFaultCount += thisApp.records[i].prime.majorFaultCount;
			thisApp.totals.nonprime.majorFaultCount += thisApp.records[i].nonprime.majorFaultCount;

			thisApp.totals.prime.charsRead += thisApp.records[i].prime.charsRead;
			thisApp.totals.nonprime.charsRead += thisApp.records[i].nonprime.charsRead;

			thisApp.totals.prime.charsWritten += thisApp.records[i].prime.charsWritten;
			thisApp.totals.nonprime.charsWritten += thisApp.records[i].nonprime.charsWritten;

			thisApp.totals.prime.readSysCalls += thisApp.records[i].prime.readSysCalls;
			thisApp.totals.nonprime.readSysCalls += thisApp.records[i].nonprime.readSysCalls;

			thisApp.totals.prime.writeSysCalls += thisApp.records[i].prime.writeSysCalls;
			thisApp.totals.nonprime.writeSysCalls += thisApp.records[i].nonprime.writeSysCalls;

			thisApp.totals.prime.cpuDelayTime += thisApp.records[i].prime.cpuDelayTime;
			thisApp.totals.nonprime.cpuDelayTime += thisApp.records[i].nonprime.cpuDelayTime;

			thisApp.totals.prime.blkDelayTime += thisApp.records[i].prime.blkDelayTime;
			thisApp.totals.nonprime.blkDelayTime += thisApp.records[i].nonprime.blkDelayTime;

			thisApp.totals.prime.swpDelayTime += thisApp.records[i].prime.swpDelayTime;
			thisApp.totals.nonprime.swpDelayTime += thisApp.records[i].nonprime.swpDelayTime;

		}

		if ( thisApp.args.prime || thisApp.args.nonprime ) {
			sprintf(thisApp.totals.cmsCmdName, "%s","TOTALS");

			printf("\n");
			print_record(&thisApp.totals, idx);
			printf("\n");
		}

		for (i=0; i< thisApp.nrecords; i++ ) {

			print_record(&thisApp.records[i], idx);

		}

	}
	else {

		int out;

		if ( (out=openacct(NULL, "w") ) > 0 ) {

			int i;

			for (i=0; i< thisApp.nrecords; i++ ) {

				if (  create_hdr1(&thisApp.records[i].cmsAcctHdr, ACCT_CMS ) < 0 ) {

					acct_err(ACCT_WARN, "Creating output header failed.\n");
				}

				writecms(out, &thisApp.records[i]);
			}
		}
	}

	/* keep strict compilers happy */
	return 0;

}



/* parse ARGV and return the number of items handled */
static int
parse_and_validate_args(int argc, char **argv)
{
	int c;
	int idx=0;
	int need_ascii = 0;

	/* default is both */
	thisApp.args.prime = thisApp.args.nonprime = TRUE;
	
	while ( (c=getopt_long(argc, argv, 
					"apoecjnsSA", long_options, &idx)) != -1 ) {


		switch ( c ) { 

			case 'a':
				thisApp.args.ascii = TRUE;
				break;

			case 'p':
				/* turn off non-prime output */
				thisApp.args.nonprime = FALSE;
				need_ascii = 1;
				break;

			case 'o':
				/* turn off prime output */
				thisApp.args.prime = FALSE;
				need_ascii = 1;
				break;

			case 'e':
				thisApp.args.extended = TRUE;
				need_ascii = 1;
				break;

			case 'c':
				thisApp.args.sort_cpu = TRUE;
				break;

			case 'n':
				thisApp.args.sort_number = TRUE;
				break;

			case 'j':
				thisApp.args.join = TRUE;
				break;

			case 's':
				thisApp.args.internal_fmt = TRUE;
				break;

			case 'S':
				thisApp.args.sorted_fmt = TRUE;
				break;

			case 'A':
				thisApp.args.all_jobs = TRUE;
				break;


			case '?':
			default:
				acct_err(ACCT_ABORT, CM_USAGE, "csacms");
		}
	}

	/* 
	 * if both prime and non-prime are "turned off" then turn both back on - 
	 * documented behavior
	 */ 
	if ( thisApp.args.prime == FALSE && thisApp.args.nonprime == FALSE )
		thisApp.args.prime = thisApp.args.nonprime = TRUE;

	if (need_ascii) {
		if ( (thisApp.args.prime || thisApp.args.nonprime || thisApp.args.extended ) 
			&& ! thisApp.args.ascii ) {

		acct_err(ACCT_FATAL,"-p or -o or -e requires ascii (-a) output to be enabled\n");
		exit(1);
		}
	}


	if ( thisApp.args.all_jobs && ! thisApp.args.sorted_fmt ) {
		acct_err(ACCT_FATAL,"-A requires sorted pacct file input format (-S) to be enabled\n");
		exit(1);

	}

	if ( thisApp.args.internal_fmt && thisApp.args.sorted_fmt ) {
		acct_err(ACCT_FATAL,"Cannot specify both internal summary and sorted pacct format for input files.\n");
		exit(1);
	}

	if ( thisApp.args.sort_cpu && thisApp.args.sort_number ) { 
		acct_err(ACCT_FATAL,"Cannot enable sorting by both CPU time and # of command invocations at the same time.\n");
		exit(1);
	}

	/* return the start of the non-option entries in ARGV */

	return optind;
}


#ifdef FUTURE

#define CMS_PROC	1
#define CMS_CPU		2
#define CMS_SYS		3
#define CMS_CONN	4
#define CMS_REAL	5
#define CMS_KCORE	6
#define CMS_KVIRT	7
#define CMS_CHARR	8
#define CMS_CHARW	9
#define CMS_BLKR	10
#define CMS_BLKW	11
#define CMS_RSYS	12
#define CMS_WSYS	13
#define CMS_HWCORE	14
#deinfe CMS_HWVIRT	15
#define CMS_PGSWAP	16
#define CMS_MAJFLT	17
#define CMS_MINFLT	18
#define CMS_BIO		19
#define CMS_RIO		20
#define CMS_RUNQ	21


static double
cmssum(struct cms *cms, int fld)
{
    switch (fld) of {
    	case CMS_PROC:
	    return (cms->prime.procCount + cms->nonprime.procCount);

    	case CMS_CPU:
	    return (cms->prime.cpuTime + cms->nonprime.cpuTime);

    	case CMS_SYS:
	    break;

    	case CMS_CONN:
	    break;

    	case CMS_REAL:
	    break;

    	case CMS_KCORE:
	    break;

    	case CMS_KVIRT:
	    break;

    	case CMS_CHARR:
	    break;

    	case CMS_CHARW:
	    break;

    	case CMS_BLKR:
	    break;

    	case CMS_BLKW:
	    break;

    	case CMS_RSYS:
	    break;

    	case CMS_WSYS:
	    break;

    	case HWCORE:
	    break;

    	case HWVIRT:
	    break;

    	case PGSWAP:
	    break;

    	case MAJFLT:
	    break;

    	case MINFLT:
	    break;

    	case CMS_BIO:
	    break;

    	case CMS_RIO:
	    break;

	case CMS_RUNQ:
	    break;

	default:
	    break;
    }

}
#endif

static void
summarize_pacct_file(struct acctent *ae)
{

	struct cms record;
	time_t duration, pop_time[2];
	float pratio; // prime to total ratio
	double cpu_mins, sys_mins, kcore=0.0, kvirt=0.0;

	if ( ae->csa == NULL ) {
		return;
	}

	memset(&record, 0, sizeof(struct cms));

	strncpy(record.cmsCmdName, ae->csa->ac_comm, 15);

	duration = TIME_2_SECS( ae->csa->ac_etime);

	if ( duration == 0 )
		duration = 1;

	pop_calc(ae->csa->ac_btime, duration, pop_time);
	
	pratio = 1.0 * pop_time[PRIME] / ( pop_time[PRIME] + pop_time[NONPRIME] );

	if ( pop_time[PRIME] > pop_time[NONPRIME] ) {

		record.prime.procCount = 1;
		record.nonprime.procCount = 0;
	}
	else {
		record.prime.procCount = 0;
		record.nonprime.procCount = 1;
	}


	cpu_mins = TIME_2_MINS(ae->csa->ac_stime + ae->csa->ac_utime); 
	sys_mins = TIME_2_MINS(ae->csa->ac_stime); 

	record.prime.cpuTime = cpu_mins * pratio;
	record.prime.systemTime = sys_mins * pratio;
	record.prime.realTime = TIME_2_MINS(ae->csa->ac_etime) *pratio; 

	/* initialize these to make summing them easier (no special cases) */
	record.prime.kcoreTime = record.nonprime.kcoreTime = 0.0;
	record.prime.kvirtualTime = record.nonprime.kvirtualTime = 0.0;
	record.prime.highWaterCoreMem = record.nonprime.highWaterCoreMem = 0.0;
	record.prime.highWaterVirtMem = record.nonprime.highWaterVirtMem = 0.0;
	record.prime.minorFaultCount = record.nonprime.minorFaultCount = 0.0;
	record.prime.majorFaultCount = record.nonprime.majorFaultCount = 0.0;

	record.prime.charsRead = record.nonprime.charsRead = 0.0; 
	record.prime.charsWritten = record.nonprime.charsWritten = 0.0;

	record.prime.readSysCalls = record.nonprime.readSysCalls = 0.0;
	record.prime.writeSysCalls = record.nonprime.writeSysCalls = 0.0;

	record.prime.cpuDelayTime = record.nonprime.cpuDelayTime = 0.0;
	record.prime.blkDelayTime = record.nonprime.blkDelayTime = 0.0;
	record.prime.swpDelayTime = record.nonprime.swpDelayTime = 0.0;
		
	if ( ae->mem ) {

		switch ( thisApp.config.mem_integral ) {

			case 1:
				kcore = TIME_2_MINS(MB_2_KB((double)ae->mem->ac_core.mem1));
				kvirt = TIME_2_MINS(MB_2_KB((double)ae->mem->ac_virt.mem1));
				break;
			case 2:
				kcore = TIME_2_MINS(MB_2_KB((double)ae->mem->ac_core.mem2));
				kvirt = TIME_2_MINS(MB_2_KB((double)ae->mem->ac_virt.mem2));
				break;
			case 3:
				kcore = TIME_2_MINS(MB_2_KB((double)ae->mem->ac_core.mem3));
				kvirt = TIME_2_MINS(MB_2_KB((double)ae->mem->ac_virt.mem3));
				break;

			default:
				acct_err(ACCT_FATAL,
						"Bad value for MEMORY_INTEGRAL\n");
				break;

		}

		/* these two are not split, since the value is the maximum 
		 * ammount of memory used during the execution of the program */
		record.prime.highWaterCoreMem = 
			record.nonprime.highWaterCoreMem = ae->mem->ac_core.himem;
		record.prime.highWaterVirtMem = 
			record.nonprime.highWaterVirtMem = ae->mem->ac_virt.himem;

		record.prime.minorFaultCount = ae->mem->ac_minflt * pratio;
		record.prime.majorFaultCount = ae->mem->ac_majflt * pratio;


	}

	record.prime.kcoreTime = kcore * pratio;
	record.prime.kvirtualTime = kvirt * pratio;

	if ( ae->io ) {
	
		record.prime.charsRead =  ae->io->ac_chr * pratio;
		record.prime.charsWritten =  ae->io->ac_chw * pratio;

		record.prime.readSysCalls =  ae->io->ac_scr * pratio;
		record.prime.writeSysCalls =  ae->io->ac_scw * pratio;

	}

	if ( ae->delay ) {
	
		record.prime.cpuDelayTime = NSEC_2_MINS( ae->delay->ac_cpu_delay_total ) * pratio;
		record.prime.blkDelayTime = NSEC_2_MINS( ae->delay->ac_blkio_delay_total ) * pratio;
		record.prime.swpDelayTime = NSEC_2_MINS( ae->delay->ac_swapin_delay_total ) * pratio;

	}

	if ( pratio == 1.0 ) {
		record.nonprime.cpuTime = 0.0;
		record.nonprime.systemTime = 0.0;
		record.nonprime.realTime = 0.0;
		record.nonprime.kcoreTime = 0.0;
		record.nonprime.kvirtualTime = 0.0;

		if ( ae->mem ) {
			record.nonprime.minorFaultCount = 0.0;
			record.nonprime.majorFaultCount = 0.0;
		}

		if ( ae->io ) {
			record.nonprime.charsRead =  0.0;
			record.nonprime.charsWritten =  0.0;

			record.nonprime.readSysCalls =  0.0;
			record.nonprime.writeSysCalls =  0.0;
		}

		if ( ae->delay ) {
			record.nonprime.cpuDelayTime =  0.0;
			record.nonprime.blkDelayTime =  0.0;
			record.nonprime.swpDelayTime =  0.0;
		}

	}
	else {
		record.nonprime.cpuTime = cpu_mins * (1.0 - pratio );
		record.nonprime.systemTime = sys_mins * (1.0 - pratio );
		record.nonprime.realTime = TIME_2_MINS(ae->csa->ac_etime) * (1.0 - pratio );
		record.nonprime.kcoreTime = kcore * (1.0 - pratio );
		record.nonprime.kvirtualTime = kvirt * (1.0 - pratio );

		if ( ae->mem ) {
			record.nonprime.minorFaultCount = ae->mem->ac_minflt * ( 1.0 - pratio );
			record.nonprime.majorFaultCount = ae->mem->ac_majflt * ( 1.0 - pratio );
		}

		if ( ae->io ) {
			record.nonprime.charsRead =  ae->io->ac_chr * ( 1.0 - pratio );
			record.nonprime.charsWritten =  ae->io->ac_chw * ( 1.0 - pratio );

			record.nonprime.readSysCalls =  ae->io->ac_scr * ( 1.0 - pratio );
			record.nonprime.writeSysCalls =  ae->io->ac_scw * ( 1.0 - pratio );
		}

		if ( ae->delay ) {
			record.nonprime.cpuDelayTime = NSEC_2_MINS( ae->delay->ac_cpu_delay_total) * ( 1.0 - pratio );
			record.nonprime.blkDelayTime = NSEC_2_MINS( ae->delay->ac_blkio_delay_total) * ( 1.0 - pratio );
			record.nonprime.swpDelayTime = NSEC_2_MINS( ae->delay->ac_swapin_delay_total) * ( 1.0 - pratio );
		}
	}


	save_record_data(&record);

}

static int
save_record_data(struct cms *record)
{

	int hashkey = 0;
	int i=1;
	struct cms *ptr = NULL;


	while ( record->cmsCmdName[i] ) {

		if ( iscntrl(record->cmsCmdName[i]) )
		   record->cmsCmdName[i] = '?';

		hashkey = ( hashkey  * (16-i) ) + record->cmsCmdName[i] ;

		i++;
	}

	hashkey %= MAXCSACMS;

	i=0;
	while ( i < thisApp.nrecords ) {
		if ( ! strcmp(thisApp.records[i].cmsCmdName, record->cmsCmdName) ) {
			ptr = &thisApp.records[i];
			break;
		}

		if ( thisApp.records[i].cmsCmdName[0] == '\0' ) {

			strcpy( thisApp.records[i].cmsCmdName, record->cmsCmdName);
			ptr = &thisApp.records[i];

			break;
		}

		if ( ++i == thisApp.nrecords ) {

			thisApp.nrecords += MAXCSACMS;

			thisApp.records = (struct cms *) realloc(thisApp.records, 
					thisApp.nrecords * sizeof(struct cms) );

			if ( thisApp.records == NULL ) {

				acct_perr(ACCT_FATAL, errno, 
						"Insufficient memory when reallocaing 'struct cms'");
			}


			memset(thisApp.records+thisApp.nrecords - MAXCSACMS, 
					0, MAXCSACMS * sizeof(struct cms *));

		}

	}

	/* ptr must be valid by now */

	add_cms_records(record, ptr);

	return i;
}


static int
sort_cpu_time(const void *ptr1, const void *ptr2)
{

	struct cms *cms1 = ((struct cms *) ptr1);
	struct cms *cms2 = ((struct cms *) ptr2);



	/* either or neither specified */
	if ( thisApp.args.prime == thisApp.args.nonprime) {

		return MAX(SUM(cms1,cpuTime), SUM(cms2,cpuTime));

	}
	else if ( thisApp.args.prime ) {

		return MAX(cms1->prime.cpuTime, cms2->prime.cpuTime);

	}
	else {
		return MAX(cms1->nonprime.cpuTime, cms2->nonprime.cpuTime);
	}	
		
}

static int
sort_number(const void *ptr1, const void *ptr2)
{

	struct cms *cms1 = ((struct cms *) ptr1);
	struct cms *cms2 = ((struct cms *) ptr2);

	/* either or neither specified */
	if ( thisApp.args.prime == thisApp.args.nonprime) {

		return MAX(SUM(cms1, procCount), SUM(cms2, procCount));

	}
	else if ( thisApp.args.prime ) {

		return MAX(cms1->prime.procCount, cms2->prime.procCount);

	}
	else {
		return MAX(cms1->nonprime.procCount, cms2->nonprime.procCount);
	}	
		
}

static int
sort_kcore(const void *ptr1, const void *ptr2)
{

	struct cms *cms1 = ((struct cms *) ptr1);
	struct cms *cms2 = ((struct cms *) ptr2);


	/* either or neither specified */
	if ( thisApp.args.prime == thisApp.args.nonprime) {

		return MAX(SUM(cms1, kcoreTime), SUM(cms2, kcoreTime));

	}
	else if ( thisApp.args.prime ) {

		return MAX(cms1->prime.kcoreTime, cms2->prime.kcoreTime);

	}
	else {
		return MAX(cms1->nonprime.kcoreTime, cms2->nonprime.kcoreTime);
	}	
		
}

static void
compress_hash()
{
	int i, j;

	for ( i=0, j=0; i < thisApp.nrecords; i++ ) {

		if ( thisApp.records[i].cmsCmdName[0] ) {

			memmove(thisApp.records[j].cmsCmdName, thisApp.records[i].cmsCmdName, 
				strlen(thisApp.records[i].cmsCmdName)+1);

			thisApp.records[j].prime.procCount = thisApp.records[i].prime.procCount;
			thisApp.records[j].nonprime.procCount = thisApp.records[i].nonprime.procCount;

			thisApp.records[j].prime.cpuTime = thisApp.records[i].prime.cpuTime;
			thisApp.records[j].nonprime.cpuTime = thisApp.records[i].nonprime.cpuTime;

			thisApp.records[j].prime.systemTime = thisApp.records[i].prime.systemTime;
			thisApp.records[j].nonprime.systemTime = thisApp.records[i].nonprime.systemTime;

			thisApp.records[j].prime.realTime = thisApp.records[i].prime.realTime;
			thisApp.records[j].nonprime.realTime = thisApp.records[i].nonprime.realTime;

			thisApp.records[j].prime.kcoreTime = thisApp.records[i].prime.kcoreTime;
			thisApp.records[j].nonprime.kcoreTime = thisApp.records[i].nonprime.kcoreTime;

			thisApp.records[j].prime.kvirtualTime = thisApp.records[i].prime.kvirtualTime;
			thisApp.records[j].nonprime.kvirtualTime = thisApp.records[i].nonprime.kvirtualTime;

			thisApp.records[j].prime.highWaterCoreMem = thisApp.records[i].prime.highWaterCoreMem;
			thisApp.records[j].nonprime.highWaterCoreMem = thisApp.records[i].nonprime.highWaterCoreMem;

			thisApp.records[j].prime.highWaterVirtMem = thisApp.records[i].prime.highWaterVirtMem;
			thisApp.records[j].nonprime.highWaterVirtMem = thisApp.records[i].nonprime.highWaterVirtMem;


			thisApp.records[j].prime.minorFaultCount = thisApp.records[i].prime.minorFaultCount;
			thisApp.records[j].nonprime.minorFaultCount = thisApp.records[i].nonprime.minorFaultCount;

			thisApp.records[j].prime.majorFaultCount = thisApp.records[i].prime.majorFaultCount;
			thisApp.records[j].nonprime.majorFaultCount = thisApp.records[i].nonprime.majorFaultCount;

			thisApp.records[j].prime.charsRead = thisApp.records[i].prime.charsRead;
			thisApp.records[j].nonprime.charsRead = thisApp.records[i].nonprime.charsRead;

			thisApp.records[j].prime.charsWritten = thisApp.records[i].prime.charsWritten;
			thisApp.records[j].nonprime.charsWritten = thisApp.records[i].nonprime.charsWritten;

			thisApp.records[j].prime.readSysCalls = thisApp.records[i].prime.readSysCalls;
			thisApp.records[j].nonprime.readSysCalls = thisApp.records[i].nonprime.readSysCalls;

			thisApp.records[j].prime.writeSysCalls = thisApp.records[i].prime.writeSysCalls;
			thisApp.records[j].nonprime.writeSysCalls = thisApp.records[i].nonprime.writeSysCalls;

			thisApp.records[j].prime.cpuDelayTime = thisApp.records[i].prime.cpuDelayTime;
			thisApp.records[j].nonprime.cpuDelayTime = thisApp.records[i].nonprime.cpuDelayTime;

			thisApp.records[j].prime.blkDelayTime = thisApp.records[i].prime.blkDelayTime;
			thisApp.records[j].nonprime.blkDelayTime = thisApp.records[i].nonprime.blkDelayTime;

			thisApp.records[j].prime.swpDelayTime = thisApp.records[i].prime.swpDelayTime;
			thisApp.records[j].nonprime.swpDelayTime = thisApp.records[i].nonprime.swpDelayTime;

			j++;
		}
	}

	thisApp.nrecords = j;

}

static void
print_record(struct cms *cms, int idx)
{

#define SUM_OR_SIMPLE(a,b) \
	(thisApp.args.prime && thisApp.args.nonprime ? 	\
	 (cms->prime.a + cms->nonprime.a) :		\
	 ((b == 0) ?					\
	  cms->prime.a :				\
	  cms->nonprime.a				\
	 ) )

/*
#define SUM_OR_SIMPLE(a,b) (thisApp.args.prime && thisApp.args.nonprime ? (a)[PRIME]+(a)[NONPRIME] : a[b])
 */
	
	if (SUM_OR_SIMPLE(procCount,idx) == 0)
		return;
	printf("%-16.16s", cms->cmsCmdName);
	printf(" %8lld", SUM_OR_SIMPLE(procCount,idx));
	printf(" %12.2f", SUM_OR_SIMPLE(kcoreTime, idx));
	printf(" %12.2f", SUM_OR_SIMPLE(kvirtualTime, idx));
	printf(" %10.2f", SUM_OR_SIMPLE(cpuTime,idx));
	printf(" %12.2f", SUM_OR_SIMPLE(realTime,idx));

	if (SUM_OR_SIMPLE(cpuTime, idx) == 0)
		if (idx == 0)
			cms->prime.cpuTime = 1;
		else
			cms->nonprime.cpuTime = 1;

	printf(" %9.2f", SUM_OR_SIMPLE(kcoreTime, idx) / SUM_OR_SIMPLE(cpuTime, idx));
	printf(" %9.2f", SUM_OR_SIMPLE(kvirtualTime, idx) / SUM_OR_SIMPLE(cpuTime, idx));

	if (SUM_OR_SIMPLE(procCount, idx) == 0)
		if (idx == 0)
			cms->prime.procCount = 1;
		else
			cms->nonprime.procCount = 1;

	printf(" %8.2f", SUM_OR_SIMPLE(cpuTime, idx) / SUM_OR_SIMPLE(procCount, idx));

	if (SUM_OR_SIMPLE(realTime, idx) == 0)
		if (idx == 0)
			cms->prime.realTime = 1;
		else
			cms->nonprime.realTime = 1;

	printf(" %6.2f", SUM_OR_SIMPLE(cpuTime, idx) / SUM_OR_SIMPLE(realTime, idx));
	printf(" %11.0f", SUM_OR_SIMPLE(charsRead, idx) / 1024);
	printf(" %11.0f", SUM_OR_SIMPLE(charsWritten, idx) / 1024);

	if (thisApp.args.extended) {
		printf(" %11.0f", cms->prime.highWaterCoreMem);
		printf(" %11.0f", cms->prime.highWaterVirtMem);
		printf(" %9.0f", SUM_OR_SIMPLE(readSysCalls, idx));
		printf(" %9.0f", SUM_OR_SIMPLE(writeSysCalls, idx));
		printf(" %10.2f", SUM_OR_SIMPLE(systemTime, idx));
		printf(" %9.0f", SUM_OR_SIMPLE(minorFaultCount, idx));
		printf(" %9.0f", SUM_OR_SIMPLE(majorFaultCount, idx));
		printf(" %11.2f", SUM_OR_SIMPLE(cpuDelayTime, idx));
		printf(" %11.2f", SUM_OR_SIMPLE(blkDelayTime, idx));
		printf(" %11.2f", SUM_OR_SIMPLE(swpDelayTime, idx));
	}

	printf("\n");

}


static void
add_cms_records (struct cms *from, struct cms *to)
{
	to->prime.procCount += from->prime.procCount;
	to->nonprime.procCount += from->nonprime.procCount;

	to->prime.cpuTime += from->prime.cpuTime;
	to->nonprime.cpuTime += from->nonprime.cpuTime;

	to->prime.systemTime += from->prime.systemTime;
	to->nonprime.systemTime += from->nonprime.systemTime;

	to->prime.realTime += from->prime.realTime;
	to->nonprime.realTime += from->nonprime.realTime;

	to->prime.kcoreTime += from->prime.kcoreTime;
	to->nonprime.kcoreTime += from->nonprime.kcoreTime;

	to->prime.kvirtualTime += from->prime.kvirtualTime;
	to->nonprime.kvirtualTime += from->nonprime.kvirtualTime;

	to->prime.highWaterCoreMem += from->prime.highWaterCoreMem;
	to->nonprime.highWaterCoreMem += from->nonprime.highWaterCoreMem;

	to->prime.highWaterVirtMem += from->prime.highWaterVirtMem;
	to->nonprime.highWaterVirtMem += from->nonprime.highWaterVirtMem;

	to->prime.minorFaultCount += from->prime.minorFaultCount;
	to->nonprime.minorFaultCount += from->nonprime.minorFaultCount;

	to->prime.majorFaultCount += from->prime.majorFaultCount;
	to->nonprime.majorFaultCount += from->nonprime.majorFaultCount;

	to->prime.charsRead += from->prime.charsRead;
	to->nonprime.charsRead += from->nonprime.charsRead;

	to->prime.charsWritten += from->prime.charsWritten;
	to->nonprime.charsWritten += from->nonprime.charsWritten;

	to->prime.readSysCalls += from->prime.readSysCalls;
	to->nonprime.readSysCalls += from->nonprime.readSysCalls;

	to->prime.writeSysCalls += from->prime.writeSysCalls;
	to->nonprime.writeSysCalls += from->nonprime.writeSysCalls;

	to->prime.cpuDelayTime += from->prime.cpuDelayTime;
	to->nonprime.cpuDelayTime += from->nonprime.cpuDelayTime;

	to->prime.blkDelayTime += from->prime.blkDelayTime;
	to->nonprime.blkDelayTime += from->nonprime.blkDelayTime;

	to->prime.swpDelayTime += from->prime.swpDelayTime;
	to->nonprime.swpDelayTime += from->nonprime.swpDelayTime;
}

