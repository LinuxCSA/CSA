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
 *  csaedit.c:
 *      Deletes selected records from the specified accounting file.  The
 *      selection of records is determined by the user.
 *
 *      csaedit expects that the user first verify the accounting file with
 *      the csaverify(1M) command.  If csaverify indicates that there is
 *      invalid data, the user must first delete the invalid data.  This can
 *      be done by passing as input to csaedit the file containing information
 *      about invalid records that csaverify produces.
 *
 *      After deleting the bad records, the user may choose to view and/or
 *      delete valid data.
 *
 *      csaedit outputs the data to standard output or to a file.  The output
 *      data can be in binary or ASCII form.  Binary data can only be written
 *      out to a file.  Only valid data can be output.
 *
 *	Usage:
 *           csaedit [-P pacct] -b offsetfile -o outfile [-v]
 *           csaedit [-P pacct] [-o outfile] -A [-m] [-t] [-v]
 *           csaedit [-P pacct] -r reclist -o outfile [-v]
 *           csaedit [-P pacct] -r reclist [-o outfile] -A | -x [-m] [-t] [-v]
 *
 *      File Name Options
 *        -P pacct       Specifies a pacct or spacct accounting file.
 *        -o outfile     Specifies the output file.  Default is stdout.
 *
 *	Output Content Options:
 *        -A             Specifies ASCII output for valid data.  The default
 *                       is to output binary data.
 *        -m             Outputs memory values.
 *        -t             Outputs CPU times.  For consolidated workload
 *                       mgmt records, queue wait time is also displayed.
 *        -v             Specifies verbose mode (written to stderr).
 *        -x             Specifies no execute mode.  Only the valid records to
 *                       be deleted are displayed.  The selected records are
 *                       not actually deleted.  The records are displayed in
 *                       ASCII form; they are written to stdout or to a file
 *                       if the -o option is specified.
 *
 *	Record Selection Options:
 *        -b offsetfile	 Specifies the file that contains information about
 *                       invalid records that are to be deleted.  Information
 *                       about an invalid record consists of the record's byte
 *                       offset and length.  File produced by csaverify(1M).
 *        -r reclist     Specifies the record numbers of the records to be
 *                       deleted.  reclist is a comma-separated list.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <string.h>
#include <time.h>

#include "csa.h"
#include "csaacct.h"

#include "acctdef.h"
#include "acctmsg.h"
#include "csaedit.h"

#define UNKNOWN  "UNKNOWN"
#define REC_INC  10	      /* Increment for record list. */
#define BLANK    " "

char  *Prg_name;

int    ascii = FALSE;         /* Ascii output. */
FILE  *ascii_ostream = NULL;  /* Stream associated with the ascii output
                                 file. */
int    binary_ofd = -1;       /* File descriptor of the binary output file. */
int    no_exec = FALSE;       /* No execute mode. */
char  *offsetfile = NULL;     /* File containing byte offset and length of
                                 invalid records - produced by csaverify. */
FILE  *offset_stream;         /* Stream associated with the byte offset
                                 file. */
char  *outfile = NULL;        /* Output file name. */
int    output_cpu = FALSE;    /* CPU time output flag. */
int    output_mem = FALSE;    /* Memory output flag. */
char  *pacct = WPACCT;        /* Accounting file name. */
int    pacct_fd;              /* Accounting file descriptor. */
int   *reclist = NULL;        /* List of valid records to be deleted - by
                                 record number. */
int    rlsize = 0;            /* # of valid records to be deleted. */
int    verbose = FALSE;       /* Verbose flag. */

char           acctentbuf[ACCTENTSIZ];	/* Accounting record buffer. */
struct acctent acctrec = {acctentbuf};

void delete_invalid ();
void process_valid ();
void print_asciihdr ();
void print_asciiout (int);
int rec_sort (const void *, const void *);


