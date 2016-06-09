/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2004-2007 Silicon Graphics, Inc All Rights Reserved.
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
#include <stdint.h>
#include <sys/types.h>
#include <string.h>

#ifdef HAVE_PROJ_H
#include <proj.h>
#endif	/* HAVE_PROJ_H */

#define MAXPROJECTS  1000

#ifndef MAXPROJNAMELEN
#define MAXPROJNAMELEN 32
#endif	/* MAXPROJNAMELEN */

#ifndef _PATH_PROJID
#define _PATH_PROJID	"/etc/projid"
#endif	/* _PATH_PROJID */

#ifndef _PATH_PROJECT
#define _PATH_PROJECT	"/etc/project"
#endif	/* _PATH_PROJECT */

static	int psize = 0;
static	struct plist {
	char	project[MAXPROJNAMELEN];
	uint64_t prid;
} pl[MAXPROJECTS];

uint64_t projid(const char *);

/*
 * name_to_prid converts project names to project ids.
 * It keeps a local plist for fast access.
 */
uint64_t
name_to_prid(char *name)
{

	return 0;
}


/*
 * prid_to_name converts project id to project name.
 * It keeps a local plist for fast access.
 */
char *
prid_to_name(uint64_t prid)
{
	return("?");
}

#ifndef HAVE_PROJID
/*
 * projid searches the projid file for the project name and returns the
 * corresponding numeric project ID.
 */

uint64_t
projid(const char *name)
{
	return -1;
}
#endif	/* HAVE_PROJID */

#ifndef HAVE_PROJNAME
/*
 * projname searches the projid file for the project ID prid and stores the
 * corresponding ASCII name in the buffer buf, up to a maximum of len-1
 * characters.  The resulting string will always be null-terminated.
 */

int
projname(uint64_t prid, char *buf, size_t len)
{

	buf[0] = '\0';

	return(0);
}
#endif	/* HAVE_PROJNAME */

#ifndef HAVE_GETDFLTPROJUSER
/*
 * getdfltprojuser searches the project file for the user name and returns
 * the first project ID associated with that user.
 */

uint64_t
getdfltprojuser(const char *name)
{
	return 0;
}
#endif	/* HAVE_GETDFLTPROJUSER */
