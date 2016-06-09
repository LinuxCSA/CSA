/* 
 * CSA uid-ops uid <--> user name handling/lookups
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

#include <pwd.h>
#include <string.h>
#include <sys/types.h>
/* #include "acctdef.h" */

/* Max number of distinct login names for one run of the accounting sw.
 */
#define MAX_LOGINS 10001
#define USER_NAME_LENGTH 32 /* 32 taken from glibc's login.c */


/* Structure for the cache used by the routines */
struct pwcache_entry {
	char	user_name[USER_NAME_LENGTH+1];
	uid_t	uid;
};


/* global Static array of cache cache entries */
static struct pwcache_entry pwcache[MAX_LOGINS];
static int pwcache_count = 0;


/* prototypes */
static struct pwcache_entry * find_cached_uid(uid_t uid);
char * uid_to_name (uid_t uid);
static struct pwcache_entry * find_cached_uname(char * name);
uid_t name_to_uid (char *name);
static int add_cached_pwentry(char *name, uid_t uid);

/* uid_to_name:
 *
 * This routine takes user id as input argument and tries to resolve it.
 * It returns a character string of the user name associated with the
 * user id on success; it returns "?" string on failure.
 */
char * uid_to_name (uid_t uid) {
	struct pwcache_entry *pw_cached;
	struct passwd *pw_system;

	pw_cached = find_cached_uid(uid);

	if (pw_cached != NULL) {
		/* return it straight from the cache */
		/* printf("DEBUG - found IN the cache...\n"); */
		return (pw_cached->user_name);
	}

	/* Not in the cache, need to do a lookup */

	pw_system = getpwuid(uid);

	if (pw_system != NULL) {
		/* Put it in the cache if possible */
		/* printf("DEBUG - NOT found in the cache, found on the system...\n"); */
		add_cached_pwentry(pw_system->pw_name, pw_system->pw_uid);
		return (pw_system->pw_name);
	}

	/* not in the cache, not on the system, return -1 */
	/* printf("DEBUG - NOT FOUND AT ALL\n"); */
	return("?");	

}

/* find_cached_uid:
 *
 * Find a cached entry by user UID
 *
 * Return NULL on failure, cache entry on success
 * 
 */
static struct pwcache_entry * find_cached_uid(uid_t uid) {
	int i;  /* iterations */

	/* Looking through the cache... */
	for (i = 0 ; i < pwcache_count ; i++) {
		if (pwcache[i].uid == uid) {
			/* found a match */
			return &pwcache[i];
		}
	}

	return NULL;  /* no match */
}

/* name-to-uid:
 *
 * This routine is to convert a user name to a user id. Return the user id
 * if successful or return -1 on failure. The content of the static array
 * of S_password_entry_cache are preserved after the first access for fast
 * turnaround later on.
 */
uid_t name_to_uid (char *name) {
	struct pwcache_entry *pw_cached;
	struct passwd *pw_system;

	pw_cached = find_cached_uname(name);

	if (pw_cached != NULL) {
		/* return it straight from the cache */
		/* printf("DEBUG - found IN the cache...\n"); */
		return (pw_cached->uid);
	}

	/* Not in the cache, need to do a lookup */

	pw_system = getpwnam(name);

	if (pw_system != NULL) {
		/* Put it in the cache if possible */
		/* printf("DEBUG - NOT found in the cache, found on the system...\n"); */
		add_cached_pwentry(pw_system->pw_name, pw_system->pw_uid);
		return (pw_system->pw_uid);
	}

	/* not in the cache, not on the system, return -1 */
	/* printf("DEBUG - NOT FOUND AT ALL\n"); */
	return(-1);	

}

/* find_cached_uname:
 *
 * Find a cached entry by user name
 *
 * Return NULL on failure, cache entry on success
 * 
 */
static struct pwcache_entry * find_cached_uname(char * name) {
	int i;  /* iterations */

	/* Looking through the cache... */
	for (i = 0 ; i < pwcache_count ; i++) {
		if (strncmp(name, pwcache[i].user_name, USER_NAME_LENGTH) == 0) {
			/* found a match */
			return &pwcache[i];
		}
	}

	return NULL;  /* no match */
}

/* add_cached_pwentry:
 * 
 * Add an entry to the cache if possible.
 * Return 0 on success, non-zero failure.
 */
static int add_cached_pwentry(char *name, uid_t uid) {

	if (pwcache_count < MAX_LOGINS) {
		strncpy(pwcache[pwcache_count].user_name, name, USER_NAME_LENGTH);
		pwcache[pwcache_count].uid = uid;
		pwcache_count++;
		return(0);
	}
	return(1); /* cache full */
}
