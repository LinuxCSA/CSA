/*
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Contact information:  Silicon Graphics, Inc., 1140 East Arques Avenue
 * Sunnyvale, CA  94085, or:
 *
 * http://www.sgi.com
 */
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "acctdef.h"
#include "acctmsg.h"
#include "cacct.h"

struct cacct cacct_record;

#define MAX_NAME_LEN 512
static char name[MAX_NAME_LEN]; /* unused at this time */
/* MAX_NAME_LEN - 1 to account for trailing NULL */
#define INPUT_FMT_STR "%ld %511s %lf"

/*
 * Read stdin, looking for uid, name, block tuples.
 */
int main(int argc, char *argv[])
{
	int input_status, cacct_fd;

	if ((cacct_fd = openacct(NULL, "w")) == -1) {
		acct_perr(ACCT_FATAL, errno, "Error occured opening stdout");
		exit(1);
	}

	memset((char *)&cacct_record, '\0', sizeof(struct cacct));

	if (create_hdr1(&cacct_record.ca_hdr, ACCT_CACCT) == -1) {
		acct_perr(ACCT_FATAL, errno, "Error occured creating record");
		exit(1);
	}

	cacct_record.ca_dc = 1;

	while((input_status = scanf(INPUT_FMT_STR, &cacct_record.ca_uid, name,
				    &cacct_record.ca_du)) != EOF) {
		if (input_status != 3) {
			fprintf(stderr, _("Error occurred scanning  '%s'."),
				"stdin");
			exit(1);
		}

		cacct_record.ca_prid = CACCT_NO_ID;
		cacct_record.ca_gid = CACCT_NO_ID;
		cacct_record.ca_jid = CACCT_NO_ID;
		cacct_record.ca_ash = CACCT_NO_ID;

		if (writecacct(cacct_fd, &cacct_record) !=
		    (sizeof(struct cacct))) {
			acct_perr(ACCT_FATAL, errno, "Error occurred writing "
				  "record");
			exit(1);
		}

		cacct_record.ca_uid = 0;
		cacct_record.ca_du = 0.0;
	}

	closeacct(cacct_fd);

	exit(0);
}
