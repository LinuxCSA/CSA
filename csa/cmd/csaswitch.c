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

/*****************************************************************************
 *
 * csaswitch.c:  Check the status of the different types of system accounting,
 *               enable or disable them, and switch files for maintainability.
 *
 *	usage:	csaswitch [-D level] -c check -n name
 *		csaswitch [-D level] -c halt
 *		csaswitch [-D level] -c off -n namelist
 *		csaswitch [-D level] -c on [-n namelist] [-m memthreshold]
 *				[-t timethreshold] [-P pathname]
 *		csaswitch [-D level] -c status
 *		csaswitch [-D level] -c switch
 *
 *	where:	-D level	- Debug level (0-3).
 *		-c [ check | halt | off | on | status | switch ] - An
 *				  indication of what this program is to do.
 *		-n name		- Process, daemon, or record accounting name.
 *		-n namelist	- A single name or a comma separated list of
 *				  names.
 *		-m memthreshold	- Memory threshold value - in Kbytes.
 *		-t timethreshold - Time threshold value - in seconds.
 *		-P pathname	- Pathname of the accounting file to use.
 *
 * NOTES
 *	The valid names are defined by the name_list[] array below.  The list
 *	of accounting methods in this array must match the order of the list
 *	in the ac_kdrcd enum in <linux/acct.h>.
 *
 *	If necessary, the file specified by <pathname> is created.  The file's
 *	owner is set to user "csaacct", group is set to group "csaacct", and the
 *	mode is set to 0664.
 *
 *	---> The group will actually be set to whatever is returned by
 *	     executing:
 *
 *   init_char(CHGRP, ADM, TRUE);
 *
 * DEPENDENCIES
 *	The ability for this program to correctly display the current status
 *	of all accounting methods (i.e. -c status) is dependent on the kernel
 *	returning an actctl structure such that the elements in the ac_stat
 *	array are in the order they are defined in the ac_kdrcd enum in
 *	<csa.h>.
 *
 * RETURNS
 *	0	- The requested function was performed successfully.
 *	!= 0	- Some sort of an error occurred and nothing was changed.
 *
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_LIBCAP
#include <sys/capability.h>
#endif	/* HAVE_LIBCAP */

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctctl.h"
#include "acctdef.h"
#include "acctmsg.h"
#include "csa.h"
#include "csa_api.h"

#define ADM	"csaacct"	/* Default owner and group to use when creating the
			   the accounting file.
			   NOTE:  The group is overwritten by specifying a
			   value for the csa.conf label "CHGRP".
			 */
#define MODE	0664	/* The accounting file's mode. */

extern	int	db_flag;	/* debug flag */

char	*Prg_name;		/* Program name */

/*
 * The following structure defines an entry in the name_list[] array below:
 *
 *	Field		Meaning
 *	----------	------------------------------------------------------
 *	id		An indentifier used in the kernel to identify this
 *			accounting method.
 *	flag		A flag byte... See below.
 *	name		A character string which identifies this accounting
 *			method, specified with the -n option.
 *	req_state	The request state of this accounting method.
 *	req_param	The request parameter (i.e., threshold) of this
 *			accounting method.
 *	config_name	Name of the accounting method in the csa.conf file.
 */
struct acct_method
{
    ac_kdrcd    id;           /* Identifier of accounting method */
    int         flag;	      /* Flags...See below */
    char       *name;         /* Name of accounting method */
    int         req_state;    /* Request state */
    int64_t     req_param;    /* Request parameter (i.e., threshold) */
    char       *config_name;  /* Name in the csa.conf file */
};

/*
 * STAT_FLAG - Used to indicate which accounting methods are actually
 *             "active."  Only the ones that are active will be displayed on
 *             a "status" request and searched for in the csa.conf file.
 */
#define	STAT_FLAG	010

#define NOT_SET  -1

/*
 * The list of accounting methods in this array must match the order of the
 * list in the ac_kdrcd enum in <linux/csa_kern.h>.
 */
struct acct_method name_list[] =
{
    /*  id,              flag, name,	  req_state, p,  config_name */
    { ACCT_KERN_CSA,      010, "csa",	  NOT_SET,   0,  CSA_START   },
    { ACCT_KERN_JOB_PROC, 000, "job",	  NOT_SET,   0,  NULL        },
    { ACCT_DMD_WKMG,      010, "wkmg",	  NOT_SET,   0,  WKMG_START  },
    { ACCT_DMD_SITE1,     000, "site1",	  NOT_SET,   0,  NULL        },
    { ACCT_DMD_SITE2,     000, "site2",	  NOT_SET,   0,  NULL        },
    { ACCT_MAXKDS,        000, "",	  NOT_SET,   0,  NULL        },

    { ACCT_RCD_MEM,       010, "mem",	  NOT_SET,   0,  MEM_START   },
    { ACCT_RCD_IO,        010, "io",	  NOT_SET,   0,  IO_START    },
    { ACCT_RCD_DELAY,     010, "delay",	  NOT_SET,   0,  DELAY_START },
    { ACCT_THD_MEM,       010, "memt",	  NOT_SET,   0,  MEMT_START  },
    { ACCT_THD_TIME,      010, "time",	  NOT_SET,   0,  TIME_START  },
    { ACCT_RCD_SITE1,     000, "rsite1",  NOT_SET,   0,  NULL        },
    { ACCT_RCD_SITE2,     000, "rsite2",  NOT_SET,   0,  NULL        },
    { ACCT_MAXRCDS,       000, "",	  NOT_SET,   0,  NULL        },
};

char *status_list[] = { "Off", "Error", "On" };


/*****************************************************************************
 *
 * NAME
 *	check_name	- Verifies whether the specified accounting method is
 *			  valid.
 *
 * SYNOPSIS
 *	int = check_name( name );
 *
 *	name		r	A pointer to the name of the accounting method
 *				to be verified.
 *
 * DESCRIPTION
 *	This routine loops through the name_list[] array looking for the entry
 *	whose name matches the given <name>.  This value is returned to the
 *	caller and is intended to define the <ac_ind> field of the structure
 *	passed to the acctctl() system call.
 *
 *	NOTE:	See "linux/acct.h" for the list of valid accounting methods.
 *
 * RETURNS
 *	-1	- If the specified <name> does NOT match any entry.
 *	>=0	- The index in name_list[] whose entry matches <name>.
 *
 *****************************************************************************/
