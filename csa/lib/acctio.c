/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2006-2007 Silicon Graphics, Inc.  All Rights Reserved.
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
 *	I/O library routines for accounting files
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <utmp.h>
#include <unistd.h>
#include <sys/param.h>
#include "csaacct.h"

#include "acctdef.h"
#include "acctmsg.h"
#include "cacct.h"
#include "cms.h"
#include "convert.h"
#include "csa.h"

/*
 *	G E N E R A L    D E F I N I T I O N S
 *	======================================
 */

/*
 *	functions
 */
static	int	checktype(struct acctent *acctrec, void *rec);
static	int	stepback(int fd);
static	int	validtype(struct achead *header);

/*
 *	M A C R O S
 *	===========
 */

/*
 *	FILE management
 */
FILE	*streams[FOPEN_MAX];

#define	FD_TO_STREAM(FD)	streams[(FD)]
#define	STREAM_TO_FD(ST)	fileno((ST))
#define	FD_SEEK(FD,P,W)		fseek(FD_TO_STREAM(FD), (P), (W))
#define	FD_TELL(FD)		ftell(FD_TO_STREAM(FD))

/*
 *	Header macros.
 */
#define	MY_FLAGS(REC)	((struct achead *)REC)->ah_flag

/*
 *	basic IO:
 *	- FIXD_READ  : read fixed size record
 *	- VLEN_READ  : read variable length records & call conversion routine
 *	               when needed
 *	- FIXD_WRITE : write record
 */
#define	FIXD_READ(FD,BUF,SIZE)						\
	if (readacct(FD, (char *)BUF, SIZE) != SIZE) {			\
		return (0);						\
	}

#define	VLEN_READ(FD,BUF,CNT,CFUN,MSZ) {				\
	char	*BB;							\
	struct	achead	*HEAD;						\
	int	LEN;							\
	int	SIZE;							\
									\
	BB = (char *)BUF;						\
	CNT = sizeof (struct achead);					\
	if ((SIZE = readacct (FD, BB, CNT)) == CNT) {			\
	    HEAD = (struct achead *)BB;					\
	    LEN = MIN(MSZ, HEAD->ah_size) - CNT;			\
	    if (HEAD->ah_magic == ACCT_MAGIC && validtype(HEAD)) {     	\
		if (LEN > 0) {						\
		    BB += CNT;						\
		    if (readacct (FD, BB, LEN) == LEN) {		\
			if (OTFC_NEEDED(HEAD)) {			\
			    CNT = CFUN ((void *)BUF,0);			\
			    CNT = CNT ? CNT : -1; /* CFUN error ? */	\
			} else {					\
			    CNT += LEN;		/* success */		\
			}						\
		    } else {						\
			CNT = -3;		/* 2nd read == error */	\
		    }							\
		}							\
	    } else {							\
	        CNT = -1;			/* Header error */	\
	    }								\
	} else {							\
		if (SIZE) {						\
			CNT = -2;		/* 1st read == error */	\
		} else {						\
			CNT = 0;		/* 1st read == EOF */	\
		}							\
	}								\
}

#define	FIXD_WRITE(FD,BUF,SIZE)						\
	if (writeacct(FD, (char *)BUF, SIZE) != SIZE) {			\
		acct_err(ACCT_CAUT,					\
		       _("%s: An error was returned from the '%s' routine."), \
			FUNC_NAME, "write()");				\
		return (-1);						\
	}

#define STEPBACK(NBYTES) {                                              \
        if (FD_SEEK (fd, -NBYTES, SEEK_CUR) < 0) {                      \
                FD_SEEK (fd, 0, SEEK_SET);                              \
                return (-1);                                            \
        }                                                               \
}

/*
 *	G E N E R I C    I / O    R O U T I N E S
 *	=========================================
 */
/*
 *	openacct - open an accounting file for IO
 *
 *	oflag == 'r'	: open with O_RDONLY
 *	oflag == 'w'	: open with O_RDWR | O_CREAT | O_TRUNC
 *	oflag == 'a'	: open with O_RDWR | O_CREAT
 *
 *	if no name is given open either 'stdin' or 'stdout' depending 
 *	on the oflag ('r' -> stdin; else -> stdout)
 *
 *	returns fd upon success else -1
 */
