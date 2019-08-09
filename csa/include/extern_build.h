/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2006-2007 Silicon Graphics, Inc. All Rights Reserved.
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

extern	struct	acctent	acctent;	/* accounting record buffer */

extern	struct	File	*file_tbl;	/* pointer to global file table */
extern  struct  wm_track *tr_wm;	/* global workload mgmt tracking */

extern	int	upIndex;		/* Maximum inuse uptime table index */
extern	int	upind;			/* Current uptime table index */
extern	int	Max_upTimes;		/* Maximum number of uptimes */
extern	int	Max_upIndex;		/* Maximum uptimes index */
extern	struct	Uptime	*uptime;	/* boot and reboot table from cfg rcd */

extern	int64_t	Pacct_offset;		/* offset into pacct file */
extern	int	Total_crecs;		/* total core records */
extern	int	Total_frecs;		/* total file records */

/*
 * Number of seconds between first process and an accounting system restart.
 * Used because pacct record time stamps are process start time not stop times.
 * Only important on the first uptime because from then on we use the
 * stop times not the start times as the boundary.
 */
extern	char	*Prg_name;
extern	char	*spacct;
extern	char	*pacct;

extern	time_t	cutoff;		/* time cutoff value */

extern	int	a_opt;	/* assume system crash/restart at uptime change */
extern	int	A_opt;	/* Abort on errors option */
extern	int	i_opt;	/* ignore bad records option */
extern	int	n_opt;	/* supress workload mgmt  unite option */
extern	int	S_opt;	/* Segment size option */
extern	int	t_opt;	/* Timer option */

extern	int	db_flag;	/* debug flag */

extern	int	MEMINT;		/* memory integral */
extern	int	MAXFILES;	/* max # of files to be processed */


/*
 *	routine prototypes.
 */
extern	void	Ndebug(char *, ...);

int	Build_segment(int);					/* build.c */
void	Build_segment_hash_tbl(int);				/* build.c */
void	Build_filetable();					/* build.c */
void	Build_tables(); 					/* build.c */
void	Condense_wm();						/* wkmgmt.c */
void	Dump_acct_hdr(struct achead *);				/* dump.c */
void	Dump_acctent(struct acctent *, char *);			/* dump.c */
void	Dump_rec(void *, int);					/* dump.c */
void	Dump_process_rec(struct acctcsa *);			/* dump.c */
void    Dump_wm_rec(struct wm_track *);                         /* dump.c */
void    Dump_wm_tbl(struct wm_track *, int, char *);            /* dump.c */
void    Dump_cont_wm(struct wm_track *, int, char *);           /* dump.c */
void    Dump_segment_rec(struct segment *, int, int);           /* dump.c */
void    Dump_segment_tbl();                                     /* dump.c */
void	Dump_tpbs_rec(struct tapebs *);				/* dump.c */
void	Dump_uptime_rec(int);					/* dump.c */
void	Dump_uptime_tbl(char *);				/* dump.c */
struct	segment	*expand_seg();					/* build.c */
struct	Crec *get_crec(struct wkmgmtbs *, uint); 		/* build.c */
struct	Frec	*get_frec(off_t, uint, uint);			/* build.c */
int	get_next_file(char *, int, int);			/* files.c */
struct	segment	*get_segptr(int, int, uint64_t, time_t, time_t, time_t);
								/* build.c */
struct  segment *seg_hashtable(struct segment **, int, uint64_t, int); 
								/* build.c*/
void	init_cmd(int, char**);					/* init.c */
void	init_config();						/* init.c */
void	init_wm();						/* wkmgmt.c */
int	open_file(char *, int);					/* files.c */
void	close_all_input_files();				/* files.c */
void	close_file(char *);					/* files.c */
void	Create_srec();						/* write.c */
double	compute_SBU(int, struct acctent *);			/* sbu.c */
int	process_eop(int, int, int);				/* process.c */
int	process_special(int, int, int);				/* special.c */
int	process_wm(int, int);					/* wkmgmt.c */
void	Shift_uptime(int, char *, int);				/* build.c */
void	add_segs_wm();						/* wkmgmt.c */

int	upcmp(const void *, const void *);			/* xxx.c */
