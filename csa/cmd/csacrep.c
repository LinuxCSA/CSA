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

/*
 * csacrep:
 *	Output a report from a consolidated accounting file (cacct).
 *	
 * Options include:
 *	-b	Output fee and SBU data
 *	-c	Output CPU time, core and virtual memory integrals
 *	-d	Output cumulative disk usage count and # of samples
 *	-f	Report full data
 *	-g	Output group ID
 *	-h	Display heading
 *	-J	Output job ID
 *	-j	Output # of procs and jobs
 *	-n	Output prime/non-prime data
 *	-P	Print project number (default is project name)
 *	-p	Sort by project ID (default is no sort)
 *	-V	Output minor and major faults
 *	-u	Sort by user ID (default is no sort)
 *	-w	Output CPU, block I/O, and swap in delays
 *	-x	Output characters transferred, # of
 *	  	logical requests (read and write system calls)
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "cacct.h"
#include "sbu.h"

#define	ISIZ     200      /* initial number of table entries */
#define	ZERO_MT 0.000001  /* CPU time floor for multi-tasking record */

char  *Prg_name;

int    MTASK_HI;          /* high mtask report bucket */
int    MTASK_LO;          /* low mtask report bucket */

int    b_flag = 0;
int    c_flag = 0;
int    d_flag = 0;
int    f_flag = 0;
int    g_flag = 0;
int    h_flag = 0;
int    J_flag = 0;
int    j_flag = 0;
int    n_flag = 0;
int    P_flag = 0;
int    V_flag = 0;
int    w_flag = 0;
int    x_flag = 0;

int    Psort = 0;
int    Usort = 0;

struct cacct  *clist;
int    csize = 0;         /* current entries used */
int    max_size = 0;      /* max table size */

char  *optstring = "bcdfghJjnPpVuwx";

void enter(struct cacct *);
void full_print(struct cacct *);
void hdprint();
void select_print(struct cacct *, int);

void usage()
{
    acct_err(ACCT_ABORT,
           "Command Usage:\n%s\t[-p | -u] [-bcdfghJjnPVwx]",
    	Prg_name);
}

int cmp(const void *e1, const void *e2)
{
    struct cacct  *c1;
    struct cacct  *c2;

    c1 = (struct cacct *) e1;
    c2 = (struct cacct *) e2;

    if (Psort)
    {
        if (c1->ca_prid - c2->ca_prid)
            return(c1->ca_prid - c2->ca_prid);
        else
            return(c1->ca_uid - c2->ca_uid);
    }
    else
    {
        if (c1->ca_uid - c2->ca_uid)
            return(c1->ca_uid - c2->ca_uid);
        else
            return(c1->ca_prid - c2->ca_prid);
    }
}

int
main(int argc, char **argv) 
{
    int           c;
    struct cacct  cbuf;
    int	          fd;
    int           one = 1;

    Prg_name = argv[0];
    while ((c = getopt(argc, argv, optstring)) != EOF)
    {
        switch((char) c)
        {
        case 'b':
            b_flag++;
            break;
        case 'c':
            c_flag++;
            break;
        case 'd':
            d_flag++;
            break;
        case 'f':
            f_flag++;
            break;
        case 'g':
            g_flag++;
            break;
        case 'h':
            h_flag++;
            break;
        case 'J':
            J_flag++;
            break;
        case 'j':
            j_flag++;
            break;
        case 'n':
            n_flag++;
            break;
        case 'P':
            P_flag++;
            break;
        case 'p':
            Psort = 1;
            break;
	case 'V':
	    V_flag = 1;
	    break;
        case 'u':
            Usort = 1;
            break;
        case 'w':
            w_flag++;
            break;
        case 'x':
            x_flag++;
            break;
        default:
            usage();
            exit(1);
        }
    }

    if (Psort + Usort > 1)
    {
        acct_err(ACCT_FATAL,
               _("The (-%s) and the (-%s) options are mutually exclusive."),
        	"p", "u");
        usage();
        exit(1);
    }

    /*
     *	Get the system parameters from the configuration file.
     */
    if ((fd = openacct (NULL, "r")) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the opening of file '%s'."),
        	"stdin");
    if (Psort + Usort == 0)
    {
        /*
         *	Read each cacct record and write the requested output.
         */
        while (readcacct (fd, &cbuf) == sizeof (struct cacct))
        {
            if (f_flag)
                full_print(&cbuf);
            else
            {
                if (h_flag && one)
                {
                    one = 0;
                    hdprint();
                }
                select_print(&cbuf, 0);
            }
        }
    }
    else
    {
        int  i;
        
        while (1)
        {
            if (readcacct (fd, &cbuf) == sizeof (struct cacct))
                enter(&cbuf);
            else
                break;
        }
        qsort((char *) clist, csize, sizeof(struct cacct), cmp);
        for (i = 0; i < csize; i++)
        {
            if (f_flag)
                full_print(&clist[i]);
            else
            {
                if (h_flag && one)
                {
                    one = 0;
                    hdprint();
                }
                select_print(&clist[i], 0);
            }
        }
    }

    exit(0);
}