int check_name(char *name)
{
    int	 i;
    int  n = sizeof(name_list) / sizeof(*name_list);

    for (i = 0; i < n; i++)
    {
        if (! strcasecmp(name_list[i].name, name))
        {
            if (i == ACCT_MAXKDS)
                continue;
            else
                break;
        }
    }

    if (i < n)
        return( i );

    return( -1 );
}

/*****************************************************************************
 *
 * NAME
 *	check_request	- Verifies the user's request.
 *
 * SYNOPSIS
 *	int = check_request( request, switch_file );
 *
 *	request		r	A pointer to the request to be verified.
 *	switch_file	w	Indicates whether this is a switch request.
 *
 * DESCRIPTION
 *	This routine returns the appropriate -defined- value associated with
 *	the indicated request, as specified below.  This value is intended to
 *	be used as the first parameter passed on the acctctl() system call.
 *
 *		<request>	Value returned
 *		---------	-----------------
 *		check		AC_CHECK
 *		halt 		AC_HALT
 *		off		AC_STOP
 *		on		AC_START
 *		status		AC_KDSTAT
 *		switch		AC_START  (switch_file = TRUE)
 *
 *	NOTE:	See "linux/acct.h" for the list of valid requests.
 *
 * RETURNS
 *	-1	- If the specified <request> is NOT valid.
 *	>= 0	- The request to be used on the acctctl() system call.
 *
 *****************************************************************************/
int check_request(char * request, int *switch_file)
{
    if ( ! strcasecmp(request, "check") )
        return( AC_CHECK );

    if ( ! strcasecmp(request, "halt") )
        return( AC_HALT );

    if ( ! strcasecmp(request, "off") )
        return( AC_STOP );

    if ( ! strcasecmp(request, "on") )
        return( AC_START );

    if ( ! strcasecmp(request, "status") )
        return( AC_KDSTAT );

    if ( ! strcasecmp(request, "switch") )
    {
        *switch_file = TRUE;
        return( AC_START );
    }
    
    return( -1 );
}

/*****************************************************************************
 *
 * NAME
 *	parse_name_list	- Parse and validate the accounting method name list.
 *
 * SYNOPSIS
 *	int = parse_name_list(str, state);
 *
 *	str 		r	Name list string to parse.
 *	state		r	Requested accounting state.
 *
 * DESCRIPTION
 *	This routine parses the name list string, validates the names,
 *	and sets the requested state in the name_list[] structure.
 *
 *
 * RETURNS
 *	>= 0	- Number of accounting method names.
 *	-1	- If an error was detected in accounting method name list.
 *
 *****************************************************************************/
int parse_name_list(char *str, int state)
{
    int	    ind, indx;
    int	    optargc;
    char  **optargv;

    if (db_flag > 2)
        Ndebug("parse_name_list(3): '%s'\n", str);

    /* Parse the name list string. */
    if ((optargc = getoptlst(str, &optargv)) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("There was insufficient memory available when allocating '%s'."),
        	"optargv");

    /* Search for each accounting method name. */
    for (ind = 0; ind < optargc; ind++)
    {
        if ((indx = check_name(optargv[ind])) < 0)
            return(-1);
        else
            name_list[indx].req_state = state;
    }

    return(optargc);
}

/*****************************************************************************
 *
 * NAME
 *	do_error	- Controls error processing.
 *
 * SYNOPSIS
 *	do_error( error, level, s1, s2 );
 *
 *	level		r	An indication of the <error>'s severity.
 *	error		r	An indication of the error that occurred.
 *	s1, s2		r	Optional information to be included in the
 *				error message.
 *
 * DESCRIPTION
 *	This routine displays an error message describing the <error> that
 *	occurred and terminates this program's execution if <level> is either
 *	ACCT_FATAL or ACCT_ABORT.
 *
 *	If the <error> is a parameter specification error -and- this program's
 *	execution is to be terminated, a command usage message will also be
 *	displayed.
 *
 *	NOTES:	The usage message will NOT be displayed if the given <level>
 *		is ACCT_ABORT.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If <level> is either ACCT_FATAL or ACCT_ABORT, this routine
 *		does NOT return.
 *
 *****************************************************************************/
void do_error(accterr level, char *error, char *s1, char *s2)
{
    if (s1 == (char *) NULL)
        s1 = "NULL";

    if (s2 == (char *) NULL)
        s2 = "NULL";

    acct_err(level, _(error), s1, s2);

    /* acct_err() has already done an exit() on ACCT_ABORT at this point. */
    
    if (level != ACCT_FATAL)
        return;

    if (csa_auth() != 0)
        acct_err(ACCT_FATAL,
               "Command Usage:\n%s\t[-D level] -c check -n name\n%s\t[-D level] -c status",
		 Prg_name, Prg_name);
    else
        acct_err(ACCT_FATAL,
               "Command Usage:\n%s\n\t[-D level] -c check -n name\n\t[-D level] -c halt\n\t[-D level] -c off -n namelist\n\t[-D level] -c on [-n namelist] [-m memthresh] [-t timethresh] [-P path]\n\t[-D level] -c status\n\t[-D level] -c switch\n\n\tValid arguments to the -n option are:\n\t\tcsa, wkmg, io, mem, delay, memt, time",
		 Prg_name);

    exit(-1);
}

