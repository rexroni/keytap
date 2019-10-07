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
#include <getopt.h>
#include <signal.h>

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
#include "config.h"

static volatile bool keep_going = true;
static void quit_on_signal(int signum){
    keep_going = false;
}

int serve_loop(grab_t *grabs, app_t app, void *app_data){
  int i, ret, n_kbs;
  keyboard_t kbs[MAX_KBS];
  open_inputs(kbs, &n_kbs, grabs, app.send, app_data);
  int inot = open_inotify();

  if (n_kbs == 0) {
    fprintf(stderr, "couldn't open any inputs\n");
    return 1;
  }

  int retval = 0;

  fd_set rd_fds, wr_fds;
  while (keep_going) {
    FD_ZERO(&rd_fds);
    FD_ZERO(&wr_fds);

    int max_fd = -1;
    if(app.prep_select){
        max_fd = app.prep_select(app_data, &rd_fds, &wr_fds);
    }

    FD_SET(inot, &rd_fds);
    if(inot > max_fd)
        max_fd = inot;
    for (i = 0; i < n_kbs; i++) {
      FD_SET(kbs[i].fd, &rd_fds);
      if (kbs[i].fd > max_fd)
        max_fd = kbs[i].fd;
    }

    // struct timeval timeout;
    ret = select(1 + max_fd, &rd_fds, &wr_fds, NULL, NULL);
    if(ret == -1){
        if(errno == EINTR){
            // signal interrupted us, restart loop
            continue;
        }
        perror("select");
        retval = 1;
        break;
    }

    if(app.handle_select){
        app.handle_select(app_data, &rd_fds, &wr_fds);
    }

    // // if the timeout was reached, try to resolve immediately
    // if(timeout.tv_sec == 0 && timeout.tv_usec == 0){
    //     while(resolve(&r));
    // }

    for (i = 0; i < n_kbs; i++) {
      if (FD_ISSET(kbs[i].fd, &rd_fds)) {
        struct input_event ev;
        ret = read(kbs[i].fd, &ev, sizeof(struct input_event));
        if (ret > 0){
          struct resolver *r = &kbs[i].resolv;
          // add event to unresolved, or drop it if there isn't space for it
          if(r->ur_len < URMAX){
            r->unresolved[(r->ur_start + r->ur_len++) % URMAX] = ev;
            while(resolve(r));
          }
        }else if(errno != EAGAIN){
          // close this keyboard and left-shift the remaining fds
          close(kbs[i].fd);
          // TODO: do something to release any pressed keys here
          size_t nkbs_after = MAX_KBS - i - 1;
          memmove(&kbs[i], &kbs[i+1], sizeof(*kbs) * nkbs_after);
          n_kbs--;
          // don't skip the new i-th entry of kbs
          i--;
        }
      }
    }

    if (FD_ISSET(inot, &rd_fds)) {
      handle_inotify_events(inot, kbs, &n_kbs, grabs, app.send, app_data);
    }
  }

  for(int i = 0; i < n_kbs; i++){
    close(kbs[i].fd);
  }
  close(inot);

  return retval;
}


int main_serve(grab_t *grabs, char *host, char *port){
    usleep(250000);

    kbd_server_t server = {0};
    app_t server_app = {
        .send=server_send_event,
        .prep_select=server_prep_select,
        .handle_select=server_handle_select,
    };

    server.accept_fd = gai_open(host, port, true);
    if (server.accept_fd < 0) {
        fprintf(stderr, "couldn't open output\n");
        return 1;
    }

    int retval = serve_loop(grabs, server_app, &server);

    close(server.accept_fd);
    return retval;
}

int send_event_locally(void *data, struct input_event ev){
  int *fd = data;

  return write(*fd, &ev, sizeof(ev));
}

