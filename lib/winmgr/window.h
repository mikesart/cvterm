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
