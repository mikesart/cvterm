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
} client;

typedef struct
{
    window *w;
    handler h;
    char message[128];
} statusbar;

typedef struct
{
    window *w;
    handler h;
    client *c;
    statusbar *sb;
} testwin;

uint32_t testwin_proc(testwin *t, int id, const message_data *data)
{
    switch (id)
    {
    case WM_POSCHANGED:
        if (data->pos_changed.resized)
        {
            int width = rect_width(data->pos_changed.rc_new);
            int height = rect_height(data->pos_changed.rc_new);
            rect rc;
            rect_set(&rc, 0, 0, width, height - 1);
            window_set_pos(t->c->w, &rc);
            rect_set(&rc, 0, height - 1, width, height);
            window_set_pos(t->sb->w, &rc);
        }
        break;

    case WM_DESTROY:
        handler_destroy(t->h);
        free(t);
        break;
    }

    return 0;
}

uint32_t client_proc(client *c, int id, const message_data *data)
{
    switch (id)
    {
    case WM_PAINT:
        {
            WINDOW *win = window_WIN(c->w);
            werase(win);

            // Get the ncurses window size to check for size mismatch
            int cx, cy;
            getmaxyx(win, cy, cx);
            int x = (cx - 10) / 2;
            int y = (cy - 3) / 2;
            wmove(win, y, x);
            char str[32];
            sprintf(str, "ncurses: %d,%d", cx, cy);
            wmove(win, y, x);
            waddstr(win, str);
            rect rc;
            window_rect(c->w, &rc);
            sprintf(str, "window: %d,%d", rc.right - rc.left, rc.bottom - rc.top);
            wmove(win, y + 1, x);
            waddstr(win, str);
        }
        break;

    case WM_DESTROY:
        handler_destroy(c->h);
        free(c);
        break;
    }

    return 0;
}

uint32_t statusbar_proc(statusbar *sb, int id, const message_data *data)
{
    switch (id)
    {
    case WM_PAINT:
        {
            WINDOW *win = window_WIN(sb->w);
            rect rc;
            window_rect(sb->w, &rc);
            wattron(win, A_REVERSE);
            mvwhline(win, 0, 0, ' ', rc.right - rc.left);
            mvwprintw(win, 0, 0, sb->message);
            wattroff(win, A_REVERSE);
        }
        break;

    case WM_DESTROY:
        handler_destroy(sb->h);
        free(sb);
        break;
    }

    return 0;
}

client *client_create(window *parent)
{
    client *c = malloc(sizeof(statusbar));
    memset(c, 0, sizeof(*c));
    c->h = handler_create(c, (handler_proc)client_proc);
    c->w = window_create(parent, NULL, c->h, 0);
    return c;
}

statusbar *statusbar_create(window *parent, const char *message)
{
    statusbar *sb = malloc(sizeof(statusbar));
    memset(sb, 0, sizeof(*sb));
    strncpy(sb->message, message, sizeof(sb->message) - 1);
    sb->h = handler_create(sb, (handler_proc)statusbar_proc);
    sb->w = window_create(parent, NULL, sb->h, 0);
    return sb;
}

testwin *testwin_create(const char *message)
{
    testwin *t = malloc(sizeof(testwin));
    memset(t, 0, sizeof(*t));
    t->h = handler_create(t, (handler_proc)testwin_proc);
    t->w = window_create(NULL, NULL, t->h, 0);
    t->c = client_create(t->w);
    t->sb = statusbar_create(t->w, message);

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
    layout *lay2 = layout_split(lay1, testwin_create("child2")->w, 0, SIZE_HALF, DIR_DOWN);
    layout *lay3 = layout_split(lay2, testwin_create("child3")->w, 1, SIZE_HALF, DIR_RIGHT);
    layout *lay4 = layout_split(lay3, testwin_create("child4")->w, 0, SIZE_HALF, DIR_UP);

    main_loop();

    laymgr_destroy(lm);
    winmgr_shutdown();

    clog_free(0);

    return 0;
}
