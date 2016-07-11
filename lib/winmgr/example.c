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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include "winmgr.h"

#define CLOG_MAIN
#include "clog.h"

#define ID_CHILD_1 1
#define ID_CHILD_2 2

typedef struct
{
    window *w;
    handler h;
    char message[128];
} testwin;

uintptr_t testwin_proc(testwin *t, int id, const message_data *data)
{
    switch (id)
    {
    case WM_PAINT:
        {
            WINDOW *win = window_WIN(t->w);
            werase(win);
            wmove(win, 0, 0);
            wborder(win, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER,
                ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);

            // Get the ncurses window size to check for size mismatch
            int cx, cy;
            getmaxyx(win, cy, cx);
            int x = (cx - 10) / 2;
            int y = (cy - 3) / 2;
            wmove(win, y, x);
            waddstr(win, t->message);
            char str[32];
            sprintf(str, "ncurses: %d,%d", cx, cy);
            wmove(win, y + 1, x);
            waddstr(win, str);
            rect rc;
            window_rect(t->w, &rc);
            sprintf(str, "window: %d,%d", rc.right - rc.left, rc.bottom - rc.top);
            wmove(win, y + 2, x);
            waddstr(win, str);
        }
        break;

    case WM_DESTROY:
        handler_destroy(t->h);
        free(t);
        break;
    }

    return 0;
}

testwin *testwin_create(const char *message, const rect *rc, int id)
{
    testwin *t = malloc(sizeof(testwin));
    memset(t, 0, sizeof(*t));
    strncpy(t->message, message, sizeof(t->message) - 1);
    t->h = handler_create(t, (handler_proc)testwin_proc);
    t->w = window_create(NULL, rc, t->h, id);
    return t;
}

typedef struct
{
    handler h;
    handler h_old;
} root;

void get_child_rect(int id, rect *rc)
{
    rect rc_parent;
    window_rect(NULL, &rc_parent);

    if (id == ID_CHILD_1)
        rect_set(rc, 0, 0, (rc_parent.right - rc_parent.left) / 2, rc_parent.bottom);

    if (id == ID_CHILD_2)
        rect_set(rc, (rc_parent.right - rc_parent.left) / 2, 0, rc_parent.right, rc_parent.bottom);
}

uintptr_t root_proc(root *r, int id, const message_data *data)
{
    switch (id) {
    case WM_POSCHANGED:
        {
            rect rc;
            get_child_rect(ID_CHILD_1, &rc);
            window_set_pos(window_find_window(NULL, ID_CHILD_1), &rc);
            get_child_rect(ID_CHILD_2, &rc);
            window_set_pos(window_find_window(NULL, ID_CHILD_2), &rc);
        }
        break;

    case WM_DESTROY:
        {
            handler h_old = r->h_old;
            handler_destroy(r->h);
            free(r);
            return handler_call(h_old, id, data);
        }
        break;
    }

    return handler_call(r->h_old, id, data);
}


root *root_create()
{
    root *r = malloc(sizeof(root));
    memset(r, 0, sizeof(*r));
    r->h = handler_create(r, (handler_proc)root_proc);
    r->h_old = window_set_handler(NULL, r->h);
    return r;
}

void main_loop()
{
    int fd = message_fd();
    int fd_resize = winmgr_resize_fd();

    for ( ;; )
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        FD_SET(fd_resize, &fds);

        int fd_max = fd > fd_resize ? fd : fd_resize;
        int ret = select(fd_max + 1, &fds, NULL, NULL, NULL);

        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
        }

        if (FD_ISSET(fd_resize, &fds))
        {
            winmgr_resize();
        }

        if (FD_ISSET(fd, &fds))
        {
            message msg;
            while (message_get(&msg))
                message_dispatch(&msg);
        }
    }
}

void breakhere() {}

int main(int argc, char **argv)
{
#if 0
    char c;
    read(0, &c, 1);
    breakhere();
#endif

    //clog_init_path(0, "example.log");
    clog_init_fd(0, 1);
    clog_set_fmt(0, "%f(%F:%n): %l: %m\n");

    if (!winmgr_init())
    {
        clog_error(CLOG(0), "winmgr_init failed.");
        clog_free(0);
        return 1;
    }

    // The root will manage two children.
    root_create();

    rect rc;
    get_child_rect(ID_CHILD_1, &rc);
    testwin_create("child1", &rc, ID_CHILD_1);
    get_child_rect(ID_CHILD_2, &rc);
    testwin_create("child2", &rc, ID_CHILD_2);

    main_loop();

    winmgr_shutdown();

    clog_free(0);

    return 0;
}
