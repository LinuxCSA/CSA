/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2007 Silicon Graphics, Inc All Rights Reserved.
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

#ifndef	_CONVERT_H
#define	_CONVERT_H

#include "cacct.h"
#include "cms.h"

/*
 *	This file contains the definitions that are required
 *	for the conversion routines.
 */

extern	int	rev_tab[];

/*
 *	On-the-fly conversion macros
 *	History: 05000		Linux 2.4, Linux 2.6.7
 *		 06000		Linux 2.6.16-rc1
 *
 * NOTE: The header revision number was defined as 02400 in earlier version.
 *	 However, since ah_revision was defined as 15-bit field (ah_magic
 *	 takes up 17 bits), the revision number is read as twice the value in 
 *	 new code. So, define it to be 05000 accordingly.
 */
#define	OTFC_BASE_REV	05000	/* the least Rev of CSA we support in Linux */

/* Record needs conversion if ah_revision is not current. */
#define	OTFC_NEEDED(H)	((H)->ah_revision != rev_tab[(H)->ah_type])

#define	CHECK_REV(H) {							\
	if (((H)->ah_revision < OTFC_BASE_REV) ||			\
	    ((H)->ah_revision > rev_tab[(H)->ah_type])) {             	\
		acct_err(ACCT_CAUT,                                     \
			_("%s: Bad record revision number %d., type=%d, rev=%d"),        \
			FUNC_NAME, (H)->ah_revision, (H)->ah_type, rev_tab[(H)->ah_type]);                   \
		return (0);						\
	}								\
}

/*
 *	Conversion record type definitions
 */
typedef	enum {
	REC_CSA,
	REC_MEM,
	REC_IO,
	REC_DELAY,
	REC_SOJ,
	REC_EOJ,
	REC_CFG,
	REC_WKMG,
	REC_JOB,
	REC_CACCT,
	REC_CMS,
	REC_MAX} acct_cvt;

/*
 *	Function prototypes for conversion routines
 */
int	convert_hdr(struct achead *, int, int);

int	convert_pacct(void *, int);
int	convert_pacct_csa(void *, int);
int	convert_pacct_mem(void *, int);
int	convert_pacct_io (void *, int);
int	convert_pacct_delay(void *, int);
int	convert_pacct_soj(void *, int);
int	convert_pacct_eoj(void *, int);
int	convert_pacct_cfg(void *, int);
int	convert_pacct_wkmg(void *, int);
int	convert_pacct_job(void *, int);

int	convert_cacct(void *, int);
int	convert_cms(void *, int);

int	verify_pacct(FILE *);
int	verify_cacct(struct cacct *, FILE *);
int	verify_cms(struct cms *, FILE *);

#endif	/* _CONVERT_H */
