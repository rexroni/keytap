#ifndef APP_H
#define APP_H

#include <linux/input.h>

typedef int (*send_t)(void*, struct input_event);

typedef struct {
    send_t send;
    // returns a max_fd value
    int (*prep_select)(void*, fd_set *rd_fds, fd_set *w_fds);
    void (*handle_select)(void*, fd_set *rd_fds, fd_set *w_fds);
} app_t;

#endif // APP_H
