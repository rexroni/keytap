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

#include "config.h"

#ifndef NDEBUG
#define DEBUG(...) printf(__VA_ARGS__)

#include "names.h"

char *type_str(int t) {
  static char buf[256];

  switch (t) {
  case EV_SYN:
    return "EV_SYN";
  case EV_KEY:
    return "EV_KEY";
  case EV_MSC:
    return "EV_MSC";
  default:
    sprintf(buf, "(type %d)", t);
    return buf;
  }
}

void print_event(char *tag, struct input_event ev) {
  if (ev.type || ev.code || ev.value) {
    printf("%4s: %s", tag, type_str(ev.type));
    if (ev.type == EV_KEY)
    else
      printf(" %15d %6d\n", ev.code, ev.value);
  }
}

#else
#define DEBUG(...)
void print_event(char *tag, struct input_event ev) {}
#endif

int send_event_locally(void *data, struct input_event ev){
  print_event("out", ev);
  int *fd = data;

  return write(*fd, &ev, sizeof(ev));
}

int open_output() {
  int i;
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    return -1;
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_SYN);
  for (i = 0; i < 256; i++)
    ioctl(fd, UI_SET_KEYBIT, i);

  struct uinput_user_dev uidev;

  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "keytap");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1111;
  uidev.id.product = 0x0001;
  uidev.id.version = 1;

  write(fd, &uidev, sizeof(uidev));
  ioctl(fd, UI_DEV_CREATE);

  return fd;
}

#define MAX_KBS 16

bool device_name_check(const char *name){
    for(size_t i = 0; i < sizeof(grab_by_name) / sizeof(*grab_by_name); i++){
        if(strcasestr(name, grab_by_name[i])){
            return true;
        }
    }
    return false;
}

int open_input(char *dev){
  int fd = open(dev, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "%s: %s\n", dev, strerror(errno));
    return -1;
  }

  char buf[256];
  ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
  // DEBUG("%s\n", buf);
  if(device_name_check(buf)){
    int ret = ioctl(fd, EVIOCGRAB, 1);
    if (ret < 0) {
      fprintf(stderr, "%s: %s\n", dev, strerror(errno));
      close(fd);
      return -1;
    } else {
      DEBUG("reading from %s (%s)\n", dev, buf);
      return fd;
    }
  }
  return -1;
}

int open_inputs(int *res) {
  int n = 0;

  char dev[512];

  DIR *d = opendir("/dev/input");
  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (ent->d_name != strstr(ent->d_name, "event"))
      continue;

    snprintf(dev, sizeof(dev), "/dev/input/%s", ent->d_name);

    if(n < MAX_KBS){
      int fd = open_input(dev);
      if(fd < 0)
        continue;

      res[n++] = fd;
    }
  }

  return n;
}

void handle_inotify_events(int inot, int *in_fds, int* n_kbs){
    // most of this section is straight from `man 7 inotify`
    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    ssize_t len = read(inot, buf, sizeof(buf));
    if(len == -1 && errno != EAGAIN){
        perror("read");
        exit(3);
    }

    // EAGAIN means we are out of events for now.
    if(len <= 0) return;

    // read all the events we just got
    for(char *ptr = buf; ptr < buf + len;
            ptr += sizeof(struct inotify_event) + event->len){
        event = (const struct inotify_event *)ptr;

        if(*n_kbs < MAX_KBS){
            char dev[512];
            snprintf(dev, sizeof(dev), "/dev/input/%s", event->name);
            int fd = open_input(dev);
            if(fd < 0)
                continue;
            in_fds[(*n_kbs)++] = fd;
        }
    }
}

#define MAX_CODE 256

int open_inotify() {
  int inot = inotify_init();
  inotify_add_watch(inot, "/dev/input", IN_CREATE);

  return inot;
}

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

/* if you find one of these values in the release_map, then that means
   "instead of sending a keyrelease event, just stop using this keymap" */
enum keymap {
    KEYMAP_NONE = 0,
    KEYMAP_F = 256,
};

