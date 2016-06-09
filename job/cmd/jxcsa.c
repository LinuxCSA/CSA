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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <job.h>


int
main(int argc, char *argv[])
{
	char input_str[6];
        int run = -1;
        int rv;

        if (argc == 2) {
		sscanf(argv[1], "%5s", &input_str);
		if (strcmp(input_str, "start") == 0)
			run = 1;
		else if (strcmp(input_str, "stop") == 0)
			run = 0;
	}

	if (run == -1) {
		fprintf(stderr, "Usage: jxcsa {start|stop}\n");
		exit(1);
	}

	rv = job_runcsa(run);
	if (rv == -1) {
		if (errno == ENOENT)
			fprintf(stderr, "jobd does not appear to be running.\n");
		else
			perror("job_runcsa");
		exit(1);
	}

	exit(0);
}
