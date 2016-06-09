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
 * csaverify.c:
 *	Check accounting records for valid data.
 *	Mostly, this proves that the file can be read by the appropriate
 *	read routine and that the record headers are intact and valid.
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

#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "csa.h"
#include "csaacct.h"

#define HDR_SIZE     sizeof (struct achead)

#define VALID		0	/* if verification succeeds */
#define INVALID		1	/* if verification fails */

/* Return values for check_last_read_rec() and verify_primary().
   verify_primary() also uses VALID (0), which is defined in acctdef.h.
*/
#define ERR_CONTINUE  2
#define ERR_QUIT      3

extern int  db_flag;          /* Debug flag */
extern int  rev_tab[];

char     *Prg_name = "";

int       off_or_file = FALSE;  /* Indicates whether the just read record is a
                                   config OFF or switch FILE record. */
int64_t   curr_loc = 0;         /* Current file byte position. */
int       is_spacct = FALSE;    /* Indicates whether the input file is a
                                   sorted pacct file. */
int	  is_recycled = FALSE;	/* Indicates whether the input file is a
				   recycled pacct file. */
char     *level = "";           /* Debug level in character string. */
int       maxbad = -1;          /* Max # bad records to report - default is to
                                   report all. */
int       nbad = 0;             /* Number of bad records found. */
int       nbad_error = FALSE;   /* Indicates whether MAX bad records has been
                                   exceeded. */
int       ngood = 0;            /* Number of good records found. */
char     *outfile = NULL;       /* Output file name where info. about invalid
                                   records is written - byte offset & length.
                                */
FILE     *outfile_stream = NULL;/* Stream associated with the output file. */
char     *pacct = WPACCT;       /* Accounting file name. */
int       pacct_fd;             /* Accounting file descriptor. */
int64_t   prev_loc = 0;         /* Previous file byte position. */
int64_t   prevprev_loc = 0;     /* Previous to previous byte position. */
int       verbose = FALSE;      /* Verbose report format option. */

char   acctentbuf[ACCTENTSIZ];  /* Accounting record buffer. */
struct acctent  acctrec = {acctentbuf};

void prc_pacct();


void usage()
{
    acct_err(ACCT_ABORT,
           "Command Usage:\n%s\t[-P pacct] [-o outfile] [-m nrec] [-v] [-D]",
    	Prg_name);
}


/*
 *  Main program.
 */
main (int argc, char **argv)
{
    extern char  *optarg;
    extern int    optind;
    
    int c;
    char *cptr;

    Prg_name = argv[0];

    while ((c = getopt (argc, argv, "P:o:m:vD")) != EOF)
    {
        switch ((char) c)
        {
        case 'P':
            pacct = optarg;
	    /* strlen of optarg can not be 0 */
	    /* csarecy appends a '0' to pacct to indicate a recycled file */
	    /* cptr points to the last byte of pacct */
	    cptr = pacct + strlen(pacct) - 1;
	    if (*cptr == '0')
		is_recycled = TRUE;
            break;
        case 'o':
            outfile = optarg;
            break;
        case 'm':
            maxbad = atoi (optarg);
            break;       
        case 'v':
            verbose = TRUE;
            break;
        case 'D':
            db_flag = 1;
            fprintf (stdout, "Debugging option set to level %d.\n", db_flag);
            break;
        default:
            usage ();
        }
    }

    if (optind < argc)
        acct_err(ACCT_ABORT,
               _("Extra arguments were found on the command line."),
        	Prg_name);
    
    /* Process the pacct (or sorted pacct) file. */
    prc_pacct ();

    /* Close off files. */
    closeacct (pacct_fd);
    if (outfile_stream != NULL)
        fclose (outfile_stream);
    
    if (nbad_error)
        exit (2);
    if (nbad)
        exit (1);

    exit (0);
}


/*
 *  A configuration record was detected.  Verify the state change.
 */