#define	FUNC_NAME		"openacct()"
int
openacct (char *name, const char *oflag)
{
	int	fd = -1;
	FILE	*st;

	/*
	 * open the file
	 */
	if (name) {
		st = fopen(name, oflag);

	} else if (*oflag == 'r') {
		st = stdin;

	} else {
		st = stdout;
	}

	if (st) {

		/*
		 * save FILE for later
		 */
		fd = STREAM_TO_FD(st);
		FD_TO_STREAM(fd) = st;
	}

	return (fd);
}
#undef	FUNC_NAME


/*
 *	closeacct - close an accounting file
 *
 */
#define	FUNC_NAME	"closacct()"
void
closeacct (int fd)
{
	if (fd >= 0) {
		fclose(FD_TO_STREAM(fd));
		FD_TO_STREAM(fd) = NULL;
	}
}
#undef	FUNC_NAME


/*
 *	readacct - read from an accounting file
 *
 *	returns	no. of bytes read
 */
#define	FUNC_NAME	"readacct()"
int
readacct (int fd, char *buff, int size)
{
	return fread(buff, 1, size, FD_TO_STREAM(fd));
}
#undef	FUNC_NAME


/*
 *	writeacct - write to an accounting file
 *
 *	returns no. of bytes written
 */
#define	FUNC_NAME	"writeacct()"
int
writeacct(int fd, char *buff, int size)
{
	return fwrite(buff, 1, size, FD_TO_STREAM(fd));
}
#undef	FUNC_NAME


/*
 *	seekacct - wrapper for accounting seek function
 *
 *	call a seek function for an accounting file to a specific place. 
 *
 *	returns	current byte position of the file
 */
#define	FUNC_NAME	"seekacct()"
off_t
seekacct (int fd, off_t pos, int whence)
{
	FD_SEEK(fd, pos, whence);
	return(FD_TELL(fd));
}
#undef	FUNC_NAME


/*
 *	P A C C T    I / O    R O U T I N E S
 *	=====================================
 */
/*
 *	readpacct - read single pacct record
 *
 *	buffer must provide enough space for either
 *	the accounting header (struct achead) or the entire accounting
 *	entry
 *
 *	returns number of bytes read (or -1, -2, or -3 on error)
 */
#define	FUNC_NAME	"readpacct()"
int
readpacct (int fd, char *buff, int size)
{
	int		nbytes;

	/*
	 * sanity check on buffer
	 */
	if ((buff == NULL) || (size < sizeof(struct achead)) ) {
		acct_err(ACCT_CAUT,
		       _("%s: An entry buffer was not provided or was misaligned."),
			FUNC_NAME);
		return(-1);
	}

	buff[0] = 0;
	VLEN_READ(fd, buff, nbytes, convert_pacct, size);

	return (nbytes);
}
#undef	FUNC_NAME


/*
 *	readacctent - read acctent structure
 *
 * 	readacctent reads the next process accounting entry.  An entry consists
 *	of from one or more accounting records (csa.h format).  The file
 *	may be read either forward or backward starting from its current
 *	position.
 *
 *	*******    IMPORTANT NOTE ABOUT RECORD SIZES  ********
 *	The backwards processing of the pacct file starts at the EOF and
 *	reads backwards 8 bytes at a time, looking for a valid header.
 *	All record types share the same header (8 bytes in size), but the 
 *	overall size of the records varies.  This means, then, that each
 *	record type size must be divisible by 8 in order to find a header.
 *	Record types are defined in csa.h and csaacct.h
 *
 *	readacctent returns the number of bytes that comprise the next entry.
 *	If an error is encountered, a -1, -2, or -3 is returned, with no
 *	guarantee of file position or the state of the acctent pointers.
 *
 *	The caller must provide an entry buffer of ACCTENTSIZ bytes; this is
 *	done by setting the proper field in the acctent structure.
 */