// the maximum number of unresolved events before we start dropping events
#define URMAX 32

// the state of the resolver thread, which decides how to interpret keys
struct resolver {
    // We can either send to a local keyboard device or to a network socket
    int (*send)(void *send_data, struct input_event ev);
    void *send_data;
    /* key events received, but we haven't decided how to treat them.  No key
       can be resolved until all of the keys before it are resolved. */
    struct input_event unresolved[URMAX];
    size_t ur_len;
    size_t ur_start;
    /* when we decide how to treat a keypress, we have to remember what key to
       release.  This also implicitly maps out which keys are pressed. */
    int release_map[256];
    /* If we have an unresolvable event, we mark the time that it will become
       resolvable by timeout */
    struct timeval resolvable_time;
    bool use_resolvable_time;
    enum keymap current_keymap;
};

// helper function for difference of two struct timespec's (computes "a - b")
long msec_diff(struct timeval a, struct timeval b){
    long diff = (a.tv_usec / 1000) - (b.tv_usec / 1000);
    return diff + (a.tv_sec - b.tv_sec) * 1000;
}

struct timeval msec_after(struct timeval tv, long millis){
    // handle integer seconds
    tv.tv_sec += millis / 1000;
    // calculate msec
    long msec = (millis % 1000) + (tv.tv_usec / 1000);
    // handle msec overflow
    tv.tv_sec += msec > 1000;
    tv.tv_usec = (tv.tv_usec % 1000) + (msec % 1000) * 1000;
    return tv;
}

// returns the timeout to be used for select(), which is either out or NULL
struct timeval *select_timeout(struct resolver *r, struct timeval *out){
    // don't pass a timeout if none is valid.
    if(!r->use_resolvable_time){
        return NULL;
    }

    struct timespec now_timespec;
    clock_gettime(CLOCK_REALTIME, &now_timespec);
    struct timeval now;
    TIMESPEC_TO_TIMEVAL(&now, &now_timespec);

    long mdiff = msec_diff(r->resolvable_time, now);
    if(mdiff < 0){
        // somehow the timeout has expired, zero length timeout
        out->tv_sec = 0;
        out->tv_usec = 0;
    }else{
        out->tv_sec = mdiff / 1000;
        out->tv_usec = (mdiff % 1000) * 1000;
    }
    return out;
}

/* Given a pressed key X, check unresolved events to deterimine which is first:
     - X has timed out (X is a modifier)
     - X has been released (X is not a modifier)
     - another key has been pressed and released (X is a modifier)
     - actually, neither has happened yet (resolvable time will be set) */
enum waveform {
    WAVEFORM_TIMEOUT,
    WAVEFORM_MAIN_KEY_RELEASED,
    WAVEFORM_ANOTHER_KEY,
    WAVEFORM_NONE_YET,
};
enum waveform check_waveform(const struct resolver *r, struct input_event ev){
    struct timespec now_timespec;
    clock_gettime(CLOCK_REALTIME, &now_timespec);
    struct timeval now;
    TIMESPEC_TO_TIMEVAL(&now, &now_timespec);

    // is the keypress old enough that we know it is a modifier?
    if(msec_diff(now, ev.time) > 200){
        return WAVEFORM_TIMEOUT;
    }

    int keys_pressed[256] = {0};
    for(size_t i = 1; i < r->ur_len; i++){
        struct input_event ev2 = r->unresolved[(r->ur_start + i) % URMAX];
        if(ev2.value == 0 && ev2.code == ev.code){
            // the main key was released first, it's just its normal self
            return WAVEFORM_MAIN_KEY_RELEASED;
        }else if(ev2.value == 1 && ev2.code < MAX_CODE){
            keys_pressed[ev2.code] = 1;
        }else if(ev2.value == 0 && ev2.code < MAX_CODE && keys_pressed[ev2.code]){
            // some other key was pressed and released, main key is a modifier
            return WAVEFORM_ANOTHER_KEY;
        }
    }

