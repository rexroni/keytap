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
      printf(" %15s %6s\n", key_names[ev.code], val_names[ev.value]);
    else
      printf(" %15d %6d\n", ev.code, ev.value);
  }
}

#else
#define DEBUG(...)
void print_event(char *tag, struct input_event ev) {}
#endif

int send_input_event(int fd, struct input_event ev) {
  print_event("out", ev);
  return write(fd, &ev, sizeof(ev));
}

int send_uinput_vals(int fd, int type, int code, int value) {
  struct input_event ev;
  ev.type = type;
  ev.code = code;
  ev.value = value;
  return send_input_event(fd, ev);
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

int open_inputs(int *res) {
  int n = 0;

  char dev[512];
  char buf[256];
  int input;
  int ret;

  char devdir[] = "/dev/input";

  DIR *d = opendir(devdir);
  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (ent->d_name != strstr(ent->d_name, "event"))
      continue;

    snprintf(dev, sizeof(dev), "%s/%s", devdir, ent->d_name);

    input = open(dev, O_RDWR);
    if (input < 0) {
      fprintf(stderr, "%s: %s\n", dev, strerror(errno));
      close(input);
      continue;
    }

    ioctl(input, EVIOCGNAME(sizeof(buf)), buf);
    // DEBUG("%s\n", buf);
    if (n < MAX_KBS && device_name_check(buf)) {
      ret = ioctl(input, EVIOCGRAB, 1);
      if (ret < 0) {
        close(input);
        printf("%s: %s\n", dev, strerror(errno));
      } else {
        res[n++] = input;
        DEBUG("reading from %s (%s)\n", dev, buf);
      }
    }
  }

  return n;
}

#define MAX_CODE 256

int open_inotify() {
  int inot = inotify_init();
  inotify_add_watch(inot, "/dev/input", IN_CREATE | IN_DELETE);

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
    int *out_fd;
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
                    send_input_event(*r->out_fd, ev);
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
                        send_input_event(*r->out_fd, ev);
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
                        send_input_event(*r->out_fd, ev);
                        r->release_map[KEY_D] = KEY_D;
                        break;
                    case WAVEFORM_TIMEOUT:
                    case WAVEFORM_ANOTHER_KEY:
                        ev.code = KEY_LEFTMETA;
                        // when "D" is released, release the meta key
                        r->release_map[KEY_D] = KEY_LEFTMETA;
                        send_input_event(*r->out_fd, ev);
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
                send_input_event(*r->out_fd, ev);
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
                    send_input_event(*r->out_fd, ev);
            }
            resolved = true;
        }
    }else{
        // non EV_KEY events are passed through unchanged
        if (ev.type == EV_SYN)
            send_input_event(*r->out_fd, ev);
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
                    send_input_event(*r->out_fd, ev);
                    // mark the end of an input packet of keypresses
                    send_uinput_vals(*r->out_fd, EV_SYN, SYN_REPORT, 0);
                    // one less element
                    r->ur_len--;
            }
        }
    }
    return resolved;
}


int main() {
  usleep(250000);

  int i, ret;
  int in_fds[MAX_KBS];
  int n_kbs = open_inputs(in_fds);
  int out_fd = open_output();
  int inot = open_inotify();
  char buf[16384];

  if (n_kbs == 0) {
    fputs("couldn't open any inputs", stderr);
    return 1;
  }
  if (out_fd < 0) {
    fputs("couldn't open output", stderr);
    return 1;
  }

  // the main thread will read from the device

  // have a thread responsible for resolving keypresses
  struct resolver r = {.out_fd=&out_fd};

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

  fd_set fds;
  while (1) {
    FD_ZERO(&fds);
    FD_SET(inot, &fds);
    int max_fd = inot;
    for (i = 0; i < n_kbs; i++) {
      FD_SET(in_fds[i], &fds);
      if (in_fds[i] > max_fd)
        max_fd = in_fds[i];
    }

    struct timeval timeout;
    select(1 + max_fd, &fds, NULL, NULL, NULL);

    // if the timeout was reached, try to resolve immediately
    if(timeout.tv_sec == 0 && timeout.tv_usec == 0){
        while(resolve(&r, fmap_exp));
    }

    for (i = 0; i < n_kbs; i++) {
      if (FD_ISSET(in_fds[i], &fds)) {
        struct input_event ev;
        ret = read(in_fds[i], &ev, sizeof(struct input_event));
        if (ret > 0){
          // add event to unresolved, or drop it if there isn't space for it
          if(r.ur_len < URMAX){
            r.unresolved[(r.ur_start + r.ur_len++) % URMAX] = ev;
            while(resolve(&r, fmap_exp));
          }
        }
      }
    }

    if (FD_ISSET(inot, &fds)) {
      DEBUG("reloading inputs\n");
      read(inot, buf, sizeof(buf));
      for (i = 0; i < n_kbs; i++) {
        close(in_fds[i]);
      }
      n_kbs = open_inputs(in_fds);
    }
  }
}
