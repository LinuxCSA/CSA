/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2007 Silicon Graphics, Inc  All Rights Reserved.
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

#define NEWACCT		"NEWACCT"	/* new format report options */
#define MAX_FNAME	128
#define KBYTES		1024		/* bytes per kilobyte */
#define NOTAVAIL        -1	/* used when acct rec item not avail */
#define NAVSTR		"NA"	/* used when acct rec item not avail */
#define	SEC_PER_MIN	60	/* number of seconds per minute */

struct flowtree {
	uint32_t 	pid;
	uint32_t	ppid;
	uint64_t	jid;
	time_t		btime;
	double		ctime;
	char		comm[16];
	struct flowtree	*child;
	struct flowtree	*sibling;
};

extern	struct	acctent	acctrec;
extern	struct	flowtree flowtree;

extern	struct	achead	hdr;		/* accounting record header */
extern	struct	nqshdr	*nqhdr;		/* nqs buffer pointer */
extern	struct	tplist	*tp;		/* tape buffer pointer */

extern	char	*Prg_name;
extern	int	eof_mark;
extern	int	range[];
extern	int	*defmarks[];
extern	int	jafd;		/* file descriptor for file I/O */
extern	int	MEMINT;		/* memory integral index */

extern	char	*fn;		/* pointer to file name */
extern	int	temp_file;	/* temporary file used flag */
#ifdef HAVE_REGCOMP
#include <regex.h>
extern	regex_t	**names;
#else	/* HAVE_REGCOMP */
#ifdef HAVE_REGCMP
extern	char	**names;
#endif	/* HAVE_REGCMP */
#endif	/* HAVE_REGCOMP */

enum {
	OPTION_DELAY_CPU,
	OPTION_DELAY_BLKIO,
	OPTION_DELAY_SWAPIN,
	OPTION_DELAY_RUNTIME,
	OPTION_DELAY_LAST
};

extern	int	a_opt;		/* a option: select by array session ID */
extern	int	c_opt;		/* c option: command report */
extern	int	d_opt;		/* d option: daemon report */
extern	int	D_opt;		/* D option: debug option */
extern	int	e_opt;		/* e option: extended summary report */
extern	int	f_opt;		/* f option: command flow report */
extern	int	g_opt;		/* g option: select by group ID */
extern	int	h_opt;		/* h option: report hiwater mark */
extern	int	j_opt;		/* j option: select by job ID */
extern	int	J_opt;		/* J option: select all special pacct records */
extern	int	l_opt;		/* l option: long report */
extern	int	m_opt;		/* m option: mark position */
extern	int	M_opt;		/* M option: select by mark positioning */
extern	int	n_opt;		/* n option: select by command name */
extern	int	o_opt;		/* o option: other command report */
extern	int	p_opt;		/* p option: select by project ID */
extern	int	r_opt;		/* r option: raw mode */
extern	int	s_opt;		/* s option: summary report */
extern	int	t_opt;		/* t option: terminate job accounting */
extern	int	u_opt;		/* u option: select by user ID */
extern	int	y_opt;		/* y option: delay accounting report */

extern	int	numcpu;		/* current # of used multitasking CPUs */
extern	int	max_numcpu;	/* maximum # of used multitasking CPUs */

extern	uint64_t s_ash;		/* array session ID to select by */
extern	uint32_t s_gid;		/* group ID to select by */
extern	uint64_t s_jid;		/* job ID to select by */
extern	uint64_t s_prid;	/* project ID to select by */
extern	uint32_t s_uid;		/* user ID to select by */

extern	int	db_flag;	/* debug option value */

extern	void	bld_nqs();
extern	void	bld_tape();
extern	int	**getmarks(int, char **);
extern	struct	tphdr	*find_tp_base(char *);
extern	struct	rsvhdr	*find_tp_rsv(char *);
extern	int	pr_process();
extern	int	pr_special();
extern	void	printcmdhdr(int);
extern	void	printflwt(struct flowtree *, int, int);
extern	void	printmttime(double *);
extern	void	printohdr();
extern	void	printsumhdr();
extern	void	rpt_nqs();
extern	void	rpt_process();
extern	void	rpt_tape();
