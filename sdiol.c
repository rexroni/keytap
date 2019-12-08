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

#include <systemd/sd-daemon.h>

#include "app.h"
#include "server.h"
#include "time_util.h"
#include "networking.h"
#include "resolver.h"
#include "devices.h"
#include "config.h"
#include "names.h"
#include "permissions.h"

static volatile bool keep_going = true;
static void quit_on_signal(int signum){
    keep_going = false;
}

// command line inputs
typedef struct {
    char *config;
    bool systemd;
    bool verbose;
    char* timeout;
    char* user_group;
    char* mode;
} opts_t;

// run-time config (post-processed version of opts_t)
typedef struct {
    config_t *config;
    bool systemd;
    bool verbose;
    int timeout;
    char *user;
    char* group;
    char* mode;
} runopts_t;

int serve_loop(const runopts_t *runopts, app_t app, void *app_data){
  struct timeval exit_time;
  bool timed_exit = false;
  if(runopts->timeout > 0){
      printf("preparing to exit after %d seconds\n", runopts->timeout);
      exit_time = msec_after(timeval_now(), runopts->timeout * 1000);
      timed_exit = true;
  }

  // let user release the enter key after running the command
  usleep(250000);

  int i, ret, n_kbs;
  keyboard_t kbs[MAX_KBS];
  open_inputs(kbs, &n_kbs, runopts->config->grabs, app.send, app_data,
          runopts->verbose);
  int inot = open_inotify();

  if (n_kbs == 0) {
    fprintf(stderr, "couldn't open any inputs\n");
    return 1;
  }

  int retval = 0;

  // notify systemd we are up (if --systemd or -d was given)
  if(runopts->systemd){
    sd_notify(0, "READY=1");
  }

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

    struct timeval time_till_exit;
    struct timeval *timeout = NULL;
    if(timed_exit){
        time_till_exit = timeval_diff(exit_time, timeval_now());
        timeout = &time_till_exit;
    }

    ret = select(1 + max_fd, &rd_fds, &wr_fds, NULL, timeout);
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

    // if we reached the timeout, exit
    if(timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0){
        printf("exiting due to timeout\n");
        break;
    }

    for (i = 0; i < n_kbs; i++) {
      if (FD_ISSET(kbs[i].fd, &rd_fds)) {
        struct input_event ev;
        ret = read(kbs[i].fd, &ev, sizeof(struct input_event));
        if (ret > 0){
          struct resolver *r = &kbs[i].resolv;
          // add event to unresolved, or drop it if there isn't space for it
          if(r->ur_len < URMAX){
            r->unresolved[(r->ur_start + r->ur_len++) % URMAX] = ev;
            // print names of keypresses
            if(runopts->verbose && ev.type == EV_KEY && ev.value == 1){
                printf("%s\n", get_input_name(ev.code));
            }
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
      handle_inotify_events(inot, kbs, &n_kbs, runopts->config->grabs,
              app.send, app_data, runopts->verbose);
    }
  }

  if(runopts->systemd){
      sd_notify(0, "STOPPING=1");
  }

  for(int i = 0; i < n_kbs; i++){
    close(kbs[i].fd);
  }
  close(inot);

  return retval;
}


int main_serve_unix(const runopts_t *runopts, char *socket, char *lock){
    // obtain a file lock and bind to the socket path
    int lockfd;
    int sockfd = unix_socket_open(socket, lock, &lockfd);
    if(sockfd < 0){
        return 1;
    }

    int retval = -1;

    // set permissions, defaulting to exclusive user access
    char *mode = runopts->mode ? runopts->mode : "600";
    int ret = set_file_perms(socket, runopts->user, runopts->group, mode);
    if(ret != 0){
        goto cu_socket;
    }

    // start listening
    ret = listen(sockfd, 5);
    if(ret != 0){
        perror("listen");
        goto cu_socket;
    }

    kbd_server_t server = {
        .accept_fd = sockfd,
    };

    app_t server_app = {
        .send=server_send_event,
        .prep_select=server_prep_select,
        .handle_select=server_handle_select,
    };

    retval = serve_loop(runopts, server_app, &server);

    for(size_t i = 0; i < server.nclients; i++){
        close(server.clients[i]);
    }

cu_socket:
    unix_socket_close(sockfd, lockfd);

    return retval;
}


int main_serve_tcp(const runopts_t *runopts, char *host, char *port){
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

    int retval = serve_loop(runopts, server_app, &server);

    for(size_t i = 0; i < server.nclients; i++){
        close(server.clients[i]);
    }
    close(server.accept_fd);
    return retval;
}

int send_event_locally(void *data, struct input_event ev){
  int *fd = data;

  return write(*fd, &ev, sizeof(ev));
}

int main_local(const runopts_t *runopts){
    int out_fd = open_output();
    if (out_fd < 0) {
        fprintf(stderr, "couldn't open output\n");
        return 1;
    }
    app_t local_app = {
        .send=send_event_locally,
    };

    int retval = serve_loop(runopts, local_app, &out_fd);

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

// read from a file descriptor; we don't care what kind
int main_read(const runopts_t *runopts, int fd){
    int out_fd = open_output();
    if (out_fd < 0) {
        fprintf(stderr, "couldn't open output\n");
        return 1;
    }

    if(runopts->systemd){
        sd_notify(0, "READY=1");
    }

    int retval = 0;

    // just loop over reading from the file descriptor
    char buffer[1024];
    size_t blen = 0;
    while(keep_going){
        // if the buffer is more than half full, just empty it
        if(blen > sizeof(buffer)/2){
            blen = 0;
        }

        // read in up to half of the buffer
        ssize_t rlen = read(fd, &buffer[blen], sizeof(buffer) / 2);
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
            fprintf(stderr, "event stream closed\n");
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
                // print names of keypresses
                if(runopts->verbose && ev.type == EV_KEY && ev.value == 1){
                    printf("%s\n", get_input_name(ev.code));
                }
                send_event_locally(&out_fd, ev);
            }
            // discard the line
            memmove(buffer, &buffer[idx + 1], blen - idx - 1);
            blen -= idx + 1;
        }
    }

    if(runopts->systemd){
        sd_notify(0, "STOPPING=1");
    }

    close(out_fd);

    return retval;
}

int main_connect(const runopts_t *runopts, char *host, char *port){

    int sock = gai_open(host, port, false);
    if (sock < 0) {
        fprintf(stderr, "couldn't open output\n");
        return 1;
    }

    int retval = main_read(runopts, sock);

    close(sock);

    return retval;
}

void print_help(void){
    printf(
        "usage: sdiol local                  # modify local IO\n"
        "usage: sdiol serve unix_socket      # serve IO over a unix socket\n"
        "usage: sdiol read                   # read IO from STDIN\n"
        "\n"
        "# insecure, experimental features:\n"
        "usage: sdiol serve-tcp [host] port  # serve IO over the network\n"
        "usage: sdiol connect host port      # read IO from network\n"
        "\n"
        "general options:\n"
        " -h, --help           print this help text\n"
        " -c, --config FILE    set config file (default /etc/sdiol/conf.lua)\n"
        " -v, --verbose        print useful info while running\n"
        "     --timeout N      exit after N seconds (for testing)\n"
        "     --systemd        run as systemd Type=notify service\n"
        "\n"
        "options specific to sdiol serve:\n"
        " --chown-socket USER:GROUP  set user and group of unix socket\n"
        " --chmod-socket MODE        set mode of unix socket (default 600)\n"
    );
}

// Separate positional args and options; return 0 on success or -1 on error
int parse_opts(int argc, char **argv, int *nargs, char ***args, opts_t *opts){
    // configure cli options
    char *optstring = "hc:v";
    struct option longopts[] = {
        {.name="help", .has_arg=0, .flag=NULL, .val='h'},
        {.name="config", .has_arg=1, .flag=NULL, .val='c'},
        {.name="verbose", .has_arg=0, .flag=NULL, .val='v'},
        {.name="timeout", .has_arg=1, .flag=NULL, .val='t'},
        {.name="systemd", .has_arg=0, .flag=NULL, .val='d'},
        {.name="chown-socket", .has_arg=1, .flag=NULL, .val='o'},
        {.name="chmod-socket", .has_arg=1, .flag=NULL, .val='p'},
        {0},
    };

    // set default options
    *opts = (opts_t){
        .config="/etc/sdiol/conf.lua",
    };

    // read all options
    int opt;
    while((opt = getopt_long(argc, argv, optstring, longopts, NULL)) > -1){
        switch(opt){
            case 'h':
                print_help();
                exit(0);
                break;
            case 'c':
                opts->config = optarg;
                break;
            case 'v':
                opts->verbose = true;
                break;
            case 't':
                opts->timeout = optarg;
                break;
            case 'd':
                opts->systemd = true;
                break;
            case 'o':
                opts->user_group = optarg;
                break;
            case 'm':
                opts->mode = optarg;
                break;
            default:
                fprintf(stderr, "invalid option during parsing\n");
                return -1;
        }
    }

    *nargs = argc - optind;
    *args = &argv[optind];
    return 0;
}

// *user and *group will need to be freed
int split_user_group(char *user_group, char **user, char **group){
    if(user_group == NULL){
        return 0;
    }

    // validate/split the string
    char *colon = NULL;
    char *c;
    for(c = user_group; *c != '\0'; c++){
        // ignore non-colon characters
        if(*c != ':') continue;

        if(colon != NULL){
            fprintf(stderr, "invalid USER:GROUP: %s\n", user_group);
            return -1;
        }
        colon = c;
    }
    if(colon == NULL){
        fprintf(stderr, "invalid USER:GROUP: %s\n", user_group);
        return -1;
    }

    // allocate new strings
    size_t user_len = (uintptr_t)colon - (uintptr_t)user_group + 1;
    *user = malloc(user_len);
    if(!*user){
        perror("malloc");
        return -1;
    }

    char *gstart = colon + 1;
    size_t group_len = (uintptr_t)c - (uintptr_t)(gstart) + 1;
    *group = malloc(group_len);
    if(!*group){
        perror("malloc");
        free(*user);
        return -1;
    }

    // write into new strings
    memset(*user, 0, user_len);
    memcpy(*user, user_group, user_len - 1);

    memset(*group, 0, group_len);
    memcpy(*group, gstart, group_len - 1);

    return 0;
}

// returns -1 on error
int runopts_build(runopts_t *runopts, const opts_t *opts){
    *runopts = (runopts_t){0};

    int ret;
    ret = split_user_group(opts->user_group, &runopts->user, &runopts->group);
    if(ret != 0){
        return -1;
    }

    // read config file
    if(opts->config){
        runopts->config = config_new(opts->config);
        if(!runopts->config){
            goto fail_user_group;
        }
    }

    if(opts->timeout){
        runopts->timeout = atoi(opts->timeout);
    }

    // pass along simple values
    runopts->systemd = opts->systemd;
    runopts->verbose = opts->verbose;
    runopts->mode = opts->mode;

    return 0;

fail_user_group:
    free(runopts->user);
    free(runopts->group);
    return -1;
}

void runopts_free(runopts_t *runopts){
    config_free(runopts->config);
    free(runopts->user);
    free(runopts->group);
}


char *get_lock_path(char *socket){
    // allocate a string big enough for "socket" + ".lock" + "\0"
    size_t len = strlen(socket) + strlen(".lock");
    char *lock = malloc(len + 1);
    if(!lock) return NULL;

    sprintf(lock, "%s.lock", socket);
    return lock;
}


int main(int argc, char **argv) {
    int retval = 1;

    if(names_init()){
        fprintf(stderr, "failed to initialize memory\n");
        return 1;
    }

    // parse command line arguments
    int nargs;
    char **args;
    opts_t opts = {0};
    if(parse_opts(argc, argv, &nargs, &args, &opts)){
        print_help();
        goto cu_names;
    }

    runopts_t runopts;
    if(runopts_build(&runopts, &opts)){
        goto cu_names;
    }

    // prepare for signals
    signal(SIGINT, quit_on_signal);
    signal(SIGTERM, quit_on_signal);
    signal(SIGPIPE, SIG_IGN);

    // interpret position arguments
    if(nargs == 0){
        goto help;
    }
    if(nargs > 0){
        if(!strcmp(args[0], "local")){
            if(nargs != 1){
                goto help;
            }
            retval = main_local(&runopts);
            goto cu_opts;
        }

        if(!strcmp(args[0], "serve")){
            if(nargs != 2){
                goto help;
            }
            char *socket = args[1];
            // build a lockfile to protect the unix socket
            char *lock = get_lock_path(socket);
            if(lock == NULL){
                retval = 1;
                goto cu_opts;
            }
            retval = main_serve_unix(&runopts, socket, lock);
            // free the lockfile path string
            free(lock);
            goto cu_opts;
        }

        if(!strcmp(args[0], "read")){
            if(nargs != 1){
                goto help;
            }
            // stream results from stdin
            retval = main_read(&runopts, 0);
            goto cu_opts;
        }

        if(!strcmp(args[0], "serve-tcp")){
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
            retval = main_serve_tcp(&runopts, host, port);
            goto cu_opts;
        }

        if(!strcmp(args[0], "connect")){
            if(nargs != 3){
                goto help;
            }
            char *host = args[1];
            char *port = args[2];
            retval = main_connect(&runopts, host, port);
            goto cu_opts;
        }
    }

help:
    print_help();
    retval = 1;

cu_opts:
    runopts_free(&runopts);
cu_names:
    names_free();
    return retval;
}