/*****************************************************************************
 *
 * NAME
 *	do_perror	- Controls error processing that includes an errno
 *			  message code.
 *
 * SYNOPSIS
 *	do_perror( level, errno, error, s1, s2 );
 *
 *	level		r	An indication of the <error>'s severity.
 *	errnm		r	System errno.
 *	error		r	An indication of the error that occurred.
 *	s1, s2		r	Optional information to be included in the
 *				error message.
 *
 * DESCRIPTION
 *	This routine displays an error message describing the <error> that
 *	occurred and terminates this program's execution if <level> is either
 *	ACCT_FATAL or ACCT_ABORT.
 *
 *	If the <error> is a parameter specification error -and- this program's
 *	execution is to be terminated, a command usage message will also be
 *	displayed.
 *
 *	NOTES:	The usage message will NOT be displayed if the given <level>
 *		is ACCT_ABORT.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If <level> is either ACCT_FATAL or ACCT_ABORT, this routine
 *		does NOT return.
 *
 *****************************************************************************/
void do_perror(accterr level, int errnm, char *error, char *s1, char *s2)
{
    if (s1 == (char *) NULL)
        s1 = "NULL";

    if (s2 == (char *) NULL)
        s2 = "NULL";

    acct_perr(level, errnm, _(error), s1, s2);

    /* acct_perr() has already done an exit() on ACCT_ABORT at this point. */
    
    if (level != ACCT_FATAL)
        return;

    exit(errnm);

}

/*****************************************************************************
 *
 * NAME
 *	get_config	- Get information from the csa.conf file.
 *
 * SYNOPSIS
 *	get_config();
 *
 * DESCRIPTION
 *	This routine gets information from the csa.conf file as to what
 *	should be turned on, along with the thresholds.  It sets the requested
 *	state (on) in the name_list[] structure.
 *
 *	It is expected that at least one accounting method is found to be on
 *	in csa.conf.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
void get_config()
{
    int        any_on = 0;
    char      *config_str;
    int        i;
    int	       optargc;
    char     **optargv;
    int64_t    threshold;

    if (db_flag > 2)
        Ndebug("get_config(3):\n");

    for (i = 0; i < ACCT_MAXRCDS; i++)
    {
        if (! (name_list[i].flag & STAT_FLAG))
            continue;

        config_str = init_char(name_list[i].config_name, NULL, TRUE);
        if (config_str == NULL)
            continue;

        if (db_flag > 2)
            Ndebug("    %s\t'%s'\n", name_list[i].config_name, config_str);
        
        /* Parse the config string. */
        if ((optargc = getoptlst(config_str, &optargv)) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("There was insufficient memory available when allocating '%s'."),
            	"optargv");

        if (! strcasecmp(optargv[0], "on"))
        {
            if (name_list[i].id == ACCT_THD_MEM ||
                name_list[i].id == ACCT_THD_TIME)
            {
                if (optargc == 1)
                {
                    acct_err(ACCT_CAUT,
                           _("A threshold value is required for %s in the csa.conf file."),
			     name_list[i].config_name);
                    continue;
                }
                threshold = atoll(optargv[1]);
                if (threshold < 0)
                {
                    acct_err(ACCT_CAUT,
                           _("Threshold value for %s in the csa.conf file must be positive."),
			     name_list[i].config_name);
                    continue;
                }
                name_list[i].req_param = threshold;
            }
            name_list[i].req_state = ACS_ON;
            any_on++;
        }
        else
            continue;
    }

    /* Must have something on. */
    if (any_on == 0)
        acct_err(ACCT_ABORT,
               _("None of the accounting methods in the csa.conf file is on."));

    return;
}
    
/*****************************************************************************
 *
 * NAME
 *	do_file		- Verify the accounting file.
 *
 * SYNOPSIS
 *	check_file_retval do_file(acct_path);
 *
 *	acct_path	r	The accounting file to be verified.
 *
 * DESCRIPTION
 *	This routine calls check_file() to verify:
 *
 *	1.  <acct_path> exists and is a regular file.
 *	2.  <acct_path>'s owner and group are set appropriately.
 *	3.  <acct_path>'s permissions are set appropriately.
 *	4.  <acct_path>'s MAC label is set appropriately - for Trusted IRIX.
 *
 * RETURNS
 *	check_file_retval	- Return value from check_file().
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
check_file_retval do_file(char *acct_path)
{
    char               *group;
    char               *owner = ADM;
    check_file_retval   retval;

    /* Get the group information from the csa.conf file.  If there is
       nothing in csa.conf, we'll use our default, ADM. */
    group = init_char(CHGRP, ADM, TRUE);

    if ((retval = check_file(acct_path, owner, group, MODE)) != CHK_SUCCESS)
    {
        if (retval == CHK_BAD_OWNER)
            acct_err(ACCT_FATAL,
                   _("Unable to get the User ID of user '%s'."),
		     owner);
        else if (retval == CHK_BAD_GROUP)
            acct_err(ACCT_FATAL,
                   _("Unable to get the Group ID of group '%s'."),
		     group);
        else if (retval == CHK_NOT_CREATED)
            acct_err(ACCT_FATAL,
                   _("An error occurred creating file '%s'."),
		     acct_path);
        else if (retval == CHK_STAT_FAILED)
            acct_err(ACCT_FATAL,
                   _("An error occurred getting the status of file '%s'."),
		     acct_path);
        else if (retval == CHK_CANNOT_SET_GROUP)
            acct_err(ACCT_FATAL,
                   _("An error occurred changing the group on file '%s' to '%s'."),
            	acct_path, group);
        else if (retval == CHK_CANNOT_SET_MODE)
            acct_err(ACCT_FATAL,
                   _("An error occurred changing the mode on file '%s' to '%o'."),
            	acct_path, MODE);
        else if (retval == CHK_CANNOT_SET_OWNER)
            acct_err(ACCT_FATAL,
                   _("An error occurred changing the owner on file '%s' to '%s'."),
            	acct_path, owner);
        else if (retval == CHK_FILE_NOT_REGULAR)
            acct_err(ACCT_FATAL,
                   _("File '%s' exists, but it is not a regular file."),
		     acct_path);
        else if (retval == CHK_CANNOT_GET_MAC)
            acct_err(ACCT_FATAL,
                   _("Unable to get the MAC label for '%s'."),
		     "dbadmin");
        else if (retval == CHK_CANNOT_SET_MAC)
            acct_err(ACCT_FATAL,
                   _("Unable to set the MAC label of file %s."),
		     acct_path);
        else
            acct_err(ACCT_FATAL,
                   _("An unknown/unrecoverable internal error occurred."));
    }

    return(retval);
}

