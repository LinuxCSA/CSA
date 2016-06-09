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

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_MAC_H
#include <sys/mac.h>
#include <sys/capability.h>
#endif	/* HAVE_MAC_H */

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <malloc.h>

#include "acctdef.h"

/*****************************************************************************
 *
 * NAME
 *      check_file      - Make sure the specified file exists as specified.
 *
 * SYNOPSIS
 *      check_file_retval = check_file( file, own, grp, mode );
 *
 *      file            r       The name of the file to be verified, changed
 *                              or created.
 *      own             r       The file's owner.
 *      grp             r       The file's group.
 *      mode            r       The file's mode.
 *
 * DESCRIPTION
 *      This routine determines whether <file> exists -and- is a regular file.
 *      If the file doesn't exist, this routine will create it.
 *
 *      In either case, the file's owner will be set to <own>, group will be
 *      set to <grp>, mode will be set to <mode>, and MAC label will be set
 *      to dbadmin (for Trusted IRIX only).
 *
 *      NOTES:  If the owner cannot be changed to <own> because the executing
 *              user does NOT have permission, the error is ignored and this
 *              routine will return a successful indication.
 *
 *              Only the -low order- 9 bits of <mode> are expected to be used.
 *
 *              All file descriptors opened by the processing contained in
 *              this routine are closed -before- control is returned.
 *
 *              If an error occurs and the file was NOT created, this routine
 *              attempts to put the file back to the way it existed upon entry
 *              to this routine.
 *
 *              If an error occurs and the file was created by this routine,
 *              it is removed -before- control is returned to the caller.
 *
 * RETURNS
 *      0       - If the file exists as requested.
 *      1       - If <own> is NOT a valid user name.
 *      2       - If <grp> is NOT a valid group name.
 *      3       - If <mode> has more than the -low order- 9 bits set.
 *      4       - If the file could NOT be creat()'d.
 *      5       - If the stat() system call failed.
 *      6       - If the file's mode could NOT be set as requested.
 *      7       - If the file's group could NOT be set as requested.
 *      8       - If the file's owner could NOT be set as requested.
 *      9       - If <file> exists -and- is NOT a regular file.
 *     10       - If we cannot get the mac_t structure for dbadmin.
 *     11       - If the file's MAC label could NOT be set as requested.
 *
 *****************************************************************************/
check_file_retval
check_file( char *file, char *own, char *grp, mode_t mode )
{
        uid_t   uid;
        gid_t   gid;

#ifdef HAVE_MAC_H
	mac_t   mac_label;
#endif	/* HAVE_MAC_H */
        int     we_created_it = 0;
        check_file_retval  rc = CHK_SUCCESS;
        int     fd;

        struct stat	stbuf;

        if ( ( mode & ~0777 ) != 0 )
                return( CHK_BAD_MODE );

        uid = name_to_uid( own );
        if ( uid < 0 )
                return( CHK_BAD_OWNER );

        gid = name_to_gid( grp );
        if ( gid < 0 )
                return( CHK_BAD_GROUP );

        if ( stat( file, &stbuf ) < 0 )
        {
                if ( ( fd = creat( file, mode ) ) < 0 )
                        return( CHK_NOT_CREATED );

                (void) close( fd );
                we_created_it = 1;

                if ( stat( file, &stbuf ) < 0 )
                        rc = CHK_STAT_FAILED;
        }

        if ( rc == CHK_SUCCESS && ! ( stbuf.st_mode & S_IFREG ) )
                rc = CHK_FILE_NOT_REGULAR;

        if ( rc == CHK_SUCCESS && stbuf.st_gid != gid )
        {
                if ( chown( file, stbuf.st_uid, gid ) < 0 )
                        rc = CHK_CANNOT_SET_GROUP;
        }

        if ( rc == CHK_SUCCESS && ( stbuf.st_mode & 0777 ) != mode )
        {
                if ( chmod( file, mode ) < 0 )
                        rc = CHK_CANNOT_SET_MODE;
        }
        if ( rc == CHK_SUCCESS && stbuf.st_uid != uid )
        {
                if ( chown( file, uid, gid ) < 0 )
                {
                        if ( errno != EPERM )
                                rc = CHK_CANNOT_SET_OWNER;
                }
        }

#ifdef HAVE_MAC_H
	if ( rc == CHK_SUCCESS && sysconf(_SC_MAC) )
	{
		/*
		 *	Set the MAC label of the accounting file to dbadmin.
		 */
		if (( mac_label = mac_from_text( "dbadmin" ) ) == NULL)
			rc = CHK_CANNOT_GET_MAC;
		else if ( mac_set_file( file, mac_label ) < 0 )
			rc = CHK_CANNOT_SET_MAC;
		mac_free( mac_label );
	}
#endif	/* HAVE_MAC_H */
	
        if ( rc != CHK_SUCCESS )
        {
                if ( we_created_it == 1 )
                        (void) unlink( file );
                else
                {
                        (void) chown( file, stbuf.st_uid, stbuf.st_gid );
                        (void) chmod( file, ( stbuf.st_mode & 0777 ) );
                }
        }

        return( rc );
}


