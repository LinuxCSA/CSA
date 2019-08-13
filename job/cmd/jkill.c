/*
 * Copyright (c) 2000-2007 Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <job.h>
#include <ctype.h>
#include <strings.h>

struct sigmap {
	char *name;
	int  number;
};

struct sigmap sigmap[] = {
		/* POSIX signals */
	{ "HUP",	SIGHUP },	/* 1 */
	{ "INT",	SIGINT }, 	/* 2 */
	{ "QUIT",	SIGQUIT }, 	/* 3 */
	{ "ILL",	SIGILL }, 	/* 4 */
	{ "ABRT",	SIGABRT }, 	/* 6 */
	{ "FPE",	SIGFPE }, 	/* 8 */
	{ "KILL",	SIGKILL }, 	/* 9 */
	{ "SEGV",	SIGSEGV }, 	/* 11 */
	{ "PIPE",	SIGPIPE }, 	/* 13 */
	{ "ALRM",	SIGALRM }, 	/* 14 */
	{ "TERM",	SIGTERM }, 	/* 15 */
	{ "USR1",	SIGUSR1 }, 	/* 10 (arm,i386,m68k,ppc), 30 (alpha,sparc*), 16 (mips) */
	{ "USR2",	SIGUSR2 }, 	/* 12 (arm,i386,m68k,ppc), 31 (alpha,sparc*), 17 (mips) */
	{ "CHLD",	SIGCHLD }, 	/* 17 (arm,i386,m68k,ppc), 20 (alpha,sparc*), 18 (mips) */
	{ "CONT",	SIGCONT }, 	/* 18 (arm,i386,m68k,ppc), 19 (alpha,sparc*), 25 (mips) */
	{ "STOP",	SIGSTOP },	/* 19 (arm,i386,m68k,ppc), 17 (alpha,sparc*), 23 (mips) */
	{ "TSTP",	SIGTSTP },	/* 20 (arm,i386,m68k,ppc), 18 (alpha,sparc*), 24 (mips) */
	{ "TTIN",	SIGTTIN },	/* 21 (arm,i386,m68k,ppc,alpha,sparc*), 26 (mips) */
	{ "TTOU",	SIGTTOU },	/* 22 (arm,i386,m68k,ppc,alpha,sparc*), 27 (mips) */
	/* Miscellaneous other signals */
#ifdef SIGTRAP
	{ "TRAP",	SIGTRAP },	/* 5 */
#endif
#ifdef SIGIOT
	{ "IOT",	SIGIOT }, 	/* 6, same as SIGABRT */
#endif
#ifdef SIGEMT
	{ "EMT",	SIGEMT }, 	/* 7 (mips,alpha,sparc*) */
#endif
#ifdef SIGBUS
	{ "BUS",	SIGBUS },	/* 7 (arm,i386,m68k,ppc), 10 (mips,alpha,sparc*) */
#endif
#ifdef SIGSYS
	{ "SYS",	SIGSYS }, 	/* 12 (mips,alpha,sparc*) */
#endif
#ifdef SIGSTKFLT
	{ "STKFLT",	SIGSTKFLT },	/* 16 (arm,i386,m68k,ppc) */
#endif
#ifdef SIGURG
	{ "URG",	SIGURG },	/* 23 (arm,i386,m68k,ppc), 16 (alpha,sparc*), 21 (mips) */
#endif
#ifdef SIGIO
	{ "IO",		SIGIO },	/* 29 (arm,i386,m68k,ppc), 23 (alpha,sparc*), 22 (mips) */
#endif
#ifdef SIGPOLL
	{ "POLL",	SIGPOLL },	/* same as SIGIO */
#endif
#ifdef SIGCLD
	{ "CLD",	SIGCLD },	/* same as SIGCHLD (mips) */
#endif
#ifdef SIGXCPU
	{ "XCPU",	SIGXCPU },	/* 24 (arm,i386,m68k,ppc,alpha,sparc*), 30 (mips) */
