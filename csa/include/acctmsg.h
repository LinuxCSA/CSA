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
#ifndef	_ACCTMSG_H
#define	_ACCTMSG_H

/*
 * Gettext definitions
 */

#include <libintl.h>
#define _(String) (String)
#define N_(String) (String)
#define textdomain(Domain)
#define bindtextdomain(Package, Directory)

/*
 * General message system definitions.
 */

#define MAX_ERR_LEN	80	/* Length of error message line */
#define	MAX_ERR_LNS	10	/* Number of lines in message buffer */

#define	ACCT_BUFL	MAX_ERR_LEN*MAX_ERR_LNS

#define	ACCT_GRP	"acct.cat" /* Message system group name */

typedef	enum
{
	ACCT_INFO,	/* Informational error messages */
	ACCT_EFFY,	/* Efficiency error messages */
	ACCT_CAUT,	/* Caution error messages */
	ACCT_WARN,	/* Warning error messages */
	ACCT_FATAL,	/* Fatal w/o abort error messages */
	ACCT_ABORT,	/* Fatal w/ abort error messages */
	ACCT_MAX
} accterr;


/*
 * Error message system routine definitions.
 */
extern void	acct_err(accterr, char*, ...);
extern void	acct_perr(accterr, int, char*, ...);

extern void	Ndebug(char*, ...);
extern void	Nerror(char*, ...);

/*
 * I/O message system definitions (replaced with gettext).
 */
#define IO_OPEN		"An error occurred during the opening of file '%s'."
#define IO_STAT		"An error occurred getting the status of file '%s'."
#define IO_READ		"An error occurred during the reading of file '%s'."
#define IO_READ1	"An error occurred during the reading of file '%s' %s."
#define IO_WRITE	"An error occurred during the writing of file '%s'."
#define IO_WRITE1	"An error occurred during the writing of file '%s' %s."
#define IO_SEEK		"An error occurred during the positioning of file '%s'."
#define IO_SEEK1	"An error occurred during the positioning of file '%s' %s."
#define IO_REMOVE	"An error occurred during the removal of file '%s'."
#define IO_TRUNC	"An error occurred during the truncation of file '%s'."
#define IO_CREATE	"An error occurred creating file '%s'."
#define IO_GID		"An error occurred changing the group on file '%s' to '%s'."
#define IO_PRID		"An error occurred changing the project ID on file '%s'."
#define IO_MODE		"An error occurred changing the mode on file '%s' to '%o'."
#define IO_OWNER	"An error occurred changing the owner on file '%s' to '%s'."
#define IO_OPENDIR	"An error occurred during the opening of directory '%s'."

#define IO_NOTREGULAR	"File '%s' exists, but it is not a regular file."
#define IO_LINK		"An error occurred linking file '%s'."
#define IO_UNLINK	"An error occurred unlinking file '%s'."

/*
 * Memory message system definitions.
 */
#define MEM_ALLOC	"There was insufficient memory available when allocating '%s'."
#define MEM_ALLOC1	"%s: There was insufficient memory available when allocating '%s'."
#define MEM_REALLOC	"There was insufficient memory available when reallocating '%s'."
#define MEM_INT		"An invalid value for MEMINT (%d) was found in the configuration file.\n  MEMINT must be between 1 and 3."

/*
 * Parameter message system definitions.
 */
#define PM_ASH		"An unknown array session handle '%d' was given on the -a parameter."
#define PM_GID		"An unknown Group name '%s' was given on the -g parameter."
#define PM_PRID		"An unknown Project name '%s' was given on the -p parameter."
#define PM_UID		"An unknown User name '%s' was given on the -u parameter."
#define PM_OPTCNT	"Extra arguments were found on the command line."
#define PM_NOOPT	"An unknown option '%s' was specified."
#define PM_OPTNUM	"Option '%s' requires a numeric argument."
#define PM_OPTPLUS	"Option '%s' requires a positive numeric argument."
#define PM_OPTDIR	"The file argument '%s' must be a directory."
#define PM_OPTPATH	"The path name '%s' is longer than the maximum (%d) allowed."
#define PM_OPTINPUT	"The input file was not specified."
#define PM_OPTMISS	"A required option is missing from the command line.\n   Please specify one of the %s options."

