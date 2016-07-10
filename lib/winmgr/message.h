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

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>

typedef uint32_t handler;

#define MM_USER     0xff00
#define MM_READABLE (MM_USER + 1)

#define MESSAGE_HEADER handler h; int id

int message_init(int data_size);
void message_shutdown();
int message_fd();
int message_get(void *msg);
void message_post(handler h, int id, const void *data);
void message_dispatch(void *msg);
void message_set_hook(handler h);
void message_hook_readable();

typedef uint32_t (*handler_proc)(void *user, int id, const void *data);

handler handler_create(void *user, handler_proc proc);
void handler_destroy(handler h);
uint32_t handler_call(handler h, int id, const void *data);

#endif // __MESSAGE_H__
