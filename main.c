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
    if (n < MAX_KBS && strcasestr(buf, "keyboard")) {
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
static int keymaps[MAX_CODE];

void init_keymaps(int maps[][2]) {
  memset(keymaps, 0, sizeof(keymaps));

  int i;
  for (i = 0; maps[i][0]; i++)
    if (maps[i][0] > 0 && maps[i][0] < MAX_CODE)
      keymaps[maps[i][0]] = maps[i][1];
}

int proc_event(int out_fd, struct input_event ev) {
  static int last_key = 0;
  print_event("in", ev);
  if (ev.type == EV_SYN)
    send_input_event(out_fd, ev);
  else if (ev.type == EV_KEY) {
    if (last_key) {
      // last key released -> emit mapped key press+release
      if (ev.value == 0 && ev.code == last_key) {
        send_uinput_vals(out_fd, EV_KEY, keymaps[last_key], 1);
        send_uinput_vals(out_fd, EV_KEY, keymaps[last_key], 0);
        last_key = 0;
      }
      // last key repeated -> revert to emit normal press
      else if (ev.value == 2 && ev.code == last_key) {
        send_uinput_vals(out_fd, EV_KEY, last_key, 1);
        last_key = 0;
      }
      // other interesting key pressed -> emit original key and watch for other
      // key
      else if (ev.value == 1 && ev.code < MAX_CODE && keymaps[ev.code]) {
        send_uinput_vals(out_fd, EV_KEY, last_key, 1);
        last_key = ev.code;
      }
      // other key pressed -> emit original key press and other key
      else {
        send_uinput_vals(out_fd, EV_KEY, last_key, 1);
        send_input_event(out_fd, ev);
        last_key = 0;
      }
    } else {
      if (ev.value == 1 && ev.code < MAX_CODE && keymaps[ev.code]) {
        last_key = ev.code;
      } else {
        send_input_event(out_fd, ev);
      }
    }
  }

  return 0;
}

int open_inotify() {
  int inot = inotify_init();
  inotify_add_watch(inot, "/dev/input", IN_CREATE | IN_DELETE);

  return inot;
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

  int maps[][2] = {{KEY_CAPSLOCK, KEY_ESC},   {KEY_RIGHTSHIFT, KEY_BACKSPACE},
                   {KEY_LEFTALT, KEY_MINUS},  {KEY_RIGHTALT, KEY_EQUAL},
                   {KEY_LEFTSHIFT, KEY_MUTE}, {KEY_LEFTMETA, KEY_VOLUMEDOWN},
                   {KEY_SYSRQ, KEY_VOLUMEUP}, {0, 0}};

  init_keymaps(maps);

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
    select(1 + max_fd, &fds, NULL, NULL, NULL);

    for (i = 0; i < n_kbs; i++) {
      if (FD_ISSET(in_fds[i], &fds)) {
        struct input_event ev;
        ret = read(in_fds[i], &ev, sizeof(struct input_event));
        if (ret > 0)
          proc_event(out_fd, ev);
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
