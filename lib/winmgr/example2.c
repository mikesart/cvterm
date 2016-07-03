#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include "winmgr.h"

#define CLOG_MAIN
#include "clog.h"

typedef struct
{
    window *w;
    handler h;
    char message[128];
} testwin;

uint32_t testwin_proc(testwin *t, int id, const message_data *data)
{
    switch (id)
    {
    case WM_PAINT:
        {
            WINDOW *win = window_WIN(t->w);
            werase(win);
            wmove(win, 0, 0);
            wborder(win, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER,
                ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);

            // Get the ncurses window size to check for size mismatch
            int cx, cy;
            getmaxyx(win, cy, cx);
            int x = (cx - 10) / 2;
            int y = (cy - 3) / 2;
            wmove(win, y, x);
            waddstr(win, t->message);
            char str[32];
            sprintf(str, "ncurses: %d,%d", cx, cy);
            wmove(win, y + 1, x);
            waddstr(win, str);
            rect rc;
            window_rect(t->w, &rc);
            sprintf(str, "window: %d,%d", rc.right - rc.left, rc.bottom - rc.top);
            wmove(win, y + 2, x);
            waddstr(win, str);
        }
        break;

    case WM_DESTROY:
        handler_destroy(t->h);
        free(t);
        break;
    }

    return 0;
}

testwin *testwin_create(const char *message)
{
    testwin *t = malloc(sizeof(testwin));
    memset(t, 0, sizeof(*t));
    strncpy(t->message, message, sizeof(t->message) - 1);
    t->h = handler_create(t, (handler_proc)testwin_proc);
    t->w = window_create(NULL, NULL, t->h, 0);
    return t;
}

void main_loop()
{
    int fd = message_fd();
    int fd_resize = winmgr_resize_fd();

    for ( ;; )
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        FD_SET(fd_resize, &fds);

        int fd_max = fd > fd_resize ? fd : fd_resize;
        int ret = select(fd_max + 1, &fds, NULL, NULL, NULL);

        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
        }

        if (FD_ISSET(fd_resize, &fds))
        {
            winmgr_resize();
        }

        if (FD_ISSET(fd, &fds))
        {
            message msg;
            while (message_get(&msg))
                message_dispatch(&msg);
        }
    }
}

void breakhere() {}

int main(int argc, char **argv)
{
#if 0
    char c;
    read(0, &c, 1);
    breakhere();
#endif

    //clog_init_path(0, "example.log");
    clog_init_fd(0, 1);
    clog_set_fmt(0, "%f(%F:%n): %l: %m\n");

    if (!winmgr_init())
    {
        clog_error(CLOG(0), "winmgr_init failed.");
        clog_free(0);
        return 1;
    }

    laymgr *lm = laymgr_create(NULL);
    layout *lay1 = laymgr_root(lm);
    layout_set_window(lay1, testwin_create("child1")->w);
    layout *lay2 = layout_split(lay1, testwin_create("child2")->w, 1, SIZE_HALF, DIR_DOWN);
    layout *lay3 = layout_split(lay2, testwin_create("child3")->w, 1, SIZE_HALF, DIR_RIGHT);
    layout *lay4 = layout_split(lay3, testwin_create("child4")->w, 1, SIZE_HALF, DIR_UP);

    main_loop();

    laymgr_destroy(lm);
    winmgr_shutdown();

    clog_free(0);

    return 0;
}