/*****************************************************************************
 *
 * NAME
 *	move_file	- Move the pacct file to pacct#.
 *
 * SYNOPSIS
 *	move_file(old_path, new_path);
 *
 *	old_path	r	pacct path name.
 *	new_path	w	pacct# path name.
 *
 * DESCRIPTION
 *	This routine finds the # to be used for the pacct# file, links the
 *	pacct# file to the pacct file, then unlinks the pacct file.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
void move_file(char *old_path, char *new_path)
{
    int          ind = 1;
    struct stat  stbuf;

    /* Find the # to be used for the pacct# file. */
    while (TRUE)
    {
        sprintf(new_path, "%s%d", old_path, ind);
        if (stat(new_path, &stbuf) < 0)
            break;
        ind++;
    }

    if (db_flag > 2)
        Ndebug("move_file(3): old_path = '%s', new_path = '%s'\n",
               old_path, new_path);

    /* Link the new file to the old file. */
    if (link(old_path, new_path) < 0)
        do_perror(ACCT_ABORT, errno,
		_("An error occurred linking file '%s'."),
		 new_path, NULL);


    /* Unlink the old file. */
    if (unlink(old_path) < 0)
        do_perror(ACCT_ABORT, errno,
		_("An error occurred unlinking file '%s'."),
		 old_path, NULL);
        
    return;
}

/*****************************************************************************
 *
 * NAME
 *	do_start	- Turn "on" the indicated accounting method(s).
 *
 * SYNOPSIS
 *	int do_start(n_specified, mem_threshold, time_threshold, acct_path);
 *
 *	n_specified	r	Indicates whether the -n option was specified.
 *	mem_threshold	r	Memory threshold for the -m option.
 *	time_threshold	r	Time threshold for the -t option.
 *	acct_path	rw	The data file to be used by the accounting
 *				method(s) being turned on.
 *
 * DESCRIPTION
 *	This routine consults the csa.conf file for anything that is not
 *	supplied by the user.  It calls do_file() to verify the accounting
 *	file.
 *
 *	If a linked record is being turned on, the CSA base record ("csa") is
 *	also turned on since there can be no linked records without the base
 *	record.  The same happens when a threshold is being set since
 *	thresholds will not have any effect if csa isn't running.
 *
 * RETURNS
 *      0       - If the indicated accounting method(s) were turned on.
 *      -1      - If do_file() terminated in error.
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
int do_start(int n_specified, int64_t mem_threshold, int64_t time_threshold,
             char *acct_path)
{
    int            any_on = FALSE;
    struct csa_status_req	kdstat_req;
    struct csa_start_req	start_req;
    int            i, j;

    /* If the -n option wasn't specified, get information from the csa.conf
       file as to what should be turned on, along with the thresholds. */
    if (n_specified == FALSE)
        get_config();
    
    /* Get the kernel and daemon accounting status. */
    bzero((char *) &kdstat_req, sizeof(kdstat_req));
    kdstat_req.st_num = NUM_KDS;
    if (csa_kdstat(&kdstat_req) != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to get the Kernel and Daemon accounting status information.")
        	);

    /* Can't have the linked records or thresholds without the base record. */
    if ((name_list[ACCT_RCD_MEM].req_state == ACS_ON ||
         name_list[ACCT_RCD_IO].req_state == ACS_ON ||
         name_list[ACCT_RCD_DELAY].req_state == ACS_ON ||
         name_list[ACCT_THD_MEM].req_state == ACS_ON ||
         name_list[ACCT_THD_TIME].req_state == ACS_ON) &&
        kdstat_req.st_stat[ACCT_KERN_CSA].am_status != ACS_ON)
    {
        name_list[ACCT_KERN_CSA].req_state = ACS_ON;
    }

    /* The memory and time thresholds specified on the command line override
       those in the csa.conf file. */
    if (mem_threshold > 0)
        name_list[ACCT_THD_MEM].req_param = mem_threshold;
    if (time_threshold > 0)
        name_list[ACCT_THD_TIME].req_param = time_threshold;

    /* If no accounting method is currently on, we need the accounting file
       information.  If it is not supplied, we'll get it from the csa.conf
       file.  If there is nothing in csa.conf, we'll use our default,
       DEFPACCT.
    */
    if (strlen(acct_path) == 0)
    {
        /* Check if any accounting method is on. */
        for (i = 0; i < ACCT_MAXKDS; i++)
        {
            if (kdstat_req.st_stat[i].am_status == ACS_ON)
            {
                any_on = TRUE;
                break;
            }
        }
        if (any_on == FALSE)
        {
            strcpy(acct_path, init_char(PACCT_FILE, DEFPACCT, TRUE));
            if (db_flag > 0)
                Ndebug("do_start(1): Path missing for ON\n");
        }
    }

    if (strlen(acct_path))
    {
        if (do_file(acct_path) != CHK_SUCCESS)
            return(-1);
    }

    if (db_flag > 0)
    {
        Ndebug("do_start(1):\n");
        Ndebug("    Memory threshold: %lld\n", mem_threshold);
        Ndebug("    Time threshold: %lld\n", time_threshold);
        Ndebug("    Accounting pathname: '%s'\n", acct_path);
        if (db_flag > 1)
        {
            Ndebug("do_start(2):\n");
            Ndebug("    acctctl(AC_START):\n");
        }
    }
    
    bzero((char *) &start_req, sizeof(start_req));
    
    for (i = 0, j = 0; i < ACCT_MAXRCDS; i++)
    {
        if (i == ACCT_MAXKDS)
            continue;
        if (name_list[i].req_state == ACS_ON)
        {
            start_req.sr_method[j].sr_id = name_list[i].id;
            start_req.sr_method[j].param = name_list[i].req_param;
            if (db_flag > 1)
            {
                Ndebug("        id = %d, name = %s, parameter = %lld\n",
                       name_list[i].id, name_list[i].name,
                       start_req.sr_method[j].param);
            }
            j++;
        }
    }
    start_req.sr_num = j;
    strcpy(start_req.sr_path, acct_path);

    if (csa_start(&start_req) != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to %s accounting."),
		  "enable");

    return(0);
}

