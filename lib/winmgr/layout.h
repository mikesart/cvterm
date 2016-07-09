#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "window.h"

typedef struct laymgr laymgr;
typedef struct layout layout;

// Split direction passed in to layout_split
#define DIR_LEFT 0
#define DIR_UP 1
#define DIR_RIGHT 2
#define DIR_DOWN 3

// Special size constant to pass into layout_split. If half doesn't fit,
// the closest size that does fit will be used.
#define SIZE_HALF -1

// The layout hosting a window doesn't change as splits occur. However the
// root layout may change as splits occur so instead of stashing it around
// call laymgr_root() instead.
laymgr *laymgr_create(window *host);
void laymgr_destroy(laymgr *lm);
layout *laymgr_root(laymgr *lm);
layout *laymgr_find(laymgr *lm, window *w);

int layout_set_window(layout *lay, window *w);
window *layout_window(layout *lay);
layout *layout_split(layout *lay, window *w, int splitter, int size, int dir);
void layout_close(layout *lay);
int layout_move_edge(layout *lay, int delta, int edge);

#endif // __LAYOUT_H__