#define PM_CALL0	"An error was returned from routine '%s'."
#define PM_CALL1	"An error was returned from routine '%s' for the (-%s) option '%s'."
#define PM_OPTINV	"An invalid option (-%s) was found on the control statement."
#define PM_OPTNORPT	"The (-%s) option cannot be selected when issuing a report."
#define PM_OPTEXC	"The (-%s) and the (-%s) options are mutually exclusive."
#define PM_OPTINC	"The (-%s) option must be used with the (-%s) option."
#define PM_OPTARG	"The (-%s) option's argument, '%s', is invalid."
#define PM_OPTPRT	"The (-%s) option requires another print option."
#define PM_OPTNPRT	"The (-%s) option may not be used with other print options."
#define PM_OPTONE	"The (-%s) option must be used by itself."
#define PM_OPTDUP	"The (-%s) option was specified more than once."
#define PM_OPTSEL	"The (-%s) option can be used with only one of the (%s) options."
#define PM_OPTRPT	"The (-%s) option must be used with a report option."
#define PM_OPTINV1	"Some invalid options were used with the (-%s) option."
#define PM_OPTINT	"The (-%s) option is only valid in interactive mode."
#define PM_ARGREQ	"Option -%s requires an argument."
#define PM_GETOPT	"An unknown/unsupported getopt() error occurred."
#define PM_DUPARG	"Argument '%s' overrides a previous one given for option -%s."
#define PM_NOTUSED	"Option -%s is not used with command line as specified."
#define PM_OPTREQ	"Option -%s is a required option."
#define PM_OPTREQ1	"Option -%s is required given the command line as specified."

/*
 * Library message system definitions.
 */
#define LB_CONFIG	"%s: The configuration file '%s' is empty."
#define LB_PARAM	"%s: The '%s' parameter must be added to the configuration file."
#define LB_NULLPARAM	"%s: A parameter requested of the config() routine was null."
#define LB_PARAMERR	"%s: Parameter token found on line %d starts with a non-alpha character."
#define LB_NOPARAM	"%s: The '%s' parameter is missing from configuration file.\n    Please add '%s' to the configuration file."
#define LB_NOPARAMN	"%s: The '%s' %s name is missing from configuration file.\n    Please add the '%s' %s name to the configuration file."
#define LB_NOPARAMV	"%s: The '%s' %s value is missing from configuration file.\n    Please add the '%s' %s value to the configuration file."
#define LB_NOPARAMVS	"%s: The '%s' values are missing from configuration file.\n   Please add the %s values to the configuration file."
#define LB_DEVICE	"%s: The '%s' device name should be surrounding by double quotation marks."

