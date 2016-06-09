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

#ifndef _JOB_H
#define _JOB_H

#include <stdint.h>
#include <asm/unistd.h>

typedef uint64_t jid_t;

jid_t
job_create( jid_t jid_requested, uid_t uid, int options );

jid_t
job_getjid( pid_t pid );

jid_t
job_waitjid( jid_t jid, int *status, int options );

int
job_killjid( jid_t jid, int sig );

int
job_getjidcnt( void );

int
job_getjidlist( jid_t *jid, int bufsize );	/* buffer size in bytes*/

int
job_getpidcnt( jid_t jid );

int
job_getpidlist( jid_t jid, pid_t *pid, int bufsize ); /* buffer size in bytes*/

uid_t
job_getuid( jid_t jid );

pid_t
job_getprimepid( jid_t jid );

int
job_sethid( unsigned int hid );

int
job_detachjid( jid_t jid );

jid_t
job_detachpid( pid_t pid );

jid_t
job_attachpid( pid_t pid, jid_t jid_requested );

int
job_runcsa( int run );

#ifdef JOB_IRIX_COMPAT

/*
 * IRIX compatibility
 */
#define getjid() job_getjid(getpid())
#define killjob(jid,signal) job_killjid((jid),(signal))
#define makenewjob(rjid,uid) job_create((rjid),(uid),0)
/* no compatibility interfaces for setwaitjobpid(2) and waitjob(2) */

#endif	// JOB_IRIX_COMPAT

#endif	// _JOB_H
