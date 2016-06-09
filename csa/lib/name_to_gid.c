/* 
 * CSA gid-ops gid <--> group name handling/lookups
 *
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.  All Rights Reserved.
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
#endif

#include <grp.h>
#include <string.h>
#include <sys/types.h>
/* #include "acctdef.h" */

/* Max number of distinct login names for one run of the accounting sw.
 */
#define MAX_GROUPS 1000
#define GROUP_NAME_LENGTH 32 /* 32 taken from glibc's login.c */


/* Structure for the cache used by the routines */
struct grcache_entry {
	char	group_name[GROUP_NAME_LENGTH+1];
	gid_t	gid;
};


/* global Static array of cache cache entries */
static struct grcache_entry grcache[MAX_GROUPS];
static int grcache_count = 0;


/* prototypes */
static struct grcache_entry * find_cached_gid(gid_t gid);
char * gid_to_name (gid_t gid);
static struct grcache_entry * find_cached_gname(char * name);
gid_t name_to_gid (char *name);
static int add_cached_grentry(char *name, gid_t gid);

/* gid_to_name:
 *
 * This routine takes group id as input argument and tries to resolve it.
 * It returns a character string of the group name associated with the
 * group id on success; it returns "?" string on failure.
 */
char * gid_to_name (gid_t gid) {
	struct grcache_entry *pw_cached;
	struct group *gr_system;

	pw_cached = find_cached_gid(gid);

	if (pw_cached != NULL) {
		/* return it straight from the cache */
		/* printf("DEBUG - found IN the cache...\n"); */
		return (pw_cached->group_name);
	}

	/* Not in the cache, need to do a lookup */

	gr_system = getgrgid(gid);

	if (gr_system != NULL) {
		/* Put it in the cache if possible */
		/* printf("DEBUG - NOT found in the cache, found on the system...\n"); */
		add_cached_grentry(gr_system->gr_name, gr_system->gr_gid);
		return (gr_system->gr_name);
	}

	/* not in the cache, not on the system, return -1 */
	/* printf("DEBUG - NOT FOUND AT ALL\n"); */
	return("?");	

}

/* find_cached_gid:
 *
 * Find a cached entry by user UID
 *
 * Return NULL on failure, cache entry on success
 * 
 */
static struct grcache_entry * find_cached_gid(gid_t gid) {
	int i;  /* iterations */

	/* Looking through the cache... */
	for (i = 0 ; i < grcache_count ; i++) {
		if (grcache[i].gid == gid) {
			/* found a match */
			return &grcache[i];
		}
	}

	return NULL;  /* no match */
}

/* name-to-gid:
 *
 * This routine is to convert a group name to a group id. Return the group id
 * if successful or return -1 on failure. The content of the static array
 * of S_password_entry_cache are preserved after the first access for fast
 * turnaround later on.
 */
gid_t name_to_gid (char *name) {
	struct grcache_entry *pw_cached;
	struct group *gr_system;

	pw_cached = find_cached_gname(name);

	if (pw_cached != NULL) {
		/* return it straight from the cache */
		/* printf("DEBUG - found IN the cache...\n"); */
		return (pw_cached->gid);
	}

	/* Not in the cache, need to do a lookup */

	gr_system = getgrnam(name);

	if (gr_system != NULL) {
		/* Put it in the cache if possible */
		/* printf("DEBUG - NOT found in the cache, found on the system...\n"); */
		add_cached_grentry(gr_system->gr_name, gr_system->gr_gid);
		return (gr_system->gr_gid);
	}

	/* not in the cache, not on the system, return -1 */
	/* printf("DEBUG - NOT FOUND AT ALL\n"); */
	return(-1);	

}

/* find_cached_gname:
 *
 * Find a cached entry by group name
 *
 * Return NULL on failure, cache entry on success
 * 
 */
static struct grcache_entry * find_cached_gname(char * name) {
	int i;  /* iterations */

	/* Looking through the cache... */
	for (i = 0 ; i < grcache_count ; i++) {
		if (strncmp(name, grcache[i].group_name, GROUP_NAME_LENGTH) == 0) {
			/* found a match */
			return &grcache[i];
		}
	}

	return NULL;  /* no match */
}

/* add_cached_grentry:
 * 
 * Add an entry to the cache if possible.
 * Return 0 on success, non-zero failure.
 */
static int add_cached_grentry(char *name, gid_t gid) {

	if (grcache_count < MAX_GROUPS) {
		strncpy(grcache[grcache_count].group_name, name, GROUP_NAME_LENGTH);
		grcache[grcache_count].gid = gid;
		grcache_count++;
		return(0);
	}
	return(1); /* cache full */
}