#define LB_PRIME	"%s: Coding error: hp->h_sec (%d) has an invalid value."
#define LB_HOLTBL	"%s: The holidays table setup failed.\n   The default values will be used for the erroneous entry and all\n   other entries that follow in the holidays file."
#define LB_HOLUPD	"%s: ***UPDATE %s WITH NEW HOLIDAYS***\n   The dates in the file are not for the current year."
#define LB_HOLYEAR	"%s: An invalid year (%d) was specified in the holidays file.\n   Valid years are 1970 to 1999."
#define LB_HOLPARM	"%s: There must be 3 parameters in the holiday file on the following line:\n%s."
#define LB_HOLPRIME	"%s: The prime/nonprime time is specified more than once for '%s'."
#define LB_HOLDAY	"%s: There is an invalid day (%d) specification in the holiday file."
#define LB_HOLDAYS	"%s: The %d prime/nonprime days must be specified (weekday, Saturday, Sunday) in the holiday file.\n   Only %d days were found in the holidays file."
#define LB_HOLNODAY	"%s: A invalid <day prime_time nonprime_time> line was\n   specified in the file '%s'."
#define LB_HOLNOYEAR	"%s: A invalid <year prime_time nonprime_time> line was\n   specified in the file '%s'."
#define LB_HOLCNT	"%s: There are too many holiday dates in the holidays file.\n   Increase the value of NHOLIDAY in the configuration file."
#define LB_HOLDATE	"%s: A invalid day (%d) of the year was specified in the holiday file.\n  Valid days are 1 to 365 or 366 in a leap year.\n  %s"
#define LB_HOLTIME	"%s: An invalid prime(%d) or nonprime(%d) time was specified.\n   Valid times are 0000 to 2359."
#define LB_HOLPSTART	"%s: An invalid prime start time (%d) was specified in the holidays file."
#define LB_HOLVALUE	"%s: The '%s' value cannot be specified in the holidays file with\n  a numeric time on the same line."
#define LB_HOLTWICE	"%s: The '%s' value cannot be specified in the holidays file twice\n  on the same line."
#define LB_HOLCODE	"%s: An error was detected when '%s' was specified as the\n   nonprime start time.  This is probably caused by a coding error."
#define LB_HOLSTART	"%s: An invalid prime start time (%s) or\n   nonprime start time (%s) was specified in the holidays file."
#define LB_HOLNPSTART	"%s: An invalid nonprime start time (%d) was specified in the holidays file."

#define LB_CALL		"%s: An error was returned from the '%s' routine."
#define LB_CALL1	"%s: An error was returned from the '%s' routine for a '%s' file."
#define LB_CALL2	"%s: An error was returned from the '%s' routine for a pacct %s record."

#define LB_HOLDAY1	"%s: There is an invalid day (%d) specification for month (%d) in the holiday file.\n  %s"
#define LB_HOLMONTH	"%s: There is an invalid month (%d) specification in the holiday file.\n  %s"
#define LB_HOLLINE	"%s: There is an invalid line format in the holiday file.\n   %s"

#define LB_BUFFER	"%s: An entry buffer was not provided or was misaligned."
#define LB_SEEKBOD	"%s: The file was not positioned at the beginning of an entry."
#define LB_SEEKEOD	"%s: The file was not positioned at the end of an entry."
#define LB_NOCONT	"%s: An unexpected entry continuation was found."
#define LB_RECERR	"%s: Records of the same type (%d) are not contiguous in the entry."

#define LB_NOREC1	"%s: An unknown record type (%#o) was found with flag (%d)."
#define LB_NOREC2	"%s: An unknown record type (%#o) was found."
#define LB_BADREC1	"%s: An invalid record type (%d) was found for user '%s', Job ID (0x%llx).\n  The SBU will be set to 0.0."
#define LB_BADREC2	"%s: An invalid record type (%d) was found.\n   Returning an SBU of %f."

#define LB_DEVMIN	"%s: The device index (%d) is less than the minimum value.\n   It must be greater than zero."
#define LB_DEVMAX	"%s: The device index (%d) exceeds the maximum value.\n   It must be smaller than %d."
#define LB_NUMARGS	"%s: The wrong number of arguments were passed.\n   Expected %d arguments but %d arguments were passed.  The program may\n   need to be updated."
#define LB_MTWF	"%s: The multitasking weighting factors are being ignored,\n   since MAX_CPUS (%d) in /etc/csa.conf must be greater than\n   or equal to the number of available CPUs (%d)."

#define LB_BADREV	"%s: Bad record revision number %d."

/*
 * Program message system definitions.
 */
#define PC_CALL		"An error was returned from the call to the '%s' routine."
#define PC_CALL1	"An error was returned from the call to the '%s' routine."

#define PC_CODEERR	"A problem caused by a coding error %d was detected in the '%s' routine."
#define PC_CONFIG	"The value of %s (%d) is less than the minimum required value.\n   Increase the value in the configuration file."
#define PC_CONF_A_TSIZE	"The value of %s (%d) is too small in config file.\n   Increase the value of A_TSIZE in /etc/csa.conf to the \n   number of tty devices used."