    return WAVEFORM_NONE_YET;
}

/* helper function which tries to resolve the oldest key event.  Returns false
   if it deems the event unresolvable, setting the resolver.resolve_time as
   appropriate. */
bool resolve(struct resolver *r, int *fmap_exp){
    if(r->ur_len == 0){
        return false;
    }

    // grab the oldest event
    struct input_event ev = r->unresolved[r->ur_start % URMAX];

    bool resolved = false;
    r->use_resolvable_time = false;

    if(ev.type == EV_KEY){
        // key released
        if(ev.value == 0){
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            if(ev.code < MAX_CODE){
                ev.code = r->release_map[ev.code];
                r->release_map[ev.code] = 0;
            }
            switch(ev.code){
                case KEYMAP_F:
                    // disable KEYMAP_F if it is the current one
                    if(r->current_keymap == KEYMAP_F)
                        r->current_keymap = KEYMAP_NONE;
                    break;
                case 0:
                    // we must have sent this key release early; do nothing.
                    break;
                default:
                    r->send(r->send_data, ev);
            }
            resolved = true;
        }
        // key pressed
        else if(ev.value == 1){
            // F key changes the keymap
            if(ev.code == KEY_F){
                enum waveform waveform = check_waveform(r, ev);
                switch(waveform){
                    case WAVEFORM_MAIN_KEY_RELEASED:
                        resolved = true;
                        // a normal F
                        r->send(r->send_data, ev);
                        r->release_map[KEY_F] = KEY_F;
                        break;
                    case WAVEFORM_TIMEOUT:
                    case WAVEFORM_ANOTHER_KEY:
                        resolved = true;
                        // don't emit F; switch keymaps
                        r->current_keymap = KEYMAP_F;
                        // when "F" is released, disable the keymap
                        r->release_map[KEY_F] = KEYMAP_F;
                        break;
                    case WAVEFORM_NONE_YET:
                        r->resolvable_time = msec_after(ev.time, 200);
                        r->use_resolvable_time=true;
                        break;
                }
            // D key behaves like a meta key
            }else if(ev.code == KEY_D){
                enum waveform waveform = check_waveform(r, ev);
                switch(waveform){
                    case WAVEFORM_MAIN_KEY_RELEASED:
                        resolved = true;
                        // a normal D
                        r->send(r->send_data, ev);
                        r->release_map[KEY_D] = KEY_D;
                        break;
                    case WAVEFORM_TIMEOUT:
                    case WAVEFORM_ANOTHER_KEY:
                        ev.code = KEY_LEFTMETA;
                        // when "D" is released, release the meta key
                        r->release_map[KEY_D] = KEY_LEFTMETA;
                        r->send(r->send_data, ev);
                        resolved = true;
                        break;
                    case WAVEFORM_NONE_YET:
                        r->resolvable_time = msec_after(ev.time, 200);
                        r->use_resolvable_time=true;
                        break;
                }
            }else{
                int original_code = ev.code;
                // normal key was pressed
                switch(r->current_keymap){
                    case KEYMAP_NONE: break;
                    case KEYMAP_F: ev.code = fmap_exp[original_code]; break;
                }
                // remember how to release the key
                if(original_code < MAX_CODE){
                    r->release_map[original_code] = ev.code;
                }
                r->send(r->send_data, ev);
                resolved = true;
            }
        }
        // key repeated
        else if(ev.value == 2){
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            if(ev.code < MAX_CODE){
                ev.code = r->release_map[ev.code];
            }
            switch(ev.code){
                case KEYMAP_F:
                    // ignore repeats of keymap modifiers
                    break;
                default:
                    r->send(r->send_data, ev);
            }
            resolved = true;
        }
    }else{
        // non EV_KEY events are passed through unchanged
        if (ev.type == EV_SYN)
            r->send(r->send_data, ev);
        resolved = true;
    }

    if(resolved){
        // one less element
        r->ur_len--;
        // but we start one later
        r->ur_start = (r->ur_start + 1) % URMAX;
    }else{
        /* if we decided we couldn't resolve the oldest event, but the newest
           event is a key release, emit the key release immediately.  We know
           that the initial press must have been resolved because if the
           initial press had come after the unresolvable key, then now that
           we have the release the unresolvable key would be resolvable. */
        ev = r->unresolved[(r->ur_start + r->ur_len - 1) % URMAX];
        if(ev.value == 0){
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            if(ev.code < MAX_CODE){
                ev.code = r->release_map[ev.code];
            }
            switch(ev.code){
                case KEYMAP_F:
                    // disable KEYMAP_F if it is the current one
                    if(r->current_keymap == KEYMAP_F)
                        r->current_keymap = KEYMAP_NONE;
                    // one less element
                    r->ur_len--;
                    break;
                case KEY_LEFTALT:
                case KEY_RIGHTALT:
                case KEY_LEFTCTRL:
                case KEY_RIGHTCTRL:
                case KEY_LEFTMETA:
                case KEY_RIGHTMETA:
                case KEY_LEFTSHIFT:
                case KEY_RIGHTSHIFT:
                    // modifier keys don't get resolved early
                    break;
                default:
                    r->send(r->send_data, ev);
                    // mark the end of an input packet of keypresses
                    struct input_event syn_ev = {
                        .type = EV_SYN,
                        .code = SYN_REPORT,
                        .value = 0,
                    };
                    r->send(r->send_data, syn_ev);
                    // one less element
                    r->ur_len--;
            }
        }
    }
    return resolved;
}

