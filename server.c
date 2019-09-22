#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int server_send_event(void *app_data, struct input_event ev){
    kbd_server_t *s = app_data;
    // drop the event if we have no clients
    if(s->nclients == 0)
        return 0;

    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer), "%lu:%lu:%lu:%lu:%lu\n",
        (unsigned long)ev.type,
        (unsigned long)ev.value,
        (unsigned long)ev.code,
        (unsigned long)ev.time.tv_sec,
        (unsigned long)ev.time.tv_usec);

    // if there's not room to buffer the entire event, just drop it.
    if(s->fc_len + len > sizeof(s->for_client)){
        fprintf(stderr, "Warning: full buffer, dropping events");
        return 0;
    }

    memcpy(&s->for_client[s->fc_len], buffer, len);
    s->fc_len += len;

    return len;
}

int server_prep_select(void *app_data, fd_set *r_fds, fd_set *w_fds){
    kbd_server_t *s = app_data;
    int max_fd = -1;

    // watch for incoming connections
    FD_SET(s->accept_fd, r_fds);
    if(s->accept_fd > max_fd)
        max_fd = s->accept_fd;

    // monitor clients for broken connections
    for(size_t i = 0; i < s->nclients; i++){
        FD_SET(s->clients[i], r_fds);
        if(s->clients[i] > max_fd)
            max_fd = s->clients[i];
    }

    // do we need to write to the active client?
    if(s->nclients > 0 && s->fc_len > 0){
        FD_SET(s->clients[s->active_client], w_fds);
        if(s->clients[s->active_client] > max_fd)
            max_fd = s->clients[s->active_client];
    }

    return max_fd;
}

void server_close_client(kbd_server_t *s, size_t i){
    close(s->clients[i]);
    size_t nafter = sizeof(s->clients)/sizeof(*s->clients) - i - 1;
    memmove(&s->clients[i], &s->clients[i+1],
            sizeof(*s->clients) * nafter);
    // how to update active_client?
    if(s->active_client == i){
        s->active_client = 0;
        s->fc_len = 0;
    }else if(s->active_client > i){
        s->active_client--;
    }
    s->nclients--;
}

void server_handle_select(void *app_data, fd_set *r_fds, fd_set *w_fds){
    kbd_server_t *s = app_data;

    // check if we can write to the active client
    if(s->nclients > 0 && FD_ISSET(s->clients[s->active_client], w_fds)){
        ssize_t len = send(s->clients[s->active_client], s->for_client,
                s->fc_len, 0);
        if(len <= 0){
            fprintf(stderr, "failed to write to active client\n");
            server_close_client(s, s->active_client);
        }else{
            s->fc_len -= len;
        }
    }

    // check for any disconnected clients
    for(size_t i = 0; i < s->nclients; i++){
        if(FD_ISSET(s->clients[i], r_fds)){
            char buffer[4096];
            ssize_t len = read(s->clients[i], buffer, sizeof(buffer));
            // we never actually expect to read from a client
            if(len > 1){
                fprintf(stderr, "read unexpected bytes from a client\n");
            }else{
                fprintf(stderr, "client connection terminated\n");
                server_close_client(s, i);
                i--;
            }
        }
    }

    // handle accept
    if(FD_ISSET(s->accept_fd, r_fds)){
        struct sockaddr_in c_addr;
        socklen_t c_len = sizeof(c_addr);
        int client = accept(s->accept_fd, (struct sockaddr*)&c_addr, &c_len);
        if(client < 0){
            perror("accept");
            exit(7);
        }

        if(s->nclients + 1 > sizeof(s->clients) / sizeof(*s->clients)){
            fprintf(stderr, "too many clients\n");
            // too many clients already
            close(client);
            return;
        }

        s->clients[s->nclients++] = client;

        // ui hack: make the latest client active and kick the previous client.
        if(s->nclients > 1){
            close(s->clients[0]);
            s->nclients--;
            s->clients[0] = client;
        }
    }
}