#define PC_NOREC	"An unknown record type (%4o) was found in the '%s' routine."
#define PC_NOREC1	"An unknown record type (%4o) was found in the '%s' routine\n   for %s %s %s."
#define PC_NOACT	"An unknown action (%d) was found in the '%s' routine."
#define PC_NOFNC	"An unknown function (%d) was found in the '%s' routine."
#define PC_NOMODE	"An unknown mode (%d) was found in the '%s' routine."
#define PC_NONAME	"An invalid name '%s' was supplied."
#define PC_NOGID	"Unable to get the Group ID of group '%s'."
#define PC_NOPRID	"Unable to get the Project ID of project '%s'."
#define PC_NOUID	"Unable to get the User ID of user '%s'."

#define PC_HEADER	"A invalid magic value was detected in the header for the %s record."
#define PC_DAEMON	"An invalid daemon identifier (%d) was found in the header field."
#define PC_POSITION	"The position in file '%s' is wrong for the next %s record.\n   Examine the data file for correct version of the file.\n   (Match the binary with the record structure.)"
#define PC_SEEK1	"An error occurred on the seek to the first '%s' record."
#define PC_SEEK2	"An error occurred on the seek to the next '%s' record."
#define PC_JOBID	"The EOJ Job ID (0x%llx) does not match the process record Job ID (0x%llx)."
#define PC_NPREC	"An unexpected record type (%d) was detected as a primary type in the\n   '%s' routine."

#define PC_NOPERM	"Permission denied.\n   You must have the CAP_ACCT_MGT capability to change accounting state."

#define EV_NULL		"The environment variable %s is NULL."
#define EV_DECODE	"The environment variable %s = '%s' could not be decoded, ignoring the option."

/*
 * csaaddc command message system definitions.
 */
#define CA_USAGE	"Command Usage:\n%s\t[-A] [-D debug] [-jpu [-g]] [-o ofile] [-t] [-v] ifile(s)"

/*
 * csabuild command message system definitions.
 */
#define CB_USAGE	"Command Usage:\n%s\t[-a] [-A] [-D debug] [-i] [-n]\n\t\t[-o time] [-P pacct] [-s spacct] [-t]"

#define CB_NOSEG	"No segment was returned from the hash table in the '%s' routine."
#define CB_NOREC2	"An unknown record type (%d) was found in the '%s' routine\n   during the header phase."
#define CB_NOREC3	"An unknown record type (%d) was found in file '%s%d'\n   in the '%s' routine."
#define CB_SEGPTR	"A non-null segment pointer was found on a job in the '%s' routine."
#define CB_BADSEGPTR	"A null segment pointer was found during sorting in the '%s' routine."

#define CB_ELYTIME	"An earlier than expected time stamp was discovered\n   in the %s file '%s%d' near offset (%lld).  Examine the data set."
#define CB_BADSEQNO	"An invalid (or multiple) sequence number, machine ID, or Job ID\n   occurred in the file '%s%d'\n  near offset (%lld).\n   Job ID 0x%llx is associated with NQS requests ID's %lld and %lld."
#define CB_OPEN		"Could not open the first pacct file '%s'."
#define CB_FILECNT	"Too many open files (%d) occurred in the '%s' routine."
#define CB_FILEDES	"An invalid file descriptor was found in the '%s' routine."
#define CB_NOSTART	"The Accounting start or stop times were not found."
#define CB_NOUPTIME	"The uptime file '%s%d' must exist. "
#define CB_BADREQID	"An invalid (or multiple) request ID, array ID, or Job ID\n\ occurred in the file '%s%d'\n  near offset (%lld).\n\  Job ID 0x%llx is associated with workload management request ID's %lld and %lld."
#define CB_MAXJID	"A Job ID (0x%llx) exceeded the system maximum Job ID value (%x)."
#define CB_BADJID	"A Job ID (0x%llx) less than zero was found for a record type %d."
#define CB_BADTIME	"An invalid time stamp value was found in record type (%d)."
#define CB_UPTIME	"All of the boot times have been used."
#define CB_RECEOJ	"A pacct EOJ record was found with no other records."
#define CB_NOSUBTYPE	"An unknown NQ_SENT subtype (%d) was found in the '%s' routine."

