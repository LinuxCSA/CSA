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
#include <time.h>  // for timezone handling

#include <getopt.h> // use GNU's version of getopt 

#include <regex.h>

#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

#include <linux/acct.h>

#include "acctmsg.h"
#include "acctdef.h"
#include "cms.h"

/* #define DEBUG 1 */

#define PROCESS_PRIVILEGED ((struct passwd *) -1)
#define PROCESS_UNKNOWN ((struct passwd *) -2)

#define GIVE_ME_MORE __LINE__
#define THAT_IS_ALL 1

/* getopt options */
struct option long_options[] = {

	{ 0, 0, 0, 0 }
};

typedef struct _AppData {

	struct _args {
		char * outfile;
		time_t existing_before;
		struct group * group;
		u_int64_t jid;
		char * cmd;
		time_t existing_after;
		struct passwd * user;
		int total_time;
		time_t ending_before; 
		double hog_factor;
		int io_transfer;
		int system_time;
		time_t start_after;

		struct {
			int backwards;
			int read_sep;
			int hex_flags;
			int hog_factor;
			int io_written;
			int kernel_minutes;
			int mem_size;
			int pid;
			int cpu_factor;
			int separate_times;
			int no_headings;
			int session_handle;
			int grp_ids;
			int job_ids;
			int wait_times;
			int high_water;
			int nice;
			int proj_id;
			int job_data;
			int numeric_uids;
			int numeric_gids;
			int combine_io_times;
			int date_changes;
			int proc_start;
			int proc_end;
			int skip_fields;
			int config_records; /* this is intentionally last in the struct */
		} print;
		
	} args;

	struct {
		int day_count;
		time_t now;
		time_t begin_time;
		time_t current_start; // current records start time
		time_t current_midnight; // midnight for now

		/* like the counter parts in args, but are absolute */
		time_t start_after;
		time_t existing_before;
		time_t existing_after;
		time_t ending_before;

		struct {
			int cfg;
			int start_of_job;
			int end_of_job;
			int cmd;
		} totals;

	} calcs;

	struct {

		int mem_integral;

	} settings;

	int out_fd;
	int exit;

} AppData;

typedef struct {

	u_int64_t elapsed;
	u_int64_t user_time;
	u_int64_t sys_time;
	u_int64_t cpu_time;
	u_int64_t corehimem;
	u_int64_t virthimem;
	u_int64_t coremem;
	u_int64_t virtmem;
	u_int64_t min_fault;
	u_int64_t maj_fault;
	u_int64_t scr;
	u_int64_t scw;
	u_int64_t chr;
	u_int64_t chw;
	time_t end_time;
	struct acctdelay delay;
} Rec;

Rec thisRec = {0};

AppData thisApp = {0};

/* internal function prototypes */

static int parse_and_validate_args(int argc, char **argv);
static int parse_time(const char *str, time_t *time);
static void print_config_record(struct acctent *);
static time_t midnight(time_t t);
static int process_single_record(struct acctent *acct);
static void print_record(struct acctent *acct);
static void process_input_file(char *acct_str);

int
main(int argc, char **argv)
{
	int argv_files;
	char *acct_str;

	/* initialize all application data */
	memset(&thisApp, 0, sizeof(thisApp));

	thisApp.settings.mem_integral = init_int(MEMORY_INTEGRAL, 1, TRUE );

	if ( thisApp.settings.mem_integral < 1 || thisApp.settings.mem_integral > 3 ) {

		acct_err(ACCT_FATAL, "Invalid value for MEMORY_INTEGRAL.");
	}

	argv_files = parse_and_validate_args(argc, argv);

	tzset();

	/* loop through all files supplied on cmd line */
	if ( argv_files && argv[argv_files] ) {
		while ( argv_files < argc ) {

			process_input_file(argv[argv_files]);

			argv_files++;
		}
	}
	else {
		char *stdin_is_tty = ttyname(STDIN_FILENO);
		int stdin_is_dev_null = 0;

		if (stdin_is_tty || stdin_is_dev_null) {
			/* check /etc/csa.conf */
			acct_str = init_char("PACCT_FILE", "/var/csa/day/pacct", TRUE);
		}
		else {
			thisApp.args.print.backwards = 0; /* stdin has to be processed forward */
			acct_str = NULL; /* tell process_input_file() to use stdin */
		}

		process_input_file(acct_str);
	}


	/* summary output */


	/* keep strict compilers happy */
	return thisApp.exit;

}