int main_local(grab_t *grabs){
    usleep(250000);
    int out_fd = open_output();
    if (out_fd < 0) {
        fprintf(stderr, "couldn't open output\n");
        return 1;
    }
    app_t local_app = {
        .send=send_event_locally,
    };

    int retval = serve_loop(grabs, local_app, &out_fd);

    close(out_fd);
    return retval;
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
        fprintf(stderr, "couldn't open output\n");
        return 1;
    }

    int sock = gai_open(host, port, false);
    if (sock < 0) {
        fprintf(stderr, "couldn't open output\n");
        close(out_fd);
        return 1;
    }

    int retval = 0;

    // just loop over reading from the socket
    char buffer[1024];
    size_t blen = 0;
    while(keep_going){
        // if the buffer is more than half full, just empty it
        if(blen > sizeof(buffer)/2){
            blen = 0;
        }

        // read in up to half of the buffer
        ssize_t rlen = read(sock, &buffer[blen], sizeof(buffer) / 2);
        if(rlen == -1){
            if(errno == EINTR){
                continue;
            }
            perror("read");
            retval = 2;
            break;
        }
        if(rlen == 0){
            // normal shutdown on other end
            fprintf(stderr, "server shut down\n");
            break;
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

    close(sock);
    close(out_fd);

    return retval;
}

void print_help(void){
    printf(
        "usage: keytap local              # modify local keyboards\n"
        "usage: keytap serve [host] port  # serve local keyboard on network\n"
        "usage: keytap connect host port  # read from a network keyboard\n"
        "\n"
        "options:\n"
        " -h, --help           print this help text\n"
        " -c, --config FILE    set config file (default /etc/keytap/conf.lua)\n"
    );
}

typedef struct {
    char *config;
} opts_t;

// Separate positional args and options; return 0 on success or -1 on error
int parse_opts(int argc, char **argv, int *nargs, char ***args, opts_t *opts){
    // configure cli options
    char *optstring = "hc:";
    struct option longopts[] = {
        {.name="config", .has_arg=1, .flag=NULL, .val='c'},
        {.name="help", .has_arg=1, .flag=NULL, .val='h'},
        {0},
    };

    // set default options
    *opts = (opts_t){
        .config="/etc/keytap/conf.lua",
    };

    // read all options
    int opt;
    while((opt = getopt_long(argc, argv, optstring, longopts, NULL)) > -1){
        switch(opt){
            case 'c':
                opts->config = optarg;
                break;
            case 'h':
                print_help();
                exit(0);
                break;
            default:
                return -1;
        }
    }

    *nargs = argc - optind;
    *args = &argv[optind];
    return 0;
}

int main(int argc, char **argv) {
    // parse command line arguments
    int nargs;
    char **args;
    opts_t opts = {0};
    if(parse_opts(argc, argv, &nargs, &args, &opts)){
        print_help();
        return 1;
    }

    // read config file
    config_t *config = NULL;
    if(opts.config){
        config = config_new(opts.config);
        if(!config){
            return 1;
        }
    }

    // prepare for signals
    signal(SIGINT, quit_on_signal);
    signal(SIGTERM, quit_on_signal);
    signal(SIGPIPE, SIG_IGN);

    int retval = 1;

    // interpret position arguments
    if(nargs == 0){
        goto help;
    }
    if(nargs > 0){
        if(!strcmp(args[0], "local")){
            if(nargs != 1){
                goto help;
            }
            retval = main_local(config->grabs);
            goto done;
        }

        if(!strcmp(args[0], "serve")){
            char *port;
            char *host;
            if(nargs == 2){
                host = NULL;
                port = args[1];
            }else if(nargs == 3){
                host = args[1];
                port = args[2];
            }else{
                goto help;
            }
            retval = main_serve(config->grabs, host, port);
            goto done;
        }

        if(!strcmp(args[0], "connect")){
            if(nargs != 3){
                goto help;
            }
            char *host = args[1];
            char *port = args[2];
            retval = main_connect(host, port);
            goto done;
        }
    }

help:
    print_help();
    retval = 1;

done:
    config_free(config);
    return retval;
}