void enter(struct cacct *rec)
{
    int  sz;

    /*
     *	If first call.
     */
    if (max_size == 0)
    {
        max_size = ISIZ;
        sz = sizeof(struct cacct) * max_size;
        if ((clist = (struct cacct *) malloc(sz)) == NULL)
            acct_perr(ACCT_ABORT, errno,
            	_("There was insufficient memory available when allocating '%s'."),
            	"struct cacct");
    }

    /*
     *	If table is overflowing.
     */
    if (csize == max_size)
    {
        max_size = max_size + ISIZ;
        sz = sizeof(struct cacct) * max_size;
        if ((clist = (struct cacct *) realloc(clist, sz)) == NULL)
            acct_perr(ACCT_ABORT, errno,
            	_("There was insufficient memory available when reallocating '%s'."),
            	"struct cacct");
    }

    /*
     *	Add to table.  Let compiler do vector copy.
     */
    clist[csize++] = *rec;
}

/*
 *	Full print routine.
 */
void full_print (struct cacct *rec)
{
    int    dm_used = 0;
    char  *group;
    int    i;
    char  *name;
    char  *project;
    int    tape_used = 0;
    char  *user;

    project = prid_to_name(rec->ca_prid);
    if (project[0] == '?')
        project = "Unknown";
    printf ("Project Name / PRID : %8.8s / %-5lld  ", project, rec->ca_prid);

    user = uid_to_name(rec->ca_uid);
    if (user[0] == '?')
        user = "Unknown";
    printf ("Login Name / UID   : %8.8s / ", user);
    if (rec->ca_uid == CACCT_NO_ID)
        printf ("Unknown\n");
    else
        printf ("%d\n", rec->ca_uid);
    
    printf ("No. of Processes   : %8lld           ", rec->ca_pc);
    printf ("No. of Jobs        : %8lld\n", rec->ca_njobs);
    printf ("Disk Blocks        : %8.0f           ", rec->ca_du);
    printf ("Disk Samples       : %8d\n", rec->ca_dc);
    
    printf ("Job Id             : ");
    if (rec->ca_jid == CACCT_NO_ID)
        printf ("%8.8s           ", "Unknown");
    else
        printf ("%-#18llx ", rec->ca_jid);
    printf ("Group Id           : ");
    group = gid_to_name(rec->ca_gid);
    if (group[0] == '?')
        group = " Unknown";
    printf ("%8.8s\n", group);
    
    printf ("Fee                : %8.2f           ", rec->ca_fee);
    printf ("SBU's              : %8.2f\n", rec->ca_sbu);
    /* printf ("Migrated Blocks    : %8lld\n", rec->ca_dmdu); */
    printf ("                              --------------------\n");

    printf ("CPU time               (P/N) : %8.0f / %8.0f  seconds\n",
            rec->prime.cpuTime, rec->nonprime.cpuTime);
    printf ("KCORE * CPU Time       (P/N) : %8.0f / %8.0f  Kbytes * mins\n",
            rec->prime.kcoreTime, rec->nonprime.kcoreTime);
    printf ("KVIRT * CPU Time       (P/N) : %8.0f / %8.0f  Kbytes * mins\n",
            rec->prime.kvirtualTime, rec->nonprime.kvirtualTime);
    printf ("Total K-Chars Xferred  (P/N) : %8.2f / %8.2f \n",
            rec->prime.charsXfr / 1024.0, rec->nonprime.charsXfr / 1024.0);
    printf ("Total Log. I/O Req.    (P/N) : %8lld / %8lld \n",
            rec->prime.logicalIoReqs, rec->nonprime.logicalIoReqs);
    printf ("Total Minor Faults     (P/N) : %8lld / %8lld \n",
	    rec->prime.minorFaultCount, rec->nonprime.minorFaultCount);
    printf ("Total Major Faults     (P/N) : %8lld / %8lld \n",
	    rec->prime.majorFaultCount, rec->nonprime.majorFaultCount);
    printf ("CPU Delay              (P/N) : %8.0f / %8.0f  seconds\n",
            rec->prime.cpuDelayTime, rec->nonprime.cpuDelayTime);
    printf ("Block I/O Delay        (P/N) : %8.0f / %8.0f  seconds\n",
            rec->prime.blkDelayTime, rec->nonprime.blkDelayTime);
    printf ("Swap In Delay          (P/N) : %8.0f / %8.0f  seconds\n",
            rec->prime.swpDelayTime, rec->nonprime.swpDelayTime);

    /* Workload management */
    printf ("Number of Batch Requests       : %8lld\n", rec->can_nreq);
    printf ("Total User CPU Time          : %8.4f seconds\n",
            TIME_2_SECS(rec->can_utime));
    printf ("Total System CPU Time        : %8.4f seconds\n",
            TIME_2_SECS(rec->can_stime));


    /* NOTE - Workload management information needs to be printed when it is
       available. */
    

    printf ("\n");
    printf ("================================================================================\n");
    printf ("\n");
}

