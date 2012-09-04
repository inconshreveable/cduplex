#include <stdlib.h>
#include <ev.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include "duplex.h"
#include <stdio.h>
#include <unistd.h>

static void
out_cb(EV_P_ ev_io *w, int revents)
{
    char buffer[1025];
    int len = read(w->fd, buffer, 1024);
    buffer[len] = '\0';
    printf("Got data: %s", buffer);
}

static void
close_cb(EV_P_ ev_timer *w, int revents)
{
    close(* ((int *) w->data));
}

int main(int argc, char **argv)
{
    int s[2];
    int t[2];
    fdjoin *fjoin, *fjoin_stdin;
    struct ev_loop *loop;
    ev_timer timer;
    ev_io out_watcher;

    socketpair(AF_UNIX, SOCK_STREAM, 0, s);
    socketpair(AF_UNIX, SOCK_STREAM, 0, t);

    fcntl(0, F_SETFL, O_NONBLOCK);
    fcntl(s[0], F_SETFL, O_NONBLOCK);
    fcntl(s[1], F_SETFL, O_NONBLOCK);
    fcntl(t[0], F_SETFL, O_NONBLOCK);
    fcntl(t[1], F_SETFL, O_NONBLOCK);

    loop = ev_default_loop (0);
    if (FDERR_OK != (join(loop, /* STDIN */ 0, s[0], 0, NULL, &fjoin_stdin))) {
        printf("Failed to join!\n");
        return EXIT_FAILURE;
    }

    if (FDERR_OK != (join(loop, s[1], t[0], 0, NULL, &fjoin))) {
        printf("Failed to join!\n");
        return EXIT_FAILURE;
    }

    ev_io_init(&out_watcher, out_cb, t[1], EV_READ);
    ev_io_start(loop, &out_watcher);

    timer.data = &(s[1]);
    ev_timer_init(&timer, close_cb, 5.5, 0.);
    ev_timer_start(loop, &timer);

    ev_run(loop, 0);

    return EXIT_SUCCESS;
}

