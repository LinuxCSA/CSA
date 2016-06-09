/*
 * Copyright (c) 2000-2007 Silicon Graphics, Inc. 
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

#include <stdarg.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syslog.h>

#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION 

#include <security/_pam_macros.h>
#include <security/pam_modules.h>

#ifndef LINUX_PAM 
#include <security/pam_appl.h>
#endif

#include "job.h"


static int disabled = 0;
static int verbose = 0;

#define vsyslog		if (verbose) syslog


static void parse_args(int argc, const char **argv)
{
	int i;

	verbose = 0;

	for (i = 0; i < argc; i++)
		if (strcmp(argv[i], "verbose") == 0)
			verbose = 1;
}


/*
 * PAM framework looks for these entry-points to pass control to the 
 * account management module.
 */

/* 
 *  * Not all PAMified apps invoke session management modules.  So, we supply
 *  * this account management function for such cases.  Whenever possible, it
 *  * is still bet to use the session management version.
 *  */
PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc,
                const char **argv)
{
        int             retcode = PAM_SUCCESS;
        struct passwd   *passwd = NULL;
        char            *username = NULL;
        char            *service = NULL;
	jid_t		jid;

	parse_args(argc, argv);

        /* Get the username of the user associated with this job */
        retcode = pam_get_item(pamh, PAM_USER, (void *) &username);
        if (username == NULL || retcode != PAM_SUCCESS) {
                syslog(LOG_CRIT, "open_session - error recovering username");
                return PAM_AUTH_ERR;
        }

        /* Get the uid value for the user associated with this job */
        passwd = getpwnam(username);
        if (!passwd) {
                syslog(LOG_CRIT, "open_session - error getting passwd entry");
                return PAM_AUTH_ERR;
        }


        /* Get the service used to create this job */
        retcode = pam_get_item(pamh, PAM_SERVICE, (void *) &service);
        if (service == NULL || retcode != PAM_SUCCESS) {
                syslog(LOG_CRIT, "open_session - error recovering service");

                return PAM_AUTH_ERR;
        }

#if 0   /* this allowed root logins without creating jobs */

        if (passwd->pw_uid == 0) {
                vsyslog(LOG_INFO, "(%s) POE(pid=%d): user(uid=%d),"
                                " skipping job creation for superuser\n",
                                service,
                                getpid(),
                                passwd->pw_uid);
                disabled = 1;
                return PAM_SUCCESS;
        }       
#endif  

        /* Request creation of new job */
	jid = job_create(0, passwd->pw_uid, 0 );
        if (jid == (jid_t)-1) {
                /* Record failure to create job & session will fail */
                vsyslog(LOG_INFO, "job_create(...): %s",
                                strerror(errno));
#if 0
                return PAM_AUTH_ERR;
#else
		disabled = 1;
                return PAM_SUCCESS;
#endif
        }

	/* Record status of creating new job */
	if (jid == (jid_t)0) {
		/* Job creation was disabled */
		vsyslog(LOG_INFO, "(%s) POE(pid=%d): job creation disabled\n",
				service, getpid());
		disabled = 1;
	} else {
		/* New job successfully created */
        	vsyslog(LOG_INFO, "(%s) POE(pid=%d): job(jid=%0#18Lx) created for user %s(uid=%d) - using account management, no POE exit message will appear\n",
                        	service, getpid(), 
				(unsigned long long)jid,
                        	username, passwd->pw_uid);
	}

        return PAM_SUCCESS;
}


/*
 * PAM framework looks for these entry-points to pass control to the
 * session module.
 */
 

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, 
		const char **argv)
{
	int 		retcode = PAM_SUCCESS;
	struct passwd	*passwd = NULL;
	char 		*username = NULL;
	char 		*service = NULL;
	jid_t		jid;

	parse_args(argc, argv);

	/* Get the username of the user associated with this job */
	retcode = pam_get_item(pamh, PAM_USER, (void *) &username); 
	if (username == NULL || retcode != PAM_SUCCESS) {
		syslog(LOG_CRIT, "open_session - error recovering username");
		return PAM_SESSION_ERR;
	}

	/* Get the uid value for the user associated with this job */
	passwd = getpwnam(username);
	if (!passwd) {
		syslog(LOG_CRIT, "open_session - error getting passwd entry");
		return PAM_SESSION_ERR;
	}


	/* Get the service used to create this job */	
	retcode = pam_get_item(pamh, PAM_SERVICE, (void *) &service);
	if (service == NULL || retcode != PAM_SUCCESS) {
		syslog(LOG_CRIT, "open_session - error recovering service");
		return PAM_SESSION_ERR;
	}

#if 0

	if (passwd->pw_uid == 0) {
		vsyslog(LOG_INFO, "(%s) POE(pid=%d): user(uid=%d),"
				" skipping job creation for superuser\n",
				service,
				getpid(),
				passwd->pw_uid);
		disabled = 1;
		return PAM_SUCCESS;
	}	
#endif	


	/* Request creation of new job */
	jid = job_create(0, passwd->pw_uid, 0);
	if (jid == (jid_t)-1) {
		/* Record failure to create job & session will fail */
		vsyslog(LOG_INFO, "job_create(...): %s",
				strerror(errno));
#if 0
		return PAM_SESSION_ERR;
#else
		disabled = 1;
		return PAM_SUCCESS;
#endif
	} 

	/* Record status of creating new job */
	if (jid == (jid_t)0) {
		/* Job creation was disabled */
		vsyslog(LOG_INFO, "(%s) POE(pid=%d): job creation disabled\n",
				service, getpid());
		disabled = 1;
	} else {
		/* New job successfully created */
        	vsyslog(LOG_INFO, "(%s) POE(pid=%d): job(jid=%0#18Lx) created for user %s(uid=%d) - using session management, a POE exit message will appear\n",
                        	service, getpid(), 
				(unsigned long long)jid,
                        	username, passwd->pw_uid);
	}

	return PAM_SUCCESS; 
}


PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, 
		const char **argv)
{
	int 		retcode = PAM_SUCCESS;
	char 		*username = NULL;
	char 		*service = NULL;
	jid_t		jid = (jid_t)0;

	parse_args(argc, argv);

	/* Get the username of the user associated with this job */
	retcode = pam_get_item(pamh, PAM_USER, (void *) &username);
	if (username == NULL || retcode != PAM_SUCCESS) {
		syslog(LOG_CRIT, "open_session - error recovering username");
		return PAM_SESSION_ERR;
	}

	/* Get the service used to create this job */	
	retcode = pam_get_item(pamh, PAM_SERVICE, (void *) &service);
	if (service == NULL || retcode != PAM_SUCCESS) {
		syslog(LOG_CRIT, "open_session - error recovering service");
		return PAM_SESSION_ERR;
	}

	if (!disabled) {
		jid = job_getjid(getpid());
		if (jid == (jid_t)-1) {
			syslog(LOG_CRIT, "job_getjid(...) - error getting job ID");
		}
		else if (jid > (jid_t)0) {
			vsyslog(LOG_INFO, "(%s) POE(pid=%d): job(jid=%0#18Lx) POE process is exiting\n", 
					service, getpid(), (unsigned long long)jid);
		}
	}

	if (jid == (jid_t)0){
		vsyslog(LOG_INFO, "(%s) POE(pid=%d): POE process is exiting. Not attached to any job\n",
				service, getpid());
	}

	return PAM_SUCCESS;
}