/* parse ARGV and return the number of items handled */
static int
parse_and_validate_args(int argc, char **argv)
{
	int c;
	int idx=0;

	thisApp.args.hog_factor = 0.0;

	while ( (c=getopt_long(argc, argv, 
					"AC:E:GH:I:JLMNO:PS:TUVWXYZbce:fg:hij:kmn:o:prs:tu:vw", 
					long_options, &idx)) != -1 ) {


		switch ( c ) { 

			case 'A':
				thisApp.args.print.session_handle = TRUE;
				break;

			case 'C':
				errno=0;
				thisApp.args.total_time = (int) strtol(optarg, 
						(char **) NULL, 10);

				if ( errno )
					acct_perr(ACCT_WARN, errno, "-C %s",optarg);

				break;

			case 'E':

				if ( parse_time(optarg, &thisApp.args.ending_before) )
					acct_err(ACCT_WARN, "-E %s: bad format\n",optarg);
				
				break;

			case 'G':
				thisApp.args.print.numeric_gids = TRUE;
				break;

			case 'H':
				errno = 0;
				thisApp.args.hog_factor = strtod(optarg, (char **) NULL);
				if ( errno )
					acct_perr(ACCT_WARN, errno, "-H %s",optarg);
				break;

			case 'I':
				errno = 0;
				thisApp.args.io_transfer = (int) strtol(optarg, (char **) NULL, 10);
				if ( errno )
					acct_perr(ACCT_WARN, errno, "-I %s",optarg);

				break;

			case 'J':
				thisApp.args.print.job_ids = TRUE;
				break;

			case 'L':
				thisApp.args.print.config_records = TRUE;
				break;

			case 'M':
				thisApp.args.print.high_water = TRUE;
				break;

			case 'N':
				thisApp.args.print.nice = TRUE;
				break;

			case 'O':
				errno = 0;
				thisApp.args.system_time = (int) strtol(optarg, (char **) NULL, 10);
				if ( errno )
					acct_perr(ACCT_WARN, errno, "-O %s",optarg);

				break;

			case 'P':
				thisApp.args.print.proj_id = TRUE;
				break;

			case 'S':
				if ( parse_time(optarg, &thisApp.args.start_after) )
					acct_err(ACCT_WARN, "-S %s: bad format\n",optarg);
				break;

			case 'T':
				thisApp.args.print.job_data = TRUE;
				break;

			case 'U':
				thisApp.args.print.numeric_uids = TRUE;
				break;

			case 'V':
				thisApp.args.print.combine_io_times = TRUE;
				break;

			case 'W':
				thisApp.args.print.date_changes = TRUE;
				break;

			case 'X':
				thisApp.args.print.proc_start = TRUE;
				break;

			case 'Y':
				thisApp.args.print.proc_end = TRUE;
				break;

			case 'Z':
				thisApp.args.print.skip_fields = TRUE;
				break;

			case 'b':
				thisApp.args.print.backwards = TRUE;
				break;

			case 'c':
				thisApp.args.print.read_sep = TRUE;
				break;

			case 'e':
				if ( parse_time(optarg, &thisApp.args.existing_before) )
					acct_err(ACCT_WARN, "-e %s: bad format\n",optarg);
				break;

			case 'f':
				thisApp.args.print.hex_flags = TRUE;
				break;

			case 'g':
				thisApp.args.group = getgrnam(optarg);

				if ( thisApp.args.group == NULL ) {
					static struct group g;
					gid_t gid;

					gid = (gid_t) strtol(optarg, (char **) NULL, 10);

					g.gr_gid = gid;

					thisApp.args.group = &g;

				}	

				if ( thisApp.args.group == NULL )
					acct_err(ACCT_WARN, "-g %s: unknown group\n",optarg);

				break;

			case 'h':
				thisApp.args.print.hog_factor = TRUE;
				break;

			case'i':
				thisApp.args.print.io_written = TRUE;
				break;

			case 'j':
				errno=0;
					
				thisApp.args.jid = strtoull(optarg, (char **) NULL, 16);

				if ( errno )
					acct_perr(ACCT_WARN, errno, "-j %s",optarg);

				break;

			case 'k':
				thisApp.args.print.kernel_minutes = TRUE;
				break;

			case 'm':
				thisApp.args.print.mem_size = TRUE;
				break;

			case 'n':
				thisApp.args.cmd = strdup(optarg);
				break;

			case 'o':
				thisApp.args.outfile = strdup(optarg);
				break;

			case 'p':
				thisApp.args.print.pid = TRUE;
				break;

			case 'r':
				thisApp.args.print.cpu_factor = TRUE;
				break;

			case 's':
				if ( parse_time(optarg, &thisApp.args.existing_after) )
					acct_err(ACCT_WARN, "-s %s: bad format\n",optarg);

				break;

			case 't':
				thisApp.args.print.separate_times = TRUE;
				break;

			case 'u':

				if ( strstr(optarg,"#" )) {
					thisApp.args.user = PROCESS_PRIVILEGED;
				}
				if ( strstr(optarg,"?" )) {
					thisApp.args.user = PROCESS_UNKNOWN;
				}

				if ( thisApp.args.user == NULL ) {

					thisApp.args.user = getpwnam(optarg);

					if ( thisApp.args.user == NULL ) {
						static struct passwd u;
						char *cptr;
						uid_t uid = -1;

						cptr = optarg;
						while (isspace(*cptr))
							cptr++;
						/* don't need to skip '-'.
						 * negative uid would fail.
						 */
						if (*cptr=='+')
							cptr++;
						if (isdigit(*cptr)) {
							uid = (uid_t) strtol(cptr, (char **) NULL, 10);
							u.pw_uid = uid;

							thisApp.args.user = &u;
						}

					}	
				}

				if ( thisApp.args.user == NULL )
					acct_err(ACCT_ABORT,
					   "-u %s: unknown user\n",optarg);

				break;
				

			case 'v':

				thisApp.args.print.no_headings = TRUE;
				break;

			case 'w':
				thisApp.args.print.wait_times++;
				break;

			case '?':
			default:
				acct_err(ACCT_ABORT, CO_USAGE,"csacom");
		}
	}


	/* check supplied options for validity */

	/* if outfile is specified all printing is suppressed */
	if ( thisApp.args.outfile /* -o */ ) {

		memset(&thisApp.args.print, 0, sizeof(thisApp.args.print));

		errno=0;
		thisApp.out_fd = openacct(thisApp.args.outfile, "w");

		if ( thisApp.out_fd == -1 ) { 
			acct_perr(ACCT_ABORT, errno, 
					"Error occured opening output file '%s'.", 
					thisApp.args.outfile);

		}
	}

	if ( thisApp.args.print.backwards /* -b */ 
			&& thisApp.args.print.date_changes /* -W */ ) {

		acct_err(ACCT_FATAL, "-b conflicts with -W.\n");

	}

	/* the default print is -m (mem_size), but only if nothing else
	 * is requested.
	 *
	 * if -L is the only requested print option then turn on -m
	 */
	if ( (memchr(&thisApp.args.print, 1, sizeof(thisApp.args.print)) == NULL) ||
		 (memchr(&thisApp.args.print, 1, sizeof(thisApp.args.print)) == &thisApp.args.print.config_records )) {
			thisApp.args.print.mem_size = TRUE;
	}
	if (thisApp.args.print.wait_times > 1)
		thisApp.args.print.mem_size = FALSE;


	if ( thisApp.args.print.job_data /* -T */ ) {

		int b, v, L;

		b = thisApp.args.print.backwards;
		v = thisApp.args.print.no_headings;
		L = thisApp.args.print.config_records;

		memset(&thisApp.args.print, 0, sizeof(thisApp.args.print));
			
		thisApp.args.print.backwards = b;
		thisApp.args.print.no_headings = v;
		thisApp.args.print.config_records = L;

		thisApp.args.print.job_data = TRUE;
		
	}

	if ( thisApp.args.print.combine_io_times /* -V */ && 
			! ( thisApp.args.print.io_written /* -i */ || 
				thisApp.args.print.read_sep /* -c */ ||
				thisApp.args.print.wait_times /* -w */)) {
		acct_err(ACCT_FATAL, "-V must be specified with one of -c, -i or -w.\n");
	}

	/* if -W then turn off all other options except -L */
	if ( thisApp.args.print.date_changes /* -W */ ) {

		int L;

		L = thisApp.args.print.config_records;

		memset(&thisApp.args, 0, sizeof(thisApp.args));

		thisApp.args.print.config_records = L;
		thisApp.args.print.date_changes = TRUE;

	}
	
	
	/* return the start of the non-option entries in ARGV */

	return optind;
}


