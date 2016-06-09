/*
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef _CSA_API_H
#define _CSA_API_H

#include "csa.h"
#include <fcntl.h>
#include <linux/ioctl.h>

#define ADM	"csaacct"	/* Default owner and group to use when 
				   creating the the accounting file.
				   NOTE:  The group is overwritten by 
				   specifying a value for the csa.conf label 
				   "CHGRP". */
#define MODE	0664	/* The accounting file's mode. */


/*
 * CSA Library interfaces
 */

/*
 * CSA Accounting Method Status struct
 */
struct csa_am_stat {
	int	am_id;		/* accounting method ID */
	int	am_status;	/* accounting method status */
	int64_t	am_param;	/* accounting method parameter */
};

/*
 * CSA_START request
 */
struct csa_start_req {
	int	sr_num;		/* num of entries in sr_method array */
	char	sr_path[ACCT_PATH+1];  /* path name for accounting file */
	struct method_info {
		int	sr_id;		/* Accounting Method type id */
		int64_t	param;		/* Entry parameter */
	} sr_method[NUM_KDRCDS];
};

/*
 * CSA_STOP request
 */
struct csa_stop_req {
	int	pr_num;		/* num of entries in pr_id[] array */
	int	pr_id[NUM_KDRCDS];	/* Accounting Method type id */
};

/*
 * CSA_CHECK request and reply
 */
struct csa_check_req {
	struct csa_am_stat ck_stat;
};

/*
 * CSA_KDSTAT/CSA_RCDSTAT request
 */
struct csa_status_req {
	int 	st_num;		/* num of entries in kd_method array */
	char	st_path[ACCT_PATH+1];
	struct csa_am_stat st_stat[NUM_KDRCDS];
};

/*
 * CSA_JASTART/CSA_JASTOP request
 */
struct csa_job_req {
	char	job_path[ACCT_PATH+1];
};

/* 
 * CSA_WRACCT request
 */
struct csa_wra_req {
	int		wra_did;	/* Daemon Index */
	int		wra_len;	/* Length of buffer (bytes) */
	uint64_t 	wra_jid;	/* Job ID */
	char		*wra_buf;	/* Daemon accounting buffer */
};

/*
 * Function codes for incremental accounting; currently not used.
 */
typedef enum {
	INC_NONE,	/* Zero entry place holder */
	INC_DELTA,	/* Change clock delta for incremental accounting */
	INC_EVENT,	/* Cause incremental accounting event now */
	INC_MAX
} inc_funcode;

/*
 * Prototypes
 */
int csa_start(struct csa_start_req *);

int csa_stop(struct csa_stop_req *);

int csa_halt();

int csa_check(struct csa_check_req *);

int csa_kdstat(struct csa_status_req *);

int csa_rcdstat(struct csa_status_req *);

int csa_jastart(struct csa_job_req *);

int csa_jastop(struct csa_job_req *);

int csa_wracct(struct csa_wra_req *);

int csa_auth();


#endif	/* _CSA_API_H */