/*
 *	Print heading.
 */
void hdprint()
{
    int   i;
    char  t1[9];
    char  l1[4056], l2[4056], l3[4056];

    l1[0] = l2[0] = l3[0] = '\0';

    strcat (l1, "PROJECT    USER    LOGIN  ");
    if (P_flag)
        strcat (l2, "  ID        ID     NAME   ");
    else
        strcat (l2, " NAME       ID     NAME   ");
    strcat (l3, "======== ======== ========");

    if (J_flag)
    {
        strcat(l1, "        JOB        ");
        strcat(l2, "        ID         ");
        strcat(l3, " ==================");
    }

    if (g_flag)
    {
        strcat(l1, "  GROUP ");
        strcat(l2, "  NAME  ");
        strcat(l3, " =======");
    }

    if (n_flag)
    {
        strcat (l1, " PRIME/ ");
        strcat (l2, " N-PRIME");
        strcat (l3, " =======");
    }

    if (c_flag)
    {
        strcat (l1, "  CPU-TIM  KCORE *  KVIRT *");
        strcat (l2, "  [SECS]   CPU-MIN  CPU-MIN");
        strcat (l3, " ======== ======== ========");
    }

    if (w_flag)
    {
        strcat (l1, "        DELAY [SECS]       ");
        strcat (l2, "   CPU     BLOCK     SWAP  ");
        strcat (l3, " ======== ======== ========");
    }

    if (x_flag)
    {
        strcat (l1,"  K-CHARS LOG-I/O ");
        strcat (l2,"  XFERRED REQUESTS");
        strcat (l3," ======== ========");
    }

    if (V_flag)
    {
	strcat (l1,"    MINOR    MAJOR"); 
	strcat (l2,"    FAULTS   FAULTS");
	strcat (l3,"  ======== ========");
    }

    if (j_flag)
    {
        strcat (l1, "   # OF     # OF  ");
        strcat (l2, "   PROCS    JOBS  ");
        strcat (l3, " ======== ========");
    }

    if (d_flag)
    {
        strcat (l1, "   DISK     DISK  ");
        strcat (l2, "  BLOCKS   SAMPLES");
        strcat (l3, " ======== ========");
    }

    if (b_flag)
    {
        strcat (l1, "    FEE     SBUs  ");
        strcat (l2, "                  ");
        strcat (l3, " ======== ========");
    }

    printf ("%s\n", l1);
    printf ("%s\n", l2);
    printf ("%s\n", l3);
}