void usage ()
{
    acct_err(ACCT_ABORT,
           "Command Usage:\n%s\t[-P pacct] -b offsetfile -o outfile [-v]\n%s\t[-P pacct] [-o outfile] -A [-m] [-t] [-v]\n%s\t[-P pacct] -r reclist -o outfile [-v]\n%s\t[-P pacct] -r reclist [-o outfile] -A | -x [-m] [-t] [-v]",
    	Prg_name, Prg_name, Prg_name, Prg_name);
}


main (int argc, char **argv)
{
    extern char  *optarg;
    
    int    c;
    int    maxrec = 0;  /* Max size of the list of valid records to be
                           deleted. */
    int    size;
    char  *token;       /* Parameter list token. */

    Prg_name = argv[0];

    /*	Get the options. */
    while ((c = getopt (argc, argv, "Ab:mo:P:r:tvx")) != EOF)
    {
        switch ((char) c)
        {
        case 'A':                 /* Ascii output. */
            ascii = TRUE;
            break;
        case 'b':                 /* Input file containing byte offset */
            offsetfile = optarg;  /* and length of invalid records. */
            break;
        case 'm':                 /* Memory output. */
            output_mem = TRUE;
            break;
        case 'o':                 /* Output file. */
            outfile = optarg;
            break;
        case 'P':                 /* pacct input file. */
            pacct = optarg;
            break;
        case 'r':     /* List of record #'s of valid records to be skipped. */
            if ((reclist = (int *) malloc (REC_INC * sizeof (int))) == NULL)
                acct_perr(ACCT_ABORT, errno,
                	_("There was insufficient memory available when allocating '%s'."),
                	"record list");

            maxrec = REC_INC;
            while ((token = strtok (optarg, ",")) != NULL)
            {
                reclist[rlsize] = atoi (token);
                rlsize++;
                optarg = NULL;
                if (rlsize == maxrec)
                {
                    maxrec += REC_INC;
                    size = maxrec * sizeof (int);
                    if ((reclist = (int *) realloc ((int *) reclist, size))
                        == NULL)
                    {
                        acct_perr(ACCT_ABORT, errno,
                        	_("There was insufficient memory available when reallocating '%s'."),
                        	"record list");
                    }
                }
            }
            /* Now sort the list in ascending order. */
            qsort ((char *) reclist, rlsize, sizeof (int), rec_sort);
            break;
        case 't':                 /* CPU time output. */
            output_cpu = TRUE;
            break;
        case 'v':                 /* Verbose mode. */
            verbose = TRUE;
            break;
        case 'x':                 /* No execute mode. */
            no_exec = TRUE;
            break;
        default:
            usage ();
        }
    }

    /* Check for legal parameter combinations. */
    if ((ascii && no_exec) ||
        (no_exec && reclist == NULL) ||
        (outfile == NULL && !ascii && !no_exec) ||
        (offsetfile == NULL && !ascii && reclist == NULL) ||
        ((output_cpu || output_mem) && !ascii && !no_exec) ||
        (offsetfile != NULL && (reclist != NULL || outfile == NULL || ascii ||
                                no_exec)))
    {
        /* NOTE - In the last check, there is no need to check if output_cpu
           or output_mem is set because they can only be set if ascii or
           no_exec is also set.
        */
        usage ();
    }

    /* Set up. */
    if ((pacct_fd = openacct (pacct, "r")) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the opening of file '%s'."),
        	pacct);
    
    if (ascii || no_exec)
        ascii_ostream = (outfile != NULL) ? fopen (outfile, "w") : stdout;
    else
        binary_ofd = openacct (outfile, "w");
    if ((ascii_ostream == NULL) && (binary_ofd < 0))
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the opening of file '%s'."),
        	outfile);

    if (no_exec)
        fprintf (ascii_ostream, "Input records to be deleted:\n\n");
    if (ascii || no_exec)
        print_asciihdr ();

    /* Process the data. */
    if (offsetfile != NULL)  /* Delete invalid records using information */
        delete_invalid ();   /* from the byte offset file. */
    else
        process_valid ();    /* Display and/or delete valid data. */

    /* Close off files. */
    closeacct (pacct_fd);
    if (ascii_ostream != NULL)
        fclose (ascii_ostream);
    if (binary_ofd >= 0)
        closeacct (binary_ofd);
    if (offset_stream != NULL)
        fclose (offset_stream);
    
    exit (0);
}


