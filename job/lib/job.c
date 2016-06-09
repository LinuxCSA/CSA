/*
 * Copyright (c) 2003-2007 Silicon Graphics, Inc.  
 * All Rights Reserved.
 * 
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Job library updated to support ioctl /proc interface */

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <job.h>
#include <string.h>
#include <errno.h>
#include <jobctl.h>

jid_t 
job_create( jid_t jid_requested, uid_t uid, int options )
{
    struct job_create jcreate;
    int rv;

    memset(&jcreate, 0, sizeof(jcreate));
    jcreate.r_jid = 0;		/* return job ID for created job */
    jcreate.jid = jid_requested;		/* Jid value requested */
    jcreate.pid = getpid();	/* PID of calling process */
    jcreate.user = uid;		/* UID of user associated with job */
    jcreate.options = options;	/* creation options */

    rv = jobctl(JOB_CREATE, (void *)&jcreate, sizeof(jcreate));
    if (rv < 0)
	return (jid_t)-1;

    return (jid_t)jcreate.r_jid;
}

jid_t
job_getjid( pid_t pid )
{
    struct job_getjid jgetjid;
    int rv;

    memset(&jgetjid, 0, sizeof(jgetjid));
    jgetjid.r_jid = 0;
    jgetjid.pid = pid;

    rv = jobctl(JOB_GETJID, (void *)&jgetjid, sizeof(jgetjid));
    if (rv < 0)
	return (jid_t)-1;

    return (jid_t)jgetjid.r_jid;
}

jid_t
job_waitjid( jid_t jid, int *status, int options )
{
    struct job_waitjid waitjid;
    int rv;

    memset(&waitjid, 0, sizeof(waitjid));
    waitjid.r_jid = 0;
    waitjid.jid = jid;
    waitjid.stat = 0;
    waitjid.options = options;

    rv = jobctl(JOB_WAITJID, (void *)&waitjid, sizeof(waitjid));
    if (rv < 0)
	return (jid_t)-1;

    *status = waitjid.stat;
    return (jid_t)waitjid.r_jid;
}

int
job_killjid( jid_t jid, int sig )
{
    struct job_killjid killjid;
    int rv;

    memset(&killjid, 0, sizeof(killjid));
    killjid.r_val = 0;
    killjid.jid = jid;
    killjid.sig = sig;

    rv = jobctl(JOB_KILLJID, (void *)&killjid, sizeof(killjid));
    if (rv < 0)
	return -1;

    return killjid.r_val;
}

int
job_getjidcnt()
{
    struct job_jidcnt jidcnt;
    int rv;

    memset(&jidcnt, 0, sizeof(jidcnt));
    jidcnt.r_val = 0;		

    rv = jobctl(JOB_GETJIDCNT, (void *)&jidcnt, sizeof(jidcnt));
    if (rv < 0)
	return -1;

    return jidcnt.r_val;
}

int
job_getjidlist( jid_t *jid, int bufsize )
{
    struct job_jidlst jidlst;
    int rv;

    memset(&jidlst, 0, sizeof(jidlst));
    jidlst.r_val = bufsize/sizeof(jid_t); /* how many JIDs fit in buffer */
    jidlst.jid = jid;

    rv = jobctl(JOB_GETJIDLST, (void *)&jidlst, sizeof(jidlst));
    if (rv < 0)
	return -1;

    return jidlst.r_val;
}

int
job_getpidcnt( jid_t jid )
{
    struct job_pidcnt pidcnt;
    int rv;

    memset(&pidcnt, 0, sizeof(pidcnt));
    pidcnt.r_val = 0;
    pidcnt.jid = jid;

    rv = jobctl(JOB_GETPIDCNT, (void *)&pidcnt, sizeof(pidcnt));
    if (rv < 0)
	return -1;

    return pidcnt.r_val;
}

int
job_getpidlist( jid_t jid, pid_t *pid, int bufsize )
{
    struct job_pidlst pidlst;
    int rv;

    memset(&pidlst, 0, sizeof(pidlst));
    pidlst.r_val = bufsize/sizeof(pid_t);	/* buffer size in bytes */
    pidlst.pid = pid;
    pidlst.jid = jid;

    rv = jobctl(JOB_GETPIDLST, (void *)&pidlst, sizeof(pidlst));
    if (rv < 0)
	return -1;

    return pidlst.r_val;
}

uid_t
job_getuid( jid_t jid )
{
    struct job_user user;
    int rv;

    memset(&user, 0, sizeof(user));
    user.r_user = 0;
    user.jid = jid;

    rv = jobctl(JOB_GETUSER, (void *)&user, sizeof(user));
    if (rv < 0)
	return (uid_t)-1;

    return (uid_t)user.r_user;
}

pid_t
job_getprimepid( jid_t jid )
{
    struct job_primepid primepid;
    int rv;

    memset(&primepid, 0, sizeof(primepid));
    primepid.r_pid = 0;
    primepid.jid = jid;

    rv = jobctl(JOB_GETPRIMEPID, (void *)&primepid, sizeof(primepid));
    if (rv < 0)
	return (pid_t)-1;

    return (pid_t)primepid.r_pid;
}

int
job_sethid( unsigned int hid )
{
    struct job_sethid sethid;
    int rv;

    memset(&sethid, 0, sizeof(sethid));
    sethid.r_hid = 0;	/* not used */
    sethid.hid = hid;

    rv = jobctl(JOB_SETHID, (void *)&sethid, sizeof(sethid));

    return rv;
}

int
job_detachjid( jid_t jid )
{
    struct job_detachjid detachjid;
    int rv;

    memset(&detachjid, 0, sizeof(detachjid));
    detachjid.r_val = 0;
    detachjid.jid = jid;

    rv = jobctl(JOB_DETACHJID, (void *)&detachjid, sizeof(detachjid));
    if (rv < 0)
	return -1;

    return detachjid.r_val;
}

jid_t
job_detachpid( pid_t pid )
{
    struct job_detachpid detachpid;
    int rv;

    memset(&detachpid, 0, sizeof(detachpid));
    detachpid.r_jid = 0;
    detachpid.pid = pid;

    rv = jobctl(JOB_DETACHPID, (void *)&detachpid, sizeof(detachpid));
    if (rv < 0)
	return (jid_t)-1;

    return (jid_t)detachpid.r_jid;
}

jid_t
job_attachpid(pid_t pid, jid_t jid_requested)
{
    struct job_attachpid attachpid;
    int rv;

    memset(&attachpid, 0, sizeof(attachpid));
    attachpid.r_jid = jid_requested;
    attachpid.pid = pid;

    rv = jobctl(JOB_ATTACHPID, (void *)&attachpid, sizeof(attachpid));
    if (rv < 0)
	return (jid_t)-1;

    return jid_requested;
}

int
job_runcsa(int run)
{
    struct job_runcsa runcsa;
    int rv;

    memset(&runcsa, 0, sizeof(runcsa));
    runcsa.r_val = 0;
    runcsa.run = run;

    rv = jobctl(JOB_RUNCSA, (void *)&runcsa, sizeof(runcsa));
    if (rv < 0)
	return -1;

    return runcsa.r_val;
}
