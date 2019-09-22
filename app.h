#ifndef APP_H
#define APP_H

#include <linux/input.h>

typedef struct {
    int (*send)(void*, struct input_event);
    // returns a max_fd value
    int (*prep_select)(void*, fd_set *r_fds, fd_set *w_fds);
    void (*handle_select)(void*, fd_set *r_fds, fd_set *w_fds);
} app_t;

#endif // APP_H
