#include <stdlib.h>
#include <string.h>
#include "splitter.h"

struct splitter
{
    window *w;
    handler h;
    layout *lay;
};

void splitter_paint(splitter *spltr)
{
    rect rc;
    window_rect(spltr->w, &rc);
    WINDOW *win = window_WIN(spltr->w);

    // TODO(scottlu): find crosspoints and draw those appropriately

    if (rc.right - rc.left == 1)
    {
        // Vertical splitter
        //wvline(win, ACS_VLINE | A_BOLD, rc.bottom - rc.top); // causes dashed lines
        wvline(win, ACS_VLINE, rc.bottom - rc.top);
    }

    if (rc.bottom - rc.top == 1)
    {
        // Horizontal splitter
        whline(win, ACS_HLINE | A_BOLD, rc.right - rc.left);
    }
}

uintptr_t splitter_proc(splitter *spltr, int id, const message_data *data)
{
    switch (id)
    {
    case WM_PAINT:
        splitter_paint(spltr);
        break;

    case WM_DESTROY:
        handler_destroy(spltr->h);
        free(spltr);
        break;
    }

    return 0;
}

splitter *splitter_create(window *parent, layout *lay)
{
    splitter *spltr = (splitter *)malloc(sizeof(splitter));
    memset(spltr, 0, sizeof(*spltr));
    spltr->h = handler_create(spltr, (handler_proc)splitter_proc);
    spltr->w = window_create(parent, NULL, spltr->h, 0);
    spltr->lay = lay;
    return spltr;
}

void splitter_destroy(splitter *spltr)
{
    window_destroy(spltr->w);
}

void splitter_set_layout(splitter *spltr, layout *lay)
{
    spltr->lay = lay;
}

window *splitter_window(splitter *spltr)
{
    return spltr->w;
}
