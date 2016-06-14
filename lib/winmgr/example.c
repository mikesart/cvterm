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
            wmove(win, 0, 0);
            waddstr(win, t->message);
        }
        break;
    }

    return 0;
}

testwin *testwin_create(const char *message, int x, int y)
{
    testwin *t = malloc(sizeof(testwin));
    memset(t, 0, sizeof(*t));
    strncpy(t->message, message, sizeof(t->message) - 1);
    t->h = handler_create(t, (handler_proc)testwin_proc);
    rect rc;
    rect_set(&rc, x, y, x + 20, y + 10);
    t->w = window_create(NULL, &rc, t->h, WF_VISIBLE);

    return t;
}

void testwin_destroy(testwin *t)
{
    window_destroy(t->w);
    handler_destroy(t->h);
    free(t);
}

void main_loop()
{
    int fd = message_fd();

    for ( ;; )
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        int ret = select(fd + 1, &fds, NULL, NULL, NULL);

        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
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
        clog_error(CLOG(0), "winmgr_init. Check TERM=xterm");
        clog_free(0);
        return 1;
    }

    testwin *t1 = testwin_create("child1", 10, 20);
    testwin *t2 = testwin_create("child2", 50, 20);

    main_loop();

    testwin_destroy(t2);
    testwin_destroy(t1);

    winmgr_shutdown();

    clog_free(0);

    return 0;
}
