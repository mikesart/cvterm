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

#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "handle.h"
#include "message.h"

typedef struct
{
    handler_proc proc;
    void *user;
} message_handler;

typedef struct
{
    MESSAGE_HEADER;
} message_header;

typedef struct queue_item
{
    struct queue_item *next;
    message_header header;
    // data_size bytes is appended
} queue_item;

typedef struct
{
    queue_item *first;
    int data_size;
    int pipe[2];
    int readable;
    handler hook;
} message_queue;

static message_queue *s_queue;

//
// message (queue) functions
//

int message_init(int data_size)
{
    message_queue *queue = s_queue;

    if (!queue)
    {
        int pipe_fds[2];
        if (pipe(pipe_fds))
            return 0;
        queue = malloc(sizeof(message_queue));
        memset(queue, 0, sizeof(*queue));
        queue->data_size = data_size;
        queue->pipe[0] = pipe_fds[0];
        queue->pipe[1] = pipe_fds[1];
        handle_table_init();
        s_queue = queue;
    }

    return 1;
}

void message_shutdown()
{
    message_queue *queue = s_queue;

    if (queue)
    {
        for (queue_item *item = queue->first; item != NULL; item = queue->first)
        {
            queue->first = item->next;
            free(item);
        }
        close(queue->pipe[0]);
        close(queue->pipe[1]);
        free(queue);
        handle_table_shutdown();
        s_queue = NULL;
    }
}

int message_fd()
{
    message_queue *queue = s_queue;
    return queue->pipe[0];
}

void set_readable(int readable)
{
    message_queue *queue = s_queue;

    if (!readable)
    {
        if (queue->readable)
        {
            // Read the char in the pipe so the outer select will block
            unsigned char b;
            if (read(queue->pipe[0], &b, sizeof(b)))
                queue->readable = 0;
        }
    }
    else
    {
        if (!queue->readable)
        {
            // Write a char to the pipe so the outer select will be signaled
            // and check for messages.
            unsigned char b = 0;
            if (write(queue->pipe[1], &b, sizeof(b)))
                queue->readable = 1;
        }
    }
}

void message_hook_readable()
{
    set_readable(1);
}

int message_get(void *msg)
{
    message_queue *queue = s_queue;

    if (queue->first)
    {
        queue_item *next = queue->first->next;
        memcpy(msg, &queue->first->header, sizeof(message_header) + queue->data_size);
        free(queue->first);
        queue->first = next;
        return 1;
    }

    int readable = handler_call(queue->hook, MM_READABLE, NULL);
    if (!readable && !queue->first)
        set_readable(0);

    return 0;
}

void message_post(handler h, int id, const void *data)
{
    message_queue *queue = s_queue;

    int size = sizeof(queue_item) + queue->data_size;
    queue_item *item = malloc(size);
    memset(item, 0, size);
    item->header.h = h;
    item->header.id = id;
    if (data)
        memcpy(item + 1,  data, queue->data_size);

    queue_item **ppitem = &queue->first;
    while (*ppitem)
        ppitem = &((*ppitem)->next);
    *ppitem = item;
    item->next = NULL;
    set_readable(1);
}

void message_dispatch(void *msg)
{
    message_queue *queue = s_queue;
    message_header *header = (message_header *)msg;
    if (queue->data_size)
    {
        handler_call(header->h, header->id, header + 1);
    }
    else
    {
        handler_call(header->h, header->id, NULL);
    }
}

void message_set_hook(handler hook)
{
    message_queue *queue = s_queue;
    queue->hook = hook;
    set_readable(1);
}

//
// handler functions
//

handler handler_create(void *user, handler_proc proc)
{
    message_handler *m = malloc(sizeof(message_handler));
    memset(m, 0, sizeof(*m));
    m->user = user;
    m->proc = proc;
    return handle_alloc(m);
}

void handler_destroy(handler h)
{
    message_handler *m = (message_handler *)handle_get_ptr(h);
    if (m)
    {
        handle_free(h);
        free(m);
    }
}

uint32_t handler_call(handler h, int id, const void *data)
{
    message_handler *m = (message_handler *)handle_get_ptr(h);
    if (m)
    {
        return m->proc(m->user, id, data);
    }

    return 0;
}
