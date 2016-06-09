/*
 *
 * Copyright (c) 2000-2007 Silicon Graphics, Inc.  All Rights Reserved.
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
 *
 * Contact information:  Silicon Graphics, Inc., 1140 East Arques Avenue,
 * Sunnyvale, CA  94085, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 *
 *
 * Description:   This file, include/linux/jobctl.h, contains the data
 *       definitions used by job to communicate with pagg via the /proc/job
 *       ioctl interface.
 *
 */

#include <jobctl.h>


/* The jobctl() function communicates with the job daemon 
 * via sockets.
 */
int jobctl(int request, void *data, int datasz) {
    int sockfd, n, rc = 0;
    unsigned int servlen;
    struct sockaddr_un  serv_addr;
    struct job_hdr	*datap = data;

    datap->req = request;
    datap->err = 0;
    datap->size = datasz;

    bzero((char *)&serv_addr,sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, JOBD_SOCKET);
    servlen = strlen(serv_addr.sun_path) +
                  sizeof(serv_addr.sun_family);

    if ((sockfd = socket(PF_UNIX, SOCK_STREAM,0)) < 0) {
	return(-1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, servlen)<0) {
	return(-1);
    }

    n=write(sockfd, datap, datasz);
    if (n < 0) {
	return(-1);
    }

    n = read(sockfd, datap, datasz);
    if (n < 0) {
	return(-1);
    }

    if (datap->err) {
       errno = datap->err;
       rc = -1;
    } else {
	/* We still have to read the jid/pid list for these requests */
	if (request == JOB_GETJIDLST) {
		struct job_jidlst *jidlst = data;
		if (jidlst->r_val >= 0) {
			n = read(sockfd, (void *)jidlst->jid, 
			    (jidlst->r_val*sizeof(jid_t)));
			if (n < 0) {
				return(-1);
			}
		}
    	} else if (request == JOB_GETPIDLST) {
		struct job_pidlst *pidlst = data;
		if (pidlst->r_val >= 0) {
			n = read(sockfd, (void *)pidlst->pid, 
			    (pidlst->r_val*sizeof(pid_t)));
			if (n < 0) {
				return(-1);
			}
		}
    	}
    }

    close(sockfd);

    return(rc);
}

