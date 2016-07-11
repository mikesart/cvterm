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

#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <ncurses.h>
#include "message.h"
#include "rect.h"

typedef struct winmgr winmgr;
typedef struct window window;

enum
{
    WM_CREATE = 1,
    WM_DESTROY,
    WM_PAINT,
    WM_POSCHANGED,
    WM_GETMINSIZE,
    WM_SETFOCUS,
    WM_LOSEFOCUS,
    WM_QUIT,
    WM_USER = 0x1000
};

typedef union
{
    struct
    {
        window *w;
    } create;

    struct
    {
        const rect *rc_old;
        const rect *rc_new;
        int resized;
    } pos_changed;

    struct
    {
        window *w;
    } focus_change;

    struct
    {
        int *width;
        int *height;
    } size_min;

} message_data;

typedef struct
{
    MESSAGE_HEADER;
    message_data data;
} message;

int winmgr_init();
void winmgr_shutdown();
void winmgr_update();
int winmgr_resize_fd();
void winmgr_resize();
void winmgr_set_focus(window *w);
window *winmgr_focus();

window *window_create(window *parent, const rect *rc, handler h, int id);
void window_destroy(window *w);
void window_invalidate(window *w);
void window_set_visible(window *w, int visible);
int window_set_pos(window *w, const rect *rc);
window *window_find_window(window *w, int id);
handler window_set_handler(window *w, handler h);
handler window_handler(window *w);
void window_rect(window *w, rect *rc);
void window_map_point(window *from, window *to, int *x, int *y);
WINDOW *window_WIN();

#endif // __WINDOW_H__
