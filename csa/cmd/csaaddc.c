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
 *	csaaddc.c - add cacct records together.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/types.h>
#include "csaacct.h"

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "acctdef.h"
#include "acctmsg.h"
#include "cacct.h"

extern int db_flag;

int	errflag		= 0;
char	*Prg_name		= "";
char	*outfile	= NULL;
FILE	*ofd;
int	bofd		= -1;
int	ifd		= -1;

struct	map	{
	uint64_t	prid;
	uid_t		uid;
	uint64_t	jid;
	gid_t		gid;
	struct	cacct	*acp;
};

struct	map	*acmap	= NULL;
#define	MAPSIZE		1000		/* initial map size */
#define	MAPINC		250		/* map increment */

int	mapsize	= 0;
int	total	= 0;
struct	cacct	tcb;			/* total structure if requested */
int	verbose	= 0;
int	asciiout = 0;
int	con_prid = 0;
int	con_gid = 0;
int	con_jid = 0;
int	con_uid = 0;

static	void	addmap(uid_t, uint64_t, uint64_t, gid_t, int);
static	void	enter();
static	struct	cacct	*findacd(uid_t, uint64_t, uint64_t, gid_t);
static	void	wrascii(struct cacct *);
static	void	wroutput();

static	void
usage() {
	acct_err(ACCT_ABORT,
	       "Command Usage:\n%s\t[-A] [-D debug] [-jpu [-g]] [-o ofile] [-t] [-v] ifile(s)",
		Prg_name);
}


main(int argc, char **argv)
{
	char		ch;
	int		c;
	extern char	*optarg;
	extern int	optind;

	Prg_name = argv[0];
	while((c = getopt(argc, argv, "AD:gjo:tuv")) != EOF) {
		ch = (char)c;
		switch(ch) {
		case 'A':
			asciiout = 1;
			break;
		case 'D':
			db_flag = atoi(optarg);
			if ((db_flag < 0) || (db_flag > 2)) {
				acct_err(ACCT_FATAL,
				       _("The (-%s) option's argument, '%s', is invalid."),
					"D", optarg);
				Nerror("Option -D valid values are 0 thru 2\n");
				usage();
			}
			break;
		case 'g':
			con_gid = 1;
			break;
		case 'j':
			con_jid = 1;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'p':
			con_prid = 1;
			break;
		case 't':
			total = 1;
			break;
		case 'u':
			con_uid = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		} /* end switch */
	} /* end while getopt */

	/*
	 *	If no consolidation options were specified, use -pu.
	 */
	if ((con_prid + con_gid + con_jid + con_uid) == 0) {
		con_prid = con_uid = 1;
	}

	/*
	 *	If group only, usage error since not all records have gids.
	 */
	if ((con_prid + con_jid + con_uid) == 0) {
		usage();
	}

	/*
	 *	Open the output file
	 */
	if (asciiout) {
		bofd = -1;
		ofd = outfile == NULL ? stdout :  fopen (outfile,"w");
	} else {
		ofd = NULL;
		bofd = outfile == NULL ?
			openacct(NULL, "w") : openacct(outfile, "w");
	}
	if ((bofd < 0) &&
	    (ofd == NULL)) {
		acct_perr(ACCT_ABORT, errno,
			_("An error occurred during the opening of file '%s'."),
			outfile ?
			outfile : "stdout");
	}

	/*
	 *	Make sure we have at least 1 input file
	 */
	if (argv[optind] == NULL) {
		acct_err(ACCT_FATAL,
		       _("The input file was not specified.")
			);
		usage();
	}

	/*
	 *	Process each inputfile.
	 */
	for( ; optind < argc; optind++) {
		if ((ifd = openacct(argv[optind], "r")) < 0) {
			acct_perr(ACCT_ABORT, errno,
				_("An error occurred during the opening of file '%s'."),
				argv[optind]);
		}
		enter();
		closeacct(ifd);
	}

	/*
	 *	Write output file.
	 */
	if(total) {
		tcb.ca_prid = 0;
		tcb.ca_uid = 0;
	}
	wroutput();
	if (bofd > 0) {
		closeacct(bofd);
	}

	exit(errflag);
}