/* return zero on success, anything else is a failure. 
 *
 * returned time is relative... its needs to be added to the record start
 * time - crappy design that we need to continue for compatability reasons.
 *
 * valid format:  [Ddd:]hh[:mm[:ss]]
 *
 */
static int
parse_time(const char *str, time_t *time)
{
	int day, hr, min, sec;
	time_t t;
	
	day = hr = min = sec = 0;

	*time = t = 0;

	/* The string must start with a 'D', a 'd', or a digit */
	if (!isdigit(str[0]) && str[0] != 'D' && str[0] != 'd') {
		acct_err(ACCT_FATAL, "Invalid time argument: %s\n", str);
		return 1;
	}

	if (isdigit(str[0])) {
		/* We must have at least the hh */
		if (sscanf(str, "%d:%d:%d", &hr, &min, &sec) < 1) {
			acct_err(ACCT_FATAL, 
			    "Invalid time arguemnt: %s. HOUR is required.\n",
			    str);
			return 1;
		}
	} else {
		/* the character after 'D' or 'd' must be a digit */
		if (!isdigit(str[1])) {
			acct_err(ACCT_FATAL, "Invalid time argument: %s\n",str);
			return 1;
		}
		/* We must at least have Day and Hour */
		if (sscanf(str+1, "%d:%d:%d:%d", &day, &hr, &min, &sec) < 2) {
			acct_err(ACCT_FATAL, 
			    "Invalid time arguemnt: %s. HOUR is required.\n",
			    str);
			return 1;
		}
	}
#ifdef DEBUG
	printf("day=%d, hr=%d, min=%d, sec=%d\n", day, hr, min, sec);
#endif

	/* Validate day, hr, min, and sec */
	if ( (day < 0) ||
	    ( (hr > 23)  || (hr < 0) )  ||
	    ( (min > 59) || (min < 0) ) ||
	    ( (sec > 59) || (sec < 0) ) ) {
		acct_err(ACCT_FATAL, 
	    	    "Invalid time arguemnt: day=%d, hr=%d, min=%d, sec=%d\n",
		    day, hr, min, sec);
		return 1;
	}

	t += ( day * 86400 );
	t += ( hr * 3600 );
	t += ( min * 60 );
	t += ( sec );

	*time = t;

	return 0;
}


/* returns the time_t at midnight for the day that contains 't'
 */
static time_t
midnight(time_t t)
{

	if ( localtime(&t)->tm_isdst ) 
		return ( (t+3600) - ((t+3600) % 86400 ) );
	else
		return ( t - (t % 86400 ) );
}


static void
print_config_record(struct acctent *acct)
{

	struct ac_utsname *uname;
	char t[21];

	if (acct->cfg == NULL)
		return;
	else
		uname = &acct->cfg->ac_uname;

	fprintf(stdout,"*** System: %s %s %s %s %s\n",
			uname->sysname, uname->nodename, uname->release, 
			uname->version, uname->machine);

	strftime(t,20,"%T %D",localtime(&acct->cfg->ac_curtime));

	switch ( acct->cfg->ac_event ) {
		case AC_CONFCHG_BOOT:
			/* need different value of time */
			strftime(t,20,"%T %D",localtime(&acct->cfg->ac_boottime));
			fprintf(stdout,"*** Accounting boot record %s\n",t);
			break;

		case AC_CONFCHG_FILE:
			fprintf(stdout,"*** Accounting file switched at %s\n",t);
			fprintf(stdout,
					"    Kernel & Daemon mask = 0%05o, Record mask = 0%05o\n",
					acct->cfg->ac_kdmask, acct->cfg->ac_rmask);
			break;

		case AC_CONFCHG_ON:
			fprintf(stdout,"*** Accounting started at %s\n",t);
			fprintf(stdout,
					"    Kernel & Daemon mask = 0%05o, Record mask = 0%05o\n",
					acct->cfg->ac_kdmask, acct->cfg->ac_rmask);
			break;

		case AC_CONFCHG_OFF:
			fprintf(stdout,"*** Accounting stopped at %s\n",t);
			fprintf(stdout,
					"    Kernel & Daemon mask = 0%05o, Record mask = 0%05o\n",
					acct->cfg->ac_kdmask, acct->cfg->ac_rmask);
			break;

		default:
			fprintf(stdout,"*** Accounting changed at %s\n",t);
			fprintf(stdout,
					"    Kernel & Daemon mask = 0%05o, Record mask = 0%05o\n",
					acct->cfg->ac_kdmask, acct->cfg->ac_rmask);
			break;
	}

}


