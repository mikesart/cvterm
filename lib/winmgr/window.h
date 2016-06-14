#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <ncurses.h>
#include "message.h"
#include "rect.h"

typedef struct winmgr winmgr;
typedef struct window window;

// Window flags
#define WF_VISIBLE      1
#define WF_CONTAINER    2

// Standard window messages
enum messages
{
    WM_CREATE = 1,
    WM_DESTROY,
    WM_PAINT,
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
        int key;
        uint8_t modifiers;
    } key;

    struct
    {
        char utf8[6 + 1]; // Unicode chars can be utf-8 encoded in 6 bytes
        uint8_t modifiers;
    } chr;

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

window *window_create(window *parent, const rect *rc, handler h, uint32_t flags);
void window_destroy(window *w);
void window_invalidate(window *w);
void window_set_visible(window *w, int visible);
handler window_set_handler(window *, handler h);
WINDOW *window_WIN();

#endif // __WINDOW_H__
