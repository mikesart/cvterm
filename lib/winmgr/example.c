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
    char greetings[128];
} root;

uint32_t root_proc(root *r, int id, const message_data *data)
{
    switch (id)
    {
    case WM_PAINT:
        {
            window_eraserect(r->w, &data->paint.rc);
            window_set_draw_attrs(r->w, DRAW_ATTR_BOLD);

            // Colorful greetings message
            int count = strlen(r->greetings);
            for (int i = 0; i < count; i++)
            {
                window_set_fg_color(r->w, i & 15);
                window_set_bg_color(r->w, 15 - (i & 15));
                window_set_cursor_pos(r->w, i, 0);
                char chars[2] = { r->greetings[i], 0 };
                window_drawtext(r->w, chars);
            }
        }
        break;

    case WM_CHAR:
        if (strlen(r->greetings) < sizeof(r->greetings) - 1)
        {
            strcat(r->greetings, data->chr.utf8);
            window_invalidate_rect(r->w, NULL);
        }
        break;
    }

    return 0;
}

root *root_create(const char *greetings)
{
    root *r = malloc(sizeof(root));
    memset(r, 0, sizeof(*r));
    strncpy(r->greetings, greetings, sizeof(r->greetings) - 1);
    r->h = handler_create(r, (handler_proc)root_proc);
    r->w = winmgr_create_root_window(r->h);

    return r;
}

void root_destroy(root *r)
{
    handler_destroy(r->h);
    window_destroy(r->w);
    free(r);
}

void main_loop()
{
    int fd = message_fd();

    for ( ;; )
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(fd, &fds);

        int ret = select(fd + 1, &fds, NULL, NULL, NULL);

        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
        }

        if (FD_ISSET(0, &fds))
        {
            // This won't be on the outside like this if/when the fd eventing is
            // handled by the messaging system.
            winmgr_input_readable();
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

    if (!winmgr_init(NULL, 0, 1))
    {
        clog_error(CLOG(0), "winmgr_init. Check TERM=xterm");
        clog_free(0);
        return 1;
    }

    root *r = root_create("hello world!");
    window_set_focus(r->w);

    main_loop();

    root_destroy(r);

    winmgr_shutdown();

    clog_free(0);

    return 0;
}