static int
process_single_record(struct acctent *acct)
{

	time_t start, end;


	memset(&thisRec, 0, sizeof(thisRec));
		
	if ( acct->eoj ) {

		if ( thisApp.args.print.job_data ) {

			/* Note: eoj->ac_etime is in [secs since 1970] */
			end = acct->eoj->ac_etime;

			if ( ( thisApp.calcs.existing_after && 
						end < thisApp.calcs.existing_after ) ||
				   	( thisApp.calcs.ending_before && 
					  end > thisApp.calcs.ending_before ) )
				return GIVE_ME_MORE;

			if ( thisApp.args.jid && 
					( thisApp.args.jid != acct->eoj->ac_jid ) )
				return GIVE_ME_MORE;

			if ( thisApp.args.user && 
					( thisApp.args.user != PROCESS_PRIVILEGED ) &&
					( thisApp.args.user != PROCESS_UNKNOWN ) &&
					( thisApp.args.user->pw_uid != acct->eoj->ac_uid ))
				return GIVE_ME_MORE;

			if ( thisApp.args.user == PROCESS_UNKNOWN && 
					uid_to_name(acct->eoj->ac_uid)[0] != '?' ) 
				return GIVE_ME_MORE;

			
			fprintf(stdout,"%#18llx", acct->eoj->ac_jid);
			fprintf(stdout," %8d", acct->eoj->ac_uid);
			fprintf(stdout," %8.8s", &ctime(&acct->eoj->ac_etime)[11]);
			fprintf(stdout," %7.7s", "E-O-Job");
			fprintf(stdout," %10lld", acct->eoj->ac_corehimem);
			fprintf(stdout," %10lld", acct->eoj->ac_virthimem);
			fprintf(stdout," %4d", acct->eoj->ac_nice);
			fprintf(stdout," %8d", acct->eoj->ac_gid);
			fprintf(stdout,"\n");
			
			thisApp.calcs.totals.end_of_job++;

			return GIVE_ME_MORE;
		}
		else
			return GIVE_ME_MORE;
	}

	if ( acct->soj ) {

		if ( thisApp.args.print.job_data ) {

			start = (acct->soj->ac_type == AC_SOJ ? acct->soj->ac_btime : acct->soj->ac_rstime );

			if ( (thisApp.calcs.start_after && 
						start < thisApp.calcs.start_after) || 
					(thisApp.args.existing_before && 
					 start > thisApp.args.existing_before) )
				return GIVE_ME_MORE;

			if ( thisApp.args.jid 
					&& ( thisApp.args.jid != acct->soj->ac_jid ) )
				return GIVE_ME_MORE;

			if ( thisApp.args.user &&
					( thisApp.args.user != PROCESS_PRIVILEGED ) &&
					( thisApp.args.user != PROCESS_UNKNOWN ) &&
					( thisApp.args.user->pw_uid != acct->soj->ac_uid ))
				return GIVE_ME_MORE;

			if ( thisApp.args.user == PROCESS_UNKNOWN && 
					uid_to_name(acct->soj->ac_uid)[0] != '?' ) 
				return GIVE_ME_MORE;

			
			fprintf(stdout,"%#18llx", acct->soj->ac_jid);
			fprintf(stdout," %8d", acct->soj->ac_uid);
			if ( acct->soj->ac_type == AC_SOJ ) {
				fprintf(stdout," %8.8s", &ctime(&acct->soj->ac_btime)[11]);
				fprintf(stdout," %7.7s", "S-O-Job");
			}
			else {
				fprintf(stdout," %8.8s", &ctime(&acct->soj->ac_rstime)[11]);
				fprintf(stdout," %7.7s", "R-O-Job");
			}
			fprintf(stdout,"\n");
			
			thisApp.calcs.totals.start_of_job++;

			return GIVE_ME_MORE;
		}
		else
			return GIVE_ME_MORE;
	}	

	if ( acct->cfg ) {

		if ( thisApp.args.print.config_records ) {

			start = acct->cfg->ac_curtime;

			/* these two "if's"  looks bogus, but its what the spec says */

			if ( ( thisApp.calcs.existing_after && 
						start < thisApp.calcs.existing_after) || 
					( thisApp.calcs.ending_before && 
					  start > thisApp.calcs.ending_before) )
				return GIVE_ME_MORE;

			if ( (thisApp.calcs.start_after && 
						start < thisApp.calcs.start_after) || 
					( thisApp.calcs.existing_before && 
					  start > thisApp.calcs.existing_before ))
				return GIVE_ME_MORE;

			thisApp.calcs.totals.cfg++;

			print_config_record(acct);

			return GIVE_ME_MORE;

		}
	}
		
	if ( thisApp.args.print.job_data ) {
		return GIVE_ME_MORE;
	}

	/* ignoring daemon records for now */
	if ( acct->wmbs )
		return GIVE_ME_MORE;

	if ( acct->csa == NULL ) 
		return GIVE_ME_MORE;
//		return ( thisApp.args.print.backwards ? GIVE_ME_MORE : THAT_IS_ALL );

	start = acct->csa->ac_btime;
	thisRec.elapsed = acct->csa->ac_etime;

	if ( thisRec.elapsed == 0 )
		thisRec.elapsed++;

	thisRec.end_time = end = start + TIME_2_SECS(thisRec.elapsed);

	if ( thisApp.calcs.existing_after && end < thisApp.calcs.existing_after ) 
		return ( thisApp.args.print.backwards ? THAT_IS_ALL : GIVE_ME_MORE );
	if ( thisApp.calcs.ending_before && end > thisApp.calcs.ending_before ) 
		return ( thisApp.args.print.backwards ? GIVE_ME_MORE : THAT_IS_ALL );

	if ( ( thisApp.calcs.start_after && start < thisApp.calcs.start_after ) ||
		( thisApp.calcs.existing_before && start > thisApp.calcs.existing_before ) )
		return GIVE_ME_MORE;


	if ( thisApp.args.user == PROCESS_PRIVILEGED &&
			!( acct->csa->ac_hdr1.ah_flag & ASU ) )
		return GIVE_ME_MORE;

	thisRec.sys_time = acct->csa->ac_stime;
	thisRec.user_time = acct->csa->ac_utime;
	thisRec.cpu_time = thisRec.sys_time + thisRec.user_time;

	if ( thisRec.cpu_time == 0 )
		thisRec.cpu_time = 1;

	if ( acct->io ) {
		thisRec.chr = acct->io->ac_chr;
		thisRec.chw = acct->io->ac_chw;
	}

	if ( thisApp.args.total_time && 
			thisApp.args.total_time >= TIME_2_SECS( thisRec.cpu_time ) )
		return GIVE_ME_MORE;

	if ( thisApp.args.system_time && 
			thisApp.args.system_time >= TIME_2_SECS( thisRec.sys_time ) ) {
		return GIVE_ME_MORE;
	}

	if ( thisApp.args.jid && ( thisApp.args.jid != acct->csa->ac_jid ))
		return GIVE_ME_MORE;

	if ( thisApp.args.user && 
			( thisApp.args.user != PROCESS_PRIVILEGED ) &&
			( thisApp.args.user != PROCESS_UNKNOWN ) &&
			( thisApp.args.user->pw_uid != acct->csa->ac_uid ))
		return GIVE_ME_MORE;

	if ( thisApp.args.user == PROCESS_UNKNOWN && 
			( uid_to_name(acct->csa->ac_uid)[0] != '?' ) )
		return GIVE_ME_MORE;

	if ( thisApp.args.group && 
			( thisApp.args.group->gr_gid != acct->csa->ac_gid ))
		return GIVE_ME_MORE;

	if ( thisApp.args.cmd && 
			(strstr(acct->csa->ac_comm,thisApp.args.cmd) == NULL))
		return GIVE_ME_MORE;

	if ( thisApp.args.io_transfer && 
			thisApp.args.io_transfer > (thisRec.chr + thisRec.chw) )
		return GIVE_ME_MORE;

	if ( thisApp.args.hog_factor && 
			thisApp.args.hog_factor >= ((double) thisRec.cpu_time / (double) thisRec.elapsed) )
		return GIVE_ME_MORE;
	

	if ( acct->mem != NULL ) { 

		thisRec.corehimem = acct->mem->ac_core.himem;
		thisRec.virthimem = acct->mem->ac_virt.himem;
		thisRec.min_fault = acct->mem->ac_minflt;
		thisRec.maj_fault = acct->mem->ac_majflt;

		switch ( thisApp.settings.mem_integral ) {

			case 1:
				thisRec.coremem = acct->mem->ac_core.mem1;
				thisRec.virtmem = acct->mem->ac_virt.mem1;
				break;

			case 2:
				thisRec.coremem = acct->mem->ac_core.mem2;
				thisRec.virtmem = acct->mem->ac_virt.mem2;
				break;

			case 3:
				thisRec.coremem = acct->mem->ac_core.mem3;
				thisRec.virtmem = acct->mem->ac_virt.mem3;
				break;

		}

	}

	if ( acct->io != NULL ) { 

		thisRec.scr = acct->io->ac_scr;
		thisRec.scw = acct->io->ac_scw;
	}

	if ( acct->delay != NULL )
		memcpy(&thisRec.delay, acct->delay, sizeof(struct acctdelay));
	else
		memset(&thisRec.delay, 0, sizeof(struct acctdelay));

	if ( thisApp.args.outfile ) {
		if ( writeacctent(thisApp.out_fd, acct) <= 0 ) {
		
			thisApp.exit = -1;
			return THAT_IS_ALL;
		} else
			return GIVE_ME_MORE;
	}
	else
		print_record(acct);
	
	/* add to total... */
	thisApp.calcs.totals.cmd++;

	return GIVE_ME_MORE;
}


