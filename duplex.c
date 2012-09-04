#include "duplex.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <ev.h>
#include <unistd.h>

/*
 XXX:
    no busy-wait by pausing read io watcher + write watcher

    fix memory free on socket close
    needs better cleanup if creating second watcher in full-duplex fails
    use buffer circularly
    implement close callback
*/

// constants
#define BUF_SIZE 1024

struct fdjoin_t {
    ev_io lhs, rhs;
};

typedef struct watcher_data_t {
    int fd;
    transform_cb transform;
    char buffer[BUF_SIZE];
    int to_write, written;
    int noclose;
} watcher_data;


static watcher_data *
make_watcher_data(int fd, int flags, transform_cb transform)
{
    watcher_data *data = NULL;
    if (! (data = (watcher_data *) malloc(sizeof(watcher_data)))) {
        return NULL;
    }

    data->fd = fd;
    data->to_write = 0;
    data->written = 0;
    data->transform = transform;
    data->noclose = flags & FDFLAG_NOCLOSE;
    return data;
}

// XXX: use join->buffer as a circular buffer
static void
data_callback(EV_P_ ev_io *w, int revents)
{
    size_t bytes_read, bytes_written;
    watcher_data *join = (watcher_data *) w->data;

    if (join->to_write == join->written) {
        if (1 > (bytes_read = read(w->fd, join->buffer, BUF_SIZE))) {
            ev_io_stop(EV_A_ w);
            close(w->fd);
            if (!join->noclose) {
                close(join->fd);
            }
            return;
        }

        join->to_write = bytes_read;
        join->written = 0;

        if (join->transform) {
            join->transform(join->buffer, bytes_read);
        }
    }

    if (-1 == (bytes_written = write(join->fd, join->buffer, bytes_read))) {
        // fail
        if (EAGAIN == errno) {
        }
        return;
    }

    join->written += bytes_written;
}

static int
join_half_duplex(struct ev_loop *loop, 
                 int fd_from, 
                 int fd_to, 
                 int flags, 
                 transform_cb transform,
                 ev_io *watcher)
{
    int errcode = FDERR_OK;
    watcher_data *data = NULL;

    if (!(data = make_watcher_data(fd_to, flags, transform))) {
        errcode = FDERR_OOM;
        goto joinhalferr;
    }

    // ensure FD is nonblocking
    if (!(O_NONBLOCK & fcntl(fd_from, F_GETFL))) {
        errcode = FDERR_BLOCKINGFD;
        goto joinhalferr;
    }

    // set up watcher
    ev_io_init(watcher, data_callback, fd_from, EV_READ);
    watcher->data = data;
    ev_io_start(loop, watcher);

    return FDERR_OK;

joinhalferr:
    free(data);
    return errcode;
}

int
join (struct ev_loop *loop, 
      int fd_lhs, 
      int fd_rhs, 
      int flags, 
      transform_cb transform,
      /* out */ fdjoin **joinptr)
{
    int errcode = FDERR_OK;
    fdjoin *join = NULL;

    if (!(*joinptr = (fdjoin *) malloc(sizeof(fdjoin)))) {
        errcode = FDERR_OOM;
    }

    if (FDERR_OK != join_half_duplex(loop, fd_lhs, fd_rhs, flags, transform, &(*joinptr)->lhs)) {
        goto joinerr;
    }

    if (!(flags & FDFLAG_HALF_DUPLEX)) {
        if (FDERR_OK != join_half_duplex(loop, fd_rhs, fd_lhs, flags, transform, &(*joinptr)->rhs)) {
            goto joinerr;
        }
    }

    return FDERR_OK;

joinerr:
    free(join);
    return errcode;
}

void
unjoin(struct ev_loop *loop,
       fdjoin *join)
{
    ev_io_stop(loop, &join->lhs);
    ev_io_stop(loop, &join->rhs);
    free(join);
}