int config_record (int64_t loc)
{
    static int  state = -1;

    /* NOTE:  ctime() already puts a '\n' at the end of the string. */
    switch (acctrec.cfg->ac_event)
    {
    case AC_CONFCHG_BOOT:
        if (db_flag)
        {
            fprintf (stdout, "(1): Config(BOOT) record found:\n");
            fprintf (stdout, "    System boot time at %s\n",
                     ctime (&(acctrec.cfg->ac_boottime)));
        }
        break;

    case AC_CONFCHG_FILE:
        if (db_flag)
        {
            fprintf (stdout, "(1): Config(FILE) record found:\n");
            fprintf (stdout, "    System boot time at %s",
                     ctime (&(acctrec.cfg->ac_boottime)));
            fprintf (stdout, "    Pacct file switched at %s",
                     ctime (&(acctrec.cfg->ac_curtime)));
            fprintf (stdout, "    Kernel & Daemon mask = 0%05o, "
                     "Record mask = 0%05o\n\n",
                     acctrec.cfg->ac_kdmask, acctrec.cfg->ac_rmask);
        }

        /* When the user does a file switch, csaswitch turns on accounting
           if it's not on.  However, since the AC_CONFCHG_FILE record is the
           last record in the file, theoretically, we don't need to set state
           to ACS_ON here.
         */
        state = ACS_ON;
        off_or_file = TRUE;
        break;
        
    case AC_CONFCHG_ON:
        if (db_flag)
        {
            fprintf (stdout, "(1): Config(ON) record found:\n");
            fprintf (stdout, "    System boot time at %s",
                     ctime (&(acctrec.cfg->ac_boottime)));
            if ((state == -1) && (loc > 0) && !is_spacct)
            {
                /* First record of a regular (unsorted) pacct file should be
                   an ON configuration record. */
                fprintf (stdout, "    First accounting record was not an ON "
                         "configuration record.\n");
            }
            fprintf (stdout, "    Accounting turned on at %s",
                     ctime (&(acctrec.cfg->ac_curtime)));
            fprintf (stdout, "    Kernel & Daemon mask = 0%05o, "
                     "Record mask = 0%05o\n\n",
                     acctrec.cfg->ac_kdmask, acctrec.cfg->ac_rmask);
        }

        state = ACS_ON;
        break;

    case AC_CONFCHG_OFF:
        if (db_flag)
        {
            fprintf (stdout, "(1): Config(OFF) record found:\n");
            fprintf (stdout, "    System boot time at %s",
                     ctime (&(acctrec.cfg->ac_boottime)));
            if (state == -1)
            {
                fprintf (stdout, "    No accounting ON configuration record "
                         "found.\n");
            }
            fprintf (stdout, "    Accounting turned off at %s",
                     ctime (&(acctrec.cfg->ac_curtime)));
            fprintf (stdout, "    Kernel & Daemon mask = 0%05o, "
                     "Record mask = 0%05o\n\n",
                     acctrec.cfg->ac_kdmask, acctrec.cfg->ac_rmask);
        }

        state = ACS_OFF;
        off_or_file = TRUE;
        break;

    default:
        break;
    }

    return (0);
}


/*
 *  An error has been detected - output the bad record's byte offset in the
 *  file and its length.
 */
void err_offset (int64_t loc, int64_t length)
{
    int             offset;  /* Current offset in the output file. */
    static int64_t  old_loc = -1;
    static int      old_write_length = 0;
    
    fprintf (stdout, "  byte offset: %#llo (%lld), length: %lld\n", loc, loc,
             length);
    if (outfile != NULL)
    {
        /* Sometimes a record is printed out as an error twice.  The first
           time is due to some error in the record.  The second time is an
           "Incomplete record" error.  This means the record wasn't completely
           written out to the pacct file.  We want to write the length of the
           second error to the error file because that is the actual length of
           the incomplete record.
        */
        if (loc == old_loc)
        {
            if ((offset = ftell (outfile_stream)) < 0)
            {
                /* IO_SEEK is used here because IO_TELL is not defined. */
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	outfile);
            }
            if (ftruncate (fileno(outfile_stream), offset - old_write_length)
                < 0)
            {
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the truncation of file '%s'."),
                	outfile);
            }
            /* Have to reposition the file pointer here.  ftruncate() doesn't
               do it. */
            if (fseek (outfile_stream, -old_write_length, SEEK_CUR) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	outfile);
        }
        
        old_write_length = fprintf (outfile_stream, "%#llo (%lld), %lld\n",
                                    loc, loc, length);
        if (old_write_length < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the writing of file '%s'."),
            	outfile);
        old_loc = loc;
    }
    
    return;
}


/*
 *  Print number of good records found.
 */
void print_ngood ()
{
    char  *record;
    
    record = (ngood > 1) ? "records" : "record";
    fprintf (stdout, "%sFound %d good %s in file '%s'.\n", level, ngood,
             record, pacct);
    ngood = 0;
}


/*
 *  Process an invalid record.
 */