/*
 *	Routine to summmarize records...
 */
static void
enter() 
{
	struct cacct *acp;
	struct	cacct cb;
	int ind;
	struct	cacct	*findacd();

	while (readcacct (ifd, &cb) == sizeof (struct cacct)) {
		if (total) {
			acp = &tcb;
		} else {
			if (db_flag > 1) {
				fprintf(stderr, "enter(): calling findacd() "
				    "with uid %d (%s), prid %lld (%s), "
				    "jid 0x%llx.\n", cb.ca_uid,
				    uid_to_name(cb.ca_uid), cb.ca_prid,
				    prid_to_name(cb.ca_prid), cb.ca_jid);
			}
			if ((acp = findacd(cb.ca_uid, cb.ca_prid, cb.ca_jid,
				cb.ca_gid)) == (struct cacct *)NULL) {
				exit(-1);
			}
		}
		acp->ca_njobs += cb.ca_njobs;
		acp->prime.cpuTime += cb.prime.cpuTime;
		acp->nonprime.cpuTime += cb.nonprime.cpuTime;
		acp->prime.charsXfr += cb.prime.charsXfr;
		acp->nonprime.charsXfr += cb.nonprime.charsXfr;
		acp->prime.logicalIoReqs += cb.prime.logicalIoReqs;
		acp->nonprime.logicalIoReqs += cb.nonprime.logicalIoReqs;
		acp->prime.minorFaultCount += cb.prime.minorFaultCount;
		acp->nonprime.minorFaultCount += cb.nonprime.minorFaultCount;
		acp->prime.majorFaultCount += cb.prime.majorFaultCount;
		acp->nonprime.majorFaultCount += cb.nonprime.majorFaultCount;
		acp->prime.kcoreTime += cb.prime.kcoreTime;
		acp->nonprime.kcoreTime += cb.nonprime.kcoreTime;
		acp->prime.kvirtualTime += cb.prime.kvirtualTime;
		acp->nonprime.kvirtualTime += cb.nonprime.kvirtualTime;
		acp->prime.cpuDelayTime += cb.prime.cpuDelayTime;
		acp->nonprime.cpuDelayTime += cb.nonprime.cpuDelayTime;
		acp->prime.blkDelayTime += cb.prime.blkDelayTime;
		acp->nonprime.blkDelayTime += cb.nonprime.blkDelayTime;
		acp->prime.swpDelayTime += cb.prime.swpDelayTime;
		acp->nonprime.swpDelayTime += cb.nonprime.swpDelayTime;

		acp->ca_du += cb.ca_du;
		acp->ca_dc += cb.ca_dc;

		acp->ca_pc += cb.ca_pc;

		for(ind = 0; ind < TP_MAXDEVGRPS; ind++) {
			acp->ca_tpio[ind].cat_mounts +=
				cb.ca_tpio[ind].cat_mounts;
			acp->ca_tpio[ind].cat_reads +=
				cb.ca_tpio[ind].cat_reads;
			acp->ca_tpio[ind].cat_writes +=
				cb.ca_tpio[ind].cat_writes;
			acp->ca_tpio[ind].cat_reserv +=
				cb.ca_tpio[ind].cat_reserv;
			acp->ca_tpio[ind].cat_utime +=
				cb.ca_tpio[ind].cat_utime;
			acp->ca_tpio[ind].cat_stime +=
				cb.ca_tpio[ind].cat_stime;
		}

		acp->can_nreq += cb.can_nreq;
		acp->can_utime += cb.can_utime;
		acp->can_stime += cb.can_stime;

		acp->ca_sbu += cb.ca_sbu;
		acp->ca_fee += cb.ca_fee;
	}
}