/*****************************************************************************
 *
 * NAME
 *	do_switch	- Switch the accounting file.
 *
 * SYNOPSIS
 *	int do_switch();
 *
 * DESCRIPTION
 *	This routine switches the pacct file to pacct#, starts a fresh pacct.
 *
 *	If accounting is not currently on, do_switch() will turn it on.  It
 *	gets the accounting file information from the csa.conf file, as well
 *	as what should be turned on.
 *
 *	If a linked record is being turned on, the CSA base record ("csa") is
 *	also turned on since there can be no linked records without the base
 *	record.  The same happens when a threshold is being set since
 *	thresholds will not have any effect if csa isn't running.
 *
 *	do_switch() calls do_file() to verify the accounting file.
 *
 * RETURNS
 *	0       - If the accounting file was switched.
 *      -1      - If do_file() or acctctl() terminated in error.
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
int do_switch()
{
    char           acct_path[ACCT_PATH+1];
    int            any_on = FALSE;
    struct csa_status_req	kdstat_req;
    struct csa_start_req	start_req;
    int            error = FALSE;
    int            i, j;
    char           new_path[ACCT_PATH+1];
    struct stat    stbuf;

    new_path[0] = '\0';
    
    /* Get the kernel and daemon accounting status. */
    bzero((char *) &kdstat_req, sizeof(kdstat_req));
    kdstat_req.st_num = NUM_KDS;
    if (csa_kdstat(&kdstat_req) != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to get the Kernel and Daemon accounting status information.")
        	);

    /* Check if any accounting method is on. */
    for (i = 0; i < NUM_KDS; i++)
    {
        if (kdstat_req.st_stat[i].am_status == ACS_ON)
        {
            any_on = TRUE;
            break;
        }
    }

    if (any_on == TRUE)
    {
        strcpy(acct_path, kdstat_req.st_path);
        move_file(acct_path, new_path);  /* Move the pacct file to pacct#. */
    }
    else
    {
        /* Get information from the csa.conf file as to what should be
           turned on, along with the thresholds. */
        get_config();
        
        /* Can't have the linked records or thresholds without the base
           record. */
        if (name_list[ACCT_RCD_MEM].req_state == ACS_ON ||
            name_list[ACCT_RCD_IO].req_state == ACS_ON ||
            name_list[ACCT_RCD_DELAY].req_state == ACS_ON ||
            name_list[ACCT_THD_MEM].req_state == ACS_ON ||
            name_list[ACCT_THD_TIME].req_state == ACS_ON)
        {
            name_list[ACCT_KERN_CSA].req_state = ACS_ON;
        }

        /* Get the accounting file information from the csa.conf file.  If
           there is nothing in csa.conf, we'll use our default, DEFPACCT. */
        strcpy(acct_path, init_char(PACCT_FILE, DEFPACCT, TRUE));

        if (stat(acct_path, &stbuf) == 0)
        {
            /* Move the pacct file to pacct#. */
            move_file(acct_path, new_path);
        }
    }

    if (do_file(acct_path) != CHK_SUCCESS)
        error = TRUE;
    else
    {
        if (db_flag > 0)
        {
            Ndebug("do_switch(1):\n");
            Ndebug("    Accounting pathname: '%s'\n", acct_path);
            if (db_flag > 1)
            {
                Ndebug("do_switch(2):\n");
                Ndebug("    acctctl(AC_START):\n");
            }
        }
        
        bzero((char *) &start_req, sizeof(start_req));

        if (any_on == TRUE)
        {
            /* Have to turn something on.  If ac_sttnum is zero, acctctl()
               sets it to 1 and thus will turn on ACCT_KERN_CSA. */
            start_req.sr_method[0].sr_id = kdstat_req.st_stat[i].am_id;
            start_req.sr_method[0].param = kdstat_req.st_stat[i].am_param;
            start_req.sr_num = 1;
            if (db_flag > 1)
            {
                Ndebug("        id = %d, name = %s, parameter = %lld\n",
                       kdstat_req.st_stat[i].am_id,
                       name_list[kdstat_req.st_stat[i].am_id].name,
                       kdstat_req.st_stat[i].am_param);
            }
        }
        else
        {
            for (i = 0, j = 0; i < ACCT_MAXRCDS; i++)
            {
                if (i == ACCT_MAXKDS)
                    continue;
                if (name_list[i].req_state == ACS_ON)
                {
                    start_req.sr_method[j].sr_id = name_list[i].id;
                    start_req.sr_method[j].param = name_list[i].req_param;
                    if (db_flag > 1)
                    {
                        Ndebug("        id = %d, name = %s, "
                               "parameter = %lld\n", name_list[i].id,
                               name_list[i].name,
                               start_req.sr_method[j].param);
                    }
                    j++;
                }
            }
            start_req.sr_num = j;
        }
        strcpy(start_req.sr_path, acct_path);

        if (csa_start(&start_req) != 0)
        {
            error = TRUE;
            acct_perr(ACCT_FATAL, errno,
            	_("Unable to %s accounting."),
            	"switch");
        }
    }

    /* Restore the old accounting file. */
    if (error == TRUE && new_path[0] != '\0')
    {
        if (stat(acct_path, &stbuf) == 0)
        {
            /* Unlink the pacct file. */
            if (unlink(acct_path) < 0)
                do_perror(ACCT_ABORT, errno,
			_("An error occurred unlinking file '%s'."),
			 acct_path, NULL);
        }
        /* Link the pacct file to the pacct# file. */
        if (link(new_path, acct_path) < 0)
            do_perror(ACCT_ABORT, errno,
		    _("An error occurred linking file '%s'."),
		     acct_path, NULL);
        /* Unlink the pacct# file. */
        if (unlink(new_path) < 0)
            do_perror(ACCT_ABORT, errno,
		    _("An error occurred unlinking file '%s'."),
		     new_path, NULL);
    }

    if (error == TRUE)
        return(-1);
    
    return(0);
}

