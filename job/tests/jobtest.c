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


/* This program does some basic exercises for linux jobs 
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <job.h>
#include <malloc.h>
#include <string.h>
#include <signal.h>
#include <wait.h>

#define MAXPIDS 100
#define JOB_FAIL (jid_t)-1
#define PROCESS_WRAPPING_COUNT 40000

/* these are used by createDummyProcess */
#define PROCESS_ONLY 1
#define PRODUCE_JID 2
#define USE_SPECIFIC_JID 3
/* End createDummyProcess defines */

/* Function prototypes */
int jidExists(jid_t injid); /* Check if JID exists using job library calls */
int jkillHandlesSigZero(jid_t injid); /* Does jkill handle signal 0 properly? */
void cleanup(void); /* Kills processes created */
int handlesDuplicateJids(void); /* Does job handle trying to request dupe JIDs? */
int handlesChildJobCreate(void); /* Child tries to get its own JID */
int parentHandlesDetachedChild(void); /* child detach shouldn't affect parent */
int jdetachSanityCheck(void); /* Does jdetach work right (not crash?) */
int handlesDetach(void); /* Some checks on job_detach */
int handlesAttach(void); /* Some checks on jattach */
int handlesProcessWrapping(void); /* If all PIDs used, JIDs are OK? */
int createDummyProcess(int action, jid_t injid, int (*funct)());
							/* forks processes in various ways */

/* Globals */

int pidlist[MAXPIDS];  /* List of PIDs used during testing for easy cleanup */
int pidcount = 0;
jid_t mainjid; /* JID for this parent process, set in main, used elsewhere */

int main(int argc, char **argv) {

	mainjid  = job_create(0, getuid(), 0);

	/* printf("   jid is: %llx\n", jid); */

	if (mainjid == JOB_FAIL) {
		perror("Failed to create job - asked job to pick a JID");
		cleanup();
		return(-1);
	}

	printf("   mainjid is: %llx\n", mainjid);


	/* Now we start the various tests.  Some use the JID created above, some 
	 * don't. */


	/* Make sure we can find the returned jid */
	if (jidExists(mainjid)) {
		printf("jidExists - SUCCESS\n");
	}
	else {
		printf("jidExists FAILED Could not find assigned JID in JID list.\n");
		cleanup();
		exit(1);
	}

	/* Make sure detaching jobs work properly - including when an invalid
	 * PID is given 
	 */
	if(handlesDetach()) {
		printf("handlesDetach -- SUCCESS\n");
	}
	else {
		printf("handlesDetach - FAIL\n");
		cleanup();
		exit(1);
	}

	if(handlesAttach()) {
                printf("handlesAttach -- SUCCESS\n");
        }
        else {
                printf("handlesAttach - FAIL\n");
                cleanup();
                exit(1);
        }

	/* Check to be sure if a parent is in a JID (we already are at this point)
	 * and a child is spawned... if that child creates a JID, it should be 
	 * handled correctly.
	 */
	if(handlesChildJobCreate()) {
		printf("handlesChildJObCreate - SUCCESS\n");
	}
	else {
		printf("handlesChildJobCreate - FAIL\n");
		cleanup();
		exit(1);
	}

	/* Test to be sure, when a child detaches, the parent stays attached */
	if(parentHandlesDetachedChild()) {
		printf("parentHandlesDetachedChild - SUCCESS\n");
	}	
	else {
		printf("parentHandlesDetachedChild - FAIL\n");
		cleanup();
		exit(1);
	}

	/* Test to be we handle case where we request a JID that already
	 * is in use -- should give an error - not a crash, and not allowed.
	 */
	if(handlesDuplicateJids()) {
		printf("handlesDuplicateJids - SUCCESS\n");
	}
	else {
		printf("handlesDuplicateJids returned FAIL\n");
		cleanup();
		exit(1);
	}

	/* Test to be sure job_killjid w/signal 0 actually returns true
	 * as opposed to failing like it used to in older versions of job.
	 */
	if (jkillHandlesSigZero(mainjid)) {
		printf("jkillHandlesSigZero - SUCCESS\n");
	}
	else {
		printf("jkillHnadlesSigZero returned FAIL\n");
		cleanup();
		exit(1);
	}

	/* Test to be sure the jdetach command works for jid and pid */
	if (jdetachSanityCheck()) {
		printf("jdetachSanityCheck - SUCCESS\n");
	}
	else {
		printf("jdetachSanityCheck - FAIL\n");
		cleanup();
		exit(1);
	}

	/* Checks to see if process wrapping leads to JID dupes */
	if (handlesProcessWrapping()) {
		printf("handlesProcessWrapping - SUCCESS\n");
	}
	else {
		printf("handlesProcessWrapping - FAIL\n");
		cleanup();
		exit(1);
	}

	/* cleanup and exit with success) */
	cleanup();
	printf("Great.  All tests completed, no errors\n");
	exit(0);
}