/******************************************************************************
 *
 *  NAME
 *  	getoptlst - Get option argument list
 *
 *  SYNOPSIS
 *	int getoptlst(optarg, optargv)
 *	char *optarg;
 *	char ***optargv;
 *
 *  DESCRIPTION
 *	This routine parses the list pointed at by the given optarg.
 *	This list is not modified.  A single block of memory large
 *	enough to contain pointers to consecutive elements and the
 *	elements themselves is allocated by this routine using malloc().
 *	The returned optargv serves both as a pointer to this single
 *	block of memory and as a pointer to consecutive element
 *	pointers.  Thus the returned optargv can be used by the calling
 *	routine to obtain consecutive elements in the list and, once
 *	complete, for freeing (use free()) the space allocated by
 *	malloc().
 *
 *	An array of pointers is stored in consecutive locations
 *	beginning in the location indicated by the returned optargv.
 *	These pointers point to elements that were obtained from the
 *	list pointed at by the given optarg.  The first pointer points
 *	to the first element, the second pointer points to the second
 *	element, and so on.  The number of pointers is represented by
 *	the return value of this routine.  Also, this routine inserts
 *	a null pointer at the end of the array.  Elements pointed at by
 *	the array of pointers are represented as null-terminated
 *	character strings.
 *
 *	The list pointed at by the given optarg is assumed to be a
 *	null-terminated string of characters.  This string is not
 *	modified by this routine.  Elements within this string are
 *	separated by one of the following: white space (including
 *	blanks, tabs, and newlines), a comma, or the final null
 *	character.  Any character (other than a null character) that is
 *	preceded by a backslash is interpreted as the single character
 *	following the backslash (the '\' is removed).  The special
 *	meaning for that character, if any, is removed.  Thus, a
 *	backslash, blank, tab, newline, or comma can be forced into any
 *	element within the list by preceding it with a backslash
 *	character.  If the backslash character is the last character of
 *	the string pointed at by the given optarg (that is, it precedes
 *	a terminating null byte) then the backslash is treated as a
 *	normal character and it is not removed.  Empty fields can be
 *	recognized when the comma or null byte terminator are used as
 *	separators.  For example, the list '' contains one empty
 *	element, while the list ' , element2 ' contains one empty
 *	element followed by a second element that has a value of
 *	'element2'.
 *
 *  RETURN VALUE
 *	If an error is encountered within this routine, the return
 *	value is -1.  (The only possible error is if malloc() fails.)
 *	Otherwise, the return value equals the number of elements found
 *	in the list.  If the given optarg is null then the return value
 *	is set to zero; the optargv returned is set to null in this
 *	case.  If the given optarg is not null, the return value is set
 *	to one or greater (there is always at least one element in this
 *	case, even though it may be an empty element).
 *
 *  EXAMPLE
 *	The getoptlst routine is intended for use with the getopt()
 *	library routine.  The following code fragment shows how you
 *	might process the arguments of a command using both getopt and
 *	getoptlst.  The -l option requires an argument in this example
 *	and this argument is processed as a list of elements.
 *
 *	#include <stdio.h>
 *
 *	extern char *optarg;
 *	extern int optind, opterr;
 *
 *	main(argc, argv)
 *	int argc;
 *	char *argv[];
 *	{
 *		static int aflg = 0, bflg = 0, errflg = 0;
 *		static char *ifile, *ofile;
 *		char **optargv;
 *		int optargc, c, i;
 *		void free(), bproc();
 *		int getopt(), getoptlst();
 *
 *		while ((c = getopt(argc, argv, "abf:l:o:")) != EOF)
 *			switch (c) {
 *			case 'a':
 *				if (bflg)
 *					errflg++;
 *				else
 *					aflg++;
 *				break;
 *			case 'b':
 *				if (aflg)
 *					errflg++;
 *				else
 *					bproc();
 *				break;
 *			case 'f':
 *				ifile = optarg;
 *				break;
 *			case 'l':
 *				if ((optargc =
 *				     getoptlst(optarg, &optargv)) < 0)
 *					errflg++;
 *				else {
 *					fprintf(stdout,
 *					    "Elements: %d\n", optargc);
 *					for (i = 0; i < optargc; i++) {
 *						fprintf(stdout,
 *				    		    "optargv[%d]: '%s'\n",
 *						    i, optargv[i]);
 *					}
 *					free(optargv);
 *				}
 *				break;
 *			case 'o':
 *				ofile = optarg;
 *				break;
 *			case '?':
 *				errflg++;
 *			}
 *		if (errflg) {
 *			fprintf(stderr, "Usage: ...\n");
 *			exit (2);
 *		}
 *		for (; optind < argc; optind++) {
 *			if (!access(argv[optind], 4)) {
 *				fprintf(stdout,
 *				    "%s readable\n",
 *				    argv[optind]);
 *			} else {
 *				fprintf(stdout,
 *				    "%s NOT readable\n",
 *				    argv[optind]);
 *			}
 *		}
 *	}
 *
 *	void
 *	bproc()
 *	{
 *		fprintf(stdout, "bproc called\n");
 *	}
 *
 *  SEE ALSO
 *	getopt(3C), malloc(3C)
 *
 *****************************************************************************/

