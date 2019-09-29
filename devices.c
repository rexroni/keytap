#include "devices.h"

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

const char *grab_by_name[] = {"keyboard", "ergodox"};

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

bool device_name_check(const char *name){
    for(size_t i = 0; i < sizeof(grab_by_name) / sizeof(*grab_by_name); i++){
        if(strcasestr(name, grab_by_name[i])){
            return true;
        }
    }
    return false;
}

static int open_input(char *dev){
  int fd = open(dev, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "%s: %s\n", dev, strerror(errno));
    return -1;
  }

  char buf[256];
  ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
  if(device_name_check(buf)){
    int ret = ioctl(fd, EVIOCGRAB, 1);
    if (ret < 0) {
      fprintf(stderr, "%s: %s\n", dev, strerror(errno));
      close(fd);
      return -1;
    } else {
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

int open_inotify() {
  int inot = inotify_init();
  inotify_add_watch(inot, "/dev/input", IN_CREATE);

  return inot;
}
