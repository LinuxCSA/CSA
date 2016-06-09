/*
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

/*
 * hash.h - Declarations for structures and functions associated with 
 * hash table management 
 * Copyright (c) Chris Sturtivant, SGI, 2006
 */

#ifndef __HASH_H
#define __HASH_H

typedef struct hash_entry {
	void 	*entry_key;
	void 	*entry_data;
	struct hash_entry	*entry_next;
} hash_entry;

typedef struct {
	int	table_num_entries;
	int	table_key_size;
	hash_entry	**table_entries;	/* array of hash_entry ptrs */
} hash_table;

/* 
 * hash_create_table() allocates space for the requested size of table,
 *   and returns a pointer to the empty hash table.  The table_key_size
 *   is the size of the key in bytes.
 * hash_delete() frees
 * all the structures previously allocated for the hash table.
 *
 * hash_entry structures are marked as empty if the entry_key field is
 * NULL.
 */
extern hash_table  *hash_create_table(int num_entries, int key_size);
extern void 	 hash_delete_table(hash_table *table);

/*
 * hash_find() returns the matching data for key, or NULL if the key is
 *   not present in the table.
 * hash_enter() enters the data into the hash table under the specified
 *   key.  The key must not currently exist in the hash table.  The
 *   memory for the the key and data will be referenced in the hash table
 *   and so should not be temporary storage.  The function returns 0 on
 *   success, or non-zero on failure.
 * hash_update() enters the data into the has table under the specified
 *   key.  The key must already exist in the hash table.  The data will
 *   be copied into the table, so the data pointer need not point to
 *   permanent memory.  The function 0 on success, or non-zero on failure.
 * hash_delete() frees and deletes the entry from the hash table.  The
 *   key must exist in the table.
 */

extern void	*hash_find(void *key, hash_table *);
extern int	hash_enter(void *key, void *data, hash_table *);
extern int	hash_update(void *key, void *data, size_t data_size, hash_table *);
extern int	hash_delete(void *key, hash_table *, int delete_data);

/*
 * Error codes:
 *  HASH_OK	- No error occurred
 *  HASH_TABLE	- The table was invalid for some reason (e.g. too big).
 *  HASH_NOENT	- The key was not found in the table, or is invalid.
 *  HASH_EXISTS - The key already exists in the table
 */

#define HASH_OK		0
#define HASH_TABLE	1
#define HASH_NOENT	2
#define HASH_EXISTS	3

#endif