/*
 *	Find the cacct structure which matches the consolidation options.
 *	If jid consolidation was specified, always return a new cacct 
 *	structure.  Use of the -j option is synonymous with concatenating
 *	all of the files with the "cat" command.
 */
static struct cacct *
findacd(uid_t uid, uint64_t prid, uint64_t jid, gid_t gid)
{
	static	int availind = 0;	/* 1st available entry in acmap[] */
	int ind;
	int	siz;
	int	omapsiz;

	/*
	 *	If first call allocate data struct.
	 */
	if (acmap == NULL) {
		if (db_flag >= 1) {
			fprintf(stderr, "Allocating first map structs\n");
		}
		siz = MAPSIZE * sizeof(struct map);
		mapsize = MAPSIZE;
		if ((acmap = (struct map *)malloc(siz)) == NULL) {
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when allocating '%s'."),
				"initial map");
		}

		for(ind = 0; ind < mapsize; ind++) {
			acmap[ind].uid = -1;
		}
	}

	/*
	 *	See if the consolidation keys are already in the map.
	 */
	if (!con_jid) {
	    for(ind = 0; ind < mapsize; ind++) {
		/*
		 *	If this is the end of used entries match this one.
		 */
		if (acmap[ind].uid == -1) {
			availind = ind;
			break;
		}

		/*
		 *	Look for a match. 
		 */
		if (((con_uid == 0) ||
		     ((con_uid == 1) && (acmap[ind].uid == uid))) &&
		    ((con_prid == 0) || ((con_prid == 1) && 
		     (acmap[ind].prid == prid)) || (prid == CACCT_NO_ID)) &&
		    ((con_gid == 0) || ((con_gid == 1) &&
		     (acmap[ind].gid == gid)) || (gid == CACCT_NO_ID)) ) {

			return(acmap[ind].acp); /* got it */
		}
	   }

	} else {
	   ind = availind;
	}

	if (ind < mapsize) {
		if (db_flag > 1) {
			fprintf(stderr, "findacd(): New map struct being used");
			fprintf(stderr, " jid 0x%llx, uid %d (%s), "
				"prid %lld (%s)\n", jid, uid, uid_to_name(uid),
				prid, prid_to_name(prid));
		}

		if (acmap[ind].uid == -1) {
			addmap(uid, prid, jid, gid, ind);
			if (con_jid) {
				availind++;
			}
			return(acmap[ind].acp);

		} else {
			acct_err(ACCT_ABORT,
			       _("A problem caused by a coding error %d was detected in the '%s' routine."),
				1, "findacd()");
		}
	}

	/*
	 *	If map overflowed increase size and add to map.
	 */
	if (ind == mapsize ) {
		if (db_flag > 1) {
			fprintf(stderr,"map overflowed\n");
		}
		omapsiz = mapsize;
		mapsize += MAPINC;
		siz = mapsize * sizeof(struct map);
		if((acmap = (struct map *)realloc((char *)acmap,siz)) == NULL) {
			acct_perr(ACCT_ABORT, errno,
				_("There was insufficient memory available when reallocating '%s'."),
				"map");
		}

		for(ind = omapsiz; ind < mapsize; ind++) {
			acmap[ind].uid = -1;
		}
		if (db_flag >= 1) {
			fprintf(stderr, "findacd(): expanding map table "
				"from %d to %d\n", omapsiz, mapsize);
		}
		addmap(uid, prid, jid, gid, omapsiz);
		if (con_jid) {
			availind++;
		}
		return(acmap[omapsiz].acp);

	}

	if (db_flag >= 1) {
		fprintf(stderr,"findacd(): null cacct map entry\n");
	}

	return ((struct cacct *)NULL);
}

/*
 *	Add new entry to map.
 */