/*****************************************************************************
 *
 * NAME
 *	do_stop		- Turn "off" the indicated accounting method(s).
 *
 * SYNOPSIS
 *	do_stop();
 *
 * DESCRIPTION
 *	If the CSA base record ("csa") is being turned off, then all the
 *	linked records are also turned off since there can be no linked
 *	records without the base record.  Thresholds are also turned off
 *	since thresholds will not have any effect if csa isn't running.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
void do_stop()
{
    struct csa_stop_req	stop_req;
    int            	i, j;

    /* Can't have the linked records or thresholds without the base record. */
    if (name_list[ACCT_KERN_CSA].req_state == ACS_OFF)
    {
        name_list[ACCT_RCD_MEM].req_state = ACS_OFF;
        name_list[ACCT_RCD_IO].req_state = ACS_OFF;
        name_list[ACCT_RCD_DELAY].req_state = ACS_OFF;
        name_list[ACCT_THD_MEM].req_state = ACS_OFF;
        name_list[ACCT_THD_TIME].req_state = ACS_OFF;
    }

    if (db_flag > 1)
    {
        Ndebug("do_stop(2):\n");
        Ndebug("    acctctl(AC_STOP):\n");
    }
    
    bzero((char *) &stop_req, sizeof(stop_req));
    
    for (i = 0, j = 0; i < ACCT_MAXRCDS; i++)
    {
        if (i == ACCT_MAXKDS)
            continue;
        if (name_list[i].req_state == ACS_OFF)
        {
            stop_req.pr_id[j] = name_list[i].id;
            j++;
            if (db_flag > 1)
            {
                Ndebug("        id = %d, name = %s\n",
                       name_list[i].id, name_list[i].name);
            }
        }
    }
    stop_req.pr_num = j;

    if (csa_stop(&stop_req) != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to %s accounting."),
        	"disable");

    return;
}

/*****************************************************************************
 *
 * NAME
 *	do_halt		- Turn "off" all accounting methods.
 *
 * SYNOPSIS
 *	do_halt();
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
void do_halt()
{
    if (db_flag > 1)
        Ndebug("do_halt(2): acctctl(AC_HALT)\n");
        
    if (csa_halt() != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to %s accounting."),
        	"halt system");

    return;
}

/*****************************************************************************
 *
 * NAME
 *	do_format	- Format and display the information contained in an
 *			  "actstat" structure.
 *
 * SYNOPSIS
 *	do_format( indx, status );
 *
 *	indx		r	The index into name_list[] which identifies
 *				the accounting method to be formatted.
 *	status		r	A pointer to the "actstat" structure to
 *				format.
 *
 * DESCRIPTION
 *	This routine formats the status information into human readable text.
 *	This information is written to standard output.
 *
 *	The time threshold value is converted from microseconds to seconds
 *	before it is printed.
 *
 * RETURNS
 *	Nothing
 *
 *****************************************************************************/
void do_format(int indx, struct csa_am_stat *status)
{
    int64_t     param;
    double	time_param;
    static int  print_header = 1;
    static int  separate_output = 1;

    if (print_header)
    {
        time_t	today;

        (void) time(&today);
        fprintf(stdout, "#\tAccounting status for %s", ctime(&today));
        fprintf(stdout, "#\t      Name\tState\tValue\n");
        print_header = 0;

    }
    else if (separate_output && indx >= ACCT_RCDS)
    {
        fprintf(stdout, "\n");
        separate_output = 0;
    }

    if (status->am_param != 0)
    {
        if (status->am_id == ACCT_THD_TIME) {
	    time_param = TIME_2_SECS(status->am_param);
	    fprintf(stdout, "\t%10s\t%s\t%.0f\n", name_list[indx].name,
		status_list[status->am_status], time_param);
        } else {
            param = status->am_param;
	    fprintf(stdout, "\t%10s\t%s\t%lld\n", name_list[indx].name,
		status_list[status->am_status], param);
	}
    }
    else
    {
        fprintf(stdout, "\t%10s\t%s\n", name_list[indx].name,
                status_list[status->am_status]);
    }

    return;
}

/*****************************************************************************
 *
 * NAME
 *	do_status	- Perform a status of all accounting methods.
 *
 * SYNOPSIS
 *	do_status();
 *
 * DESCRIPTION
 *	This routine requests the current status of the various known
 *	accounting method(s) and formats the information returned into human
 *	readable text.  This information is written to standard output.
 *
 *	It formats and displays the information in the order defined by the
 *	name_list[] array.
 *
 * RETURNS
 *	Nothing
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
void do_status()
{
    struct csa_status_req	status_req;
    int            i;

    if (db_flag > 1)
    {
        Ndebug("do_status(2):\n");
        Ndebug("   csaswitch(AC_KDSTAT): # methods = %d\n", NUM_KDS);
        Ndebug("   csaswitch(AC_RCDSTAT): # methods = %d\n", NUM_RCDS);
    }

    /* Get the kernel and daemon accounting status. */
    bzero((char *) &status_req, sizeof(status_req));
    status_req.st_num = NUM_KDS;
    if (csa_kdstat(&status_req) != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to get the Kernel and Daemon accounting status information.")
        	);

    for (i = 0; i < ACCT_MAXKDS; i++)
    {
        if (name_list[i].flag & STAT_FLAG)
            do_format(i, &status_req.st_stat[i]);
    }
    
    /* Get the record accounting status. */
    bzero((char *) &status_req, sizeof(status_req));
    status_req.st_num = NUM_RCDS;
    if (csa_rcdstat(&status_req) != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to get the Record accounting status information.")
        	);

    for (i = ACCT_RCDS; i < ACCT_MAXRCDS; i++)
    {
        if (name_list[i].flag & STAT_FLAG)
            do_format(i, &status_req.st_stat[i - ACCT_RCDS]);
    }

    return;
}

