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
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <job.h>
#include <malloc.h>
#include <string.h>


#define PROC_BASE "/proc"
#define CMD_OUTPUTWIDTH 32

void printUsageHelp(int);
int getAllInfo(int, int);
int getJidInfo(uint64_t, int, int);
int printJidHeader(void);
int printJidInfo(uint64_t, uid_t, pid_t);
int printPidHeader(void);
int printPidInfo(uint64_t, uid_t, int, pid_t *);

int
main(int argc, char *argv[])
{
	int c, rc = 0;
	extern char *optarg;
	extern int opting;
	int aflg = 0;
	int jflg = 0;
	int lflg = 0;
	int pflg = 0;
	int errs = 0;
	jid_t jid = 0;


	while (((c = getopt(argc, argv, "ahj:lp")) != -1) && !errs) {
		switch (c) {
		case 'a':
			/* For all jobs */
			if (jflg) {
				/* error */
				errs++;
			}
			aflg++;
			break;
		case 'j':
			/* For a specific job */
			if (aflg) {
				/* error */
				errs++;
			}
			jflg++;
			if (optarg != NULL) 
				sscanf(optarg, "%llx", &jid);
			else 
				errs++;
			break;
		case 'l':
			/* Get usage & limits info */
			lflg++;
			break;
		case 'p':
			/* Get process info for job(s) */
			pflg++;
			break;
		case 'h':
			printUsageHelp(1);
			exit(0);
		default:
			/* usage message */
			errs++;
			break;
		}
	}
	
	if (errs) {
		printUsageHelp(0);
		exit(1);
	}

	if (aflg) {
		rc = getAllInfo(lflg, pflg);
	} else if (jflg) {
		if (!pflg)
			printJidHeader();
		else
			printPidHeader();
		rc = getJidInfo(jid, lflg, pflg);
	} else {
		if (!pflg)
			printJidHeader();
		else
			printPidHeader();
		rc = getJidInfo((jid_t)0, lflg, pflg);
	}

	exit(rc);
}

int
getAllInfo(int lflg, int pflg)
{
	int i;
	int jidcnt;
	int retval;
	int bufsize;
	jid_t *jidlist;


	jidcnt = job_getjidcnt();
	if (jidcnt == -1) {
		if (errno == ENOENT) {
			fprintf(stderr, "jobd does not appear to be running.\n");
		} else if (errno == ENODATA) {
			fprintf(stderr, "No such job\n");
		} else {
			perror("job_getjidcnt");
		}
		exit(1);
	}
	
	bufsize = jidcnt*sizeof(jid_t);
	jidlist = (jid_t *)malloc(bufsize);

	retval = job_getjidlist(jidlist, bufsize);
	if (retval == -1) {
		if (errno == ENOENT) {
			fprintf(stderr, "jobd does not appear to be running.\n");
		} else if (errno == ENODATA) {
			fprintf(stderr, "No such job\n");
		} else {
			perror("job_getjidlist");
		}
		exit(1);
	}

	if (!pflg)
		printJidHeader();
	else
		printPidHeader();

	if (jidcnt == 0)
		printf("<No JIDs found>\n");
	for (i = 0; i < jidcnt; i++)
		getJidInfo(jidlist[i], lflg, pflg);

	if (jidlist)
		free(jidlist); 
	return 0;
}


