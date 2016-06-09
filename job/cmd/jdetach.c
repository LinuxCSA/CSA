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
	jid_t jid = 0;
	pid_t pid = 0;
	int err = 0;
	int retval;
	int c;

	c = getopt(argc, argv, "j:p:");
	switch (c) {
	case 'j':
		/* detach all tasks from job */
		if (optarg != NULL)
			sscanf(optarg, "%llx", &jid);
		else 
			++err;
		break;
	case 'p':
		/* detach specified task from job */
		if (optarg != NULL)
			sscanf(optarg, "%d", &pid);
		else
			++err;
		break;
	default:
		++err;
		break;
	}

	if (err) {
		fprintf(stderr, "Usage: jdetach -p <PID> | -j <JID>\n");
		exit(1);
	}

	if (jid) {
		retval = job_detachjid(jid);
		if (retval == -1) {
			if (errno == ENOENT) {
				fprintf(stderr, "jobd does not appear to be running.\n");
			} else if (errno == ENODATA) {
                        	fprintf(stderr, "No such job\n");
                	} else {
				perror("job_detachjid");
                	}
			exit(1);
		}

	} else if (pid) {
		jid = job_detachpid(pid);
		if (jid == -1) {
			if (errno == ENOENT) {
				fprintf(stderr, "jobd does not appear to be running.\n");
			} else if (errno == ENODATA) {
                        	fprintf(stderr, "Not part of a job\n");
                	} else {
				perror("job_detachpid");
                	}
			exit(1);
		}

	} else {
		fprintf(stderr, "Invalid PID or JID value\n");
		exit(1);
	}

	exit(0);
}