/*****************************************************************************
 *
 * NAME
 *      do_check        - Perform a status on the indicated accounting method.
 *
 * SYNOPSIS
 *	int = do_check(indx);
 *
 *	indx		r	The index into name_list[] which identifies
 *				the accounting method to be checked/formatted.
 *
 * RETURNS
 *      0       - If the given accounting method is currently "on".
 *      -1      - If the status of the given accounting method is currently
 *                not "on".
 *
 *	NOTE:	If an error is encountered, this routine will issue an error
 *		message.
 *
 *****************************************************************************/
int do_check(int indx)
{
    struct csa_check_req	check_req;

    if (db_flag > 1)
    {
        Ndebug("do_check(2): acctctl(AC_CHECK): id = %d, name = %s\n",
               name_list[indx].id, name_list[indx].name);
    }
    
    check_req.ck_stat.am_id = name_list[indx].id;
    check_req.ck_stat.am_status = ACS_OFF;
    check_req.ck_stat.am_param = 0;

    if (csa_check(&check_req) != 0)
        acct_perr(ACCT_ABORT, errno,
        	_("Unable to get the accounting status for '%s'."),
        	name_list[indx].name);


    do_format(indx, &check_req.ck_stat);

    if (check_req.ck_stat.am_status != ACS_ON)
        return(-1);

    return 0;
}


typedef enum
{
    OPT_c,
    OPT_D,
    OPT_m,
    OPT_n,
    OPT_P,
    OPT_t
} Options;

/*****************************************************************************
 *	MAIN PROGRAM
 *****************************************************************************/
