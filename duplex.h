#ifndef DUPLEX_H
#define DUPLEX_H

#define FDERR_OK 0
#define FDERR_BLOCKINGFD 1
#define FDERR_OOM 2

#define FDFLAG_HALF_DUPLEX 1
#define FDFLAG_NOCLOSE 2

struct ev_loop;
struct fdjoin_t;

typedef struct fdjoin_t fdjoin;
typedef void * (*transform_cb)(void *, int);

int
join(struct ev_loop *loop, 
     int fd_lhs, 
     int fd_rhs, 
     int flags, 
     transform_cb transform,
     fdjoin **);

void
unjoin(struct ev_loop *loop,
       fdjoin *join);

#endif
