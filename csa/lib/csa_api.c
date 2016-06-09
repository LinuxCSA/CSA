/*
 * Copyright (c) 2004-2008 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#include <linux/unistd.h>
#include <syscall.h>
#include <errno.h>
#include "acctdef.h"
#include "acctmsg.h"
#include "csa_api.h"
#include "csa_ctl.h"


static void csa_perr(accterr level, int errnm, char *fmt, ...);
static void csa_err(accterr level, char *fmt, ...);


static char *
name_of_csactl(int id)
{
    switch (id) {
	case AC_START:
		return("csa_start");
	case AC_STOP:
		return("csa_stop");
	case AC_HALT:
		return("csa_halt");
	case AC_CHECK:
		return("csa_check");
	case AC_KDSTAT:
		return("csa_kdstat");
	case AC_RCDSTAT:
		return("csa_rcdstat");
	case AC_JASTART:
		return("csa_jastart");
	case AC_JASTOP:
		return("csa_jastop");
	case AC_WRACCT:
		return("csa_wracct");
	case AC_AUTH:
		return("csa_auth");
	default:
		return("Unknown CSA library call");
    }
}

/*
 * NAME
 * 	csa_ctl
 *
 */
static int
csa_ctl(int opcode, void *data, int datasz, int update)
{
	struct csa_ctl_hdr hdr;
	int csa_ctl_fd = -1;
	int rc = 0;
	int err;

	if ((opcode < AC_START) || (opcode >= AC_MREQ)) {
		errno = EINVAL;
		csa_err(ACCT_WARN,
			_("Invalid CSA library call %d"), opcode);
		return (-1);
	}

	if (data == NULL)
		update = 0;

	hdr.req = opcode;
	hdr.err = 0;
	hdr.size = datasz;
	hdr.update = update;

	do {
		struct sockaddr_un sa_unix;
		memset((void *) &sa_unix, 0, sizeof(sa_unix));
		sa_unix.sun_family = AF_UNIX;
		strcpy(sa_unix.sun_path, CSA_CTL_SOCKET);

		if ((csa_ctl_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
			rc = -1;
			break;
		}

		if (connect(csa_ctl_fd, (struct sockaddr *) &sa_unix,
			    sizeof(sa_unix)) < 0) {
			rc = -1;
			break;
		}

		if (write(csa_ctl_fd, &hdr, sizeof(hdr)) < 0) {
			rc = -1;
			break;
		}
		if (data && write(csa_ctl_fd, data, datasz) < 0) {
			rc = -1;
			break;
		}

		if (read(csa_ctl_fd, &hdr, sizeof(hdr)) < 0) {
			rc = -1;
			break;
		}
		if (update && read(csa_ctl_fd, data, datasz) < 0) {
			rc = -1;
			break;
		}
	} while (0);

	err = errno;

	if (csa_ctl_fd != -1)
		close(csa_ctl_fd);

	if (rc < 0) {
		csa_perr(ACCT_WARN, err,
			_("Error communicating with CSA daemon"));
		return (-1);
	}

	if (hdr.err) {
		errno = hdr.err;
		rc = -1;
	}

	if (rc < 0) { /* Don't perr on a permissions error to csa_auth */
		if (errno == ENODATA)
			csa_perr(ACCT_WARN, errno,
				_("Could not find job table entry"));
		else if ((opcode != AC_AUTH) || (errno != EPERM))  
			csa_perr(ACCT_WARN, errno, 
				_("csa_ctl failure, command='%s'"), 
				name_of_csactl(opcode));
		return (-1);
	} else
		return (0);
}



char *
name_of_method(int id)
{
    switch (id) {
	case ACCT_KERN_CSA:
		return("csa");
	case ACCT_KERN_JOB_PROC:
		return("job");
	case ACCT_DMD_WKMG:
		return("wkmg");
	case ACCT_DMD_SITE1:
		return("site1");
	case ACCT_DMD_SITE2:
		return("site2");
	case ACCT_RCD_MEM:
		return("mem");
	case ACCT_RCD_IO:
		return("io");
	case ACCT_RCD_DELAY:
		return("delay");
	case ACCT_THD_MEM:
		return("memt");
	case ACCT_THD_TIME:
		return("time");
	case ACCT_RCD_SITE1:
		return("rsite1");
	case ACCT_RCD_SITE2:
		return("rsite2");
	default:
		return("Unknown Accounting Method Type");
    }
}



/*
 * NAME
 * 	csa_start	- start specified CSA accounting method(s)
 */
int
csa_start(struct csa_start_req *start_req)
{
    struct actctl control;
    int i;


    if (start_req->sr_num > NUM_KDRCDS) {
	errno = EINVAL;
	return (-1);
    }
    if (strlen(start_req->sr_path) >= ACCT_PATH) {
	errno = EINVAL;
	return (-1);
    }
    memset((void*)&control, 0, sizeof(control));

    for (i=0; i < start_req->sr_num; i++) {
	control.ac_stat[i].ac_ind = start_req->sr_method[i].sr_id;
	control.ac_stat[i].ac_state = ACS_ON;
	if (control.ac_stat[i].ac_ind == ACCT_THD_TIME) {
	    control.ac_stat[i].ac_param =
		    TIME_2_USECS(start_req->sr_method[i].param);
	} else {
	    control.ac_stat[i].ac_param = start_req->sr_method[i].param;
	}
    }

    control.ac_sttnum = i;
    strcpy(control.ac_path, start_req->sr_path);

    return (csa_ctl(AC_START, &control, sizeof(control), 0));
}


/*
 * NAME
 * 	csa_stop	turn "off" the specified accounting method(s).
 */
int
csa_stop(struct csa_stop_req *stop_req)
{
    struct actctl control;
    int i;

    memset((void*)&control, 0, sizeof(control));

    for (i=0; i < stop_req->pr_num; i++) {
	control.ac_stat[i].ac_ind = stop_req->pr_id[i];
	control.ac_stat[i].ac_state = ACS_OFF;
	control.ac_stat[i].ac_param = 0;
    }

    control.ac_sttnum = i;
    control.ac_path[0] = '\0';

    return (csa_ctl(AC_STOP, &control, sizeof(control), 0));
}



/*
 * NAME
 * 	csa_halt	- turn "off" all accounting methods
 */
int
csa_halt()
{
    return (csa_ctl(AC_HALT, NULL, 0, 0));
}



/*
 * NAME
 * 	csa_check	- Check the status of indicated accounting method
 *
 * Return
 * 	0	- If success
 * 		   check_req->ck_status would contain the status of the
 * 		   inquired accounting method.
 * 	-1	- If error occurs. errno is valid upon return.
 */
int
csa_check(struct csa_check_req *check_req)
{
    struct actstat control;

    memset((void*)&control, 0, sizeof(control));

    control.ac_ind = check_req->ck_stat.am_id;
    control.ac_state = ACS_OFF;
    control.ac_param = 0;

    if (csa_ctl(AC_CHECK, &control, sizeof(control), 1)) {
	csa_err(ACCT_WARN,
		_("Unable to get the accounting status for '%s'."),
		name_of_method(check_req->ck_stat.am_id));
	return (-1);
    }

    check_req->ck_stat.am_status = control.ac_state;

    return (0);
}



/*
 * NAME
 * 	csa_kdstat	- get the kernel and daemon accouting status
 *
 * INPUT
 * 	kdstat_req	pointer to struct csa_stat_req
 *
 * 		kdstat_req->st_num should contains the number of entries in
 *		the kdstat_req->st_stat[] array.
 *
 *		kdstat_req->st_stat[] array contains the IDs of kernel-daemon
 * 		accounting methods. Linux kernel would return status of
 * 		all accounting methods requested.
 *
 * Return
 * 	0 	if success
 * 		The kdstat_req would contain status/info returned from kernel.
 * 	-1 	if failure. errno is valid upon return.
 */
int
csa_kdstat(struct csa_status_req *kdstat_req)
{
    struct actctl control;
    int i;

    if ((kdstat_req->st_num <= 0) || (kdstat_req->st_num > NUM_KDS)) {
    	errno = EINVAL;
	return (-1);
    }
    memset((void*)&control, 0, sizeof(control));
    control.ac_sttnum = kdstat_req->st_num;

    if (csa_ctl(AC_KDSTAT, &control, sizeof(control), 1)) {
	csa_err(ACCT_WARN,
                _("Unable to get the Kernel and Daemon accounting status information."));
        return (-1);
    }

    for (i=0; i < control.ac_sttnum; i++) {
    	kdstat_req->st_stat[i].am_id = control.ac_stat[i].ac_ind;
	kdstat_req->st_stat[i].am_status = control.ac_stat[i].ac_state;
	kdstat_req->st_stat[i].am_param = control.ac_stat[i].ac_param;
    }
    strncpy(kdstat_req->st_path, control.ac_path, ACCT_PATH);
    kdstat_req->st_path[ACCT_PATH] = '\0';

    return (0);
}



/*
 * NAME
 * 	csa_rcdstat	- get the record accouting status
 *
 * INPUT
 * 	rcdstat_req	pointer to struct csa_stat_req
 *
 * 		rcdstat_req->st_num should contains the number of entries in
 * 		the rcdstat_req->st_stat[] array.
 *
 * 		rcdstat_req->st_stat[] array contains the IDs of record
 * 		accounting methods. Linux kernel would return status of
 * 		all accounting methods requested.
 *
 * Return
 * 	0 	if success
 * 		The rcdstat_req would contain status/info returned from kernel.
 * 	-1 	if failure. errno is valid upon return.
 */
int
csa_rcdstat(struct csa_status_req *rcdstat_req)
{
    struct actctl control;
    int i;

    if ((rcdstat_req->st_num <= 0) || (rcdstat_req->st_num > NUM_RCDS)) {
    	errno = EINVAL;
	return (-1);
    }

    memset((void*)&control, 0, sizeof(control));
    control.ac_sttnum = rcdstat_req->st_num;

    if (csa_ctl(AC_RCDSTAT, &control, sizeof(control), 1)) {
    	csa_err(ACCT_WARN,
		_("Unable to get the Record accounting status information."));
	return (-1);
    }

    for (i=0; i < control.ac_sttnum; i++) {
    	rcdstat_req->st_stat[i].am_id = control.ac_stat[i].ac_ind;
	rcdstat_req->st_stat[i].am_status = control.ac_stat[i].ac_state;
	rcdstat_req->st_stat[i].am_param = control.ac_stat[i].ac_param;
    }
    strncpy(rcdstat_req->st_path, control.ac_path, ACCT_PATH);
    rcdstat_req->st_path[ACCT_PATH] = '\0';

    return (0);
}



/*
 * NAME
 * 	csa_jastart	- start job accounting
 */
int
csa_jastart(struct csa_job_req *job_req)
{
    struct actctl control;

    memset((void*)&control, 0, sizeof(control));
    strncpy(control.ac_path, job_req->job_path, ACCT_PATH);
    if (csa_ctl(AC_JASTART, &control, sizeof(control), 0) == -1) {
	csa_err(ACCT_WARN,
	    _("An error was returned from the call to the 'csa_jastart' routine."));
	return (-1);
    } else
	return (0);

}



/*
 * NAME
 * 	csa_jastop	- stop job accounting
 */
int
csa_jastop(struct csa_job_req *job_req)
{
    struct actctl control;

    memset((void*)&control, 0, sizeof(control));
    strncpy(control.ac_path, job_req->job_path, ACCT_PATH);
    return (csa_ctl(AC_JASTOP, &control, sizeof(control), 0));
}



/*
 * NAME
 * 	csa_wracct	- write accounting record to file
 *
 */
int
csa_wracct(struct csa_wra_req *wra_req)
{
    struct actwra wra;

    memset((void*)&wra, 0, sizeof(wra));
    wra.ac_did = wra_req->wra_did;
    wra.ac_len = wra_req->wra_len;
    wra.ac_jid = wra_req->wra_jid;
    memcpy(wra.ac_buf, wra_req->wra_buf, wra_req->wra_len);

    return (csa_ctl(AC_WRACCT, &wra, sizeof(wra), 0));
}



/*
 * Return: return  0 if success
 * 	   return -1 if failure; errno is valid upon return
 */
int
csa_auth()
{

    return (csa_ctl(AC_AUTH, NULL, 0, 0));
}



/*
 *	csa_err - Process an error message from the message system.
 */
static void
csa_err(accterr level, char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);

	vfprintf (stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

/*
 *     Check if the message if fatal.
 */
	if (level == ACCT_ABORT) {
	   exit(1);
	}
}


/*
 *	csa_perr - Process an error message from the message system that
 *	includes a errno message code.
 */
static void
csa_perr(accterr level, int errnm, char *fmt, ...)
{
	va_list args;
	char	sysstr[200], errstr[400];

	strncpy(errstr, fmt, 200);

/*
 *	Validate the errno value and get the error message.
 */
msgerr:
	sprintf(sysstr, _("\n   System Error(%d): %s.\n"), errnm,
		strerror(errnm));

	strncat(errstr, sysstr, 200);

	va_start(args, fmt);
	vfprintf (stderr, errstr, args);
	va_end(args);

/*
 *	Check if the message if fatal.
 */
	if (level == ACCT_ABORT) {
	    exit(1);
	}

	return;
}

