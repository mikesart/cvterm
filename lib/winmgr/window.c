#include <string.h>
#include <stdint.h>
#include <tickit.h>
#include "window.h"
#include "message.h"
#include "key.h"
#include "clog.h"

struct winmgr
{
    TickitTerm *tt;
    window *root;
    handler h;
};

struct window
{
    winmgr *wm;
    TickitWindow *tw;
    TickitRenderBuffer *rb;
    TickitPen *pen;
    uint32_t draw_attrs;
    int clr_fg;
    int clr_bg;
    bool setpen;
    handler h;
    uint32_t flags;
};

static winmgr *s_winmgr;

int tickit_window_event_proc(TickitWindow *tw, TickitEventType ev, void *info, void *user);
window *new_window(winmgr *wm, TickitWindow *tw, handler h, uint32_t flags);
uint32_t winmgr_proc(winmgr *wm, int id, const message_data *data);

#define WM_CHECKFLUSH (WM_USER + 0)

int winmgr_init(const char *termtype, int fd_in, int fd_out)
{
    if (s_winmgr)
        return 1;

    message_init(sizeof(message_data));

    if (!termtype)
    {
        termtype = getenv("TERM");
        if (!termtype)
            termtype = "xterm";
    }
    TickitTerm *tt = tickit_term_new_for_termtype(termtype);
    if (!tt)
        return 0;

    tickit_term_set_input_fd(tt, fd_in);
    tickit_term_set_output_fd(tt, fd_out);
    tickit_term_await_started_msec(tt, 50);
    tickit_term_observe_sigwinch(tt, true);
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
    tickit_term_clear(tt);

    winmgr *wm = malloc(sizeof(winmgr));
    memset(wm, 0, sizeof(*wm));
    wm->tt = tt;
    wm->h = handler_create(wm, (handler_proc)winmgr_proc);

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
    tickit_term_observe_sigwinch(wm->tt, false);
    tickit_term_destroy(wm->tt);
    handler_destroy(wm->h);
    free(wm);
    s_winmgr = NULL;

    message_shutdown();
}

window *winmgr_create_root_window(handler h) 
{
    winmgr *wm = s_winmgr;

    TickitWindow *troot = tickit_window_new_root(wm->tt);
    if (!troot)
        return NULL;

    wm->root = new_window(wm, troot, h, 0);

    return wm->root;
}

void winmgr_input_readable()
{
    winmgr *wm = s_winmgr;

    tickit_term_input_readable(wm->tt);
}

window *window_create(window *parent, const rect *rc, handler h, uint32_t flags)
{
    TickitRect trc;
    rect_to_tickitrect(rc, &trc);

    TickitWindowFlags tflags = 0;
    if (!(flags & WF_VISIBLE))
        tflags |= TICKIT_WINDOW_HIDDEN;

    TickitWindow *tw = tickit_window_new(parent->tw, trc, tflags);
    if (!tw)
        return NULL;

    window *w = new_window(parent->wm, tw, h, flags);

    return w;
}

window *new_window(winmgr *wm, TickitWindow *tw, handler h, uint32_t flags)
{
    window *w = malloc(sizeof(window));
    memset(w, 0, sizeof(*w));
    w->wm = wm;
    w->tw = tw;
    w->h = h;
    w->flags = flags;

    // Set which window is bound to tw.
    tickit_window_set_user(tw, w);

    // We want all these events from libtickit.
    TickitEventType event_type = TICKIT_EV_KEY | TICKIT_EV_MOUSE |
        TICKIT_EV_GEOMCHANGE | TICKIT_EV_EXPOSE | TICKIT_EV_FOCUS |
        TICKIT_EV_DESTROY;

    // Don't bother remembering the bind id. The event handler binding
    // gets cleaned up on tickit window destroy
    tickit_window_bind_event(w->tw, event_type, 0, tickit_window_event_proc, w);

    // Create a pen for drawing attributes
    w->pen = tickit_pen_new();
    w->draw_attrs = 0;
    w->clr_fg = -1;
    w->clr_bg = -1;
    w->setpen = false;

    // Notify client
    handler_call(h, WM_CREATE, NULL);

    // libtickit accumulates damage rects and waits for a call to
    // tickit_window_flush.  Ideally we'd know when this happens rather
    // than relying on this manual approach. The state is not really queued messages
    // but instead just a "dirty" flag. TODO: modify libtickit to get this.
    message_post(wm->h, WM_CHECKFLUSH, NULL);

    return w;
}

int window_destroy(window *w)
{
    // Just destroy the ticket window; that will in turn destroy children, and our
    // wrapping window * will get freed in the TICKIT_EV_DESTROY callback.
    tickit_window_destroy(w->tw);
    return 1;
}

void window_set_focus(window *w)
{
    tickit_window_take_focus(w->tw);
}

void window_invalidate_rect(window *w, const rect *rc)
{
    if (rc)
    {
        TickitRect trc;
        rect_to_tickitrect(rc, &trc);
        tickit_window_expose(w->tw, &trc);
    }
    else
    {
        tickit_window_expose(w->tw, NULL);
    }

    message_post(w->wm->h, WM_CHECKFLUSH, NULL);
}

uint32_t winmgr_proc(winmgr *wm, int id, const message_data *data)
{
    switch (id)
    {
    case WM_CHECKFLUSH:
        tickit_window_flush(wm->root->tw);
        break;
    }

    return 0;
}

