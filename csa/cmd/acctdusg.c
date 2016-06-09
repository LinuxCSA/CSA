/*
 * CSA acctdusg - Compute disk usage by login
 *
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2004-2008 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Contact information:  Silicon Graphics, Inc., 1140 East Arques Avenue,
 * Sunnyvale, CA  94085, or:
 *
 * http://www.sgi.com
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include "acctdef.h"
#include "acctmsg.h"
 
/* Globals */
#define FILENAME_SIZE 2048
#define USER_NAME_LENGTH 32  /* login.c */

/* struct to hold in disk usage info for a given user */
struct du {
	char uname[USER_NAME_LENGTH];
	uid_t uid;
	uint64_t blocks;
	struct du *next;
};

struct du *du_list = NULL; /* head of the link list of du's */


/* prototypes */
static void openError(char * f);
static void chargeFileOwner(char *, FILE *);
static int fileExists(char * fname);
static void printDiskUsage(void);

/* stub prototypes */
static int create_du_list(struct passwd *p);
static void free_du_list(void);
static void dsort ();
static int strndx(char * s, char c);
static	void	swapd(struct du *, struct du *, struct du *);


/* main */

int main (int argc, char **argv) {
	char pwFileName[FILENAME_SIZE+1] = "";  /* Name of /etc/passwd */
	char unchargedListName[FILENAME_SIZE+1] = "";  /* name of file w/list of
							  unaccounted files */
	char unchargedBak[FILENAME_SIZE+5] = ""; /* for rename() to .bak */
	int c; /* used for getopt */
	FILE	*pwFile=NULL;  /* file pointer for altenrnate pw file */
	FILE *unchargedListFile=NULL;
	struct passwd *pwEntry;
	char lineBuf [BUFSIZ+1];
	int retVal, err = 0;

	/* Command line arguement processing */
	/* getopt(3) man page from ProPack 2.4 used as a reference here */
	while (1) {
		c = getopt (argc, argv, "u:p:");
		if (c == -1)
			break;

		switch (c) {
		case 'u':
			strncpy(unchargedListName, optarg, FILENAME_SIZE);
			if (strlen(unchargedListName) == 0 ||
			    unchargedListName[0] == '-') {
			    	fprintf(stderr, "Command Usage: /usr/sbin/acctdusg [-u file] [-p file]\n");
				exit(1);
			}
			/* printf("DEBUG uncharged file list output file: %s\n", unchargedListName); */
			break;
		case 'p':
			strncpy(pwFileName, optarg, FILENAME_SIZE);
			/* printf("DEBUG alternate pw file specified: %s\n", pwFileName); */
			break;
		default:
			fprintf(stderr, "Command Usage: /usr/sbin/acctdusg [-u file] [-p file]\n");
			exit(1);
		}
	}
	/* We always want files 0644.... */
	umask(022);

	/* end of command line arguement processing */

	if (strlen(unchargedListName) != 0) {
		/* An uncharged file list was specified */
		/* Are we already open?  This shouldn't happen. */
		if (unchargedListFile != NULL) {
			fprintf(stderr, "ERROR - file already open: %s\n", unchargedListName);
			exit(1);
		}
		if (fileExists(unchargedListName) == 0) {
			/* Since the file exists, we need to back it up to .bak */
			strncpy(unchargedBak, unchargedListName, FILENAME_SIZE+5);
			strcat(unchargedBak, ".bak"); /* We know we have room for .bak */
			retVal = rename(unchargedListName, unchargedBak);
			if (retVal != 0) {
				fprintf(stderr, "Error renaming %s to %s.  Quitting.\n",
					unchargedListName, unchargedBak);
				exit(1);
			}
		}

		/* Finally, we can open the darn thing for write*/
		unchargedListFile = fopen(unchargedListName, "w");
		if (unchargedListFile == NULL) {
			perror("Unable to open file for -u option...");
			openError(unchargedListName);
		}
		/* We will be writing to this file later. */

	}

	if (strlen(pwFileName) != 0) {
		/* an alternative password file was supplied with -p */
		if (pwFile != NULL) {
			/* File is already open - reset it */
			rewind(pwFile);
		}
		else {
			/* open the file */
			pwFile = fopen(pwFileName, "r");
			if (pwFile == NULL) {
				openError(pwFileName);
			}
		}
		/* Now we are ready to process the user-supplied password file */
		while (!err && (pwEntry = fgetpwent(pwFile)) != NULL) {
			err = create_du_list(pwEntry);
		}
		fclose(pwFile);
		pwFile=NULL;
	}
	else {
		/* No alternative password file specified, use standard pw lookups */
		setpwent();

		/* Process available password entries using getpwent */
		while (!err && (pwEntry = getpwent()) != NULL) {
			err = create_du_list(pwEntry);
		}
		endpwent();
	}

	if (!err)
		dsort();

	/* Start reading from stdin and charge users for file usage */

	while (!err && (fgets(lineBuf, BUFSIZ, stdin)) != NULL) {
		retVal = strndx(lineBuf, '\n');
		/* The string should terminate at the new line  - erasing the line feed */
		if (retVal != -1) {
			lineBuf[retVal] = '\0';
		}
		chargeFileOwner(lineBuf, unchargedListFile);
	}

	if (unchargedListFile != NULL) {
		fclose(unchargedListFile);
	}

	if (!err)
		printDiskUsage();

	free_du_list();

	return(err);
}