int
main(int argc, char **argv)
{
/* XXXX this needs to change for Linux security syntax */
#define CAPABILITIES "CAP_SYS_PACCT,CAP_CHOWN,CAP_FOWNER,CAP_DAC_OVERRIDE+eip"
    
    extern char  *optarg;	/* Variables used by getopt(). */
    extern int	  optind,
                  optopt;

    char      acct_path[ACCT_PATH];
    int       authorized = 0;
    char      c[2];
#ifdef HAVE_LIBCAP
    cap_t     old_capset;
#endif	/* HAVE_LIBCAP */
    int       check_method;
    int       n_specified = FALSE; /* Indicate whether -n was specified. */
    int	      opt;
    char     *opt_string = ":c:D:m:n:P:t:";
    char     *opts[6];
    int       retval = 0;
    int       request;
    int       switch_file = FALSE;
    int64_t   mem_threshold = 0;
    int64_t   time_threshold = 0;

/*
 *	Make sure we set up our program name for error messages, etc...
 */
    Prg_name = argv[0];

    acct_path[0] = '\0';

/*
 *	Activate the needed capabilities.  XXXXX This needs to be changed to
 *	Linux security syntax.
 */
#ifdef HAVE_LIBCAP
    old_capset = cap_get_proc();
    if ((cap_set_proc(cap_from_text(CAPABILITIES))) == -1) {
	    cap_free(&old_capset);
	    acct_perr(ACCT_ABORT, errno,
		    _("Unable to activate needed capabilities"));
    }
#endif	/* HAVE_LIBCAP */
		
/*	All function codes for acctctl() do this check first.  We choose
 *	AC_AUTH because it's a safe one to use since it doesn't turn on or
 *	off anything, and it doesn't require us to pass in a structure like
 *	the ones that return the status.  Besides, we can use the result of
 *	the AC_AUTH call later to check for authorization.
 */
    if (csa_auth() != 0)
    {
        if (errno == ENOSYS)
            acct_perr(ACCT_ABORT, errno,
            	_("CSA job accounting is not available.")
            	);
        authorized = errno;
    }
    
/*
 *	Let's start by processing the command line options...
 */
    for (opt = 0; opt < (sizeof(opts) / sizeof(*opts)); opt++)
        opts[opt] = (char *) NULL;

    while ((opt = getopt(argc, argv, opt_string)) != EOF)
    {
        switch ((char) opt)
        {
        case 'c':
            if ( opts[ OPT_c ] )
                do_error(ACCT_WARN,
                       _("The (-%s) option was specified more than once."),
			 "c", NULL );
            opts[OPT_c] = optarg;
            break;

        case 'D':
            if ( opts[OPT_D] )
                do_error(ACCT_WARN,
                       _("The (-%s) option was specified more than once."),
			 "D", NULL );
            opts[OPT_D] = optarg;
            break;

        case 'm':
            if ( opts[OPT_m] )
                do_error(ACCT_WARN,
                       _("The (-%s) option was specified more than once."),
			 "m", NULL );
            opts[OPT_m] = optarg;
            break;
            
        case 'n':
            if ( opts[OPT_n] )
                do_error(ACCT_WARN,
                       _("The (-%s) option was specified more than once."),
			 "n", NULL );
            opts[OPT_n] = optarg;
            break;

        case 'P':
            if ( opts[OPT_P] ) {
                do_error(ACCT_WARN,
                       _("The (-%s) option was specified more than once."),
			 "P", NULL );
            }
            opts[OPT_P] = optarg;
            break;

        case 't':
            if ( opts[OPT_t] ) {
                do_error(ACCT_WARN,
                       _("The (-%s) option was specified more than once."),
			 "t", NULL );
            }
            opts[OPT_t] = optarg;
            break;

        case '?':
        case ':':
            c[0] = (char) optopt;
            c[1] = '\0';
            if ( (char) opt == '?' )
                do_error(ACCT_FATAL,
                       _("An unknown option '%s' was specified."),
			 c, NULL );
            else
                do_error(ACCT_FATAL,
                       _("Option -%s requires an argument."),
			 c, NULL );
            break;

        default:
            do_error(ACCT_ABORT,
                   _("An unknown/unsupported getopt() error occurred."),
		     NULL, NULL );
            break;
        }
    }

/*
 *	Now that we have interpreted everything the user has specified,
 *	let's verify the information...
 */
    if (optind < argc)
        do_error(ACCT_FATAL,
               _("Extra arguments were found on the command line."),
		 NULL, NULL);

    if ( opts[OPT_D] )
    {
        char  *t = opts[OPT_D];

        db_flag = atoi(t);
        if ((db_flag < 0) || (db_flag > 3))
            do_error(ACCT_FATAL,
                   _("The (-%s) option's argument, '%s', is invalid."),
		     "D", opts[OPT_D]);
        if (db_flag)
            Ndebug("Debug level = %d\n", db_flag);
    }
    else
        db_flag = 0;

    if (! opts[OPT_c])
        do_error(ACCT_FATAL,
               _("Option -%s is a required option."),
		 "c", NULL);
    if ((request = check_request(opts[OPT_c], &switch_file)) < 0)
        do_error(ACCT_FATAL,
               _("The (-%s) option's argument, '%s', is invalid."),
		 "c", opts[OPT_c]);
    if (((request == AC_START) || (request == AC_STOP) || (request == AC_HALT))
    	&& (authorized == EPERM))
    {
        acct_err(ACCT_ABORT,
               _("Permission denied.\n   You must have the CAP_SYS_PACCT capability to change accounting state."));
    }

    if (request == AC_CHECK)
    {
        if ( ! opts[OPT_n] )
            do_error(ACCT_FATAL,
                   _("Option -%s is required given the command line as specified."),
		    "n", NULL );
        else if ((check_method = check_name(opts[OPT_n])) < 0)
            do_error(ACCT_FATAL,
                   _("The (-%s) option's argument, '%s', is invalid."),
		     "n", opts[OPT_n] );
    }
    else if (request == AC_STOP)
    {
        if ( ! opts[OPT_n] )
            do_error(ACCT_FATAL,
                   _("Option -%s is required given the command line as specified."),
		     "n", NULL );
        else if (parse_name_list(opts[OPT_n], ACS_OFF) <= 0)
            do_error(ACCT_FATAL,
                   _("The (-%s) option's argument, '%s', is invalid."),
		     "n", opts[OPT_n] );
    }
    else if (request == AC_START && opts[OPT_n])
    {
        if (parse_name_list(opts[OPT_n], ACS_ON) <= 0)
            do_error(ACCT_FATAL,
                   _("The (-%s) option's argument, '%s', is invalid."),
		     "n", opts[OPT_n] );
        n_specified = TRUE;
    }
    else if (opts[OPT_n])
        do_error(ACCT_WARN,
               _("Option -%s is not used with command line as specified."),
		 "n", NULL );

    if (request != AC_START)
    {
        if (opts[OPT_m])
            do_error(ACCT_WARN,
                   _("Option -%s is not used with command line as specified."),
		     "m", NULL );
        if (opts[OPT_t])
            do_error(ACCT_WARN,
                   _("Option -%s is not used with command line as specified."),
		     "t", NULL );
    }
    else
    {
/*
 *	Verify there is a threshold for START {mem, time}.
 */
        if (name_list[ACCT_THD_MEM].req_state == ACS_ON && !opts[OPT_m])
            do_error(ACCT_FATAL,
                   _("Option -%s is required given the command line as specified."),
		     "m", NULL );
        if (name_list[ACCT_THD_TIME].req_state == ACS_ON && !opts[OPT_t])
            do_error(ACCT_FATAL,
                   _("Option -%s is required given the command line as specified."),
		     "t", NULL );
        
        if (opts[OPT_m])
        {
            if (name_list[ACCT_THD_MEM].req_state == ACS_ON ||
                n_specified == FALSE)
            {
                char  *m = opts[OPT_m];

                mem_threshold = atoll(m);
                if (mem_threshold < 0)
                    do_error(ACCT_FATAL,
                           _("Option '%s' requires a positive numeric argument."),
			     "m", NULL);
            }
            else
                do_error(ACCT_WARN,
                       _("Option -%s is not used with command line as specified."),
			 "m", NULL );
        }
        if (opts[OPT_t])
        {
            if (name_list[ACCT_THD_TIME].req_state == ACS_ON ||
                n_specified == FALSE)
            {
                char  *t = opts[OPT_t];

                time_threshold = atoll(t);
                if (time_threshold < 0)
                    do_error(ACCT_FATAL,
                           _("Option '%s' requires a positive numeric argument."),
			     "t", NULL);
            }
            else
                do_error(ACCT_WARN,
                       _("Option -%s is not used with command line as specified."),
			 "t", NULL );
        }
    }

/*
 *	Check if there is a path for a START request.
 */
    if ((request != AC_START || switch_file == TRUE) && opts[OPT_P])
        do_error(ACCT_WARN,
               _("Option -%s is not used with command line as specified."),
		 "P", NULL);
    else if (opts[OPT_P])
        strcpy(acct_path, opts[OPT_P]);

/*
 *	Okay, we get here then everything MUST be valid...So, let's do
 *	whatever needs to be done.
 */
    switch (request)
    {
    case AC_CHECK:
        retval = do_check(check_method);
        break;

    case AC_HALT:
        do_halt();
        break;

    case AC_STOP:
        do_stop();
        break;

    case AC_START:
        if (switch_file == TRUE)
            retval = do_switch();
        else
            retval = do_start(n_specified, mem_threshold, time_threshold,
                              acct_path);
        break;

    case AC_KDSTAT:
        do_status();
        break;
        
    default:
        acct_err(ACCT_ABORT,
               _("An unknown/unrecoverable internal error occurred."));
        break;
    }

/*
 *	Revert the effective set to its prior value and deallocate the
 *	old_capset structure.   XXXXX Needs to be changed to Linux syntax.
 */
#ifdef HAVE_LIBCAP
    if (cap_set_proc(old_capset) == 0) {
	    acct_perr(ACCT_ABORT, errno,
		    _("Unable to revert to original capabilities"));
    }
    cap_free(&old_capset);
#endif	/* HAVE_LIBCAP */
    
    exit(retval);
}