typedef struct {
    int (*send)(void*, struct input_event);
    // returns a max_fd value
    int (*prep_select)(void*, fd_set *r_fds, fd_set *w_fds);
    void (*handle_select)(void*, fd_set *r_fds, fd_set *w_fds);
} app_t;

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
          DEBUG("closing deviced\n");
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
      DEBUG("detected new input\n");
      handle_inotify_events(inot, in_fds, &n_kbs);
    }
  }
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


int gai_open(const char* host, const char* service, bool server_side){
    int out_fd;

    // prepare for getaddrinfo
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    //hints.ai_flags = server_side ? AI_PASSIVE : 0;

    // get address of host
    struct addrinfo* ai;
    int ret = getaddrinfo(host, service, &hints, &ai);
    if(ret != 0){
        return 1;
    }
    // reset error
    errno = 0;

    // connect to the host
    struct addrinfo* p;
    for(p = ai; p != NULL; p = p->ai_next){
        struct sockaddr_in* sin = (struct sockaddr_in*)p->ai_addr;
        printf("%s to ip addr %s\n", server_side ? "Binding" : "Connecting",
                inet_ntoa(sin->sin_addr));
        // create a socket
        out_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(out_fd < -1){
            perror("socket");
            continue;
        }
        // bind/listen or connect, depending
        if(server_side){
            ret = bind(out_fd, p->ai_addr, p->ai_addrlen);
            if(ret != 0){
                perror("bind");
                close(out_fd);
                continue;
            }
            ret = listen(out_fd, 5);
            if(ret != 0){
                perror("bind");
                close(out_fd);
                continue;
            }
        }else{
            printf("connecting!\n");
            ret = connect(out_fd, p->ai_addr, p->ai_addrlen);
            if(ret != 0){
                perror("connect");
                close(out_fd);
                continue;
            }
        }
        // if we made it here, we connected successfully
        break;
    }
    // make sure we found something
    if(p == NULL){
        fprintf(stderr, "failed all attempts\n");
        out_fd = -1;
    }

    freeaddrinfo(ai);

    return out_fd;
}

typedef struct {
    int clients[8];
    size_t nclients;
    size_t active_client;
    // prepared data to send to client
    char for_client[8192];
    size_t fc_len;
    int accept_fd;
} kbd_server_t;

int server_send_event(void *app_dafa, struct input_event ev){
    print_event("out", ev);

    kbd_server_t *s = app_dafa;
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
