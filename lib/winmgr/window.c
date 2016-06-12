#include <string.h>
#include <stdint.h>
#include "window.h"
#include "message.h"
#include "clog.h"

struct winmgr
{
    window *root;
    int invalid;
    handler h;
};

struct window
{
    winmgr *wm;
    window *next;
    window *parent;
    window *child;
    WINDOW *win;
    int invalid;
    rect rc;
    handler h;
    uint32_t flags;
};

static winmgr *s_winmgr;

window *new_window(winmgr *wm, window *parent, WINDOW *win, const rect *rc, handler h, uint32_t flags);
uint32_t winmgr_proc(winmgr *wm, int id, const message_data *data);

int winmgr_init()
{
    if (s_winmgr)
        return 1;

    // Initialize messaging
    message_init(sizeof(message_data));

    // Init ncurses mode
    initscr();

    // Don't buffer keystrokes
    cbreak();

    // Suppress automatic echoing of typed characters
    noecho();

    // Enable backspace, delete, and four arrow keys
    keypad(stdscr, TRUE);

    winmgr *wm = malloc(sizeof(winmgr));
    memset(wm, 0, sizeof(*wm));
    wm->h = handler_create(wm, (handler_proc)winmgr_proc);
    message_set_hook(wm->h);

    s_winmgr = wm;

    return 1;
}

void winmgr_shutdown()
{
    winmgr *wm = s_winmgr;
    if (!wm)
        return;

    if (wm->root)
    {
        window_destroy(wm->root);
        wm->root = NULL;
    }
    handler_destroy(wm->h);
    free(wm);

    endwin();

    s_winmgr = NULL;

    message_shutdown();
}

window *winmgr_create_root_window(handler h)
{
    winmgr *wm = s_winmgr;

    rect rc;
    int cy, cx;
    getmaxyx(stdscr, cy, cx);
    rect_set(&rc, 0, 0, cx, cy);

    wm->root = new_window(wm, NULL, stdscr, &rc, h, 0);

    return wm->root;
}

window *window_create(window *parent, const rect *rc, handler h, uint32_t flags)
{
    if (parent == NULL)
        return NULL;

    // Coords passed in are parent relative. Make them screen relative.
    rect rcT = *rc;
    rect_offset(&rcT, parent->rc.left, parent->rc.top);

    // A window can be visible or hidden, or a window can be visible but without
    // ncurses WINDOW (like a container of other windows).
    WINDOW *win = NULL;
    if ((flags & WF_VISIBLE) && !(flags & WF_NOPAINT))
    {
        win = newwin(rc->bottom - rc->top, rc->right - rc->left, rc->top, rc->left);
    }

    return new_window(parent->wm, parent, win, &rcT, h, flags);
}

window *new_window(winmgr *wm, window *parent, WINDOW *win, const rect *rc, handler h, uint32_t flags)
{
    window *w = malloc(sizeof(window));
    memset(w, 0, sizeof(*w));
    w->wm = wm;
    w->h = h;
    w->invalid = 0;
    w->win = win;
    w->rc = *rc;
    w->flags = flags;

    // Add it to the end of its siblings
    if (parent != NULL)
    {
        window **ppw = &parent->child;
        while (*ppw)
            ppw = &((*ppw)->next);
        *ppw = w;
        w->parent = parent;
    }

    // Notify client
    handler_call(h, WM_CREATE, NULL);

    // Mark it invalid so it gets a paint message
    if (w->win)
        window_invalidate(w);

    return w;
}

void window_destroy(window *w)
{
    // Destroy its children first
    for (window *wT = w->child; wT != NULL; wT = w->child)
    {
        window_destroy(wT);
    }

    handler_call(w->h, WM_DESTROY, NULL);

    // Unlink it    
    if (w->parent != NULL)
    {
        window **ppw = &w->parent->child;
        while (*ppw)
        {
            if (*ppw == w)
            {
                *ppw = w->next;
                break;
            }
            ppw = &((*ppw)->next);
        }
    }

    if (w->win && w->win != stdscr)
        delwin(w->win);

    // NOTE: nothing to invalidate?

    free(w);
}

void window_invalidate(window *w)
{
    if (w->win)
    {
        w->invalid = 1;
        w->wm->invalid = 1;
        message_hook_readable();
    }
}

window *find_invalid(window *w)
{
    for (window *child = w->child; child != NULL; child = child->next)
    {
        window *wT = find_invalid(child);
        if (wT != NULL)
            return wT;
    }

    if (w->invalid)
        return w;

    return NULL;
}

uint32_t winmgr_proc(winmgr *wm, int id, const message_data *data)
{
    switch (id)
    {
    case MM_READABLE:
        while (wm->invalid)
        {
            // Is there a window to paint?
            window *w = find_invalid(wm->root);
            if (w)
            {
                // Ask the window to paint.
                w->invalid = 0;
                handler_call(w->h, WM_PAINT, NULL);

                // Copy the ncurses WINDOW to the virtual screen
                wnoutrefresh(w->win);
            }
            else
            {
                // All done with painting. Update from the virtual screen
                // to the physical screen.
                doupdate();
                wm->invalid = 0;
            }
        }
        return 0;
    }

    return 0;
}

WINDOW *window_WIN(window *w)
{
    return w->win;
}