#define	FUNC_NAME	"readacctent()"
int
readacctent(int fd, struct acctent *acctrec, int backward)
{
	int		size;
	int		nbytes = 0;
	void		*rec;
	int		stepback_flag;
	char		*cent;

	/*
	 * Make sure that an entry buffer was provided by the caller.
	 * We have to assume that it is long enough - ACCTENTSIZ bytes.
	 */
	if (acctrec == NULL || acctrec->ent == NULL) {
		acct_err(ACCT_CAUT,
		       _("%s: An entry buffer was not provided or was misaligned."),
			FUNC_NAME);
		return(-1);
	}

	cent = acctrec->ent;
	memset(acctrec, 0, sizeof(struct acctent));
	acctrec->ent = cent;
	acctrec->prime = 0;

	rec = (void *)acctrec->ent;	/* beginning of entry buffer */

	/*
	 *	F O R W A R D
	 *	=============
	 */
	while(!backward) {

		/*
		 * read next record and check for errors
		 */
		size = readpacct(fd, rec, (ACCTENTSIZ - nbytes));
		if (size <= 0) {
			if (size < 0) {
				acct_err(ACCT_CAUT,
				       _("%s: An error was returned from the '%s' routine."),
					FUNC_NAME,
					"readpacct()");
			}
			nbytes = size;	/* return error / EOF */
			break;
		}

		/*
		 * Verify that the record type is valid and put into its
		 * place in acctrec
 		 */
		if (checktype(acctrec, rec) == -1) {
			return(-1);	/* error message already issued */
		}
		nbytes += size;

		/*
		 * more work ?
		 */
		if (!(MY_FLAGS(rec) & AMORE))		break;
		/*
		 * advance the buffer pointer 
		 */
		rec = (char *)rec + size;
	}

	/*
	 *	B A C K W A R D
	 *	===============
	 *
	 * NOTE: the backward read doesn't try to skip over bad data!!!!
	 */
	while (backward) {

		/*
		 * back up to the previous record and check for errors
		 */
		stepback_flag = stepback(fd);
		if (stepback_flag < 0) {
			/*
			 * stepback failed; assume start of file
			 */
			break;
		}
		/* do another readpacct only if there is room */
		if ((ACCTENTSIZ - nbytes) > 0) {
		size = readpacct(fd, rec, (ACCTENTSIZ - nbytes));
		if (size <= 0) {
			if (size < 0) {
				acct_err(ACCT_CAUT,
				       _("%s: An error was returned from the '%s' routine."),
					FUNC_NAME,
					"readpacct()");
			}
			nbytes = size;	/* return error / EOF */
			break;
		}
		} else {
			/* move forward 'struct achead' bytes to offset the
			 * stepback at the beginning of the while-loop in
			 * preparation of next buffer read.
			 */
			FD_SEEK(fd, sizeof(struct achead), SEEK_CUR);
			break;
		}
		
		/*
		 * check this record's ah_flag to see if we have to read it
		 *
		 * AMORE set:
		 *		if this is the first record we are looking, we
		 *		are in trouble else we found one we have to
		 *		read
		 * AMORE not set:
		 *		if this is the first one, just fine else we
		 *		are done for now
		 */
		if (stepback_flag & AMORE) {
			if (nbytes == 0) {
				acct_err(ACCT_CAUT,
				       _("%s: The file was not positioned at the end of an entry."),
					FUNC_NAME);
				return(-1);
			}
		} else if (nbytes) {
			break;	
		}

		/*
		 * Verify that the record type is valid and put into its
		 * place in acctrec
 		 */
		if (checktype(acctrec, rec) == -1) {
			return(-1);	/* error message already issued */
		}
		nbytes += size;

		/*
		 * get ready for more work
		 */
		stepback(fd); 
		/*
		 * advance the buffer pointer 
		 */
		rec = (char *)rec + size;
	}

	return nbytes;
}
#undef	FUNC_NAME


/*
 *	Step back nrecs pacct entries
 *
 *	Since pacct entries are variable length going backwards is not
 *	something straightforward. Just step through the file and check
 *	for magic.
 *	returns either ah_flag or -1
 */
#define	FUNC_NAME	"stepback()"
#define	IS_IN(V,L,U)	((V) < (L) || (V) > (U) ? 0 : 1)

static int
stepback(int fd)
{
	int	found;
	struct	achead	head;
	int err = 0;

	found = 0;
	while (!found) {
		STEPBACK (sizeof (struct achead));
		FIXD_READ (fd, &head, sizeof (struct achead));
		STEPBACK (sizeof (struct achead));

		if ((head.ah_magic == ACCT_MAGIC) &&
		    (validtype(&head)) &&
		    IS_IN(head.ah_revision, OTFC_BASE_REV,
			  rev_tab[head.ah_type]) &&
		    /*
		     * (struct acctcfg) is the largest in size among
		     * all supported CSA accounting records. If the
		     * assumption has been changed, need to change
		     * the code below.
		     */
		    IS_IN(head.ah_size, sizeof(struct achead),
				sizeof(struct acctcfg))) {

			found = 1;
		}
	}

	return ((int)head.ah_flag);
}
#undef	IS_IN
#undef	FUNC_NAME