/* fileExists:
 * Check if a file exists.  Return 0 if it exists.  Return 1 if it doesn't.
 * Return -1 if we got some error.
 */
static int fileExists(char * fname) {
	struct stat finfo;
	int retVal;
	int MyErrno;
	
	retVal = stat(fname, &finfo);
	MyErrno = errno;
	
	if (retVal == 0) {
		return (0); /* file exists, return 0 */
	}
	else if (MyErrno == ENOENT) {
		return(1); /* file doesn't exist */
	}
	else {
		perror("stat returned an error not handled by fileExists");
		return(-1);
	}
}

/* printDiskUsage:
 *
 * This function prints out disk usage information from the linked list of
 * users.  It simply prints the output to stdout.
 */
static void printDiskUsage(void) {
	struct du *du_entry;

	if (du_list == NULL) {
		fprintf(stderr, "Error - no users exist?  This shouldn't happen.\n");
		exit(1);
	}

	du_entry = du_list;

	while (du_entry != NULL) {
		if (du_entry->blocks != 0) {
			printf("%05u\t%-8.8s\t%7lu\n", du_entry->uid, du_entry->uname, 
				du_entry->blocks);
		}
		du_entry = du_entry->next;
	}
}

/* openError:
 * This function reports an error opening a file and exits.
 * As intput, it takes the filename (string) that we attempted to open.
 */
static void openError(char *f) {
	fprintf(stderr, "ERROR - cannot open file: %s\nExiting.\n", f);
	exit (1);
}

/* chargeFileOwner:
 *
 * This function updates the linked list of users with file usage 
 * information.
 *
 * If there are files for which we can't find owners, we write usage 
 * information to a file specified with the -u option to this program.
 * If the -u option wasn't given, we produce no such output file.
 *
 */
static void chargeFileOwner(char * fname, FILE * fptr) {
	struct stat finfo;  /* used to store struct from stat */
	struct du *du_entry;
	int retVal;

	retVal = lstat(fname, &finfo);
	if (retVal != 0) {
		return;
	}

	/* Now go through the linked list looking for the user */
	if (du_list == NULL) {
		fprintf(stderr, "Error - no users exist?  This shouldn't happen.\n");
		exit(1);
	}

	du_entry = du_list;

	while (du_entry != NULL) {
		if(finfo.st_uid == du_entry->uid) {
			/* Found the user in the list, charge that user! */
			du_entry->blocks += finfo.st_blocks;
			return;
		}
		du_entry = du_entry->next;
	}
	/* If we made it here, it means we couldn't match the owner of the file */

	if (fptr != NULL) {
		/* Ok.  We need to store orphan usage data in this file */
		fprintf(fptr, "%5u\t%7lu\t%s\n", finfo.st_uid, finfo.st_blocks, fname);
		return;
	}
}

/* create_du_list:
 *
 * This function populates the disk usage struct list (du_list) with
 * info from getpwent (account info).  It's basically used to create
 * the struct for each user on the system.  Later, this list will be
 * used to store associated disk space usage information.
 */
static int create_du_list(struct passwd *p) {
	struct du *du_new, **du_entry;

	/* printf("DEBUG - create_du_list got pw entry for: %s\n", p->pw_name); */

	/* Allocate and populate new entry. */

	du_new = (struct du *) malloc(sizeof(struct du));	
	if (du_new == NULL) {
		perror("Failed to allocate memory for disk usage structure du");
		return -1;
	}
	strcpy(du_new->uname, p->pw_name); /* bounds checked by main */
	du_new->uid = p->pw_uid;
	du_new->blocks = 0;
	du_new->next = NULL;

	/* Append new entry to the list. */

	for (du_entry = &du_list; *du_entry != NULL; du_entry = &(*du_entry)->next)
		;
	*du_entry = du_new;

	return 0;
}

static void free_du_list(void) {
	struct du *curr, *next;

	for (curr = du_list; curr != NULL; curr = next) {
		next = curr->next;
		free(curr);
	}

	return;
}


static int
strndx(char *str, char chr)
{

	int index;

	for (index=0; *str; str++,index++)
		if (*str == chr)
			return index;
	return -1;
}

/*
 *	sort by increasing uid
 */
static void
dsort()
{

	struct du *dp1, *dp2, *pdp;
	int	change;

	if(du_list == NULL || du_list->next == NULL)
		return;

	change = 0;
	pdp = NULL;

	for(dp1 = du_list; ;) {
		if((dp2 = dp1->next) == NULL) {
			if(!change)
				break;
			dp1 = du_list;
			pdp = NULL;
			change = 0;
			continue;
		}
		if(dp1->uid > dp2->uid) {
			swapd(pdp, dp1, dp2);
			change = 1;
			dp1 = dp2;
			continue;
		}
		pdp = dp1;
		dp1 = dp2;
	}
}

static void
swapd(struct du *p, struct du *d1, struct du *d2)
{
	struct du *t;

	if (p) {
		p->next = d2;
		t = d2->next;
		d2->next = d1;
		d1->next = t;
	} else {
		t = d2->next;
		d2->next = d1;
		d1->next = t;
		du_list = d2;
	}
}

