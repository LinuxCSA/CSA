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

/*
 *	csachargefee.c - CSA charge fee program which creates caact records.
 *	Typically called from within the chargefee script.  Arguments
 *	are login-name and fee.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/stat.h>
#include <grp.h>
#include "acctdef.h"
#include "csaacct.h"
#include "cacct.h"
#include "acctmsg.h"

extern struct   passwd *getpwnam();
extern int optind;

#define ADM	"csaacct"
#define MODE	0660

struct	cacct	cbuf;
char		*Prg_name;

static void
usage() {
	acct_err(ACCT_ABORT,
	       "Command Usage:\n%s\tlogin-name fee",
		Prg_name, Prg_name, Prg_name);
}


int
main(int argc, char **argv)
{
	FILE	*fd;
	int c;
	char ch;
	double fee;
	gid_t adm_gid;
	uid_t uid;
	char *s_uid, *s_fee;
	struct	stat stbuf;
	struct group *grp;
	struct passwd *pw;
	char buf[64];

	Prg_name = argv[0];

	if (argc != 3) {
		usage();
	}

	/*
	 *	Open the fee file and make sure that it has
	 *	the correct group and permissions.
	 */
	if ((fd = fopen(FEEFILE,"a")) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			FEEFILE);
	}

	if (stat(FEEFILE, &stbuf) == -1) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred getting the status of file '%s'."),
			FEEFILE);
	}

	if ((grp = (struct group *)getgrnam(ADM)) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("Unable to get the Group ID of group '%s'."),
			ADM);
	}
	adm_gid = grp->gr_gid;
	if (stbuf.st_gid != adm_gid) {
		if (chown(FEEFILE, stbuf.st_uid, adm_gid) == -1) {
			sprintf(buf, "%ld", adm_gid);
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred changing the group on file '%s' to '%s'."),
				FEEFILE, buf);
		}
	}

	if ((stbuf.st_mode & 0777) != MODE) {
		if (chmod(FEEFILE, MODE) == -1) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred changing the mode on file '%s' to '%o'."),
				FEEFILE, MODE);
		}
	}

	/* Get arguments */
	s_uid = argv[optind++];
	s_fee = argv[optind++]; 

	if ((pw = getpwnam(s_uid)) == NULL) {
		acct_err(ACCT_ABORT,
		       _("Unable to get the password entry for '%s'."),
			s_uid);
	}
	uid = pw->pw_uid;

	if ((fee = atof(s_fee)) == 0.0) {
		acct_err(ACCT_ABORT,
		       _("A charge fee of zero is not valid.\n   Increase the value of the fee in the argument list.")
			);
	}

	/* Create the cacct record */
	memset((char *)&cbuf,'\0',sizeof(struct cacct));
	if (create_hdr1(&cbuf.ca_hdr, ACCT_CACCT) < 0) {
		acct_err(ACCT_WARN,
		       _("An error was returned from the call to the '%s' routine."),
			"create_hdr1()");
	}
	cbuf.ca_uid = uid;
	cbuf.ca_prid = CACCT_NO_ID;
	cbuf.ca_fee = fee;

	/* Write data. */
	if(fwrite((char *)&cbuf,sizeof(struct cacct),1,fd) != 1) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the writing of file '%s'."),
			FEEFILE);
	}

	exit(0);
}
