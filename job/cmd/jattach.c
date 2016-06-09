/*
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <job.h>


int
main(int argc, char *argv[])
{
	jid_t jid_requested;
	pid_t pid;
	int retval = 0;

	if (argc != 3) {
		fprintf(stderr, "Usage: jattach pid jid-hex-value\n");
		exit(1);
	}

	sscanf(argv[1], "%d", &pid);
	sscanf(argv[2], "%llx", &jid_requested);
	retval = job_attachpid(pid, jid_requested);
	if (retval < 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "jobd does not appear to be running.\n");
		} else if (errno == ENODATA) {
			fprintf(stderr, "No such job\n");
		} else {
			perror("job_attachpid");
		}
		exit(retval);
	}
	exit(0);
}

