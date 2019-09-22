#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdbool.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "app.h"
#include "server.h"
#include "time_util.h"
#include "networking.h"
#include "resolver.h"
#include "devices.h"

// alternate keymap, based on the "f" key, keys not present are passed through.
int fmap[][2] = {
    {KEY_H, KEY_LEFT},
    {KEY_J, KEY_DOWN},
    {KEY_K, KEY_UP},
    {KEY_L, KEY_RIGHT},

    {KEY_U, KEY_KPLEFTPAREN},
    {KEY_I, KEY_KPRIGHTPAREN},
    {KEY_O, KEY_LEFTBRACE},
    {KEY_P, KEY_RIGHTBRACE},

    {KEY_N, KEY_GRAVE},
    {KEY_M, KEY_MINUS},
    {KEY_COMMA, KEY_EQUAL},

    {KEY_1, KEY_F1},
    {KEY_2, KEY_F2},
    {KEY_3, KEY_F3},
    {KEY_4, KEY_F4},
    {KEY_5, KEY_F5},
    {KEY_6, KEY_F6},
    {KEY_7, KEY_F7},
    {KEY_8, KEY_F8},
    {KEY_9, KEY_F9},
    {KEY_0, KEY_F10},
    {KEY_MINUS, KEY_F11},
    {KEY_BACKSLASH, KEY_F12},
};


int serve_loop(app_t app, void *app_data){
  int i, ret;
  int in_fds[MAX_KBS];
  int n_kbs = open_inputs(in_fds);
  int inot = open_inotify();

  if (n_kbs == 0) {
    fputs("couldn't open any inputs", stderr);
    return 1;
  }

  struct resolver r = {.send=app.send, .send_data=app_data};

  // expand the fmap
  int fmap_exp[256] = {0};
  for(int i = 0; i < sizeof(fmap_exp) / sizeof(*fmap_exp); i++)
    fmap_exp[i] = i;
  for(size_t i = 0; i < sizeof(fmap) / sizeof(*fmap); i++)
    fmap_exp[fmap[i][0]] = fmap[i][1];

  //int maps[][2] = {{KEY_CAPSLOCK, KEY_ESC},
  //                 {KEY_RIGHTSHIFT, KEY_BACKSPACE},
  //                 {KEY_LEFTALT, KEY_MINUS},
  //                 {KEY_RIGHTALT, KEY_EQUAL},
  //                 {KEY_LEFTSHIFT, KEY_MUTE},
  //                 {KEY_LEFTMETA, KEY_VOLUMEDOWN},
  //                 {KEY_COMPOSE, KEY_VOLUMEUP},
  //                 {0, 0}};

  fd_set r_fds, w_fds;
  while (1) {
    FD_ZERO(&r_fds);
    FD_ZERO(&w_fds);

    int max_fd = -1;
    if(app.prep_select){
        max_fd = app.prep_select(app_data, &r_fds, &w_fds);
    }

    FD_SET(inot, &r_fds);
    if(inot > max_fd)
        max_fd = inot;
    for (i = 0; i < n_kbs; i++) {
      FD_SET(in_fds[i], &r_fds);
      if (in_fds[i] > max_fd)
        max_fd = in_fds[i];
    }

    struct timeval timeout;
    select(1 + max_fd, &r_fds, &w_fds, NULL, NULL);

    if(app.handle_select){
        app.handle_select(app_data, &r_fds, &w_fds);
    }

    // if the timeout was reached, try to resolve immediately
    if(timeout.tv_sec == 0 && timeout.tv_usec == 0){
        while(resolve(&r, fmap_exp));
    }

    for (i = 0; i < n_kbs; i++) {
      if (FD_ISSET(in_fds[i], &r_fds)) {
        struct input_event ev;
        ret = read(in_fds[i], &ev, sizeof(struct input_event));
        if (ret > 0){
          // add event to unresolved, or drop it if there isn't space for it
          if(r.ur_len < URMAX){
            r.unresolved[(r.ur_start + r.ur_len++) % URMAX] = ev;
            while(resolve(&r, fmap_exp));
          }
        }else if(errno != EAGAIN){
          // close this keyboard and left-shift the remaining fds
          close(in_fds[i]);
          size_t nkbs_after = MAX_KBS - i - 1;
          memmove(&in_fds[i], &in_fds[i+1], sizeof(*in_fds) * nkbs_after);
          n_kbs--;
          // don't skip the new in_fds[i];
          i--;
        }
      }
    }

    if (FD_ISSET(inot, &r_fds)) {
      handle_inotify_events(inot, in_fds, &n_kbs);
    }
  }
}


