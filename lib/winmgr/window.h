#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <ncurses.h>
#include "message.h"
#include "rect.h"

typedef struct winmgr winmgr;
typedef struct window window;

enum messages
{
    WM_CREATE = 1,
    WM_DESTROY,
    WM_PAINT,
    WM_POSCHANGED,
    WM_USER = 1000
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
    } pos_changed;

    struct
    {
        window *w;
    } focus_change;
} message_data;

typedef struct
{
    message_header h;
    message_data data;
} message;

window *winmgr_init();
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
void window_rect(window *w, rect *rc);
WINDOW *window_WIN();

#endif // __WINDOW_H__