int invalid_record (char *err_str, int64_t offset, int64_t length)
{

    /* Output error message and file position information. */
    if (verbose)
        print_ngood ();
    fprintf (stdout, "%s: File '%s':%s\n", Prg_name, pacct, err_str);
    err_offset (offset, length);
    nbad++;

    /* Check if MAX bad. */
    if (maxbad != -1 && nbad >= maxbad)
    {
        if (verbose)
        {
            fprintf (stdout,
		_("%s: Reached the max number of bad records (%d) -\nignoring the rest of the file '%s'."),
		  Prg_name, maxbad, pacct);
        }
        nbad_error = TRUE;
    }

    fprintf (stdout, "\n");
    fflush (stdout);
    
    return (nbad_error);
}


/*
 *  Check a header word to see if it is a primary accounting record.
 */
hdr_retval check_primary_hdr (struct achead *hdr)
{
    int  size;
    
    if (hdr->ah_magic != ACCT_MAGIC)
        return (BAD_MAGIC);

    switch (hdr->ah_type)
    {
    case ACCT_KERNEL_CSA:
        size = sizeof (struct acctcsa);
        break;
    case ACCT_KERNEL_SOJ:
        size = sizeof (struct acctsoj);
        break;
    case ACCT_KERNEL_EOJ:
        size = sizeof (struct accteoj);
        break;
    case ACCT_KERNEL_CFG:
        size = sizeof (struct acctcfg);
        break;
    case ACCT_DAEMON_WKMG:
        size = sizeof (struct wkmgmtbs);
        break;
    case ACCT_JOB_HEADER:
        size = sizeof (struct acctjob);
        break;
    case ACCT_KERNEL_SITE0:
    case ACCT_KERNEL_SITE1:

    case ACCT_DAEMON_SITE0:
    case ACCT_DAEMON_SITE1:
        size = 0;  /* We don't know the size of these record types. */
        break;
    default:
        return (BAD_TYPE);
    }

    if (hdr->ah_revision != rev_tab[hdr->ah_type])
        return (BAD_REVISION);
	
    if (size > 0 && hdr->ah_size != size)
        return (BAD_SIZE);

    return (HDR_NO_ERROR);
}


/*
 *  Starting at the current position, forward one byte at a time to search
 *  for the header of a primary accounting record.
 */