int main_serve(char *host, char *port){
    usleep(250000);

    kbd_server_t server = {0};
    app_t server_app = {
        .send=server_send_event,
        .prep_select=server_prep_select,
        .handle_select=server_handle_select,
    };

    server.accept_fd = gai_open(host, port, true);
    if (server.accept_fd < 0) {
        fputs("couldn't open output", stderr);
        return 1;
    }
    return serve_loop(server_app, &server);
}

int send_event_locally(void *data, struct input_event ev){
  int *fd = data;

  return write(*fd, &ev, sizeof(ev));
}

int main_local(void){
    usleep(250000);
    int out_fd = open_output();
    if (out_fd < 0) {
        fputs("couldn't open output", stderr);
        return 1;
    }
    app_t local_app = {
        .send=send_event_locally,
    };
    return serve_loop(local_app, &out_fd);
}

int first_newline(char *string, int maxlen){
    for(int i = 0; i < maxlen; i++){
       if(string[i] == '\n'){
           return i;
       }
    }
    return -1;
}

int main_connect(char *host, char *port){
    int out_fd = open_output();
    if (out_fd < 0) {
        fputs("couldn't open output", stderr);
        return 1;
    }

    int sock = gai_open(host, port, false);
    if (sock < 0) {
        fputs("couldn't open output", stderr);
        return 1;
    }

    // just loop over reading from the socket
    char buffer[1024];
    size_t blen = 0;
    while(true){
        // if the buffer is more than half full, just empty it
        if(blen > sizeof(buffer)/2){
            blen = 0;
        }

        // read in up to half of the buffer
        ssize_t rlen = read(sock, &buffer[blen], sizeof(buffer) / 2);
        if(rlen == -1){
            perror("read");
            return 2;
        }
        if(rlen == 0){
            // normal shutdown on other end
            fprintf(stderr, "server shut down\n");
            perror("recv");
            return 2;
        }
        blen += rlen;

        // for as long as we have a complete line in the buffer...
        int idx;
        while((idx = first_newline(buffer, blen)) > -1){
            // ... try to unmarshal an event
            unsigned long type, value, code, sec, usec;
            int found = sscanf(buffer, "%lu:%lu:%lu:%lu:%lu\n",
                    &type, &value, &code, &sec, &usec);
            // was it a valid event?
            if(found == 5){
                struct input_event ev = {
                    .type=type,
                    .value=value,
                    .code=code,
                    .time={
                        .tv_sec=sec,
                        .tv_usec=usec,
                    },
                };
                //printf("%*s", idx, buffer);
                send_event_locally(&out_fd, ev);
            }
            // discard the line
            memmove(buffer, &buffer[idx + 1], blen - idx - 1);
            blen -= idx + 1;
        }
    }
}

void print_help(void){
    printf(
        "usage: keytap local              # modify local keyboards\n"
        "usage: keytap serve [host] port  # serve local keyboard on network\n"
        "usage: keytap connect host port  # read from a network keyboard\n"
    );
}

int main(int argc, char **argv) {
    if(argc > 1){
        if(!strcmp(argv[1], "--help")){
            print_help();
            return 0;
        }

        if(!strcmp(argv[1], "local")){
            if(argc != 2){
                print_help();
                return 1;
            }
            return main_local();
        }

        if(!strcmp(argv[1], "serve")){
            char *port;
            char *host;
            if(argc == 3){
                host = NULL;
                port = argv[2];
            }else if(argc == 4){
                host = argv[2];
                port = argv[3];
            }else{
                print_help();
                return 1;
            }
            return main_serve(host, port);
        }

        if(!strcmp(argv[1], "connect")){
            if(argc != 4){
                print_help();
                return 1;
            }
            char *host = argv[2];
            char *port = argv[3];
            return main_connect(host, port);
        }
    }
    print_help();
    return 1;
}