/*
 *	Verify that the record type is valid.
 */
#define	FUNC_NAME	"validtype()"
static int
validtype(struct achead *header)
{
	int type = header->ah_type;
	
	if (type == ACCT_KERNEL_CSA ||
	    type == ACCT_KERNEL_MEM ||
	    type == ACCT_KERNEL_IO ||
	    type == ACCT_KERNEL_DELAY ||
	    type == ACCT_KERNEL_SOJ ||
	    type == ACCT_KERNEL_EOJ ||
	    type == ACCT_KERNEL_CFG ||
	    type == ACCT_DAEMON_WKMG ||
	    type == ACCT_JOB_HEADER ||
	    type == ACCT_CACCT ||
	    type == ACCT_CMS ||
	    type == ACCT_KERNEL_SITE0 ||
	    type == ACCT_KERNEL_SITE1 ||
	    type == ACCT_DAEMON_SITE0 ||
	    type == ACCT_DAEMON_SITE1
	   )
		return(1);

	return(0);
}
#undef	FUNC_NAME

/*
 *	Verify that the record type is valid for a pacct file;
 *	set the appropriate pointer.
 */
#define	FUNC_NAME	"checktype()"
static int
checktype(struct acctent *acctrec, void *rec)
{

	switch (((struct achead *)rec)->ah_type) {

	case ACCT_KERNEL_CSA:
		acctrec->csa = (struct acctcsa *)rec;
		break;

	case ACCT_KERNEL_MEM:
		acctrec->mem = (struct acctmem *)rec;
		break;

	case ACCT_KERNEL_IO:
		acctrec->io = (struct acctio *)rec;
		break;

	case ACCT_KERNEL_DELAY:
		acctrec->delay = (struct acctdelay *)rec;
		break;

	case ACCT_KERNEL_SOJ:
		acctrec->soj = (struct acctsoj *)rec;
		break;

	case ACCT_KERNEL_EOJ:
		acctrec->eoj = (struct accteoj *)rec;
		break;

	case ACCT_KERNEL_CFG:
		acctrec->cfg = (struct acctcfg *)rec;
		break;

	case ACCT_DAEMON_WKMG:
		acctrec->wmbs = (struct wkmgmtbs *)rec;
		break;
		
	case ACCT_JOB_HEADER:
		acctrec->job = (struct acctjob *)rec;
		break;

	case ACCT_KERNEL_SITE0:
	case ACCT_KERNEL_SITE1:
	case ACCT_DAEMON_SITE0:
	case ACCT_DAEMON_SITE1:
		acctrec->other = (struct achead *)rec;
		break;

	default:
		acct_err(ACCT_WARN,
	        _("%s: An unknown record type (%#o) was found with flag (%d)."),
			FUNC_NAME,
			 ((struct achead *)rec)->ah_type, MY_FLAGS(rec));
		return(-1);
	}

	if (acctrec->prime == 0)
		acctrec->prime = (struct achead *)rec;

	return(0);
}
#undef	FUNC_NAME


/*
 *	writepacct - write single pacct entry
 */
#define	FUNC_NAME	"writepacct()"	/* library function name */
int
writepacct(int fd, void *rec, int more)
{
	unsigned int	flags;

	/*
	 * save the flags and set correct AMORE
	 */
	flags = MY_FLAGS(rec);
	if (more) {
		MY_FLAGS(rec) |= AMORE;
	} else {
		MY_FLAGS(rec) &= ~AMORE;
	}

	/*
	 * write record
	 */
	FIXD_WRITE(fd, (char *)rec, ((struct achead *)rec)->ah_size);

	/*
	 * restore the flags and return
	 */
	MY_FLAGS(rec) = flags;

	return(0);
}
#undef	FUNC_NAME


/*
 *	writeacctent - write an acctent structure
 *
 *	writeacctent writes the next process accounting entry. 
 *	The file will be written starting from its current position.
 *
 *	writeacctent returns the number of records that comprise the entry
 *	being written.  If an error is encountered a minus one is returned.
 */
#define	FUNC_NAME	"writeacctent()"

#define	Q_WRITE(REC,MORE) {						\
	if (REC) {							\
		if (prev) {						\
			if (writepacct(fd, prev, MORE) < 0) {		\
				return (-1);				\
			}						\
		}							\
		nrec++;							\
		prev = REC;						\
	}								\
}