/****************************************************************************/


/* handlesDetach:
 *
 * This uses job_detachpid and job_detachjid to be sure they work right
 * even when given invalid JIDs and PIDs.
 */
int handlesDetach(void) {
	jid_t jid1 = 0;
	jid_t jid2 = 0;
	int pid1 = 0;
	int pid2 = 0;

	pid1 = createDummyProcess(PRODUCE_JID, 0, NULL);
	pid2 = createDummyProcess(PRODUCE_JID, 0, NULL);

	printf("   handlesDetach Waiting 1 sec for children to be ready...\n");
	sleep(1);

	jid1 = job_getjid(pid1);
	jid2 = job_getjid(pid2);

	
	if((pid1 == 0) || (pid2 == 0)) {
		printf("   handlesDetach Error creating dummy processes\n");
		return(0);
	}

	if((jid1 == 0) || (jid2 == 0)) {
		printf("   handlesDetach Couldn't find 1 or more JIDs\n");
		return(0);
	}

	if(job_detachpid(pid1) == JOB_FAIL) {
		perror("   handlesDetach could not detach PID");
		return(0);
	}

	/* At this point, pid1 should not have an associated JID */
	/* We expect an error here */
	if(job_getjid(pid1) == JOB_FAIL) {
		printf("   handlesDetach - job_getjid is JOB_FAIL - good\n");
	}
	else {
		printf("   handlesDetach - job_getjid NOT JOB_FAIL for pid1 - BAD\n");
		printf("                   JID is: %llx\n", job_getjid(pid1));
		return(0);
	}

	/* Now check detachjid */
	if(job_detachjid(jid2) == JOB_FAIL) {
		perror("   handlesDetach could not detach JID test2 \n");
		return(0);
	}

	/* Now pid2 should not be attached since it was in jid2 */
	if(job_getjid(pid2) == JOB_FAIL) {
		printf("   handlesDetach - job_getjid is JOB_FAIL - good test2\n");
	}
	else {
		printf("   handlesDetach - job_getjid NOT JOB_FAIL for pid2 - BAD test2\n");
		return(0);
	}

	/* all good if we're here */
	return(1);


}

/* handlesAttach:
 *
 * This uses job_attachpid to be sure it works right
 * even when given invalid JIDs and PIDs.
 */