int handle_expose(window *w, TickitExposeEventInfo *info)
{
    // tickit expects this event to be handled synchronously, because the
    // drawing context (renderbuffer) is pre-clipped (unlike Windows, where
    // you call BeginPaint in the WM_PAINT handler to clip).
    //
    // For now only support rendering during expose callbacks.
    w->rb = info->rb;
    tickit_renderbuffer_save(w->rb);
    if (w->draw_attrs || w->clr_fg != -1 || w->clr_bg != -1)
        w->setpen = true;

    message_data data;
    memset(&data, 0, sizeof(data));
    tickitrect_to_rect(&info->rect, &data.paint.rc);
    handler_call(w->h, WM_PAINT, &data);

    tickit_renderbuffer_restore(w->rb);
    w->rb = NULL;

    return 1;
}

int handle_focus(window *w, TickitFocusEventInfo *info)
{
    message_data data;
    memset(&data, 0, sizeof(data));
    if (info->win)
        data.focus_change.w = tickit_window_get_user(info->win);

    switch (info->type)
    {
    case TICKIT_FOCUSEV_IN:
        handler_call(w->h, WM_SETFOCUS, &data);
        break;

    case TICKIT_FOCUSEV_OUT:
        handler_call(w->h, WM_KILLFOCUS, &data);
        break;
    }

    return 1;
}

int tickit_window_event_proc(TickitWindow *tw, TickitEventType ev, void *info, void *user)
{
    window *w = (window *)user;

    // NOTE: TicketEventType is really a flag enumeration, and ev
    // can (and sometimes does) have multiple 'event' flags set.

    if (ev & TICKIT_EV_KEY)
    {
        message_data data;
        TickitKeyEventInfo *evt = (TickitKeyEventInfo *)info;
        switch (evt->type)
        {
        case TICKIT_KEYEV_TEXT:
            if (char_message_data(evt, &data))
            {
                message_post(w->h, WM_CHAR, &data);
                return 1;
            }
            break;
        
        case TICKIT_KEYEV_KEY:
            if (key_message_data(evt, &data))
            {
                message_post(w->h, WM_KEY, &data);
                return 1;
            }
            break;
        }
    }

    if (ev & TICKIT_EV_EXPOSE)
    {
        return handle_expose(w, (TickitExposeEventInfo *)info);
    }

    if (ev & TICKIT_EV_FOCUS)
    {
        return handle_focus(w, (TickitFocusEventInfo *)info);
    }

    if (ev & TICKIT_EV_DESTROY)
    {
        // This event occurs when the ticket window is being freed.
        // Free our wrapping object.
        handler_call(w->h, WM_DESTROY, NULL);
        tickit_window_set_pen(tw, NULL);
        tickit_pen_destroy(w->pen);
        tickit_window_set_user(tw, NULL);
        free(w);
        return 1;
    }

    // Keep running the tickit hook list.
    return 0;
}

void window_set_cursor_pos(window *w, int x, int y)
{
    tickit_renderbuffer_goto(w->rb, y, x);
}

void window_eraserect(window *w, const rect *rc)
{
    TickitRect trc;
    rect_to_tickitrect(rc, &trc);
    tickit_renderbuffer_eraserect(w->rb, &trc);
}

void window_drawtext(window *w, const char *text)
{
    // _setpen is ineffcient: the implementation allocates a new pen
    // and copies every time, however setting the pen is necessary
    // for the renderbuffer to pick up any new attributes.
    if (w->setpen)
    {
        tickit_renderbuffer_setpen(w->rb, w->pen);
        w->setpen = false;
    }
    tickit_renderbuffer_text(w->rb, text);
}

void window_set_draw_attrs(window *w, uint32_t attrs)
{
    // Try to be clever about setting attrs to avoid calling
    // tickit_renderbuffer_setpen as much as possible.
    uint32_t attrs_set = attrs & ~w->draw_attrs;
    if (!attrs_set)
        return;
    w->draw_attrs |= attrs_set;
    w->setpen = true;

    if (attrs_set & DRAW_ATTR_BOLD)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_BOLD, true);
    if (attrs_set & DRAW_ATTR_UNDERLINE)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_UNDER, true);
    if (attrs_set & DRAW_ATTR_ITALIC)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_ITALIC, true);
    if (attrs_set & DRAW_ATTR_REVERSE)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_REVERSE, true);
    if (attrs_set & DRAW_ATTR_STRIKEOUT)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_STRIKE, true);
    if (attrs_set & DRAW_ATTR_BLINK)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_BLINK, true);
}

void window_clear_draw_attrs(window *w, uint32_t attrs)
{
    // Try to be clever about setting attrs to avoid calling
    // tickit_renderbuffer_setpen as much as possible.
    uint32_t attrs_clear = attrs & w->draw_attrs;
    if (!attrs_clear)
        return;
    w->draw_attrs &= ~attrs_clear;
    w->setpen = true;

    if (attrs_clear & DRAW_ATTR_BOLD)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_BOLD, false);
    if (attrs_clear & DRAW_ATTR_UNDERLINE)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_UNDER, false);
    if (attrs_clear & DRAW_ATTR_ITALIC)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_ITALIC, false);
    if (attrs_clear & DRAW_ATTR_REVERSE)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_REVERSE, false);
    if (attrs_clear & DRAW_ATTR_STRIKEOUT)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_STRIKE, false);
    if (attrs_clear & DRAW_ATTR_BLINK)
        tickit_pen_set_bool_attr(w->pen, TICKIT_PEN_BLINK, false);
}

void window_set_fg_color(window *w, int clr)
{
    if (w->clr_fg == clr)
        return;
    w->clr_fg = clr;
    w->setpen = true;
    tickit_pen_set_colour_attr(w->pen, TICKIT_PEN_FG, clr);
}

void window_set_bg_color(window *w, int clr)
{
    if (w->clr_bg == clr)
        return;
    w->clr_bg = clr;
    w->setpen = true;
    tickit_pen_set_colour_attr(w->pen, TICKIT_PEN_BG, clr);
}
