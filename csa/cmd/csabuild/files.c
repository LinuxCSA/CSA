/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
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
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include "csaacct.h"

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

#include "csabuild.h"
#include "extern_build.h"

int	files_open = 3;		/* Total number of open files */
int	File_index;		/* Index of last open_file request */
struct	File	*file_tbl;	/* pointer to global file table */

/*
 * ============================ Build_filetable () ========================
 *	Build file table builds up a table for all the files we will be needing.
 */
void
Build_filetable() 
{
	int	ind;
	int	siz;

	siz = MAXFILES * sizeof(struct File);
	if ((file_tbl = (struct File *)malloc(siz)) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"file table");
	}
	for(ind = 0; ind < MAXFILES; ind++) {
		file_tbl[ind].status = F_UNUSED;
		file_tbl[ind].fd = -1;
		file_tbl[ind].name = NULL;
	}

	return;
}


/*
 * ============================ open_file() =============================
 *	Open file routine opens a file and inserts it into the file table.
 */
int
open_file(char *path, int mode)
{
	int	ind;
	char	*str;
	char	*ty;

	if ((mode & RWMODE) == F_READ) {
		ty = "r";

	} else if((mode & RWMODE) & F_WRITE) {
		ty = "w";

	} else {
		acct_err(ACCT_ABORT,
		       _("An unknown mode (%d) was found in the '%s' routine."),
			mode, "open_file()");
	}

	/*
	 *	Check to see if it is already in the table.
	 */
	for(ind = 0; ind < MAXFILES; ind++) {
		if (file_tbl[ind].status != F_UNUSED &&
			!strcmp(path, file_tbl[ind].name)) {
			 /* found in table */
			if (file_tbl[ind].status & F_OPEN) {
				if ((file_tbl[ind].status & RWMODE) == mode) {
					File_index = ind;
					return(file_tbl[ind].fd);
				}
				continue;
			}
			/* in table but closed */
			errno = 0;
			if ((file_tbl[ind].fd = openacct(path, ty)) < 0) {
				if (db_flag > 0) {
					Ndebug("open_file(1): Error %d opening "
						"file '%s'.\n", errno, path);
				}
				return(-1);
			}
			files_open++;
			mode = mode & ~O_CREAT;
			file_tbl[ind].status = F_OPEN | mode;
			File_index = ind;
			return(file_tbl[ind].fd);
		}
	}

	/*
	 *	Ok this is a new file to be added.
	 */
	for(ind = 0; ind < MAXFILES; ind++) {
		if (file_tbl[ind].status == F_UNUSED) {
			break;
		}
	}
	if (ind == MAXFILES) {
		acct_err(ACCT_ABORT,
		       _("The file table is full.")
			);
	}

	if ((file_tbl[ind].fd = openacct(path, ty)) < 0) {
		if (db_flag > 0) {
			Ndebug("open_file(1): Error %d opening file '%s'.\n",
				errno, path);
		}
		return(-1);
	}
	files_open++;
	mode = mode & ~O_CREAT;
	file_tbl[ind].status = F_OPEN | mode;
	if ((str = malloc(strlen(path)+1)) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"expanded file table");
	}
	strcpy(str, path);
	file_tbl[ind].name = str;
	File_index = ind;

	return(file_tbl[ind].fd);
}


/*
 * =========================== close_all_input_files() ======================
 *	Close all open file_tbl[] files.
 */
void
close_all_input_files()
{
	int	ind;

	if (db_flag > 3) {
		Ndebug("close_all_input_files(4): closing all open input "
			"files (%d).\n", files_open);
	}
	for(ind = 0; ind < MAXFILES; ind++) {
		if (file_tbl[ind].status == F_OPEN) {
			closeacct(file_tbl[ind].fd);
			files_open--;
			file_tbl[ind].status = F_CLOSED;
			file_tbl[ind].fd = -1;
		}
	}
	if (db_flag > 3) {
		Ndebug("close_all_input_files(4): closed all but %d files.\n",
			files_open);
	}

	return;
}


/*
 * ============================ close_file() ===========================
 *	Close file - closes file to free file descriptor.
 */
void
close_file(char *path) 
{
	int	ind;

	for(ind = 0; ind < MAXFILES; ind++) {
		if (file_tbl[ind].status != F_UNUSED &&
			!strcmp(path,file_tbl[ind].name)) { /* found in table */
			file_tbl[ind].status = F_CLOSED;
			closeacct(file_tbl[ind].fd);
			files_open--;
			file_tbl[ind].fd = -1;
		}
	}

	return;
}


/*
 * ============================ get_next_file() ======================
 *	Get next file - returns a file descriptor ....
 */
int
get_next_file(char *prefix, int index, int action)
{
	char		fname[MAXFILENAME+1];
	int	ind;
	int		fd;
	struct	stat	statbuf;
	char		temp[10];

	/* Build the file name. */
	ind = strlen(prefix);
	strcpy(fname, prefix);
	fname[ind] = '\0';
	sprintf(temp, "%d", index);
	strcat(fname, temp);

	/*  Determine the file action. */
	if (action == F_CLOSED) {
		if (db_flag > 3) {
			Ndebug("get_next_file(4): calls close_file(%s).\n",
				fname);
		}
		close_file(fname);
		return(-1);

	} else if (action == F_OPEN) {
		if (db_flag > 3) {
			Ndebug("get_next_file(4): calls open_file(%s, "
				"F_READ).\n", fname);
		}
		if (stat(fname, &statbuf) == 0 ) {
			if ((fd = open_file(fname, F_READ)) < 0) {
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred during the opening of file '%s'."),
					fname);
			}

		} else {
			if (errno != ENOENT && db_flag >= 1) {
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred getting the status of file '%s'."),
					fname);
			}
			return(-1);
		}
		if (seekacct(fd, 0, SEEK_SET) != 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the positioning of file '%s' %s."),
				fname,
				"Sorted Pacct");
		}
		if (db_flag > 3) {
		    Ndebug("get_next_file(4): returns ok File_index(%d), "
			"name '%s'.\n", File_index, file_tbl[File_index].name);
		}
		return(fd);

	} else {
		acct_err(ACCT_CAUT,
		       _("An unknown action (%d) was found in the '%s' routine."),
			action, "get_next_file()");
	}

	return(-1);
}