#endif
#ifdef SIGXFSZ
	{ "XFSZ",	SIGXFSZ },	/* 25 (arm,i386,m68k,ppc,alpha,sparc*), 31 (mips) */
#endif
#ifdef SIGVTALRM
	{ "VTALRM",	SIGVTALRM },	/* 26 (arm,i386,m68k,ppc,alpha,sparc*), 28 (mips) */
#endif
#ifdef SIGPROF
	{ "PROF",	SIGPROF },	/* 27 (arm,i386,m68k,ppc,alpha,sparc*), 29 (mips) */
#endif
#ifdef SIGPWR
	{ "PWR",	SIGPWR },	/* 30 (arm,i386,m68k,ppc), 29 (alpha,sparc*), 19 (mips) */
#endif
#ifdef SIGINFO
	{ "INFO",	SIGINFO },	/* 29 (alpha) */
#endif
#ifdef SIGLOST
	{ "LOST",	SIGLOST }, 	/* 29 (arm,i386,m68k,ppc,sparc*) */
#endif
#ifdef SIGWINCH
	{ "WINCH",	SIGWINCH },	/* 28 (arm,i386,m68k,ppc,alpha,sparc*), 20 (mips) */
#endif
#ifdef SIGUNUSED
	{ "UNUSED",	SIGUNUSED },	/* 31 (arm,i386,m68k,ppc) */
#endif
};

int sigmapsz = sizeof(sigmap)/sizeof(sigmap[0]);

void usage(void) 
{
	fprintf(stderr, "usage: jkill [-l | [ [-signal] jid ... ] ]\n");
}

int sig_name2number(char *name)
{
	int i;

	/* 
	 * If signal name string is prepended by SIG/sig, skip the
	 * first 3 characters.
	 */
	if (!strncasecmp("sig", name, 3)) 
		name += 3;

	/* 
	 * Match the string against those in the signal map table,
	 * return the corresponsing signal number or -1 if signal not
	 * found.
	 */
	for (i = 0; i < sigmapsz; i++) {
		if (!strcasecmp(sigmap[i].name, name))
			return sigmap[i].number;
	}
	return -1;
}

int list_signals(void)
{
	int i; 
	int col = 0;

	for (i = 0; i < sigmapsz; i++) {
		printf("SIG%-7s[%2d]", sigmap[i].name, sigmap[i].number);
		if (++col == 4) {
			printf("\n");
			col = 0;
		} else { 
			printf("      ");
		}
	}
	printf("\n");
	return 0;
}


int
main(int argc, char *argv[])
{
	int argcnt = argc;
	int sig_number = SIGTERM; /* default, use if no signal specified */
	int list = 0;
	char *arg;
	jid_t jid;
	int retval;


	/* Parse the command line string for options */
	for (argc--, argv++; argc > 0; argc--, argv++) {
		arg = *argv;

		if (arg[0]!= '-') {
			/* no options, only jid values */
			break;
		}

		/* Check option for signal number */
		if ((sig_number = sig_name2number(&arg[1])) < 0) {
			/* Check if option is signal number */
			if (isdigit(arg[1])) {
				sig_number = atoi(&arg[1]);
			}

			/* option not signal, check for list option */
			else if (arg[1] == 'l' && arg[2] == '\0') {
				/* list option must be only argument */
				if (argcnt != 2) {
					usage();
					exit(1);
				} else {
					list_signals();
					return 0;
				}
			} 
			
			/* option not signal or list, so error */
			else {
				usage();
				exit(1);
			}
		}
	}

	/* process list of jid value & send signals */
	for ( ; argc > 0; argc--, argv++) {
		sscanf(*argv, "%Lx", &jid);

		retval = job_killjid(jid,sig_number);
		if (retval == -1) {
			if (errno == ENOENT) {
				fprintf(stderr, "jobd does not appear to be running.\n");
			} else if (errno == ENODATA) {
				fprintf(stderr, "No such job\n");
			} else {
				perror("job_killjid");
			}
			exit(1);
		}
	
		printf("Processes in job[%0#18Lx] sent signal %d.\n", 
				jid, sig_number);
	}
	
	exit(0);
}
