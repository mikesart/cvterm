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

testwin *s_focus;
laymgr *s_lm;
int s_splitter;
int s_edge;
int s_counter;
enum
{
    CMD_NONE = 0,
    CMD_SPLIT,
    CMD_SELECTEDGE,
    CMD_SIZE,
    CMD_NAV
};
int s_cmd;

testwin *testwin_add();
void testwin_remove(testwin *t);
#define TM_GETTESTWIN WM_USER

uintptr_t testwin_proc(testwin *t, int id, const message_data *data)
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

    case TM_GETTESTWIN:
        return (uintptr_t)t;
    }

    return 0;
}

testwin *get_testwin(window *w)
{
    return (testwin *)handler_call(window_handler(w), TM_GETTESTWIN, NULL);
}

uintptr_t client_proc(client *c, int id, const message_data *data)
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

uintptr_t statusbar_proc(statusbar *sb, int id, const message_data *data)
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
            testwin *t = s_focus;
            if (t && t->sb == sb)
            {
                wattron(win, A_BOLD);
                char sz[32];
                sprintf(sz, "%s - focus", sb->message);
                mvwprintw(win, 0, 0, sz);
                wattroff(win, A_BOLD);
            }
            else
            {
                mvwprintw(win, 0, 0, sb->message);
            }
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

void set_focus(layout *to)
{
    testwin *t = s_focus;
    if (t)
        window_invalidate(t->w);
    window *w = layout_window(to);
    s_focus = get_testwin(w);
    window_invalidate(w);
}

testwin *testwin_add()
{
    char sz[32];
    sprintf(sz, "t%d", ++s_counter);
    return testwin_create(sz);
}

void testwin_remove(testwin *t)
{
    layout *lay = laymgr_find(s_lm, t->w);
    if (t == s_focus)
    {
        layout *new = layout_navigate_ordered(lay, 1);
        set_focus(new);
    }

    if (lay)
        layout_close(lay);
    window_destroy(t->w);
}

void navigate_dir(layout *lay, int dir)
{
    testwin *focus_old = s_focus;
    rect rc;
    window_rect(focus_old->w, &rc);

    layout *layT = layout_navigate_dir(lay, 0, rect_height(&rc) - 1, dir);
    if (layT)
        set_focus(layT);
}

void dir_cmd(layout *lay, int dir)
{
    switch (s_cmd)
    {
    case CMD_NONE:
        break;
    case CMD_SPLIT:
        layout_split(lay, testwin_add()->w, s_splitter, SIZE_HALF, dir);
        break;
    case CMD_SELECTEDGE:
        s_edge = dir;
        s_cmd = CMD_SIZE;
        break;
    case CMD_SIZE:
        if (IS_DIR_VERT(s_edge) == IS_DIR_VERT(dir))
            layout_move_edge(lay, IS_DIR_PREV(dir) ? -1 : 1, s_edge);
        break;
    case CMD_NAV:
        navigate_dir(lay, dir);
        break;
    }
}

void handle_input()
{
    testwin *t = s_focus;
    layout *lay = laymgr_find(s_lm, t->w);
    int ch = wgetch(window_WIN(t->w));
    switch (ch)
    {
    case 353:
        // Shift-tab
        {
            layout *prev = layout_navigate_ordered(lay, 0);
            if (prev)
                set_focus(prev);
        }
        break;

    case '\t':
        // Tab
        {
            layout *next = layout_navigate_ordered(lay, 1);
            if (next)
                set_focus(next);
        }
        break;

    case 'c':
        s_cmd = CMD_SPLIT;
        break;

    case 'd':
        testwin_remove(t);
        break;

    case 'q':
        message_post(0, WM_QUIT, NULL);
        break;

    case 's':
        s_splitter ^= 1;
        break;

    case 'n':
        s_cmd = CMD_NAV;
        break;

    case 'S':
        s_cmd = CMD_SELECTEDGE;
        break;

    case 13:
        s_cmd = CMD_NONE;
        break;

    case KEY_LEFT:
        dir_cmd(lay, DIR_LEFT);
        break;

    case KEY_UP:
        dir_cmd(lay, DIR_UP);
        break;

    case KEY_RIGHT:
        dir_cmd(lay, DIR_RIGHT);
        break;

    case KEY_DOWN:
        dir_cmd(lay, DIR_DOWN);
        break;
    }
}

void main_loop()
{
    int fd_message = message_fd();
    int fd_resize = winmgr_resize_fd();

    for ( ;; )
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(fd_message, &fds);
        FD_SET(fd_resize, &fds);

        int fd_max = STDIN_FILENO;
        if (fd_max < fd_message)
            fd_max = fd_message;
        if (fd_max < fd_resize)
            fd_max = fd_resize;

        int ret = select(fd_max + 1, &fds, NULL, NULL, NULL);

        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
        }

        if (FD_ISSET(STDIN_FILENO, &fds))
        {
            handle_input();
        }

        if (FD_ISSET(fd_resize, &fds))
        {
            winmgr_resize();
        }

        if (FD_ISSET(fd_message, &fds))
        {
            message msg;
            while (message_get(&msg))
            {
                if (msg.id == WM_QUIT)
                    return;
                message_dispatch(&msg);
            }
        }
    }
}

void breakhere() {}

int main(int argc, char **argv)
{
#if 1
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

    s_lm = laymgr_create(NULL);
    layout *lay1 = laymgr_root(s_lm);
    layout_set_window(lay1, testwin_add()->w);
    layout *lay2 = layout_split(lay1, testwin_add()->w, 0, SIZE_HALF, DIR_DOWN);
    layout *lay3 = layout_split(lay2, testwin_add()->w, 1, SIZE_HALF, DIR_RIGHT);
    layout *lay4 = layout_split(lay3, testwin_add()->w, 0, SIZE_HALF, DIR_UP);
    set_focus(layout_navigate_ordered(laymgr_root(s_lm), 1));

    main_loop();

    laymgr_destroy(s_lm);
    winmgr_shutdown();

    clog_free(0);

    return 0;
}