int handlesAttach(void) {
        jid_t jid1 = 0;
        jid_t jid2 = 0;
        int pid1 = 0;
        int pid2 = 0;

        pid1 = createDummyProcess(PRODUCE_JID, 0, NULL);
        pid2 = createDummyProcess(PRODUCE_JID, 0, NULL);

        printf("   handlesAttach Waiting 1 sec for children to be ready...\n");
        sleep(1);

        jid1 = job_getjid(pid1);
        jid2 = job_getjid(pid2);


        if((pid1 == 0) || (pid2 == 0)) {
                printf("   handlesAttach Error creating dummy processes\n");
                return(0);
        }

        if((jid1 == 0) || (jid2 == 0)) {
                printf("   handlesAttach Couldn't find 1 or more JIDs\n");
                return(0);
        }

        /* pid1 should not be able to attach to jid2, We expect an error here */
        if(job_attachpid(pid1, jid2) == JOB_FAIL) {
                printf("   handlesAttach - job_attachpid is JOB_FAIL - good\n");
        }
        else {
                printf("   handlesAttach - job_attachpid is NOT JOB_FAIL for pid1 - BAD\n");
                printf("                   JID is: %llx\n", job_getjid(pid1));
                return(0);
        }

	/* Detach pid1 so we can test the attach operation */	
	if(job_detachpid(pid1) == JOB_FAIL) {
                perror("   handlesAttach - job_detachpid is JOB_FAIL - BAD\n");
                return(0);
        }
	
	if(job_attachpid(pid1, jid2) == JOB_FAIL) {
		printf("   handlesAttach - job_attachpid is JOB_FAIL for pid1 - BAD\n");
                printf("                   JID is: %llx\n", job_getjid(pid1));
                return(0);
        }
        else {
		printf("   handlesAttach - job_attachpid is NOT JOB_FAIL - good\n");
        }

        /* Now check detachjid */
        if(job_detachjid(jid2) == JOB_FAIL) {
                perror("   handlesAttach could not detach jid2 -BAD\n");
                return(0);
        }

        /* Now pid2 should not be attached since it was in jid2 */
        if(job_getjid(pid2) == JOB_FAIL) {
                printf("   handlesAttach - job_getjid is JOB_FAIL - good test2\n");
        }
        else {
                printf("   handlesAttach - job_getjid NOT JOB_FAIL for pid2 - BAD test2\n");
                return(0);
        }

        /* all good if we're here */
        return(1);


}

/* jdetachSanityCheck:
 *
 * This checks to be sure the jdetach user command seems to be working.
 *
 * Input: None
 * Return: 1 OK,    0 FAIL
 */
int jdetachSanityCheck(void) {
	jid_t jid1;
	jid_t jid2;
	int pid1;
	int pid2;
	char cmdline[100];
	int retcode;

	/* Create two jobs */
	pid1 = createDummyProcess(PROCESS_ONLY, 0, NULL);
	pid2 = createDummyProcess(PROCESS_ONLY, 0, NULL);

	if((pid1 == 0) || (pid2 == 0)) {
		printf("   jdetachSanityCheck Error creating dummy processes\n");
		return(0);
	}

	/* Find the associated JIDs */
	jid1 = job_getjid(pid1);
	jid2 = job_getjid(pid2);

	if((jid1 == JOB_FAIL) || (jid2 == JOB_FAIL)) {
		printf("   jdetachSanityCheck Error getting JIDs\n");
		return(0);
	}

	/* Use jdetach to detach pid1 */

	snprintf(cmdline, 100, "/usr/bin/jdetach -p %d",pid1);
	retcode = system(cmdline);	

	if(retcode != 0) {
		printf("   jdetachSanityCheck jdetach command returned error\n");
		return(0); /* fail */
	}

	/* Did it detach? getjid should return an error */
	if(job_getjid(pid1) != JOB_FAIL) {
		printf("   jdetachSanityCheck jdetach command didn't detach job.\n");
		return(0);
	}


	/* Use jdetach to detach jid2 */
	snprintf(cmdline, 100, "/usr/bin/jdetach -j %llx",jid2);
	retcode = system(cmdline);	

	if(retcode != 0) {
		printf("   jdetachSanityCheck jdetach test2 command returned error\n");
		return(0); /* fail */
	}

	/* Did it detach?  Let's check */

	if(job_getjid(pid2) != JOB_FAIL) {
		printf("   jdetachSanityCheck jdetach test2 command didn't detach job\n");
		return(0);
	}

	return(1); /* All is well that ends well */


}

