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

#include <stdio.h>
#include <malloc.h>
#include <sys/types.h>
#include <asm/types.h>
#include "csa.h"
#include "csa_job.h"
#include "hash.h"

static hash_table *job_table = NULL;

int
csa_newtable(int num_entries)
{
    job_table = hash_create_table(num_entries, sizeof(__u64));
    return ((job_table == NULL) ? -1 : 0);
}

void
csa_freetable(void)
{
    if (job_table != NULL) {
	hash_delete_table(job_table);
	job_table = NULL;
    }
}

struct csa_job *
csa_newjob(__u64 id, uid_t uid, time_t start)
{
    struct csa_job *job = (struct csa_job *) malloc(sizeof(struct csa_job));
    job->job_id = id;
    job->job_uid = uid;
    job->job_start = start;
    job->job_corehimem = 0;
    job->job_virthimem = 0;
    job->job_acctfile = NULLFD;

    if (hash_enter(&id, job, job_table) == HASH_EXISTS) {
	free(job);
	return NULL;
    }

    return job;
}

int
csa_freejob(__u64 id)
{
    return hash_delete(&id, job_table, 1);
}

struct csa_job *
csa_getjob(__u64 id)
{
    return hash_find(&id, job_table);
}