static void
print_record(struct acctent *acct)
{

	char name[32];
	
	if ( !(acct->csa->ac_hdr1.ah_flag & ASU ) ) 
		strncpy(name, acct->csa->ac_comm, 16 );
	else {
		name[0] = '#';
	
		strncpy(&name[1], acct->csa->ac_comm, 16 );
	}
	

	if ( ! thisApp.args.print.skip_fields ) {
		fprintf(stdout,"%-17.17s", name);

		strncpy(name,uid_to_name(acct->csa->ac_uid), 8);

		if (  name[0] != '?' && thisApp.args.print.numeric_uids == 0 )
			fprintf(stdout," %-8.8s", name);
		else
			fprintf(stdout," %-8d", acct->csa->ac_uid);


		fprintf(stdout," %8.8s",&ctime(&acct->csa->ac_btime)[11]);
		fprintf(stdout," %8.8s",&ctime(&thisRec.end_time)[11]);
		fprintf(stdout," %8.2f",TIME_2_SECS(thisRec.elapsed));
	}

	if ( thisApp.args.print.separate_times ) {

		fprintf(stdout," %8.2f", TIME_2_SECS(thisRec.sys_time));
		fprintf(stdout," %8.2f", TIME_2_SECS(thisRec.user_time));
		fprintf(stdout,"   %11.2f", NSEC_2_SECS(thisRec.delay.ac_cpu_run_real_total));
		fprintf(stdout,"  %11.2f", NSEC_2_SECS(thisRec.delay.ac_cpu_run_virtual_total));
	
	}
	else if ( ! thisApp.args.print.skip_fields )
		fprintf(stdout," %8.2f", TIME_2_SECS(thisRec.cpu_time));

	if ( thisApp.args.print.io_written ) { 

		if ( thisApp.args.print.combine_io_times ) { 

			fprintf(stdout," %8lld", thisRec.chr + thisRec.chw );
		}
		else {
			fprintf(stdout," %8lld", thisRec.chr );
			fprintf(stdout," %8lld", thisRec.chw );
		}
	}

	if ( thisApp.args.print.cpu_factor )
		fprintf(stdout," %6.2f", 
				(double) thisRec.user_time / (double) thisRec.cpu_time);
		
	if ( thisApp.args.print.hog_factor )
		fprintf(stdout," %10.2f", 
				(double) thisRec.cpu_time / (double) thisRec.elapsed);

	if ( thisApp.args.print.mem_size ) {
		fprintf(stdout," %11.2f", 
				MB_2_KB(thisRec.coremem / (double) thisRec.cpu_time));
		fprintf(stdout," %11.2f",
				MB_2_KB(thisRec.virtmem / (double) thisRec.cpu_time));
	}

	if ( thisApp.args.print.kernel_minutes ) {
		fprintf(stdout," %11.2f", 
				TIME_2_MINS(MB_2_KB( thisRec.coremem)));
		fprintf(stdout," %11.2f", 
				TIME_2_MINS(MB_2_KB(thisRec.virtmem)));
	}

	if ( thisApp.args.print.hex_flags )
		fprintf(stdout, " %-2x %3d", 
				acct->csa->ac_hdr1.ah_flag, acct->csa->ac_stat);

	if ( thisApp.args.print.job_ids )
		fprintf(stdout, " %#18llx", acct->csa->ac_jid);

	if ( thisApp.args.print.session_handle )
		fprintf(stdout, " 0x%#016llx", acct->csa->ac_ash);

	if ( thisApp.args.print.proj_id )
		fprintf(stdout, " %8lld", acct->csa->ac_prid);

	if ( thisApp.args.print.numeric_gids )
		fprintf(stdout, " %8d", acct->csa->ac_gid);

	if ( thisApp.args.print.read_sep ) {
		if ( thisApp.args.print.combine_io_times ) {
			fprintf(stdout, " %8lld",thisRec.scr + thisRec.scw);
		}
		else {
			fprintf(stdout, " %8lld",thisRec.scr);
			fprintf(stdout, " %8lld",thisRec.scw);
		}
	}
	
	if ( thisApp.args.print.pid ) {
		/* braindead format, doesn't account for 32bit PIDs */
		fprintf(stdout, " %6d", acct->csa->ac_pid);
		fprintf(stdout, " %6d", acct->csa->ac_ppid);
	}
	
	if ( thisApp.args.print.wait_times ) {
		if ( thisApp.args.print.combine_io_times ) {
			fprintf(stdout, " %8.2f",
					NSEC_2_SECS(thisRec.delay.ac_cpu_delay_total +
						    thisRec.delay.ac_blkio_delay_total +
						    thisRec.delay.ac_swapin_delay_total));
		}
		else if ( thisApp.args.print.wait_times == 1 ) {
			fprintf(stdout, "   %12.2f", NSEC_2_SECS(thisRec.delay.ac_cpu_delay_total));
			fprintf(stdout, "   %15.2f", NSEC_2_SECS(thisRec.delay.ac_blkio_delay_total));
			fprintf(stdout, "   %13.2f", NSEC_2_SECS(thisRec.delay.ac_swapin_delay_total));
		}
		else {
			fprintf(stdout, "   %12.2f", NSEC_2_SECS(thisRec.delay.ac_cpu_delay_total));
			fprintf(stdout, "  %5d", thisRec.delay.ac_cpu_count);
			fprintf(stdout, "   %12.2f", NSEC_2_SECS(thisRec.delay.ac_blkio_delay_total));
			fprintf(stdout, "  %5d", thisRec.delay.ac_blkio_count);
			fprintf(stdout, "   %12.2f", NSEC_2_SECS(thisRec.delay.ac_swapin_delay_total));
			fprintf(stdout, "  %5d", thisRec.delay.ac_swapin_count);
		}

	}
	
	/* not in the spec or man page
	if ( thisApp.args.print.multi ) { 
	}
	*/

	if ( thisApp.args.print.high_water ) { 

		fprintf(stdout," %9lld",thisRec.corehimem);
		fprintf(stdout," %9lld",thisRec.virthimem);
		fprintf(stdout," %8lld",thisRec.min_fault);
		fprintf(stdout," %8lld",thisRec.maj_fault);
	}

	if ( thisApp.args.print.nice ) { 

		fprintf(stdout," %4d", acct->csa->ac_nice );
		fprintf(stdout," %5d", acct->csa->ac_sched );

	}

	if ( thisApp.args.print.proc_start ) { 

		fprintf(stdout," %10.10s %4.4s", 
				&ctime(&acct->csa->ac_btime)[0],
				&ctime(&acct->csa->ac_btime)[20] );
	}

	if ( thisApp.args.print.proc_end ) { 

		fprintf(stdout," %10.10s %4.4s", 
				&ctime(&thisRec.end_time)[0],
				&ctime(&thisRec.end_time)[20] );
	}

	fprintf(stdout, "\n");
}

