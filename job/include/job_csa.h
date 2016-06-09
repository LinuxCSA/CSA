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

#ifndef _JOB_CSA_H
#define _JOB_CSA_H

#include <job.h>

/* 
 * #define CSA	1
 * undefined = JOB only (no CSA) 
 * defined = JOB multi-threaded with CSA 
*/ 

extern jid_t   jid_lookup(pid_t);

#if 0
/* socketpair file descriptors */
extern int sk_job;
extern int sk_csa;
#endif

#define JOB_EVENT_START 1
#define JOB_EVENT_END   2

struct job_event {
        int     type;   /* JOB_EVENT_START or JOB_EVENT_END */
        jid_t   jid;
        uid_t   user;
        time_t  start;
};

struct proc_exit {
        pid_t   pid;
        int     exitstat;
};

#define JOB_CSA_SOCKET "/var/run/jobd/job-csa-socket"

#endif /* _JOB_CSA_H */

