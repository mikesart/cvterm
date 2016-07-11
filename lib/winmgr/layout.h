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