#define CB_FILETBL	"The file table is full."
#define CB_ETIME	"The end time (etime) was set on a non-pacct call in the '%s' routine."

#define CB_NOBASE	"No Base or EOJ record was found in the file '%s%d' near offset %lld."
#define CB_NOJID	"No Job ID was found for a record from file '%s%d' near offset %lld."
#define CB_IGNORE	"The record was ignored."

#define CB_RCDORD	"The %s records were out of time order in file '%s%d'\n   near offset %lld due to a date change or an invalid file."
#define CB_TIMESTP	"Another record with an earlier than expected time stamp\n   was found in file '%s%d'."
#define CB_NONQS	"Could not find NQS sequence number (seqno) %lld, machine ID (mid) %lld,\n   Job ID (job) 0x%llx, uptime index (upind) %d."
#define CB_NOWM		"Could not find workload management request ID (reqid) %lld, array ID\n\  (arrayid) %d, Job ID (job) 0x%llx, uptime index (upind) %d."

#define CB_BLDTBL	"Times required to build tables: user %f seconds, sys %f seconds."
#define CB_BLDSR	"Times required to create a session file: user %f seconds, sys %f seconds."

#define CB_DUPUP	"Duplicate uptimes for %24.24s were found in files %s%d and %s%d. Ignoring one of the entries."
#define CB_DAEMON	"Bad daemon identifier (%d) in header field."

/*
 * csachargefee command message system definitions.
 */
#define CF_USAGE	"Command Usage:\n%s\tlogin-name fee"

#define CF_NOPERM	"Permission denied.\n   Unable to get the default Account ID."
#define CF_PASSWD	"Unable to get the password entry for '%s'."
#define CF_ZERO		"A charge fee of zero is not valid.\n   Increase the value of the fee in the argument list."

/*
 * csacms command message system definitions.
 */
#define CM_USAGE	"Command Usage:\n%s\t[-a [-po [-e]]] [-cjns] [-S [-A]] files"

/*
 * csacom command message system definitions.
 */
#define CO_USAGE	"Command Usage:\n%s\t[-b] [-c] [-f] [-h] [-i] [-k] [-m] [-p] [-r] [-t] [-v]\n\t[-A] [-G] [-J] [-L] [-M] [-N] [-P] [-T] [-U] [-V] [-W] [-X]\n\t[-Y] [-Z] [-C sec] [-e time] [-E time] [-g group] [-H factor]\n\t[-I chars] [-j jid] [-l line] [-n pattern] [-o ofile] [-O sec]\n\t[-s time] [-S time] [-u user] [files]"

#define CO_TIME		"The time string '%s' has an invalid start (-s or -S) value\n   or end (-e or -E) value."
#define CO_MINUTE	"The minutes string '%s' has an invalid value."
#define CO_SECOND	"The seconds string '%s' has an invalid value."
#define CO_PATTERN	"An error was detected on the pattern syntax '%s'."

/*
 * csacon command message system definitions.
 */
#define Cc_USAGE	"Command Usage:\n%s\t[-A] [-jpu [-g]] [-D level] [-s spacct] [-v]"

#define Cc_BASEEOJ	"The first record is neither a Base nor EOJ record in the '%s' file."
#define Cc_PACCTERR	"An error record was found in the '%s' file."
#define Cc_DATARCD	"An invalid data migration record type (%d) was found and ignored."
#define Cc_DEVRCD	"Too many device groups for User ID (%d), Job ID (0x%llx) at %19.19s   Ignoring this record with device group %16.16s."
#define Cc_TAPERCD	"An invalid tape record type (%d) was found and ignored\n   for User ID (%d), Job ID (0x%llx) at %19.19s."

/*
 * csacrep command message system definitions.
 */
#define CC_USAGE	"Command Usage:\n%s\t[-p | -u] [-bcdfghJjnPwx]"

/*
 * csadrep command message system definitions.
 */
