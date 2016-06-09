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
#include <sys/sysmacros.h>
#include <sys/times.h>
#include <sys/utsname.h>

#include <grp.h>
#include <malloc.h>
#include <memory.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa_conf.h"
#include "sbu.h"

#include "csaacct.h"
#include "csaja.h"
#include "extern_ja.h"

struct	flowtree	flowtree;


/*
 *	makeflwt() - Add new entry to the flow tree.
 */
void
makeflwt(struct acctcsa *csa)
{
	struct flowtree	*new;
	struct flowtree	*p1;
	struct flowtree	*p2;
	struct flowtree	*q1;
	static int	overflow = 0;

	if (overflow) {
		return;
	}

	/*  Get space for the new entry and initialize it. */
	if ((new = (struct flowtree *)malloc(sizeof(struct flowtree))) == NULL){
		acct_err(ACCT_WARN,
		       _("The command flow tree overflowed the available memory.")
			);
		overflow++;
		return;
	}
	new->ppid =  csa->ac_ppid;
	new->pid =   csa->ac_pid;
	new->jid =   csa->ac_jid;
	new->btime = csa->ac_btime;
	new->ctime = TIME_2_SECS(csa->ac_utime + csa->ac_stime); 
	strncpy(new->comm, csa->ac_comm, 16);
	new->comm[15] = '\0';
	new->child = NULL;
	new->sibling = NULL;

	/*
	 * Search the zeroth level sibling list for the children of the new
	 * process; link them into the new process's first level sibling list.
	 */
	p1 = &flowtree;
	while (p2 = p1->sibling) {
		if (p2->ppid == new->pid) {
			p1->sibling = p2->sibling;
			p2->sibling = NULL;
			if (new->child == NULL) {
				new->child = p2;
			} else {
				q1->sibling = p2;
			}
			q1 = p2;
		} else {
			p1 = p2;
		}
	}

	/*
	 * Place the new process into the zeroth level sibling list.
	 */
	p1 = &flowtree;
	while (p1->sibling && p1->sibling->btime <= new->btime) {
		p1 = p1->sibling;
	}

	new->sibling = p1->sibling;
	p1->sibling = new;

	return;
}


/*
 *	printflwhdr() - Print command flow report header.
 */
void
printflwhdr(l_opt)
{
	printf("\n\nJob Accounting - Command Flow Report\n");
	printf("====================================\n\n");
	if (l_opt) {
		printf("parent           (CPU time    PPID");
		printf("     PID      JID)  -> child   ...\n");
		printf("==================================");
		printf("==================================\n");
	} else {
		printf("parent\t\t -> child   ...\n");
		printf("===================================\n");
	}
	return;
}


/*
 *	printflwt() - Print child flow tree.
 */
void
printflwt(struct flowtree *ft, int level, int l_opt)
{
	int	ind;

	while (ft) {
		for(ind = 0; ind <= level; ind++) {
			printf("\t\t");
		}

		printf(" -> %-16.16s", ft->comm);
		if (l_opt) {
			printf(" (%9.4f)", ft->ctime);
			printf(" (%d)", ft->ppid);
			printf(" (%d)", ft->pid);
			printf(" (0x%llx)", ft->jid);
		}
		printf("\n");

		printflwt(ft->child, level + 1, l_opt);
		if ((ft = ft->sibling) == NULL)
			break;
	}

	return;
}

void
flow_rpt()
{
	struct	flowtree *ft;

	if (!r_opt) {
		printflwhdr(l_opt);
	}

	ft = &flowtree;
	while (ft = ft->sibling) {
		printf("%-16.16s", ft->comm);
		if (l_opt) {
			printf(" (%9.4f)", ft->ctime);
			printf(" (%6d)", ft->ppid);
			printf(" (%6d)", ft->pid);
			printf(" (0x%llx)", ft->jid);
		}
		printf("\n");

		printflwt(ft->child, 0, l_opt);
	}

	return;
}
