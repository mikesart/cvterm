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

window *winmgr_init()
{
    winmgr *wm = s_winmgr;
    if (wm)
        return wm->root;

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

    // create the window manager object
    wm = malloc(sizeof(winmgr));
    memset(wm, 0, sizeof(*wm));
    wm->h = handler_create(wm, (handler_proc)winmgr_proc);
    s_winmgr = wm;

    // Create the root window
    rect rc;
    int cy, cx;
    getmaxyx(stdscr, cy, cx);
    rect_set(&rc, 0, 0, cx, cy);
    wm->root = new_window(wm, NULL, stdscr, &rc, 0, WF_VISIBLE);

    // This allows the window manager to paint windows at
    // message queue idle time.
    message_set_hook(wm->h);

    return wm->root;
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

window *winmgr_get_root()
{
    winmgr *wm = s_winmgr;
    return wm->root;
}

window *window_create(window *parent, const rect *rc, handler h, uint32_t flags)
{
    winmgr *wm = s_winmgr;

    if (parent == NULL)
        parent = wm->root;

    // Coords passed in are parent relative. Make them screen relative.
    rect rcT = *rc;
    rect_offset(&rcT, parent->rc.left, parent->rc.top);

    // A window can be visible or hidden, or a window can be visible but without
    // ncurses WINDOW (like a container of other windows).
    WINDOW *win = NULL;
    if ((flags & WF_VISIBLE) && !(flags & WF_CONTAINER))
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
    message_data data;
    memset(&data, 0, sizeof(data));
    data.create.w = w;
    handler_call(h, WM_CREATE, &data);

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

handler window_set_handler(window *w, handler h)
{
    handler old = w->h;
    w->h = h;
    return old;
}

void window_set_visible(window *w, int visible)
{
    if (!visible)
    {
        if (w->flags & WF_VISIBLE)
        {
            if (w->win && w->win != stdscr)
            {
                delwin(w->win);
                w->win = NULL;
            }
            w->flags &= ~WF_VISIBLE;
        }
    }
    else
    {
        if (!(w->flags & WF_VISIBLE))
        {
            if (!(w->flags & WF_CONTAINER))
            {
                w->win = newwin(w->rc.bottom - w->rc.top, w->rc.right - w->rc.left, w->rc.top, w->rc.left);
            }
            w->flags |= WF_VISIBLE;
            window_invalidate(w);
        }
    }
}

void window_invalidate(window *w)
{
    // Mark children as invalid as well
    if (w->flags & WF_VISIBLE)
    {
        for (window *wT = w->child; wT != NULL; wT = wT->next)
            window_invalidate(wT);
    }

    // Mark this window invalid so it gets a paint message.
    if (w->win)
    {
        w->invalid = 1;
        w->wm->invalid = 1;
        message_hook_readable();
    }
}

window *find_invalid(window *w)
{
    if (!(w->flags & WF_VISIBLE))
        return NULL;

    for (window *child = w->child; child != NULL; child = child->next)
    {
        window *wT = find_invalid(child);
        if (wT != NULL)
            return wT;
    }

    if (w->invalid)
    {
        // stdscr can't be invalid if it has children.
        if (w->win != stdscr || w->child == NULL)
            return w;
    }

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
