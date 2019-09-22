#ifndef SERVER_H
#define SERVER_H

#include "app.h"

typedef struct {
    int clients[8];
    size_t nclients;
    size_t active_client;
    // prepared data to send to client
    char for_client[8192];
    size_t fc_len;
    int accept_fd;
} kbd_server_t;

int server_send_event(void *app_data, struct input_event ev);
int server_prep_select(void *app_data, fd_set *r_fds, fd_set *w_fds);
void server_close_client(kbd_server_t *s, size_t i);
void server_handle_select(void *app_data, fd_set *r_fds, fd_set *w_fds);
int main_serve(char *host, char *port);

#endif // SERVER_H