static void
addmap(uid_t uid, uint64_t prid, uint64_t jid, gid_t gid, int ind)
{
	struct cacct *acp;

	acmap[ind].uid = uid;
	acmap[ind].prid = prid;
	acmap[ind].jid = jid;
	acmap[ind].gid = gid;

	/*
	 *	get memory
	 */
	if ((acp = (struct cacct *) malloc(sizeof(struct cacct))) == NULL) {
		acct_perr(ACCT_ABORT, errno,
			_("There was insufficient memory available when allocating '%s'."),
			"struct cacct");
	}

	/*
	 *	Insure zero starts.
	 */
	memset((char *)acp, '\0', sizeof(struct cacct));

	/*
	 *	Initialize ID.
	 */
	acp->ca_uid = uid;
	acp->ca_prid = prid;
	acp->ca_jid = jid;
	acp->ca_gid = gid;

	/*
	 *	Set pointer and return
	 */
	acmap[ind].acp = acp;
	return;
}

static void
wroutput() 
{
	int ind;

	if (total) {
		if (asciiout) {
			wrascii(&tcb);
		} else {
			if (create_hdr1(&tcb.ca_hdr, ACCT_CACCT) < 0) {
				acct_err(ACCT_WARN,
				       _("An error was returned from the call to the '%s' routine."),
					"header()");
			}

			if (writecacct (bofd,&tcb) != sizeof (struct cacct)) {
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred during the writing of file '%s'."),
					outfile);
			}
		}
		return;
	}

	for(ind = 0; ind < mapsize; ind++) {
		if (acmap[ind].uid == -1) break;
		if (asciiout) {
			wrascii(acmap[ind].acp);
		} else {
			if (create_hdr1(&acmap[ind].acp->ca_hdr,
					ACCT_CACCT) < 0) {
				acct_err(ACCT_WARN,
				       _("An error was returned from the call to the '%s' routine."),
					"header()");
			}

			if (writecacct (bofd,acmap[ind].acp) !=
					 sizeof(struct cacct)) {
				acct_perr(ACCT_ABORT, errno,
					_("An error occurred during the writing of file '%s'."),
					outfile);
			}
		}
	}
	return;
}

char fmt[] = "%d\t%.8s\t%.8s\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t";
char fmtv[] = "%d\t%.8s\t%.8s\t%e %e %e %e %e %e %lld %lld %lld %lld %lld %lld %lld %lld %e %e %e %e %e %e ";

static void
wrascii(struct cacct *tp) 
{
	int j;
	char acname[13];
	char *an;
	char *un;

	if (tp->ca_prid == CACCT_NO_ID) {
		strcpy(acname, "default");
	} else {
		if ((an = prid_to_name(tp->ca_prid)) == NULL) {
			an = "Unknown";
		}
		strncpy(acname,an,12);
		acname[12] = '\0';
	}

	if ((un = uid_to_name(tp->ca_uid)) == NULL) {
		un = "Unknown";
	}

	fprintf(ofd, verbose ? fmtv : fmt,
		tp->ca_uid, un, acname,
		tp->prime.cpuTime, tp->nonprime.cpuTime,
		tp->prime.kcoreTime, tp->nonprime.kcoreTime,
		tp->prime.kvirtualTime, tp->nonprime.kvirtualTime,
		tp->prime.charsXfr, tp->nonprime.charsXfr,
		tp->prime.logicalIoReqs, tp->nonprime.logicalIoReqs,
		tp->prime.minorFaultCount, tp->nonprime.minorFaultCount,
		tp->prime.majorFaultCount, tp->nonprime.majorFaultCount,
		tp->prime.cpuDelayTime, tp->nonprime.cpuDelayTime,
		tp->prime.blkDelayTime, tp->nonprime.blkDelayTime,
		tp->prime.swpDelayTime, tp->nonprime.swpDelayTime);

	fprintf(ofd, verbose ? "%e %lld \t%d\t%e\t%e\t" :
			"%.0f\t%lld\t%d\t%.0f\t%.0f\t",
		tp->ca_du,
		tp->ca_pc,
		tp->ca_dc,
		tp->ca_fee,
		tp->ca_sbu);

	fprintf(ofd,"\n");
}