/* parentHandlesDetachedChild:
 *
 * The parent (already has a JID from main) forks and has a child.  The 
 * child runs job_detach.  Only the child should detach -- not the parent.
 *	
 *	This will test running job_detachpid right from here, and will test
 *	the child running it herself.
 *
 * Input: None
 * Return: 1 OK,    0 FAIL
 */
int parentHandlesDetachedChild(void) {

	int dummyPid;
	jid_t dummyJid;

	/* The below is a HELPER function to be passed to createDummyProcess 
	 *******************************************************************/
	int helperFunction() {
		jid_t helperjid;

		helperjid = job_detachpid(getpid());
		printf("   child helper function - detached from job: %llx\n", helperjid);

		return(0);
	}
	/* End of HELPER function.
	 *******************************************************************/

	/* Now we create a dummy process that runs the above helper function.  
	 * The net result is the process is created, a JID is assigned, and the
	 * the child detaches from that JOB via the function above. */

	dummyPid = createDummyProcess(PROCESS_ONLY, 0, helperFunction);

	/* Wait for child to get established */
	sleep(1);

	if(dummyPid == 0) {
		printf("   parentHandlesDetachedChild failed to create process\n");
		return(0);
	}

	dummyJid = job_getjid(dummyPid);

	/* This call SHOULD fail.  If it doesn't, that's a failed test */
	if(dummyJid != JOB_FAIL) {
		printf("   parentHandlesDetachedChild - dummy process has JID and should not - BAD\n");
		return(0);
	}



	/****** test 2 for this function *******/

	/* now create another dummy job, no helper function, and detach from
	 * parent. */

	dummyPid = createDummyProcess(PROCESS_ONLY, 0, NULL);

	/* Wait for child to get established */
	sleep(1);

	if(dummyPid == 0) {
		printf("   parentHandlesDetachedChild failed to create process-test 2\n");
		return(0); 
	}

	/* force the process to detach from the job */
	dummyJid = job_detachpid(dummyPid);

	if (dummyJid == JOB_FAIL) {
		printf("   job_detachpid in test2 shoulid have valid JID but doesn't\n");
		return(0);
	}

	/* Make sure the process has no JID */
	if(job_getjid(dummyPid) != JOB_FAIL) {

		printf("   parentHandlesDetachedChild-test2-dummy process has JID and should not - BAD\n");

		return(0);
	}

	/* if we get this far, we're golden */
	return(1);

}

/* handlesChildJobCreate:
 *
 * If the parent is is in a job and forks and a child then tries to create
 * a job for itself, make sure:
 *
 * - The parent still has the same original JID
 * - After the child tries to issue job_create (but NOT doing a detach first!),
 *   the child should be in its own JID and not part of the parent.
 *
 * In the past, there was a bug where the system would deadlock in this 
 * situation.
 *
 * Input: None
 * Return: 1 OK,   0 FAIL
 */

int handlesChildJobCreate(void) {
	jid_t myjid;
	int pid;

	/* At the start of this program, this process is given a JID.  Make sure
	 * we still have it */

	myjid = job_getjid(getpid());
	if(myjid == JOB_FAIL) {
		printf("   handlesChildJObCreate - couldn't even get my own JID\n");
		return(0);
	}

	if(myjid != mainjid) {  /* mainjid is from main */
		printf("   handlesChildJobCreate - getjid reports a different JID:\n");
		printf("   getjid reports: %llx while mainjid is: %llx\n", myjid, mainjid);
		return(0);
	}

	/* create process with no special JID assignments */
	pid = createDummyProcess(USE_SPECIFIC_JID, 9, NULL);

	printf("   handlesChildJobCreate got PID %d\n", pid);

	/* sleep for a second to be sure child is ready */
	sleep(1);

	if(pid == 0) {
		printf("   handleChildJobCreate couldn't create process");
		return(0);
	}

	/* Check to be sure the child has JID 9 */

	if (job_getjid(pid) != 9) {
		printf("   handlesChildJobCreate JID not the same as requested\n");
		printf("   requested 9, got %llx\n", job_getjid(pid));
		return(0);
	}

	/* we have a valid PID and the JID is set as assigned. */
	return(1);

}

