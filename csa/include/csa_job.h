/*
 * Copyright (c) 2007 Silicon Graphics, Inc All Rights Reserved.
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

#ifndef _CSA_JOB_H
#define _CSA_JOB_H

/* Formerly job_acct.h */

/*
 * --------------
 * CSA ACCOUNTING
 * --------------
 */

/*
 * For data exchange between job and csa.  The embedded defines
 * identify the sub-fields
 */
struct csa_job {
#define                 CSA_JOB_JID             001
	__u64		job_id;
#define                 CSA_JOB_UID             002
	uid_t		job_uid;
#define                 CSA_JOB_START           004
	time_t		job_start;
#define                 CSA_JOB_COREHIMEM       010
	__u64		job_corehimem;
#define                 CSA_JOB_VIRTHIMEM       020
	__u64		job_virthimem;
#define                 CSA_JOB_ACCTFILE        040
	int		job_acctfile;
};

#define MAX_JOB_ENTRIES 1024	/* Default argument to csa_newtable() */

/*
 * ===================
 * FUNCTION PROTOTYPES
 * ===================
 */
int csa_newtable(int num_entries);
void csa_freetable(void);
struct csa_job *csa_newjob(__u64 id, uid_t uid, time_t start);
int csa_freejob(__u64 id);
struct csa_job *csa_getjob(__u64 id);

#endif /* _CSA_JOB_H */