#define FAILURE -1
#define NUL '\0'

int
getoptlst(char *optarg, char ***optargv)
{
	char	c, *s1, *s2;
	int	ccount = 0, optargc = 0;

	/*
	 *  If the given optarg is a null pointer then there are
	 *  no arguments.
	 */
	if (optarg == NULL) {
		*optargv = NULL;
		return(0);
	}

	/*
	 *  Scan the string pointed at by the given optarg
	 *  counting the number of elements within the list.
	 */
	s1 = optarg;
	while (1) {
		optargc++;
		/*
		 *  Skip leading white space, if any.
		 */
		while (((c = *(s1++)) == ' ') || (c == '\t') || (c == '\n'))
			;
		/*
		 *  If a null element is encountered, increment
		 *  the character count for a NUL and break from
		 *  the loop.
		 */
		if (c == NUL) {
			ccount++;
			break;
		}
		/*
		 *  Recognize a comma as a delimiting character.
		 *  If found, it causes the definition of a null
		 *  element.  Thus the character count must be
		 *  incremented for the NUL.
		 */
		if (c == ',') {
			ccount++;
			continue;
		}
		/*
		 *  Count the characters within this element.
		 */
		while (1) {
			if ((c == '\\') && (*s1 != NUL)) {
				ccount++;
				s1++;
			} else if (c == NUL) {
				ccount++;
				break;
			} else if ((c == ' ') || (c == '\t') || (c == '\n')) {
				ccount++;
				while (((c = *s1) == ' ') ||
					(c == '\t') || (c == '\n'))
					s1++;
				if (c == ',')
					s1++;
				break;
			} else if (c == ',') {
				ccount++;
				break;
			} else
				ccount++;
			c = *(s1++);
		}
		if (c == NUL)
			break;
	}

	/*
	 *  Using optargc (which contains the number of elements
	 *  found) and ccount (which contains the character count
	 *  necessary to contain all elements) allocate a single
	 *  block of memory to store both the element pointers and
	 *  the elements.
	 */
	if ((*optargv = (char **)
	     malloc((optargc + 1) * sizeof(char *) + ccount)) == NULL)
		return(FAILURE);
	s2 = (char *)*optargv + (optargc + 1) * sizeof(char *);

	/*
	 *  Rescan the list extracting the elements and inserting
	 *  them into the malloc'd space.
	 */
	s1 = optarg;
	optargc = 0;
	while (1) {
		/*
		 *  Skip leading white space, if any.
		 */
		while (((c = *(s1++)) == ' ') || (c == '\t') || (c == '\n'))
			;
		/*
		 *  If a null element is represented as an
		 *  empty string.
		 */
		if (c == NUL) {
			(*optargv)[optargc++] = s2;
			*s2 = NUL;
			break;
		}
		/*
		 *  Recognize a comma as a delimiting character.
		 *  If found, it causes the definition of a null
		 *  element.
		 */
		if (c == ',') {
			(*optargv)[optargc++] = s2;
			*s2++ = NUL;
			continue;
		}
		(*optargv)[optargc++] = s2;
		/*
		 *  Extract the characters of this element.
		 */
		while (1) {
			if ((c == '\\') && (*s1 != NUL)) {
				*s2++ = *s1++;
			} else if (c == NUL) {
				*s2 = NUL;
				break;
			} else if ((c == ' ') || (c == '\t') || (c == '\n')) {
				*s2++ = NUL;
				while (((c = *s1) == ' ') ||
					(c == '\t') || (c == '\n'))
					s1++;
				if (c == ',')
					s1++;
				break;
			} else if (c == ',') {
				*s2++ = NUL;
				break;
			} else
				*s2++ = c;
			c = *(s1++);
		}
		if (c == NUL)
			break;
	}

	(*optargv)[optargc] = NUL;
	return(optargc);
}
