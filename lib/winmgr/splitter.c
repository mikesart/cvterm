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

#include <stdlib.h>
#include <string.h>
#include "splitter.h"

struct splitter
{
    window *w;
    handler h;
    layout *lay;
};

void splitter_paint(splitter *spltr)
{
    rect rc;
    window_rect(spltr->w, &rc);
    WINDOW *win = window_WIN(spltr->w);

    // TODO(scottlu): find crosspoints and draw those appropriately

    if (rc.right - rc.left == 1)
    {
        // Vertical splitter
        //wvline(win, ACS_VLINE | A_BOLD, rc.bottom - rc.top); // causes dashed lines
        wvline(win, ACS_VLINE, rc.bottom - rc.top);
    }

    if (rc.bottom - rc.top == 1)
    {
        // Horizontal splitter
        whline(win, ACS_HLINE | A_BOLD, rc.right - rc.left);
    }
}

uintptr_t splitter_proc(splitter *spltr, int id, const message_data *data)
{
    switch (id)
    {
    case WM_PAINT:
        splitter_paint(spltr);
        break;

    case WM_DESTROY:
        handler_destroy(spltr->h);
        free(spltr);
        break;
    }

    return 0;
}

splitter *splitter_create(window *parent, layout *lay)
{
    splitter *spltr = (splitter *)malloc(sizeof(splitter));
    memset(spltr, 0, sizeof(*spltr));
    spltr->h = handler_create(spltr, (handler_proc)splitter_proc);
    spltr->w = window_create(parent, NULL, spltr->h, 0);
    spltr->lay = lay;
    return spltr;
}

void splitter_destroy(splitter *spltr)
{
    window_destroy(spltr->w);
}

void splitter_set_layout(splitter *spltr, layout *lay)
{
    spltr->lay = lay;
}

window *splitter_window(splitter *spltr)
{
    return spltr->w;
}
