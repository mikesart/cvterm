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

#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "window.h"

typedef struct laymgr laymgr;
typedef struct layout layout;

// Direction used in layout apis
enum
{
    DIR_LEFT = 0,
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN
};

// Convenience macros
#define IS_DIR_VERT(dir) (!((dir) & 1))
#define IS_DIR_PREV(dir) (!((dir) & 2))
#define DIR_REVERSE(dir) ((dir) ^ 2)

// Special size constant to pass into layout_split. If half doesn't fit,
// the closest size that does fit will be used.
#define SIZE_HALF -1

laymgr *laymgr_create(window *host);
void laymgr_destroy(laymgr *lm);

int layout_split(window *ref, window *client, int splitter, int size, int dir);
void layout_close(window *w);
int layout_move_edge(window *w, int delta, int edge);
window *layout_navigate_dir(window *w, int x, int y, int dir);
window *layout_navigate_ordered(window *w, int next);

#endif // __LAYOUT_H__