int
writeacctent(int fd, struct acctent *acctrec)
{
	void	*prev = NULL;
	int	nrec = 0;

	/*
	 * For the sake of correct AMORE settings it is not possible to
	 * do immediate write for records. Instead, records are first
	 * stored in a temporary buffer. Subsequent writes (Q_WRITE)
	 * will cause the temporary buffer to be written to the file
	 * (with AMORE set) and the new buffer is stored. 
	 */
	Q_WRITE (acctrec->csa, 1);  /* primary type + linked records */
	Q_WRITE (acctrec->eoj, 1);  /* eoj is placed here just in case it will
				       have linked records in the future */
	Q_WRITE (acctrec->mem, 1);  /* linked records must follow primary */
	Q_WRITE (acctrec->io,  1);  /* types */
	Q_WRITE (acctrec->delay, 1);
	Q_WRITE (acctrec->soj, 1);
	Q_WRITE (acctrec->cfg, 1);
	Q_WRITE (acctrec->job, 1);
	Q_WRITE (acctrec->wmbs, 1);

	/*
	 * And now write the last buffer (this time AMORE will not
	 * be set).
	 */
	Q_WRITE (prev, 0);

	return(nrec);
}
#undef	Q_WRITE
#undef	FUNC_NAME

/*
 *	T Y P E d    I / O    R O U T I N E S
 *	=====================================
 *
 *	The following macros generate a variety of read and write routines.
 *
 *	- MAKE_F_READ
 *		Generates a routine to read a fixed length accounting
 *		record (not headed by an achead structure).
 *	- MAKE_V_READ
 *		Generates a routine to read a variable length accounting
 *		record described by an achead structure. A structure
 *		specific conversion routine is called if the record 
 *		read does not match the current revision level.
 *	- MAKE_U_READ
 *		Like MAKE_V_READ, but is used to read unions instead
 *		of strict types.
 *	- MAKE_WRITE
 *		Generates a routine to write a record.
 *	- MAKE_U_WRITE
 *		Generates a routine to write a record from a union.
 *
 *	All routines will have the identical calling interface:
 *
 *		NAME (int fd, TYPE *buffer)
 *
 *	NOTE: It is assumed that the buffer provided by the caller
 *	of the variable length read routines is large enough to hold 
 *	all possible incoming data. This will not be the case if
 *	a release X buffer contains LESS data than a release X-1
 *	buffer.
 *
 */
#define MAKE_F_READ(PROC,TYPE)						\
	int PROC (int fd, TYPE *buffer) {				\
		char    *FUNC_NAME="PROC ()";				\
									\
		FIXD_READ (fd, buffer, sizeof (TYPE));			\
		return (sizeof (TYPE));					\
	}
#define	MAKE_V_READ(PROC,TYPE,CONV)					\
	int PROC (int fd, TYPE *buffer) {				\
		int	i;						\
		char	*FUNC_NAME="PROC ()";				\
									\
		VLEN_READ (fd, buffer, i, CONV, sizeof(TYPE));		\
		return (i);						\
	}

#define	MAKE_U_READ(PROC,UNION,CONV)					\
	int PROC (int fd, UNION *buffer) {				\
		int	i;						\
		char	*FUNC_NAME="PROC ()";				\
									\
		VLEN_READ (fd, buffer, i, CONV, sizeof(UNION));		\
		if ((i > 0) && (i <= sizeof(UNION)))			\
			i = sizeof(UNION);				\
		return (i);						\
	}

#define	MAKE_WRITE(PROC,TYPE)						\
	int PROC (int fd, TYPE *buffer) {				\
		char	*FUNC_NAME="PROC ()";				\
									\
		FIXD_WRITE (fd, buffer, sizeof (TYPE));			\
		return (sizeof (TYPE));					\
	}
#define	MAKE_U_WRITE(PROC,UNION)					\
	int PROC (int fd, UNION *buffer) {				\
		struct	achead	*HEAD;					\
		char	*FUNC_NAME="PROC ()";				\
									\
		HEAD = (struct achead *)buffer;				\
		FIXD_WRITE (fd, buffer, HEAD->ah_size);			\
		return (sizeof (UNION));				\
	}


MAKE_V_READ	(readcacct,	struct cacct,      convert_cacct)
MAKE_WRITE	(writecacct,	struct cacct)

MAKE_V_READ	(readcms,	struct cms,        convert_cms)
MAKE_WRITE	(writecms,	struct cms)