int find_primary_hdr ()
{
    char  hdr[HDR_SIZE];

    while (readacct (pacct_fd, hdr, HDR_SIZE) == HDR_SIZE)
    {
        if (check_primary_hdr ((struct achead  *) hdr) == HDR_NO_ERROR)
            return TRUE;
        if (seekacct (pacct_fd, -(HDR_SIZE - 1), SEEK_CUR) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
    }
    
    return FALSE;
}


/*
 *  Check the last read record to see if it contains the header of a primary
 *  accounting record.
 *
 *  Algorithm:
 *      rewind to 1 byte past the beginning of the last read record
 *      forward one byte at a time to search for a header of a primary record
 *      IF the found primary header is in the last read record THEN
 *          flag the last read record as incomplete
 *      ELSE (the found primary header is not in the last read record or if
 *            no primary header is found)
 *          flag as bad the section from the end of the last read record to
 *          the beginning of the found primary header (or the end of file)
 *      ENDIF
 */
int check_last_read_rec (char *err_str)
{
    int64_t  offset;
    int      retval = ERR_CONTINUE;
    
    if (err_str == NULL)
        err_str = " Unrecognizable data.";
    
    /* Rewind to 1 byte past the beginning of the last read record. */
    if (seekacct (pacct_fd, prevprev_loc + 1, SEEK_SET) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the positioning of file '%s'."),
        	pacct);
    if (find_primary_hdr ())  /* Found a primary header. */
    {
        /* Rewind to the beginning of the found primary header. */
        if ((offset = seekacct (pacct_fd, -HDR_SIZE, SEEK_CUR)) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
        if (offset >= prev_loc)
        {
            if (invalid_record (err_str, prev_loc, offset - prev_loc))
                retval = ERR_QUIT;
        }
        else  /* Last read record is not good. */
        {
            prev_loc = prevprev_loc;
            if (invalid_record (" Incomplete record.", prevprev_loc,
                                offset - prevprev_loc))
            {
                retval = ERR_QUIT;
            }
        }
    }
    else  /* Did not find a primary header - must have reached the */
    {     /* end of the file. */
        /* We don't care if the max number of bad records has been reached
           because we are aborting anyway.  Therefore, we don't bother check
           the return value of invalid_record().
        */
        invalid_record (err_str, prev_loc, END_OF_FILE);
        retval = ERR_QUIT;
    }

    return (retval);
}


/*
 *  Do additional verification on a primary record.
 */
int verify_primary ()
{
    int       cant_be_primary = FALSE;
    char      err_str[180];
    int       illegally_linked = FALSE;
    int64_t   offset;
    char     *record;
    int       retval = VALID;

    err_str[0] = '\0';

    switch (acctrec.prime->ah_type)
    {
    case ACCT_KERNEL_CSA:
        if (acctrec.soj != NULL ||
            acctrec.eoj != NULL ||
            acctrec.cfg != NULL ||
            acctrec.job != NULL ||
            acctrec.wmbs != NULL ||
            acctrec.other != NULL)
        {
            illegally_linked = TRUE;
            record = "'CSA'";
        }
        break;
    case ACCT_KERNEL_MEM:
        cant_be_primary = TRUE;
        record = "'Mem'";
        break;
    case ACCT_KERNEL_IO:
        cant_be_primary = TRUE;
        record = "'I/O'";
        break;
    case ACCT_KERNEL_DELAY:
        cant_be_primary = TRUE;
        record = "'Delay'";
        break;
    case ACCT_KERNEL_SOJ:
        if (acctrec.prime->ah_flag & AMORE)
        {
            illegally_linked = TRUE;
            record = "'SOJ'";
        }
        break;
    case ACCT_KERNEL_EOJ:
        if (acctrec.prime->ah_flag & AMORE)
        {
            illegally_linked = TRUE;
            record = "'EOJ'";
        }
        break;        
    case ACCT_KERNEL_CFG:
        /* NOTE - The recycled pacct file (pacct0) is supposed to be a
           regular (unsorted) pacct file, but it has the AC_CONFCHG_BOOT
           records.  It needs these records for the uptime information.  If
           pacct0 is ever changed to be a sorted pacct file in the future,
           then this check should be put back in.
        */
        /*
        if (!is_spacct && acctrec.cfg->ac_event == AC_CONFCHG_BOOT)
        {
            strcpy (err_str, " Boot config record is not allowed in a\n"
                    "regular (unsorted) pacct file.");
        }
        else if (acctrec.prime->ah_flag & AMORE)
        */
        if (acctrec.prime->ah_flag & AMORE)
        {
            illegally_linked = TRUE;
            record = "'CFG'";
        }
        else
            config_record (prev_loc);
        break;
    case ACCT_DAEMON_WKMG:
        if (is_spacct && acctrec.wmbs->type != WM_CON)
        {
            strcpy (err_str, " Pre-processed workload management record is\n"
                    "not allowed in a sorted pacct file.");
        }
        else if (!is_spacct && acctrec.wmbs->type == WM_CON && !is_recycled)
        {
            strcpy (err_str, " Consolidated workload management record is\n"
                    "not allowed in a regular (unsorted) pacct file.");
        }
        else if (acctrec.prime->ah_flag & AMORE)
        {
            illegally_linked = TRUE;
            record = "'Wkmg'";
        }
        break;
    case ACCT_JOB_HEADER:
        if (!is_spacct)  /* Regular (unsorted) pacct file */
        {
            strcpy (err_str, " Job header record is not allowed in a\n"
                    "regular (unsorted) pacct file.");
        }
        else if (acctrec.prime->ah_flag & AMORE)
        {
            illegally_linked = TRUE;
            record = "'Job-hdr'";
        }
        break;
    default:
        break;
    }
    
    if (cant_be_primary == TRUE)
    {
        sprintf (err_str, _(" Record %s cannot be a primary record."), record);
        retval = check_last_read_rec (err_str);
    }
    else if (illegally_linked == TRUE)
    {
        /* Rewind to 1 byte past the beginning of the just read record. */
        if (seekacct (pacct_fd, prev_loc + 1, SEEK_SET) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
        
        if (find_primary_hdr ())  /* Found a primary header. */
        {
            /* Rewind to the beginning of the found primary header. */
            if ((offset = seekacct (pacct_fd, -HDR_SIZE, SEEK_CUR)) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	pacct);
            if (offset >= curr_loc)
            {
                sprintf (err_str,
		       _(" Record %s has an illegally linked record."),
		         record);
            }
            else  /* The found "primary" header must be the one that was */
            {     /* illegally linked. */
                sprintf (err_str,
		       _(" Record %s erroneously indicates that there\nis more linked record."),
		         record);
            }
            if (invalid_record (err_str, prev_loc, offset - prev_loc))
                retval = ERR_QUIT;
            else
                retval = ERR_CONTINUE;
        }
        else  /* Did not find a primary header - must have reached the */
        {     /* end of the file. */
            /* We don't care if the max number of bad records has been reached
               because we are aborting anyway.  Therefore, we don't bother
               check the return value of invalid_record().
            */
            sprintf (err_str, _(" Record %s has an illegally linked record."),
                     record);
            invalid_record (err_str, prev_loc, END_OF_FILE);
            retval = ERR_QUIT;
        }
    }
    else if (err_str[0] != '\0')  /* Illegal record is found with respect */
    {                             /* to file type (pacct vs. spacct). */
        if (invalid_record (err_str, prev_loc, curr_loc - prev_loc))
            retval = ERR_QUIT;
        else
            retval = ERR_CONTINUE;
    }

    return (retval);
}


/*
 *  Process the pacct file.
 *
 *  Algorithm:
 *      Read the first header
 *      IF this is the header of a primary record THEN
 *          IF this is a AC_CONFCHG_BOOT configuration record THEN
 *              flag that this is a sorted pacct file
 *          ENDIF
 *          rewind to the beginning of file
 *      ELSE
 *          start from the beginning of file, forward one byte at a time to
 *          search for the header of a primary record
 *          IF found THEN
 *              inform user of offset
 *          ENDIF
 *          abort
 *      ENDIF
 *
 *      WHILE readacctent() DO
 *          IF readacctent() returned an error THEN
 *              call check_last_read_rec()
 *          ELSE
 *              call the validation routine to verify the different fields of
 *              the just read record
 *              IF there was a header error THEN
 *                  call check_last_read_rec()
 *              ELSEIF there was some other error THEN
 *                  flag the just read record as bad
 *              ELSE (no error from the validation routine)
 *                  IF the primary record is not legal THEN
 *                      call check_last_read_rec()
 *                  ELSEIF the primary record has illegally linked record(s)
 *                  THEN
 *                      rewind to 1 byte past the beginning of the just read
 *                      record
 *                      forward one byte at a time to search for a header of a
 *                      primary record
 *                      flag as bad the section from the beginning of the just
 *                      read record to the beginning of the found primary
 *                      header (or the end of file if no primary header is
 *                      found)
 *                  ELSE (illegal record is found with respect to file type -
 *                        pacct vs. spacct)
 *                      flag the just read record as illegal
 *                  ENDIF
 *              ENDIF
 *          ENDIF
 *      ENDWHILE
 */
void prc_pacct()
{
    char            err_str[180];
    struct achead  *hdr;
    int64_t         offset;
    int             retval;
    int             retval2;
    int             retval3;
    int             remain_size;
    int             type;

    if (db_flag)
    {
        level = "(1): ";

        fprintf (stdout, "(1): %s: pacct(csa) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct acctcsa));
        fprintf (stdout, "(1): %s: pacct(mem) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct acctmem));
        fprintf (stdout, "(1): %s: pacct(io) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct acctio));
        fprintf (stdout, "(1): %s: pacct(delay) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct acctdelay));
        
        fprintf (stdout, "(1): %s: pacct(soj) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct acctsoj));
        fprintf (stdout, "(1): %s: pacct(eoj) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct accteoj));
        fprintf (stdout, "(1): %s: pacct(cfg) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct acctcfg));
        fprintf (stdout, "(1): %s: spacct(jobH) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct acctjob));
		
        fprintf (stdout, "(1): %s: pacct(wkmgmt) record is %ld bytes in "
                 "length.\n", Prg_name, sizeof (struct wkmgmtbs));
        fprintf (stdout, "\n");
        fflush (stdout);
    }

    /* Open the pacct and output files. */
    if ((pacct_fd = openacct (pacct, "r")) < 0)
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the opening of file '%s'."),
        	pacct);
    if ((outfile != NULL) &&
        ((outfile_stream = fopen (outfile, "w")) == NULL))
    {
        acct_perr(ACCT_ABORT, errno,
        	_("An error occurred during the opening of file '%s'."),
        	outfile);
    }
    
    /* Process the pacct file. */
    if (readacct (pacct_fd, acctrec.ent, HDR_SIZE) != HDR_SIZE)
    {
        fprintf (stdout, _("%s: File '%s': Cannot read the first record.\n"),
                 Prg_name, pacct);
        fflush (stdout);
        nbad++;
        return;
    }
    
    hdr = (struct achead *) acctrec.ent;
    if (check_primary_hdr (hdr) == HDR_NO_ERROR)
    {
        if (check_hdr (hdr, err_str, ACCT_KERNEL_CFG, sizeof (struct acctcfg))
            == HDR_NO_ERROR)
        {
            remain_size = hdr->ah_size - HDR_SIZE;
            if (readacct (pacct_fd, acctrec.ent + HDR_SIZE, remain_size) !=
                remain_size)
            {
                fprintf (stdout,
		       _("%s: File '%s': Cannot read the first record.\n"),
		         Prg_name, pacct);
                fflush (stdout);
                nbad++;
                return;
            }
            if (((struct acctcfg *) acctrec.ent)->ac_event == AC_CONFCHG_BOOT)
            {
                is_spacct = TRUE;
                if (db_flag)
                {
                    fprintf (stdout, "File '%s' is a sorted pacct file.\n\n",
                             pacct);
                }
            }
        }       
        if (!is_spacct)
        {
            if (db_flag)
            {
                fprintf (stdout, "File '%s' is a regular (unsorted) pacct "
                         "file.\n\n", pacct);
            }
        }
        /* Rewind to the beginning. */
        if (seekacct (pacct_fd, 0, SEEK_SET) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
    }
    else
    {
        /* Rewind to 1 byte past the beginning of the file. */
        if (seekacct (pacct_fd, 1, SEEK_SET) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
        if (find_primary_hdr ())  /* Found a primary header. */
        {
            /* Rewind to the beginning of the found primary header. */
            if ((offset = seekacct (pacct_fd, -HDR_SIZE, SEEK_CUR)) < 0)
                acct_perr(ACCT_ABORT, errno,
                	_("An error occurred during the positioning of file '%s'."),
                	pacct);
            fprintf (stdout,
		_("%s: File '%s': The first %lld bytes contain unrecognizable\ndata.  Please delete that section of the file and invoke csaverify again.\n"),
                     Prg_name, pacct, offset);
        }
        else  /* Did not find a primary header. */
        {
            fprintf (stdout, _("%s: File '%s' contains unrecognizable data.\n"),
                     Prg_name, pacct);
        }
        fflush (stdout);
        nbad++;
        return;
    }
		
    while (((retval = readacctent (pacct_fd, &acctrec, FORWARD)) != 0) &&
           (retval >= -1))
    {
        off_or_file = FALSE;
        if ((curr_loc = seekacct (pacct_fd, 0, SEEK_CUR)) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
        
        if (retval == -1)  /* readacctent() did not recognize the header. */
        {
            if (check_last_read_rec (NULL) == ERR_QUIT)
                return;
        }
        else if ((retval2 = invalid_pacct (&acctrec, err_str)) !=
                 INV_NO_ERROR)
        {
            if (retval2 == HEADER_ERROR)
            {
                if (check_last_read_rec (err_str) == ERR_QUIT)
                    return;
            }
            else  /* retval2 == OTHER_ERROR */
            {
                if (invalid_record (err_str, prev_loc, curr_loc - prev_loc))
                    return;
            }
        }
        else
        {
            if ((retval3 = verify_primary ()) == ERR_QUIT)
                return;
            if (retval3 == VALID)
                ngood++;
        }

        prevprev_loc = prev_loc;
        /* It's very important that prev_loc is set to the location returned
           by seekacct and not set to curr_loc because the position pointer
           in the file may have been reset by the validation routines. */
        if ((prev_loc = seekacct (pacct_fd, 0, SEEK_CUR)) < 0)
            acct_perr(ACCT_ABORT, errno,
            	_("An error occurred during the positioning of file '%s'."),
            	pacct);
    }  /* end of while (readacctent) */

    if ((retval == -2) || (retval == -3))  /* readacctent() could not read */
    {                                      /* the record. */
        if (verbose)
            print_ngood ();
        fprintf (stdout,
	       _("An error occurred during the reading of file '%s'."),
	         pacct);
	fprintf (stdout, "\n");
        err_offset (prev_loc, END_OF_FILE);
        nbad++;
    }
    else if (db_flag && !is_spacct && !off_or_file)
    {
        /* Last record of a regular (unsorted) pacct file should be an OFF
           or switch FILE configuration record. */
        fprintf (stdout, "(1): Last accounting record was not an OFF or "
                 "switch FILE configuration record.\n");
    }
    if (verbose && db_flag)
        print_ngood ();
    fflush (stdout);

    return;
}