/*
 *  Delete invalid records using byte offset and length information from the
 *  byte offset file.  The name of the byte offset file is assumed to be
 *  non-NULL.
 */
void delete_invalid ()
{
    int64_t        amount_to_read;
    struct stat    buf;
    int64_t        curr_offset = 0;
    int            done = FALSE;
    int            items_read;
    int64_t        length;
    int64_t        offset;
    int64_t        size;
    
    if ((offset_stream = fopen (offsetfile, "r")) == NULL)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the opening of file '%s'."),
        	offsetfile);
    if (verbose)
    {
        fprintf (stderr, _("Processing the byte offset file '%s'.\n"),
                 offsetfile);
    }
        
    while (TRUE)
    {
        items_read = fscanf (offset_stream, "%llo (%*lld), %lld\n", &offset,
                             &length);
        if (items_read == EOF)
            break;
        if (items_read != 2)  /* Error in reading. */
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the reading of file '%s'."),
            	offsetfile);
        
        if (offset < 0 || (length <= 0 && length != END_OF_FILE))
            acct_err(ACCT_ABORT,
                   _("Offset value %#llo or length value %lld in byte\n    offset file '%s' is illegal."),
            	offset, length, offsetfile);
        amount_to_read = offset - curr_offset;
        if (amount_to_read < 0)
            acct_err(ACCT_ABORT,
                   _("Offset values in byte offset file '%s' are out of\n    sequence - stopped at offset %#llo, length %lld."),
            	offsetfile, offset, length);
        if (verbose)
        {
            fprintf (stderr, _("Processing byte offset %#llo (%lld), length %lld.\n"),
		offset, offset, length);
        }

        if (amount_to_read == 0)  /* We are currently right at the */
        {                         /* position where we are to delete. */
            if (length == END_OF_FILE)
            {
                done = TRUE;
                break;
            }
            /* Skip the number of bytes as indicated by length. */
            if ((curr_offset = seekacct (pacct_fd, length, SEEK_CUR)) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	pacct);
        }
        else
        {
            while (amount_to_read > 0)
            {
                /* Use acctrec.ent as the I/O buffer. */
                size = amount_to_read - ACCTENTSIZ;
                if (size > 0)
                    size = ACCTENTSIZ;
                else
                    size = amount_to_read;
                if (readacct (pacct_fd, acctrec.ent, size) != size)
                    acct_perr(ACCT_ABORT, errno,
                    	_("An error occurred during the reading of file '%s'."),
                    	pacct);
                if (writeacct (binary_ofd, acctrec.ent, size) != size)
                    acct_perr(ACCT_ABORT, errno,
                    	_("An error occurred during the writing of file '%s'."),
                    	outfile);
                amount_to_read -= size;
            }
            if (length == END_OF_FILE)
            {
                done = TRUE;
                break;
            }
            /* Skip the number of bytes as indicated by length. */
            if ((curr_offset = seekacct (pacct_fd, length, SEEK_CUR)) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	pacct);
        }
    }  /* while (TRUE) */
        
    if (!done)  /* Still need to read the remaining data after the last */
    {           /* given offset in the byte offset file. */
        if (curr_offset == 0)  /* The byte offset file must be empty. */
            acct_err(ACCT_ABORT,
                   _("Byte offset file '%s' is empty."),
            	offsetfile);
        if (stat (pacct, &buf))
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred getting the status of file '%s'."),
            	pacct);
        
        amount_to_read = buf.st_size - curr_offset;
        
        while (amount_to_read > 0)
        {
            /* Use acctrec.ent as the I/O buffer. */
            size = amount_to_read - ACCTENTSIZ;
            if (size > 0)
                size = ACCTENTSIZ;
            else
                size = amount_to_read;
            if (readacct (pacct_fd, acctrec.ent, size) != size)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the reading of file '%s'."),
                	pacct);
            if (writeacct (binary_ofd, acctrec.ent, size) != size)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the writing of file '%s'."),
                	outfile);
            amount_to_read -= size;
        }            
    }

    return;
}