void process_input_file(char *acct_str)
{
	int acct_fd;
	time_t start_t;
	int status = GIVE_ME_MORE;
	char acctentbuf[ACCTENTSIZ];
	struct acctent acct;

	memset(&acct, 0, sizeof(acct));
	memset(&acctentbuf, 0, ACCTENTSIZ);

	acct.ent = acctentbuf;

	if (acct_str) {
		acct_fd = openacct(acct_str, "r");

		if ( acct_fd == -1 ) {
			acct_perr(ACCT_FATAL, errno, "Failed to open '%s'", acct_str);
			return;
		}
	}
	else {
		/* use stdin */
		acct_fd = openacct(NULL, "r");
		acct_str = "(stdin)";
	}

	/* read first record for time calculations */
	if ( ! thisApp.args.print.backwards ) {

		if ( readacctent(acct_fd, &acct, FORWARD) <= 0 ) {
			acct_perr(ACCT_ABORT, errno,
				  "Error occured reading file '%s'",
				  acct_str);
			if ( acct_fd )
				closeacct(acct_fd);
		}

	}
	else {
		seekacct(acct_fd, 0, SEEK_END );

		if ( readacctent(acct_fd, &acct, BACKWARD) <= 0 ) {
			acct_perr(ACCT_ABORT, errno,
				  "Error occured reading file '%s'",
				  acct_str);
			if ( acct_fd )
				closeacct(acct_fd);
		}
	}

	start_t = 0;

	if ( acct.csa )
		start_t = acct.csa->ac_btime;


	if ( acct.cfg ) {
		if ( acct.cfg->ac_event == AC_CONFCHG_BOOT )
			start_t = acct.cfg->ac_boottime;
		else
			start_t = acct.cfg->ac_curtime;
	}

	if ( acct.soj ) {
		if ( acct.soj->ac_type == AC_SOJ )
			start_t = acct.soj->ac_btime;
		else
			start_t = acct.soj->ac_rstime;
	}

	if ( acct.eoj )
		start_t = acct.eoj->ac_etime;

	if ( acct.wmbs )
		start_t = acct.wmbs->time;

	thisApp.calcs.begin_time =
		thisApp.calcs.current_start = midnight(start_t - timezone);

	time(&thisApp.calcs.now);

	if ( ! thisApp.args.print.date_changes ) {

		fprintf(stdout,
			"  \nACCOUNTING RECORDS FROM:   %s",ctime(&start_t));

		if ( thisApp.args.start_after ) {	/* 'S' option */

			thisApp.calcs.start_after = thisApp.args.start_after +
				thisApp.calcs.current_start + timezone;

			if ( localtime(&thisApp.calcs.current_start)->tm_isdst )
				thisApp.calcs.start_after -= 3600;

			fprintf(stdout,"START AFT: %s",
				ctime(&thisApp.calcs.start_after));
		}
		if ( thisApp.args.existing_before ) {	/* 'e' option */

			thisApp.calcs.existing_before =
				thisApp.args.existing_before + thisApp.calcs.current_start + timezone;

			if ( localtime(&thisApp.calcs.current_start)->tm_isdst )
				thisApp.calcs.existing_before -= 3600;

			fprintf(stdout,"START BEF: %s",
				ctime(&thisApp.calcs.existing_before));
		}
		if ( thisApp.args.existing_after ) {	/* 's' option */

			thisApp.calcs.existing_after = thisApp.args.existing_after +
				thisApp.calcs.current_start + timezone;

			if ( localtime(&thisApp.calcs.current_start)->tm_isdst )
				thisApp.calcs.existing_after -= 3600;

			fprintf(stdout,"END AFTER: %s",
				ctime(&thisApp.calcs.existing_after));
		}
		if ( thisApp.args.ending_before ) {	/* 'E' option */

			thisApp.calcs.ending_before = thisApp.args.ending_before +
				thisApp.calcs.current_start + timezone;

			if ( localtime(&thisApp.calcs.current_start)->tm_isdst )
				thisApp.calcs.ending_before -= 3600;

			fprintf(stdout,"END BEFORE: %s",
				ctime(&thisApp.calcs.ending_before));
		}

		if ( thisApp.args.existing_before && thisApp.args.ending_before ) {

			if ( thisApp.args.ending_before < thisApp.args.existing_before )

				thisApp.calcs.ending_before += 86400;
		}

		if ( ! thisApp.args.print.no_headings &&
		     thisApp.args.outfile == NULL ) {

			if ( thisApp.args.print.job_data ) {
				fprintf(stdout,"%18.18s", "JOB ID");
				fprintf(stdout," %8.8s", "USER ID");
				fprintf(stdout," %8.8s", "TIME");
				fprintf(stdout," %7.7s", "EVENT");
				fprintf(stdout," %10.10s", "CORE_HIMEM");
				fprintf(stdout," %10.10s", "VIRT_HIMEM");
				fprintf(stdout," %4.4s", "NICE");
				fprintf(stdout," %8.8s", "GROUP_ID");
				fprintf(stdout,"\n");
			}
			else {
				if (! thisApp.args.print.skip_fields) {
					fprintf(stdout,"%-13s", "COMMAND");
					fprintf(stdout," %12.12s", "");
					fprintf(stdout," %-8.8s", "START");
					fprintf(stdout," %-8.8s", "END");
					fprintf(stdout," %8.8s", "REAL");
				}
				if (thisApp.args.print.separate_times) {
					fprintf(stdout," %8.8s", "SYSTEM");
					fprintf(stdout," %8.8s", "USER");
					fprintf(stdout,"   %24.24s", "______CPU RUN TIME______");
				}
				else if(! thisApp.args.print.skip_fields)
					fprintf(stdout," %8.8s", "CPU");
				if ( thisApp.args.print.io_written) {
					if (thisApp.args.print.combine_io_times) {
						fprintf(stdout," %8.8s", "CHARS");
					} else {
						fprintf(stdout," %8.8s", "CHARS");
						fprintf(stdout," %8.8s", "CHARS");
					}
				}

				if (thisApp.args.print.cpu_factor)
					fprintf(stdout," %6.6s", "CPU");
				if (thisApp.args.print.hog_factor)
					fprintf(stdout," %10.10s", "HOG");
				if ( thisApp.args.print.mem_size ) {
					fprintf(stdout," %11.11s", "MEAN CORE");
					fprintf(stdout," %11.11s", "MEAN VIRT");
				}
				if ( thisApp.args.print.kernel_minutes) {
					fprintf(stdout," %11.11s", "KCORE");
					fprintf(stdout," %11.11s", "KVIRTUAL");
				}
				if ( thisApp.args.print.hex_flags)
					fprintf(stdout," %6.6s", " ");
				if ( thisApp.args.print.job_ids)
					fprintf(stdout," %18.18s", " ");
				if ( thisApp.args.print.session_handle)
					fprintf(stdout," %18.18s", "ARRAY");
				if ( thisApp.args.print.proj_id)
					fprintf(stdout," %8.8s", "PROJECT");
				if ( thisApp.args.print.numeric_gids)
					fprintf(stdout," %8.8s", " ");
				if ( thisApp.args.print.read_sep) {
					if (thisApp.args.print.combine_io_times)
						fprintf(stdout," %8.8s", "LOGICAL");
					else {
						fprintf(stdout," %8.8s", "LOGICAL");
						fprintf(stdout," %8.8s", "LOGICAL");
					}
				}
				if ( thisApp.args.print.pid ) {
					fprintf(stdout," %6.6s", "");
					fprintf(stdout," %6.6s", "");
				}
				if ( thisApp.args.print.wait_times) {
					if (thisApp.args.print.combine_io_times)
						fprintf(stdout," %8.8s", "DELAY");
					else if (thisApp.args.print.wait_times == 1) {
						fprintf(stdout,"   %12.12s", "CPU DELAY");
						fprintf(stdout,"   %15.15s", "BLOCK I/O DELAY");
						fprintf(stdout,"   %13.13s", "SWAP IN DELAY");
					}
					else {
						fprintf(stdout,"   %19.19s", "_____CPU DELAY_____");
						fprintf(stdout,"   %19.19s", "__BLOCK I/O DELAY__");
						fprintf(stdout,"   %19.19s", "___SWAP IN DELAY___");
					}
				}
				if ( thisApp.args.print.high_water) {
					fprintf(stdout," %19.19s", "HIMEM (KBYTES)");
					fprintf(stdout," %8.8s", "MINOR");
					fprintf(stdout," %8.8s", "MAJOR");
				}
				if ( thisApp.args.print.nice) {
					fprintf(stdout," %4.4s", " ");
					fprintf(stdout," %5.5s", "SCHED");
				}
				if ( thisApp.args.print.proc_start ) {
					fprintf(stdout," %15.15s", "PROCESS");
				}
				if ( thisApp.args.print.proc_end ) {
					fprintf(stdout," %15.15s", "PROCESS");
				}
				fprintf(stdout,"\n");

				if ( ! thisApp.args.print.skip_fields) {
					fprintf(stdout,"%-13s", "NAME");
					fprintf(stdout," %-12.12s", "    USER");
					fprintf(stdout," %-8.8s", "TIME");
					fprintf(stdout," %-8.8s", "TIME");
					fprintf(stdout," %8.8s", "(SECS)");
				}
				if (thisApp.args.print.separate_times) {
					fprintf(stdout," %8.8s", "(SECS)");
					fprintf(stdout," %8.8s", "(SECS)");
					fprintf(stdout,"   %11.11s", "REAL (SECS)");
					fprintf(stdout,"  %11.11s", "VIRT (SECS)");
				}
				else if (! thisApp.args.print.skip_fields)
					fprintf(stdout," %8.8s", "(SECS)");
				if ( thisApp.args.print.io_written) {
					if (thisApp.args.print.combine_io_times) {
						fprintf(stdout," %8.8s", "TRNSFD");
					} else {
						fprintf(stdout," %8.8s", "READ");
						fprintf(stdout," %8.8s", "WRITTEN");
					}
				}

				if ( thisApp.args.print.cpu_factor)
					fprintf(stdout," %6.6s", "FACTOR");
				if ( thisApp.args.print.hog_factor)
					fprintf(stdout," %10.10s", "FACTOR");
				if ( thisApp.args.print.mem_size ) {
					fprintf(stdout," %11.11s", "SIZE (KB)");
					fprintf(stdout," %11.11s", "SIZE (KB)");
				}
				if ( thisApp.args.print.kernel_minutes) {
					fprintf(stdout," %11.11s", "MIN (KB)");
					fprintf(stdout," %11.11s", "MIN (KB)");
				}
				if ( thisApp.args.print.hex_flags)
					fprintf(stdout," %6.6s", "F STAT");
				if ( thisApp.args.print.job_ids)
					fprintf(stdout," %18.18s", "JOB ID");
				if ( thisApp.args.print.session_handle)
					fprintf(stdout," %18.18s", "SESSION HANDLE");
				if ( thisApp.args.print.proj_id)
					fprintf(stdout," %8.8s", "ID");
				if ( thisApp.args.print.numeric_gids)
					fprintf(stdout," %8.8s", "GROUP ID");
				if ( thisApp.args.print.read_sep) {
					if (thisApp.args.print.combine_io_times)
						fprintf(stdout," %8.8s", "REQS");
					else {
						fprintf(stdout," %8.8s", "READS");
						fprintf(stdout," %8.8s", "WRITES");
					}
				}
				if ( thisApp.args.print.pid ) {
					fprintf(stdout," %6.6s", "PID");
					fprintf(stdout," %6.6s", "PPID");
				}
				if ( thisApp.args.print.wait_times) {
					if (thisApp.args.print.combine_io_times)
						fprintf(stdout," %8.8s", "(SECS)");
					else if (thisApp.args.print.wait_times == 1) {
						fprintf(stdout,"   %12.12s", "TOTAL (SECS)");
						fprintf(stdout,"   %15.15s", "TOTAL (SECS)");
						fprintf(stdout,"   %13.13s", "TOTAL (SECS)");
					}
					else {
						fprintf(stdout,"   %12.12s", "TOTAL (SECS)");
						fprintf(stdout,"  %5.5s", "COUNT");
						fprintf(stdout,"   %12.12s", "TOTAL (SECS)");
						fprintf(stdout,"  %5.5s", "COUNT");
						fprintf(stdout,"   %12.12s", "TOTAL (SECS)");
						fprintf(stdout,"  %5.5s", "COUNT");
					}
				}
				if ( thisApp.args.print.high_water) {
					fprintf(stdout," %9.9s", "CORE");
					fprintf(stdout," %9.9s", "VIRTUAL");
					fprintf(stdout," %8.8s", "FAULTS");
					fprintf(stdout," %8.8s", "FAULTS");
				}
				if ( thisApp.args.print.nice) {
					fprintf(stdout," %4.4s", "NICE");
					fprintf(stdout," %5.5s", "DISC");
				}
				if ( thisApp.args.print.proc_start ) {
					fprintf(stdout," %15.15s", "START DATE");
				}
				if ( thisApp.args.print.proc_end ) {
					fprintf(stdout," %15.15s", "END DATE");
				}
				fprintf(stdout,"\n");
			}
		}
	}


	if ( thisApp.args.print.date_changes ) {
		/* begin_time is the midnight of local time of 1st record */
		time_t	utc_time = thisApp.calcs.begin_time + timezone;

		fprintf(stdout," Day %4d:  %24.24s - first record.\n",
			thisApp.calcs.day_count,
			ctime(&utc_time));

		if ( thisApp.args.print.config_records && acct.cfg ) {

			print_config_record(&acct);
		}

	}
	else {
		(void) process_single_record(&acct);
	}


	while ( status != THAT_IS_ALL ) {
		if ( readacctent(acct_fd, &acct, thisApp.args.print.backwards) > 0 ) {

			if ( thisApp.args.print.date_changes ) {

				if ( acct.csa ) {

					thisApp.calcs.now = acct.csa->ac_btime;

					thisApp.calcs.current_midnight = midnight(thisApp.calcs.now - timezone);

					if ( thisApp.calcs.current_midnight > thisApp.calcs.begin_time ) {

						thisApp.calcs.day_count += ( thisApp.calcs.current_midnight - thisApp.calcs.begin_time ) / 86400;

						fprintf(stdout,
							" Day %4d:  %24.24s - date change.\n",
							thisApp.calcs.day_count,
							ctime(&thisApp.calcs.now));

						thisApp.calcs.begin_time = thisApp.calcs.current_midnight;
					}
				}
				else {
					print_config_record(&acct);
				}
			}
			else {
				status = process_single_record(&acct);
			}
		}
		else {
			status = THAT_IS_ALL;
		}
	}

	/* print the last record read */
	if ( thisApp.args.print.date_changes ) {
		fprintf(stdout,
			" Day %4d:  %24.24s - last record.\n",
			thisApp.calcs.day_count,
			ctime(&thisApp.calcs.now));
	}

	if ( thisApp.args.outfile == NULL ) {
		if ( thisApp.args.print.config_records ) {
			if ( thisApp.calcs.totals.cfg > 0 )
				fprintf(stdout,
					_("***  %d Configuration records selected\n"),
					thisApp.calcs.totals.cfg);
			else
				fprintf(stdout,
					_("***  No Configuration records selected\n"));
		}

		if ( thisApp.args.print.job_data ) {
			if ( thisApp.calcs.totals.start_of_job > 0 )
				fprintf(stdout,
					_("***  %d Start-of-job records selected\n"),
					thisApp.calcs.totals.start_of_job);
			else
				fprintf(stdout,
					_("***  No Start-of-job records selected\n"));

			if ( thisApp.calcs.totals.end_of_job > 0 )
				fprintf(stdout,
					_("***  %d End-of-job records selected\n"),
					thisApp.calcs.totals.end_of_job);
			else
				fprintf(stdout,
					_("***  No End-of-job records selected\n"));
		}
		else {
			if ( thisApp.calcs.totals.cmd > 0 )
				fprintf(stdout,
					_("***  %d Process records selected\n"),
					thisApp.calcs.totals.cmd);
			else
				fprintf(stdout,
					_("***  No Process records selected\n"));
		}
	}

	if ( acct_fd )
		closeacct(acct_fd);
}