#define CD_USAGE	"Command Usage:\n%s\t[-Aajnt] [-D level] [-V level] [-o ofile] [-s spacct]\n%s\t[-A] [-D level] [-o ofile [-N]] [-s spacct]\n%s\t[-ajnt] [-D level] [-V level] [-o ofile] files\n%s\t[-D level] [-o ofile [-N]] files"

#define CD_MAXQUEUE	"MAX_QUEUE exceeded\n   Overflow of the NQS queue table can be corrected by recompiling\n   with a larger MAX_QUEUE value."
#define CD_PACCT	"A corrupt pacct record was found in file '%s'."
#define CD_TAPEDEV	"Cannot find device group '%s' in the tape array."
#define CD_VALID	"%s."
#define CD_ZEROFILE	"File '%s' has a length of zero and is being ignored."

/*
 * csaedit command message system definitions.
 */
#define CE_USAGE	"Command Usage:\n%s\t[-P pacct] -b offsetfile -o outfile [-v]\n%s\t[-P pacct] [-o outfile] -A [-m] [-t] [-v]\n%s\t[-P pacct] -r reclist -o outfile [-v]\n%s\t[-P pacct] -r reclist [-o outfile] -A | -x [-m] [-t] [-v]"

#define CE_CODEERR	"An error was detected while checking the delete record\n    list.  This is probably caused by a coding error."
#define CE_EMPTY	"Byte offset file '%s' is empty."
#define CE_ILLEGAL	"Offset value %#llo or length value %lld in byte\n    offset file '%s' is illegal."
#define CE_INVALID	"Accounting file '%s' contains invalid records."
#define CE_SEQUENCE	"Offset values in byte offset file '%s' are out of\n    sequence - stopped at offset %#llo, length %lld."

/*
 * csagetconfig command message system definitions.
 */
#define CG_USAGE	"Command Usage:\n%s\tlabel_name"


/*
 * csajrep command message system definitions.
 */
#define CJ_USAGE	"Command Usage:\n%s\t[-ABbcFhJLqS] [-s spacct] [-TtwxZ]\n%s\t[-ABbcehJ] [-j jid] [-L] [-n seqno] [-p project] [-qS]\n\t\t[-s spacct] [-Tt] [-u login] [-wxZ]\n%s\t-N [-A] [-s spacct]"

#define CJ_DATAMIG	"A data migration record was found with an invalid request\n   type (%d) and was ignored."
#define CJ_TAPE		"An invalid tape record type (%d) was found."
#define CJ_DATAMIGSUM	"An error occurred summing the data migration records."
#define CJ_TAPESUM	"An error occurred summing the tape records."
#define CJ_OPTMAP	"Unable to map %s to %s in the '%s' routine."

/*
 * csarecy command message system definitions.
 */
#define CR_USAGE	"Command Usage:\n%s\t[-r] [-s spacct] [-A] [-D debug] [-P pacct] [-R]"

#define CR_DEBUG	"The debugging option was set to level %d."
#define CR_NOFILE	"The Record type %#o file '%s' could not be found."
#define CR_SEEK3	"An error occurred on the seek during the '%s' phase."
#define CR_BASEEOJ	"The first record is not a Base or EOJ record in the pacct file."
#define CR_NOBASEEOJ	"There were no Base or EOJ records found in the pacct file."
#define CR_TCMP		"The primary sort key '%s' matches the secondary sort key '%s'."

/*
 * csaswitch command message system definitions.
 */
#define CS_USAGE_PRIV	"Command Usage:\n%s\n\t[-D level] -c check -n name\n\t[-D level] -c halt\n\t[-D level] -c off -n namelist\n\t[-D level] -c on [-n namelist] [-m memthresh] [-t timethresh] [-P path]\n\t[-D level] -c status\n\t[-D level] -c switch\n\n\tValid arguments to the -n option are:\n\t\tcsa, nqs, wkmg, tape, io, mem, memt, time"
#define CS_USAGE_UNPRIV	"Command Usage:\n%s\t[-D level] -c check -n name\n%s\t[-D level] -c status"