/* handlesDuplicateJids:
 *
 * If two jobs request the same JID, the 1st request should succeed and the
 * 2nd request should fail.  
 *
 * Input: none
 * Return: 1 OK,   0 FAIL
 */
int handlesDuplicateJids(void) {
	int pid1;
	int pid2;

	/* use createDummyProcess to attempt to create two jobs with same JID.
	 * Since createDummyProcess sets the JID in the child section, and 
	 * because it returns a PID and not a JID, we have to check for ourselves
	 * as to what happened.
	 */

	pid1 = createDummyProcess(USE_SPECIFIC_JID, 8, NULL);
	printf("   in handlesDuplicateJids, pid1 is: %d\n", pid1);
	if(pid1 == 0) {
		printf("   handlesDuplicateJids not able to create even one process.\n");
		return(0);
	}

	pid2 = createDummyProcess(USE_SPECIFIC_JID, 8, NULL); 
	printf("   in handlesDuplicateJids, pid2 is: %d\n", pid2);
	if(pid2 == 0) {
		printf("   handlesDuplicateJids couldn't create 2nd process\n");
		return(0);
	}

	/* sleep for a second to be sure children are ready */
	sleep(1);

	/* Now we do some checks.  pid1's jid should be 8.  pid2 will have the
	 * same jid as the parent because the assignment should fail */

	if(job_getjid(pid1) == JOB_FAIL) {
		printf("   handlesDuplicateJids pid1 has no JID - BAD\n");
		return(0);
	}
	if(job_getjid(pid2) == mainjid) {
		printf("   handlesDuplicateJids pid2 has same JID as parent - GOOD\n");
		return(1);
	}
	printf("   handlesDuplicateJids - you shouldn't see this!!!\n");
	printf("   pid1 jid: %llx   pid2 jid  %llx\n", job_getjid(pid1), job_getjid(pid2));
	return(0);

}

/* handlesProcessWrapping
 *
 * If you use more than all of the available PIDs so that all numbers are
 * used at least once, be sure that there are no duplciate JIDs.
 * JIDs used to be formed based on the hostid and the parent PID.
 * If a parent exited, the children were still part of the job.  But if
 * that PID is re-used, it could lead to a duplicate JID being issued.
 *
 * Here we create a dummy process.  That dummy process creates another
 * process.  This new process creates a job ID for itself and forks.
 * The parent dies but the child sleeps.  Then we fork/waitpid/exit
 * many many times to get the PIDs to wrap, each time checking to see if
 * there are any dupe JIDs.  If there are, this exits with a failure code.  
 * The exit status is checked by the parent to see if we need to fail out or 
 * not.
 *
 * Input: none
 * Return: 1 OK,   0 FAIL
 */