/*
 *  Display and/or delete valid data.
 */
void process_valid ()
{
    int64_t  curr_loc = 0;
    int      deleteme;        /* Record deletion flag. */
    char     err_str[180];
    int      ndelete = 0;     /* Number of records deleted. */
    int      nrec = 0;        /* Number of input records. */
    int      nwrite = 0;      /* Number of records written. */
    int64_t  prev_loc = 0;
    int      retval;
    int64_t  skip_bytes = 0;  /* Number of bytes skipped. */
    
    int      ncsa = 0;        /* Number of CSA base records. */
    int      nmem_csa = 0;    /* Number of CSA mem records. */
    int      nio_csa = 0;     /* Number of CSA io records. */
    int      ndelay_csa = 0;  /* Number of CSA delay records. */
    int      nsoj = 0;        /* Number of soj records. */
    int      nroj = 0;        /* Number of roj records. */
    int      neoj = 0;        /* Number of eoj records. */
    int      ncfg = 0;        /* Number of config records. */
    int      njob = 0;        /* Number of job-hdr records. */
    int      nwkmg = 0;       /* Number of workload mgmt records. */
    int      nsite = 0;       /* Number of site records. */

    /* Process each input record. */
    while ((retval = readacctent (pacct_fd, &acctrec, FORWARD)) > 0)
    {
        nrec++;
        deleteme = FALSE;
        /* Mark this record as being deleted if it was specified by the
           user. */
        if (reclist != NULL && (ndelete < rlsize))
        {
            if (nrec == reclist[ndelete])
            {
                ndelete++;
                deleteme = TRUE;
            }
            else if (nrec > reclist[ndelete])
                acct_err(ACCT_ABORT,
                       _("An error was detected while checking the delete record\n    list.  This is probably caused by a coding error.")
                	);
        }
        
        /* Calling invalid_pacct() just as a sanity check. */
        if (invalid_pacct (&acctrec, err_str) != INV_NO_ERROR)
            acct_err(ACCT_ABORT,
                   _("Accounting file '%s' contains invalid records. %s"),
            	pacct, err_str);

        if (verbose && !no_exec)
        {
            switch (acctrec.prime->ah_type)
            {
            case ACCT_KERNEL_CSA:
                ncsa++;
                if (acctrec.mem)
                    nmem_csa++;
                if (acctrec.io)
                    nio_csa++;
                if (acctrec.delay)
                    ndelay_csa++;
                break;
            case ACCT_KERNEL_SOJ:
                if (acctrec.soj->ac_type == AC_SOJ)
                    nsoj++;
                else if (acctrec.soj->ac_type == AC_ROJ)
                    nroj++;
                break;
            case ACCT_KERNEL_EOJ:
                neoj++;
                break;        
            case ACCT_KERNEL_CFG:
                ncfg++;
                break;
            case ACCT_DAEMON_WKMG:
                nwkmg++;
                break;
            case ACCT_JOB_HEADER:
                njob++;
                break;
            case ACCT_KERNEL_SITE0:
            case ACCT_KERNEL_SITE1:

            case ACCT_DAEMON_SITE0:
            case ACCT_DAEMON_SITE1:
                nsite++;
                break;
            default:
                acct_err(ACCT_ABORT,
                       _("Accounting file '%s' contains invalid records."),
                	pacct);
                break;
            }
        }

        /* Output the record. */
        if (deleteme)
        {
            if ((curr_loc = seekacct (pacct_fd, 0, SEEK_CUR)) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	pacct);
            skip_bytes += curr_loc - prev_loc;
            if (no_exec)
                print_asciiout (nrec);
        }
        else if (ascii)
        {
            print_asciiout (++nwrite);
        }
        else if (!no_exec)
        {
            if (writeacctent (binary_ofd, &acctrec) <= 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the writing of file '%s'."),
                	outfile);
            nwrite++;
        }

        if ((prev_loc = seekacct (pacct_fd, 0, SEEK_CUR)) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
    }  /* while (readacctent) */

    if (retval != 0)  /* readacctent() could not read the record. */
        acct_err(ACCT_ABORT,
               _("Accounting file '%s' contains invalid records."),
        	pacct);
    
    if (verbose)
    {
        /* NOTE - We don't need to check for !no_exec before we print out the
           totals for the different types of record because those totals were
           only tallied if !no_exec.
        */
        fprintf (stderr, "\n");
        
        if (ncsa)
        {
            fprintf (stderr, "Found %d CSA base records.\n", ncsa);
            fprintf (stderr, "    Found %d MEMORY records.\n", nmem_csa);
            fprintf (stderr, "    Found %d I/O records.\n", nio_csa);
            fprintf (stderr, "    Found %d DELAY records.\n", ndelay_csa);
        }

        if (nsoj)
            fprintf (stderr, "Found %d SOJ records.\n", nsoj);
        if (nroj)
            fprintf (stderr, "Found %d ROJ records.\n", nroj);

        if (neoj)
            fprintf (stderr, "Found %d EOJ records.\n", neoj);

        if (ncfg)
            fprintf (stderr, "Found %d CONFIG records.\n", ncfg);
        if (njob)
            fprintf (stderr, "Found %d JOB HEADER records.\n", njob);
        if (nwkmg)
            fprintf (stderr, "Found %d WORKLOAD MANAGEMENT records.\n", nwkmg);
        if (nsite)
            fprintf (stderr, "Found %d SITE records.\n", nsite);
        
        fprintf (stderr, "Found %d input records.\n", nrec);
        if (!no_exec)
            fprintf (stderr, "Output %d records total.\n", nwrite);
        fprintf (stderr, "Ignored %lld bytes from the input file.\n",
                 skip_bytes);
    }
}


/*
 *  Output the header lines for the ascii report.
 */
void print_asciihdr ()
{
    /* First line. */
    fprintf (ascii_ostream, "REC  COMMAND          USER     "
             "       JOB          PROCESS PROJECT  GROUP    "
             "START TIME      EXIT ");
    if (output_cpu)
        fprintf (ascii_ostream, " ETIME    UTIME    STIME   ");
    if (output_mem)
        fprintf (ascii_ostream, "   HIMEM (KBYTES)   ");
    fprintf (ascii_ostream, "\n");

    /* Second line. */
    fprintf (ascii_ostream, "NUM  NAME             NAME     "
             "       ID           ID      ID       ID       "
             "(EOJ: END TIME) STAT ");
    if (output_cpu)
        fprintf (ascii_ostream, " (Sec)    (Sec)    (Sec)   ");
    if (output_mem)
        fprintf (ascii_ostream, " CORE      VIRTUAL  ");
    fprintf (ascii_ostream, "\n");

    /* Third line. */
    fprintf (ascii_ostream, "==== ================ ======== "
             "==================  ======= ======== ======== "
             "=============== ==== ");
    if (output_cpu)
        fprintf (ascii_ostream, " ======== ======== ========");
    if (output_mem)
        fprintf (ascii_ostream, " ========= =========");
    fprintf (ascii_ostream, "\n");

    return;
}


/*
 *  Print out the records in ascii.
 */
void print_asciiout (int recnum)
{
    struct acctcsa   *csa;
    struct acctcfg   *cfg;
    struct acctsoj   *soj;
    struct accteoj   *eoj;
    struct acctjob   *job;
    struct wkmgmtbs  *wmbs;
    time_t            ttime;

    uint64_t          corehimem;
    uint64_t          virthimem;
    char              name[32];
    char             *subtype;

    switch (acctrec.prime->ah_type)
    {
    case ACCT_KERNEL_CSA:
        csa = acctrec.csa;

        fprintf (ascii_ostream, "%-4d ", recnum);
        fprintf (ascii_ostream, "%-16.16s ", csa->ac_comm);
        strcpy (name, uid_to_name (csa->ac_uid));
        if (*name == '?')
            fprintf (ascii_ostream, "%-8d ", csa->ac_uid);
        else
            fprintf (ascii_ostream, "%-8.8s ", name);
        if (csa->ac_jid >= 0)
            fprintf (ascii_ostream, "%#18llx ", csa->ac_jid);
        else
            fprintf (ascii_ostream, "%18.18s ", UNKNOWN);
/*  Add back in here and in header if array ids available 
        fprintf (ascii_ostream, "0x%016llx ", csa->ac_ash);
*/
	fprintf (ascii_ostream, "%8d ", csa->ac_pid);
        fprintf (ascii_ostream, "%8lld ", csa->ac_prid);
        fprintf (ascii_ostream, "%8d ", csa->ac_gid);
        fprintf (ascii_ostream, "%15.15s ", ctime (&(csa->ac_btime)) + 4);
        fprintf (ascii_ostream, "%3d ", csa->ac_stat);
        if (output_cpu)
        {
            fprintf (ascii_ostream, " %8.4f", TIME_2_SECS (csa->ac_etime));
            fprintf (ascii_ostream, " %8.4f", TIME_2_SECS (csa->ac_utime));
            fprintf (ascii_ostream, " %8.4f", TIME_2_SECS (csa->ac_stime));
        }
        if (output_mem)
        {
            if (acctrec.mem)
            {
                corehimem = acctrec.mem->ac_core.himem;
                virthimem = acctrec.mem->ac_virt.himem;
            }
            else
            {
                corehimem = 0;
                virthimem = 0;
            }
            fprintf (ascii_ostream, " %9lld", corehimem);
            fprintf (ascii_ostream, " %9lld", virthimem);
        }
        fprintf (ascii_ostream, "\n");
        break;
        
    case ACCT_KERNEL_SOJ:
        soj = acctrec.soj;
        
        if (soj->ac_type == AC_SOJ)
        {
            fprintf (ascii_ostream, "%-4d Start-of-Job     ", recnum);
            ttime = soj->ac_btime;
        }
        else /* AC_ROJ */
        {
            fprintf (ascii_ostream, "%-4d Restart-of-Job   ", recnum);
            ttime = soj->ac_rstime;
        }
        strcpy (name, uid_to_name (soj->ac_uid));
        if (*name == '?')
            fprintf (ascii_ostream, "%-8d ", soj->ac_uid);
        else
            fprintf (ascii_ostream, "%-8.8s ", name);
        if (soj->ac_jid >= 0)
            fprintf (ascii_ostream, "%#18llx ", soj->ac_jid);
        else
            fprintf (ascii_ostream, "%18.18s ", UNKNOWN);
	fprintf (ascii_ostream, "%8s ", BLANK);
        fprintf (ascii_ostream, "%8s ", BLANK);
        fprintf (ascii_ostream, "%8s ", BLANK);
        fprintf (ascii_ostream, "%15.15s\n", ctime (&ttime) + 4);
        break;
        
    case ACCT_KERNEL_EOJ:
        eoj = acctrec.eoj;
        
        fprintf (ascii_ostream, "%-4d End-of-Job       ", recnum);
        strcpy (name, uid_to_name (eoj->ac_uid));
        if (*name == '?')
            fprintf (ascii_ostream, "%-8d ", eoj->ac_uid);
        else
            fprintf (ascii_ostream, "%-8.8s ", name);
        if (eoj->ac_jid >= 0)
            fprintf (ascii_ostream, "%#18llx ", eoj->ac_jid);
        else
            fprintf (ascii_ostream, "%18.18s ", UNKNOWN);
        fprintf (ascii_ostream, "%8s ", BLANK);
        fprintf (ascii_ostream, "%8s ", BLANK);
        fprintf (ascii_ostream, "%8d ", eoj->ac_gid);
        fprintf (ascii_ostream, "%15.15s ", ctime (&(eoj->ac_etime)) + 4);
        fprintf (ascii_ostream, "%4s ", BLANK);
        fprintf (ascii_ostream, "\n");
        break;
        
    case ACCT_KERNEL_CFG:
        cfg = acctrec.cfg;

        switch (cfg->ac_event)
        {
        case AC_CONFCHG_BOOT:
            fprintf (ascii_ostream, "%-4d System boot time at %15.15s\n",
                     recnum, ctime (&(cfg->ac_boottime)) + 4);
            break;
        case AC_CONFCHG_FILE:
            fprintf (ascii_ostream, "%-4d Accounting file switched at "
                     "%15.15s, Kernel & Daemon mask = 0%05o, "
                     "Record mask = 0%05o\n",
                     recnum, ctime (&(cfg->ac_curtime)) + 4, cfg->ac_kdmask,
                     cfg->ac_rmask);
            break;
        case AC_CONFCHG_ON:
            fprintf (ascii_ostream, "%-4d Accounting started at "
                     "%15.15s, Kernel & Daemon mask = 0%05o, "
                     "Record mask = 0%05o\n",
                     recnum, ctime (&(cfg->ac_curtime)) + 4, cfg->ac_kdmask,
                     cfg->ac_rmask);
            break;
        case AC_CONFCHG_OFF:
            fprintf (ascii_ostream, "%-4d Accounting stopped at "
                     "%15.15s, Kernel & Daemon mask = 0%05o, "
                     "Record mask = 0%05o\n",
                     recnum, ctime (&(cfg->ac_curtime)) + 4, cfg->ac_kdmask,
                     cfg->ac_rmask);
            break;
            
        }
        break;
        
    case ACCT_DAEMON_WKMG:
        wmbs = acctrec.wmbs;

	if (wmbs->type == WM_CON)
        {
            fprintf (ascii_ostream, "%-4d Cons-Workld-Mgmt ", recnum);
            strcpy (name, uid_to_name (wmbs->uid));
            if (*name == '?')
                fprintf (ascii_ostream, "%-8d ", wmbs->uid);
            else
                fprintf (ascii_ostream, "%-8.8s ", name);
            if (wmbs->jid >= 0)
                fprintf (ascii_ostream, "%#18llx ", wmbs->jid);
            else
                fprintf (ascii_ostream, "%18.18s ", UNKNOWN);
            fprintf (ascii_ostream, "0x%016llx ", wmbs->ash);
            fprintf (ascii_ostream, "%8lld", wmbs->prid);
            fprintf (ascii_ostream, "  REQID=%lld", wmbs->reqid);
            fprintf (ascii_ostream, "  ARRAYID=%d", wmbs->arrayid);
            fprintf (ascii_ostream, "  PROV=%s", wmbs->serv_provider);
            fprintf (ascii_ostream, "  START=%.15s",
                     ctime (&(wmbs->start_time)) + 4);
            subtype = (wmbs->init_subtype < 0) ? UNKNOWN :
                WM_Init_subtype[wmbs->init_subtype - 1].type;
            fprintf (ascii_ostream, "  INIT=%s", subtype);
            subtype = (wmbs->term_subtype < 0) ? UNKNOWN :
                WM_Term_subtype[wmbs->term_subtype - 1].type;
            fprintf (ascii_ostream, "  TERM=%s", subtype);
            fprintf (ascii_ostream, "  REQ=%s", wmbs->reqname);
            if (output_cpu)
            {
                fprintf (ascii_ostream, "  QWAIT=%lld", wmbs->qwtime);
                fprintf (ascii_ostream, "  UTIME=%.4f",
                         TIME_2_SECS (wmbs->utime));
                fprintf (ascii_ostream, "  STIME=%.4f",
                         TIME_2_SECS (wmbs->stime));
            }
            fprintf (ascii_ostream, "\n");
        }
        else
        {
            fprintf (ascii_ostream, "%-4d Raw-Workld-Mgmt  ", recnum);
            strcpy (name, uid_to_name (wmbs->uid));
            if (*name == '?')
                fprintf (ascii_ostream, "%-8d ", wmbs->uid);
            else
                fprintf (ascii_ostream, "%-8.8s ", name);
            if (wmbs->jid >= 0)
                fprintf (ascii_ostream, "%#18llx ", wmbs->jid);
            else
                fprintf (ascii_ostream, "%18.18s ", UNKNOWN);
            fprintf (ascii_ostream, "0x%016llx ", wmbs->ash);
            fprintf (ascii_ostream, "%8lld", wmbs->prid);
            fprintf (ascii_ostream, "  REQID=%lld", wmbs->reqid);
            fprintf (ascii_ostream, "  ARRAYID=%d", wmbs->arrayid);
            fprintf (ascii_ostream, "  PROV=%s", wmbs->serv_provider);
            fprintf (ascii_ostream, "  START=%.15s",
                     ctime (&(wmbs->time)) + 4);
            if ((wmbs->enter_time > 0) && (wmbs->enter_time < time(NULL)))
            {
                fprintf (ascii_ostream, "  ENTER=%.15s",
                         ctime (&(wmbs->enter_time)) + 4);
            }
            fprintf (ascii_ostream, "  TYPE=%s",
                     WM_type[wmbs->type - 1].type);
            fprintf (ascii_ostream, "  SUBTYPE=%s", 
                     WM_type[wmbs->type - 1].sub[wmbs->subtype - 1].type);
            fprintf (ascii_ostream, "  MACH=%s", wmbs->machname);
            fprintf (ascii_ostream, "  REQ=%s", wmbs->reqname);
            fprintf (ascii_ostream, "  QUE=%s", wmbs->quename);
            if (output_cpu)
            {
                fprintf (ascii_ostream, "  UTIME=%.4f",
                         TIME_2_SECS (wmbs->utime));
                fprintf (ascii_ostream, "  STIME=%.4f",
                         TIME_2_SECS (wmbs->stime));
            }
            fprintf (ascii_ostream, "\n");
        }
        break;

    case ACCT_JOB_HEADER:
        job = acctrec.job;

        fprintf (ascii_ostream, "%-4d Job-Header       ", recnum);
        strcpy (name, uid_to_name (job->aj_uid));
        if (*name == '?')
            fprintf (ascii_ostream, "%-8d ", job->aj_uid);
        else
            fprintf (ascii_ostream, "%-8.8s ", name);
        if (job->aj_jid >= 0)
            fprintf (ascii_ostream, "%#18llx ", job->aj_jid);
        else
            fprintf (ascii_ostream, "%18.18s ", UNKNOWN);
        fprintf (ascii_ostream, "%18s ", BLANK);
        fprintf (ascii_ostream, "%8s ", BLANK);
        fprintf (ascii_ostream, "%8s ", BLANK);
        fprintf (ascii_ostream, "%15.15s\n", ctime (&(job->aj_btime)) + 4);
        break;
        
    case ACCT_KERNEL_SITE0:
    case ACCT_KERNEL_SITE1:
        fprintf (ascii_ostream, "%-4d Site Kernel\n", recnum);
        break;
                
    case ACCT_DAEMON_SITE0:
    case ACCT_DAEMON_SITE1:
        fprintf (ascii_ostream, "%-4d Site Daemon\n", recnum);
        break;
        
    default:
        acct_err(ACCT_ABORT,
               _("Accounting file '%s' contains invalid records."),
        	pacct);
        break;
    }

    return;
}


/*
 *  Compare routine for sorting the record list by ascending record number.
 */
int rec_sort (const void *e1, const void *e2)
{
    int  *n1;
    int  *n2;

    n1 = (int *) e1;
    n2 = (int *) e2;

    if (*n1 < *n2)
        return (-1);

    return (1);
}