#define CS_START	"Unable to enable accounting."
#define CS_STOP		"Unable to disable accounting."
#define CS_HALT		"Unable to halt system accounting."
#define CS_CHECK	"Unable to get the accounting status for '%s'."
#define CS_KDSTAT	"Unable to get the Kernel and Daemon accounting status information."
#define CS_RCDSTAT	"Unable to get the Record accounting status information."
#define CS_SWITCH	"Unable to switch accounting."
#define CS_INCACCT	"Unable to process incremental accounting request."
#define CS_THDREQ	"A threshold value is required for %s in /etc/csa.conf."
#define CS_THDPLUS	"Threshold value for %s in /etc/csa.conf must be positive."
#define CS_CONF_ALLOFF	"None of the accounting methods in /etc/csa.conf is on."
#define CS_ENOPKG	"CSA is not enabled via systune."
#define CS_NOMAC	"Unable to get the MAC label for "%s"."
#define CS_SETMAC	"Unable to set the MAC label of file %s."
#define CS_INTERNAL	"An unknown/unrecoverable internal error occurred."

/*
 * csaverify command message system definitions.
 */
#define CV_USAGE	"Command Usage:\n%s\t[-P pacct] [-o outfile] [-m nrec] [-v] [-D]"

/*
 * ja command message system definitions.
 */
#define JA_USAGE	"Command %s Usage:\n%s\t[-cfs[e] [[-d] [-D] [-a ash] [-g gid] [-j jid] [-l[Ch] [-p project]]\n\t\t[-n names] [-M marks] [-P] [-r] [-u uid]]] [-t] [file]\n%s\t[-fos[e] [[-d] [-D] [-a ash] [-g gid] [-j jid] [-l[Ch] [-p project]]\n\t\t[-n names] [-M marks] [-P] [-r] [-u uid]]] [-t] [file]\n%s\t[-A] [-x] [file]\n"
#define JA_USAGE1	"Command %s Usage:\n%s\t-c [[-f] [-s] [-t]] [[-a ash] [-g gid] [-j jid] [-M marks] [-n names]\n\t[-p prid] [-u uid]] [[-l [-h]] [-d] [-e] [-J] [-r]]\n\t[-D lvl] [file] (command report)\n%s\t-f [[-c] [-s] [-t]] [[-a ash] [-g gid] [-j jid] [-M marks]\n\t[-n names] [-p prid] [-u uid]] [[-l [-h]] [-d] [-e] [-J] [-r]]\n\t[-D lvl] [file] (command flow report)\n%s\t-o [-s] [-t] [[-a ash] [-g gid] [-j jid] [-M marks] [-n names]\n\t[-p prid] [-u uid]] [[-d] [-e] [-r]] [-D lvl] [file]\n\t(command other report)\n%s\t-s [[-c] [-f] [-t]] [[-a ash] [-g gid] [-j jid] [-M marks]\n\t[-n names] [-p prid] [-u uid]] [[-l [-h]] [-d] [-e] [-r]]\n\t[-D lvl] [file] (summary report)\n%s\t-m [-D lvl] [file] (mark position request)"

#define JA_JID		"The process running does not have a valid Job ID."
#define JA_TMPDIR	"The %s environment variable is not set. Rerun %s and specify the job accounting filename."
#define JA_NONAME	"Cannot build the file name from the TMPDIR environment variable and the Job ID."
#define JA_NAMESZ	"The file name exceeds the maximum length of 128 characters."
#define JA_JOBFILE	"The job accounting file is empty or does not exist."
#define JA_COMMAND	"No commands were selected for the report."

#define JA_BADEXP	"An invalid regular expression was supplied for selection by name."
#define JA_DAEMON	"An invalid daemon identifier (%d) was found in the header field."
#define JA_TAPE		"An invalid tape record type (%d) was found and ignored."
#define JA_FLOWTREE	"The command flow tree overflowed the available memory."
#define JA_SGI		"A reserved SGI type(%d) record was found and ignored."
#define JA_SITE		"A reserved SITE type(%d) record was found and ignored."


#endif	/* _ACCTMSG_H */