int handlesProcessWrapping(void) {
	jid_t *jidlist;
	int i = 0;
	int retval = 0;
	int bufsize = 0;
	int jidcnt = 0;
	pid_t pid;
	pid_t exitpid;
	jid_t jid;
	jid_t searchjid;
	int x = 0;
	int y = 0;
	int exitstatus;

	printf("   in handlesProcessWrapping...\n");

	/* This helper function needs to fork, and then the parent has to die */
	/* The below is a HELPER function to be passed to createDummyProcess 
	 *******************************************************************/
	int helperFunction() {
		jid_t helperjid;
		int j;
		int helperpid;

		helperjid = job_create(0, getuid(), 0);
		printf("   handleProcessWrapping HELPER jid: %llx, pid: %d\n", helperjid, getpid());
		if(helperjid == JOB_FAIL) {
			perror("   handlesProcessWrapping HELPER child - error creating job");
			exit(1);
		}

		helperpid = fork();

		if(helperpid == 0) {
			/* Child process */
	
			printf("   child (of child :) pid: %d, jid: %llx,  sleeping\n",getpid(),helperjid);
			sleep(7200);
			exit(0);
		}  /* * * * end of child * * * */
		else if (helperpid == -1) {
			/* fork failed */
			perror("Fork failure in handlesProcessWrapping HELPER function.");
			return(0);
		}
		else {
			/* parent */
				  
			/* add pid to the cleanup list */
			
			pidlist[pidcount] = helperpid;
			pidcount++;
	
			printf("   parent (of child :) pid: %d exiting\n", getpid());
			exit(0); /* Parent needs to exit to trigger this */
		}

	}
	/* End of HELPER function.
	 *******************************************************************/

	pid = createDummyProcess(PROCESS_ONLY, 0, helperFunction);
	if (pid == 0) {
		printf("   handlesProcessWrapping - No PID returned for dummy...\n");
		return (0);
	}

	printf("   Forking LOTS of processes (all w/matching waitpid) to wrap PID count...\n");

	for (i=0; i<= PROCESS_WRAPPING_COUNT ; i++) {

		/* Here we create a bunch of processes that just exit to get the PIDs
		 * to wrap.  Then we see if there are dupes in the jid list... */
		
		exitpid = fork();

		if(exitpid == 0) {
			/* Child process */
			jid = job_create(0, getuid(), 0);
			if(jid == JOB_FAIL) {
				perror("   handlesProcessWrapping fork/exit - error creating job");
				exit(1);
			}
			/* printf("   handlesProcessWraping fork/exit pid: %d, jid: %llx\n", getpid(), jid); */

			/* See if there are dupes in the jid list */
			jidcnt = job_getjidcnt();
			if (jidcnt == -1) {
				perror("handlesProcessWrapping fork/exit can't get jid count");
			}

			bufsize = jidcnt*sizeof(jid_t);

			jidlist = (jid_t *)malloc(bufsize);
			if (jidlist == NULL) {
				perror("malloc in handlesProcessWrapping fork/exit failed");
				exit(1);
			}
			retval = job_getjidlist(jidlist, bufsize);

			if (retval == -1) {
				printf("   FAILURE handlesProcessWrapping fork/exit - bad return from job_getjidlist\n");
				exit(100);
			}
			for(x = 0 ; x < jidcnt ; x++) {
				searchjid = jidlist[x];
				for (y = 0; y < jidcnt ; y++) {
					if ((jidlist[y] == searchjid) && (x != y) ) {
						printf("   handlesProcessWrapping ERROR Dupe JID: %llx\n", searchjid);
						printf("   sleeping 30 seconds...\n");
						sleep(30);
						exit(200);
					}
				}
			}

			free(jidlist);
			exit(0);

		}  /* * * * end of child * * * */
		else if (exitpid == -1) {
			/* fork failed */
			perror("Fork fail in handlesProcessWrapping (creating fork/exit processes).");
			return(0);
		}
		else {
			/* parent */
				  
			/* using waitpid so we don't fork bomb and to get status of child  */
			waitpid(exitpid,&exitstatus, 0);
			if (WIFEXITED(exitstatus)) {
				if(WEXITSTATUS(exitstatus) != 0) {
					printf("   handlesProcessWrapping reported a problem...\n");
					return(0);  /* return failure */
				}
			}
		} 
	}

	return(1); /* If we got here, all is well */
}

/* createDummyProcess:
 *
 * Fork a dummy process. It will sleep for 7200 seconds (2 hours).
 * However, it should be cleaned up in cleanup in theory.
 * Input action: PROCESS_ONLY, PRODUCE_JID, or USE_SPECIFIC_JID.  
 *               Describes if simply a process is created, or if
 *               it should be part of a JOB.  If part of a JOB, should
 *               a JID be produced or should it use a given JID...
 * Input injid: Can be 0 or a given JID.
 * Input Function: NULL or pointer to function child should run
 *
 * Return: PID created
 *         (if you want the JID, use job_getjid on this PID)
 *         0 returned on failure.           
 */