int 
getJidInfo(jid_t jid, int lflg, int pflg) 
{
	pid_t		pid;
	uid_t		uid;
	int		rv;

	/* If no JID value specified, get JID for current process */
	if (jid == 0) {
		pid = getpid();
		jid = job_getjid(pid);
		if (jid == (jid_t)-1) {
			if (errno == ENOENT) {
				fprintf(stderr, "jobd does not appear to be running.\n");
				exit(1);
			} else if (errno == ENODATA) {
				printf("Not attached to any job\n");
				exit(0);
			}
			perror("job_getjid");
			exit(1);
		}

		if (jid == (jid_t)0) {
			printf("Not attached to any job\n");
			exit(0);
		}
	}
	
	/* Get the primary process for the job */
	pid = job_getprimepid(jid);
	if (pid == (pid_t)-1) {
		if (errno == ENOENT) {
			fprintf(stderr, "jobd does not appear to be running.\n");
		} else if (errno == ENODATA) {
			fprintf(stderr, "No such job\n");
		} else {
			perror("job_getprimepid");
		}
		exit(1);
	}

	/* Get the user that owns the job */
	uid = job_getuid(jid);
	if (uid == (uid_t)-1) {
		if (errno == ENOENT) {
			fprintf(stderr, "jobd does not appear to be running.\n");
		} else if (errno == ENODATA) {
			fprintf(stderr, "No such job\n");
		} else {
			perror("job_getuid");
		}
		exit(1);
	}

	if (!pflg && !lflg)
		/* Print info about the job */
		printJidInfo(jid, uid, pid);

	if (lflg) {
		/* Not yet implemented */
	}

	if (pflg) {
		int pidcnt;
		int bufsize;
		pid_t *pidlist;

		pidcnt = job_getpidcnt(jid);
		if (pidcnt == -1) {
			if (errno == ENOENT) {
				fprintf(stderr, "jobd does not appear to be running.\n");
			} else if (errno == ENODATA) {
				fprintf(stderr, "No such job\n");
			} else {
				perror("job_getpidcnt");
			}
			exit(1);
		}

		bufsize = pidcnt * sizeof(pid_t);
		pidlist = (pid_t *)malloc(bufsize);
		if (!pidlist) {
			perror("malloc");
			exit(1);
		}
		pidcnt = job_getpidlist(jid, pidlist, bufsize);
		if (pidcnt == -1) {
			if (errno == ENOENT) {
				fprintf(stderr, "jobd does not appear to be running.\n");
			} else if (errno == ENODATA) {
				fprintf(stderr, "No such job\n");
			} else {
				perror("job_getpidlist");
			}
			exit(1);
		}

		printPidInfo(jid, uid, pidcnt, pidlist);
		free(pidlist);
	}

	return 0;
}

int
printJidHeader(void) 
{
	printf("JID                OWNER        COMMAND\n");
	printf("------------------ ------------ --------------------------------\n");
}

int
printJidInfo(jid_t jid, uid_t uid, pid_t pid) 
{
	struct passwd *passwd 	= NULL;
	int fd 			= 0;
	int size 		= 0;
	char path[PATH_MAX+1];
	char cmd[CMD_OUTPUTWIDTH+1];

	passwd = getpwuid(uid);
	if (!passwd) {
		perror("getpwuid");
		exit(1);
	}

	sprintf(path, "%s/%d/cmdline", PROC_BASE, pid); 
	fd = open(path, O_RDONLY);
	size = read(fd, cmd, CMD_OUTPUTWIDTH);
	close(fd);
	cmd[size] = '\0';


	printf("%0#18llx %-12s %s\n", jid, passwd->pw_name, cmd);
}


int printPidHeader(void) 
{
	printf("JID 		   OWNER 	PID    PPID  "
			" COMMAND\n");
	printf("------------------ ------------ ------ ------"
			" --------------------------------\n");
}

int printPidInfo(jid_t jid, uid_t uid, int count, pid_t *pidlst)
{
	FILE *file;
	struct stat st;
	struct passwd *passwd;
	char path[PATH_MAX+1];
	char cmd[CMD_OUTPUTWIDTH+1];
	int dummy;
	int i;
	pid_t ppid;

	passwd = getpwuid(uid);

	for (i = 0; i < count; i++) {
		memset(path, 0, sizeof(char)*(PATH_MAX+1));
		sprintf(path, "%s/%d/stat", PROC_BASE, pidlst[i]);
		if (file = fopen(path, "r")) {
			/* XXX might need the following here...
			 * if (fstat(fileno(file), &st))
			 *       continue;
			 */
			if (fscanf(file, "%d (%[^)]) %c %d", &dummy, cmd, 
						(char *)&dummy, &ppid) == 4) {
				printf("%0#18llx %-12s %-6d %-6d %s\n", 
						jid, passwd->pw_name, 
						pidlst[i], 
						ppid, cmd);
			}
			fclose(file);
		}
	}
}





/*
int 
getAllInfo(int lflg, int pflg)
{
	printf("Not yet implemented\n");
	return 0;
}
*/

void
printUsageHelp(int help)
{
	fprintf(stderr, "Usage: jstat [-a | -j job_id] [-l] [-p] [h]\n");

	if (help) {
		/* print more verbose desription of options */
	}
}
