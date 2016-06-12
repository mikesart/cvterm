/**************************************************************************
 *
 * Copyright (c) 2016, Scott Ludwig <scottlu@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "handle.h"

typedef struct
{
    void *ptr;
    int next;
    uint16_t unique;
} handle_entry;

typedef struct handle_table
{
    handle_entry *entries;
    int count;
    int first;
    uint16_t unique;
} handle_table;

static handle_table *s_table;

#define HANDLE_COUNT_INCREMENT 32
#define NEXT_FREE_INVALID -1
#define UNIQUE_INVALID 0

void handle_table_init()
{
    if (!s_table)
    {
        handle_table *table = malloc(sizeof(handle_table));
        memset(table, 0, sizeof(*table));
        table->first = NEXT_FREE_INVALID;
        s_table = table;
    }
}

void handle_table_shutdown()
{
    handle_table *table = s_table;

    if (table)
    {
        if (table->entries)
            free(table->entries);
        free(table);
        s_table = NULL;
    }
}
    
handle handle_alloc(void *ptr)
{
    handle_table *table = s_table;

    if (table->first == NEXT_FREE_INVALID)
    {
        if (table->count == 0)
        {
            assert(table->entries == NULL);
            table->entries = (handle_entry *)malloc(sizeof(handle_entry) * HANDLE_COUNT_INCREMENT);
        }
        else
        {
            table->entries = (handle_entry *)realloc(table->entries, sizeof(handle_entry) * table->count + HANDLE_COUNT_INCREMENT);
        }
        memset(&table->entries[table->count], 0, sizeof(handle_entry) * HANDLE_COUNT_INCREMENT);

        for (int i = table->count + HANDLE_COUNT_INCREMENT - 1; i >= table->count; i--)
        {
            table->entries[i].next = table->first;
            table->first = i;
        }

        table->count += HANDLE_COUNT_INCREMENT;
    }

    uint16_t index = table->first;
    handle_entry *entry = &table->entries[index];
    table->first = entry->next;
    entry->ptr = ptr;
    table->unique++;
    if (table->unique == UNIQUE_INVALID)
        table->unique = 1;
    entry->unique = table->unique;
    entry->next = NEXT_FREE_INVALID;
    return (table->unique << 16) | index;
}

void handle_free(handle h)
{
    handle_table *table = s_table;

    if (handle_get_ptr(h))
    {
        uint16_t index = (uint16_t)h;
        handle_entry *entry = &table->entries[index];
        entry->next = table->first;
        entry->unique = UNIQUE_INVALID;
        entry->ptr = NULL;
        table->first = index;
    }
}

void *handle_get_ptr(handle h)
{
    handle_table *table = s_table;

    uint16_t index = (uint16_t)h;
    if (index < table->count)
    {
        uint16_t unique = (uint16_t)(h >> 16);
        handle_entry *entry = &table->entries[index];
        if (entry->unique != UNIQUE_INVALID && entry->unique == unique)
        {
            return entry->ptr;
        }
    }
    return NULL;
}