int createDummyProcess(int action, jid_t injid, int (*funct)()) {
	int pid;
	int status;
	jid_t newjid;
	int killreturn;


	if (pidcount >= MAXPIDS) {
		printf("ERROR createDummyProcess - using more than MAX processes\n");
		return(0);
	}

	pid = fork();

	if(pid == 0) {
		/* Child process */

		if (action == USE_SPECIFIC_JID) {
			printf("   Calling job_create -child- requesting specific JID\n");
			newjid = job_create(injid, getuid(), 0);
			if(newjid == JOB_FAIL) {
				perror("   createDummyProcess - in child - error creating job");
			}
		}
		else if(action == PRODUCE_JID) {
			/* create a job. assign a generated JID */
			printf("   Calling job_create -child- NOT requesting specific JID\n");
			newjid = job_create(0, getuid(), 0);
			if(newjid == JOB_FAIL) {
				perror("   createDummyProcess - in child - error creating job");
			}
		}

		if((*funct) != NULL) {
			printf("   Child will now run pointer to function...\n");
			(*funct)();
		}

		printf("   child pid: %d sleeping\n",getpid());
		sleep(7200);
		exit(0);
	}  /* * * * end of child * * * */
	else if (pid == -1) {
		/* fork failed */
		perror("Fork failure in createDummyProcess");
		return(0);
	}
	else {
		/* parent */
			  
		/* add pid to the cleanup list */
		
		pidlist[pidcount] = pid;
		pidcount++;


		return(pid);
	}
}

/* cleanup:
 *
 * Try to kill any remaining PIDs and any other things hanging around from
 * testing.
 * input: none
 * return: none
 */
void cleanup(void) {
	int i;

	for(i = 0 ; i < pidcount ; i++) {
		/* We're just trying our best.  We won't look at return vals from kill */
		printf("   cleanup sending SIGTERM to: %d\n",pidlist[i]);
		kill(pidlist[i], SIGTERM);
	}
}


/* jkillHandlesSigZero
 *
 * Make sure signal zero is handled w/o error.  This tests both the user
 * command and the function call in one shot.
 * Input: JID
 * Return 1 if OK, 0 if fail
 */
int jkillHandlesSigZero(jid_t injid) {
	int retcode;
	char cmdline[100];

	/* print spaces to format return from jkill better */
	snprintf(cmdline, 100, "/usr/bin/jkill -0 %llx",injid);

	retcode = system(cmdline);
	/* printf("   jkillHandlesSigZero jid: %llx  retcode: %d\n",injid,retcode); */

	if(retcode == 0) {
		return(1);
	}
	else {
		return(0);
	}

}

/* jidExists:
 *
 * Search for the jid in the jid list.  
 * Input: Job ID
 * Return: 1 if found, 0 if not found
 */
int jidExists(jid_t injid) {

	jid_t *jidlist;
	int i = 0;
	int retval = 0;
	int bufsize = 0;
	int jidcnt = 0;

	jidcnt = job_getjidcnt();

	if (jidcnt == -1) {
		perror("   jidexists Can't get count from job_getjidcnt");
		exit(1);
	}

	bufsize = jidcnt*sizeof(jid_t);
	jidlist = (jid_t *)malloc(bufsize);

	if (jidlist == NULL) {
		perror("malloc in jidexists failed");
		exit(1);
	}

	retval = job_getjidlist(jidlist, bufsize);

	if (retval == -1) {
		printf("FAILURE in jidlist - failure return from job_getjidlist\n");
		return(1);
	}
	else {
		/* find jid in the list */
		for(i = 0 ; i < jidcnt ; i++) {
			if(jidlist[i] == injid) {
				return(1);
			}
		}
	}

	/* jid not found in list */
	return(0);
	
}