/*
 * pnp is assumed to be 0 when called from main().
 * pnp is 1 on recursive call if n_flag is set.
 */
void select_print (struct cacct *rec, int pnp)
{
    char  *group;
    int	   ind;
    char  *project;
    char  *user;

    if (P_flag)
        printf ("%-8lld", rec->ca_prid);
    else
    {
        project = prid_to_name(rec->ca_prid);
        if (project[0] == '?')
            project = "Unknown";
        printf ("%-8.8s", project);
    }

    user = uid_to_name(rec->ca_uid);
    if (user[0] == '?')
        user = "Unknown";
    if (rec->ca_uid == CACCT_NO_ID)
        printf (" %-8.8s", "Unknown");
    else
        printf (" %-8d", rec->ca_uid);
    printf (" %-8.8s", user);

    if (J_flag)
    {
        if (rec->ca_jid == CACCT_NO_ID)
            printf (" %18.18s", "Unknown");
        else
            printf (" %#18llx", rec->ca_jid);
    }

    if (g_flag)
    {
        group = gid_to_name(rec->ca_gid);
        if (group[0] == '?')
            group = "Unknown";
        printf (" %-7.7s", group);
    }

    if (n_flag)
        printf (" %7.7s", pnp ? "N" : "P");

    if (c_flag)
    {
        if (pnp == 0)
            printf (" %8.0f %8.0f %8.0f",
                rec->prime.cpuTime + (n_flag ? 0 : rec->nonprime.cpuTime),
                rec->prime.kcoreTime + (n_flag ? 0 : rec->nonprime.kcoreTime),
                rec->prime.kvirtualTime + (n_flag ? 0 : rec->nonprime.kvirtualTime));

	else
            printf (" %8.0f %8.0f %8.0f",
                rec->nonprime.cpuTime,
                rec->nonprime.kcoreTime,
                rec->nonprime.kvirtualTime);
    }

    if (w_flag)
    {
        if (pnp == 0)
            printf (" %8.0f %8.0f %8.0f",
                rec->prime.cpuDelayTime + (n_flag ? 0 : rec->nonprime.cpuDelayTime),
                rec->prime.blkDelayTime + (n_flag ? 0 : rec->nonprime.blkDelayTime),
		rec->prime.swpDelayTime + (n_flag ? 0 : rec->nonprime.swpDelayTime));
	else
            printf (" %8.0f %8.0f %8.0f",
                rec->nonprime.cpuDelayTime,
                rec->nonprime.blkDelayTime,
		rec->nonprime.swpDelayTime);
    }

    if (x_flag)
    {
        if (pnp == 0)
            printf (" %8.0f %8lld",
                (rec->prime.charsXfr + (n_flag ? 0 : rec->nonprime.charsXfr)) / 1024.0,
                rec->prime.logicalIoReqs + (n_flag ? 0 : rec->nonprime.logicalIoReqs));
	else
            printf (" %8.0f %8lld",
                rec->nonprime.charsXfr,
                rec->nonprime.logicalIoReqs);
    }

    if (V_flag)
    {
        if (pnp == 0)
	    printf (" %8lld %8lld",
		rec->prime.minorFaultCount + (n_flag ? 0 : rec->nonprime.minorFaultCount),
		rec->prime.majorFaultCount + (n_flag ? 0 : rec->nonprime.majorFaultCount));
	else
	    printf (" %8lld %8lld",
		rec->nonprime.minorFaultCount,
		rec->nonprime.majorFaultCount);
    }

    if (!pnp && j_flag)
        printf (" %8lld %8lld", rec->ca_pc, rec->ca_njobs);

    if (!pnp && d_flag)
        printf (" %8.0f %8d", rec->ca_du, rec->ca_dc);

    if (!pnp && b_flag)
        printf (" %8.2f %8.2f", rec->ca_fee, rec->ca_sbu);

    printf ("\n");

    if (!pnp && n_flag)
        select_print(rec, 1);

    return;
}
