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

/*
 *	NQS and workload mgmt type and subtype names are defined in csaacct.h.
 *	The information in this file must be updated when the NQS and/or
 *	workload mgmt data in csaacct.h is changed.
 */

#define NNQ_TYPES	8		/* Number of NQ_xxx types */

struct rectypes
{
	char			*type;	/* NQ_xxxx/WM_xxx type */
	struct subtype_name	*sub;	/* subtypes */
};

struct subtype_name
{
	char	*type;
};

struct subtype_name Rec_type[] =	/* Record types */
{
/*	type		index	*/
	"INFO",		/* 1 */
	"RECV",		/* 2 */
	"INIT",		/* 3 */
	"TERM",		/* 4 */
	"DISP",		/* 5 */
	"SENT",		/* 6 */
	"SPOOL",	/* 7 */
	"CON",		/* 8 */
	""
};

struct subtype_name Info_subtype[] =	/* NQ_INFO/WM_INFO subtypes */
{
/*	type		index	*/
	"ACCTON",	/* 1 */
	"ACCOFF",	/* 2 */
	""
};

struct subtype_name NQ_Sent_subtype[] =	/* NQ_SPOOL subtypes */
{
/*	type		index	*/
	" ",		/* 1 */
	" ",		/* 2 */
	" ",		/* 3 */
	"INIT",		/* 4 */
	" ",		/* 5 */
	"TERM",		/* 6 */
	""
};

struct subtype_name Recv_subtype[] =	/* NQ_RECV subtypes */
{
/*	type		index	*/
	"NEW",		/* 1 */
	"LOCAL",	/* 2 */
	"REMOTE",	/* 3 */
	""
};



struct subtype_name NQ_Init_subtype[] =	/* NQ_INIT subtypes */
{
/*	type		index	*/
	"START",	/* 1 */
	"RSTART",	/* 2 */
	"RERUN",	/* 3 */
	"SENT",		/* 4 */
	"SPOOL",	/* 5 */
	""
};

struct subtype_name NQ_Term_subtype[] =	/* NQ_TERM subtypes */
{
/*	type		index	*/
	"EXIT",		/* 1 */
	"REQUE",	/* 2 */
	"PREMPT",	/* 3 */
	"HOLD",		/* 4 */
	"OPRRUN",	/* 5 */
	"SENT",		/* 6 */
	"SPOOL",	/* 7 */
	"RERUN",	/* 8 */
	""
};

struct subtype_name NQ_Spool_subtype[] = /* NQ_SPOOL subtypes */
{
/*	type		index	*/
	" ",		/* 1 */
	" ",		/* 2 */
	" ",		/* 3 */
	" ",		/* 4 */
	"INIT",		/* 5 */
	" ",		/* 6 */
	"TERM",		/* 7 */
	""
};

struct subtype_name NQ_Disp_subtype[] =	/* NQ_DISP subtypes */
{
/*	type		index	*/
	"NORM",		/* 1 */
	"BOOT",		/* 2 */
	""
};

struct subtype_name Con_subtype[] =	/* NQ_CON subtypes */
{
/*	type		index	*/
	"",		/* 1 */
};

struct rectypes NQS_type[NNQ_TYPES] =
{
/*	type		subtypes		index	*/
	{"INFO",	Info_subtype},		/* 1 */
	{"RECV",	Recv_subtype},		/* 2 */
	{"INIT",	NQ_Init_subtype},	/* 3 */
	{"TERM",	NQ_Term_subtype},	/* 4 */
	{"DISP",	NQ_Disp_subtype},	/* 5 */
	{"SENT",	NQ_Sent_subtype},	/* 6 */
	{"SPOOL",	NQ_Spool_subtype},	/* 7 */
	{"CON",		Con_subtype}		/* 8 */
};

/* Definitions specific to workload management. */

#define NWM_TYPES	6		/* Number of WM_xxx types */

struct subtype_name WM_Init_subtype[] =	/* WM_INIT subtypes */
{
/*	type		index	*/
	"START",	/* 1 */
	"RSTART",	/* 2 */
	"RERUN",	/* 3 */
	"SPOOL",	/* 4 */
	""
};

struct subtype_name WM_Term_subtype[] =	/* WM_TERM subtypes */
{
/*	type		index	*/
	"EXIT",		/* 1 */
	"REQUE",	/* 2 */
	"HOLD",		/* 3 */
	"RERUN",	/* 4 */
	"MIGRA",	/* 5 */
	"SPOOL",	/* 6 */
	""
};

struct subtype_name WM_Spool_subtype[] = /* WM_SPOOL subtypes */
{
/*	type		index	*/
	" ",		/* 1 */
	" ",		/* 2 */
	" ",		/* 3 */
	"INIT",		/* 4 */
	" ",		/* 5 */
	"TERM",		/* 6 */
	""
};

struct rectypes WM_type[NWM_TYPES] =
{
/*	type		subtypes		index	*/
	{"INFO",	Info_subtype},		/* 1 */
	{"RECV",	Recv_subtype},		/* 2 */
	{"INIT",	WM_Init_subtype},	/* 3 */
	{"SPOOL",	WM_Spool_subtype},	/* 4 */
	{"TERM",	WM_Term_subtype},	/* 5 */
	{"CON",		Con_subtype}		/* 6 */
};
