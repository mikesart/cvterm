#ifndef __WINDOW_H__
#define __WINDOW_H__

#include "message.h"
#include "rect.h"

typedef struct winmgr winmgr;
typedef struct window window;

// Window flags
#define WF_VISIBLE      1

// Standard window messages
enum messages
{
    WM_CREATE = 1,
    WM_DESTROY,
    WM_PAINT,
    WM_SETFOCUS,
    WM_KILLFOCUS,
    WM_CHAR,
    WM_KEY,
    WM_USER = 1000
};

// Modifier flags
#define MOD_SHIFT       1
#define MOD_ALT         2
#define MOD_CTRL        4

typedef union
{
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
        rect rc;
    } paint;

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

int winmgr_init(const char *termtype, int fd_in, int fd_out);
void winmgr_shutdown();
window *winmgr_create_root_window(handler h);
void winmgr_input_readable();

window *window_create(window *parent, const rect *rc, handler h, uint32_t flags);
int window_destroy(window *w);
void window_set_focus(window *w);
void window_invalidate_rect(window *w, const rect *rc);

void window_eraserect(window *w, const rect *rc);
void window_set_cursor_pos(window *w, int x, int y);
void window_drawtext(window *w, const char *text);

#endif // __WINDOW_H__
