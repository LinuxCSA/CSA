/*
 * Copyright (c) 2007-2008 Silicon Graphics, Inc All Rights Reserved.
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
 * hash.c - Hash table function definitions for hash table management.
 * Copyright (c) 2006, Chris Sturtivant, SGI
 */

#include <stdlib.h>
#include <string.h>

#include "hash.h"

hash_table *hash_create_table(int num_entries, int key_size)
{
hash_table  *table;

	table = malloc(sizeof(hash_table));
	if (!table)  return NULL;

	table->table_key_size = key_size;
	table->table_num_entries = num_entries;
	table->table_entries = malloc(sizeof(hash_entry (*[num_entries])));
	if (!table->table_entries)  return NULL;

	memset(table->table_entries, 0, num_entries * sizeof(hash_entry*));

	return table;
}


static void delete_entry(hash_entry *entry)
{
hash_entry  *child_entry = NULL;
	
	if (entry == NULL)
		return;

	if (entry->entry_next)  child_entry = entry->entry_next;

	if (entry->entry_key)  free(entry->entry_key);
	if (entry->entry_data)  free(entry->entry_data);

	free(entry);

	if (child_entry)
		delete_entry(child_entry);
}


void hash_delete_table(hash_table *table)
{
hash_entry  **entry;
int  i;
	/* First empty out the hash table */
	for (entry = table->table_entries; entry < table->table_entries + table->table_num_entries; entry++)  
		delete_entry(*entry);

	free(table->table_entries);
	free(table);
}


static unsigned int  get_hash(void *key, int key_size, int table_size)
{
unsigned int  index=0;

	while (key_size > 1) {
		if (key_size >= sizeof(int)) 
			index += *(unsigned int *)key;
		else if (key_size >= sizeof(short))  
			index += *(unsigned short *)key;
		else  index += *(unsigned char *)key;

		key = (char *)key + sizeof(int);
		key_size -= sizeof(int);
	}

	return index % table_size;
}


/*
 * key_match() compares two keys of length key_len
 * Returns 1 on a match, 0 on a fail
 */

int  key_match(void *key1, void *key2, int key_len)
{
char  *k1, *k2;
int	i;
	
	k1 = (char *)key1;
	k2 = (char *)key2;

	for (i = 0; i < key_len; i++) {
		if (*k1 != *k2)  return 0;
		k1++;
		k2++;
	}

	return 1;
}


void *hash_find(void *key, hash_table *table)
{
unsigned int	index;
hash_entry	*hptr;

	index = get_hash(key, table->table_key_size, table->table_num_entries);

	hptr = table->table_entries[index];
	while (hptr && hptr->entry_key && !key_match(hptr->entry_key, key, table->table_key_size))
		hptr = hptr->entry_next;

	if (!hptr || !hptr->entry_key) return NULL;
	return hptr->entry_data;
}


int hash_enter(void *key, void *data, hash_table *table)
{
unsigned int    index, key_size;
hash_entry      **hptr;

	key_size = table->table_key_size;
        index = get_hash(key, key_size, table->table_num_entries);

        hptr = table->table_entries + index;
	while (*hptr && !key_match((*hptr)->entry_key, key, key_size))
		hptr = &((*hptr)->entry_next);

	if (*hptr && key_match((*hptr)->entry_key, key, key_size))
		return HASH_EXISTS;

	/* 
	 * We need to add another entry onto the end of the list
	 * hptr points to the pointer than will hold the address of
	 * the new entry.  Both data and key parameters must point to
	 * allocated areas of memory.
	 */
	*hptr = malloc(sizeof(hash_entry));

	(*hptr)->entry_key = malloc(key_size);
	memcpy((*hptr)->entry_key, key, key_size);
	(*hptr)->entry_data = data;
	(*hptr)->entry_next = NULL;

	return HASH_OK;
}


int hash_update(void *key, void *data, size_t data_size, hash_table *table)
{
unsigned int    index, key_size;
hash_entry      *hptr;

	key_size = table->table_key_size;
        index = get_hash(key, table->table_key_size, table->table_num_entries);

        hptr = table->table_entries[index];
        while (hptr && !key_match(hptr->entry_key, key, key_size))
                hptr = hptr->entry_next;

        if (!hptr || !key_match(hptr->entry_key, key, key_size))
                return HASH_NOENT;	/* key was not found */

	/* We just overwrite the data pointed to by the entry, since the
	 * data area has already been allocated prior to hash_enter */
        memcpy(hptr->entry_data, data, data_size);

        return HASH_OK;
}


int hash_delete(void *key, hash_table *table, int delete_data)
{
unsigned int    index, key_size;
hash_entry      **hptr, *next;

	key_size = table->table_key_size;
        index = get_hash(key, table->table_key_size, table->table_num_entries);

        hptr = table->table_entries + index;
        while (*hptr && !key_match((*hptr)->entry_key, key, key_size))
                hptr = &(*hptr)->entry_next;

        if (!*hptr || !key_match((*hptr)->entry_key, key, key_size))
                return HASH_NOENT;      /* key was not found */

	next = (*hptr)->entry_next;	/* remember what comes next */
        free((*hptr)->entry_key);
	if (delete_data)
		free((*hptr)->entry_data);
	free(*hptr);

        *hptr = next;	/* Jump over the old entry in the list */

        return HASH_OK;
}
