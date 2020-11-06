#include "devices.h"
#include "config.h"
#include "names.h"

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

void enable_ev_key_value(void *arg, const char *name, uint16_t val){
    (void)name;
    int *fd = arg;
    ioctl(*fd, UI_SET_KEYBIT, val);
}

int open_output() {
  int i;
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    return -1;
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_SYN);
  ioctl(fd, UI_SET_EVBIT, EV_MSC);
  ioctl(fd, UI_SET_EVBIT, EV_REL);

  // keys and buttons
  for_each_value(enable_ev_key_value, &fd);

  // all rel events
  for (i = REL_X; i <= REL_MAX ; i++)
    ioctl(fd, UI_SET_RELBIT, i);

  struct uinput_user_dev uidev;

  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "sdiol");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1111;
  uidev.id.product = 0x0001;
  uidev.id.version = 1;

  write(fd, &uidev, sizeof(uidev));
  ioctl(fd, UI_DEV_CREATE);

  return fd;
}


// check the linked list of grabs and return a grab that matches
grab_t *check_grabs(grab_t *grabs, const char *name){
    for(grab_t *grab = grabs; grab != NULL; grab = grab->next){
        int ret = regexec(&grab->regex, name, 0, NULL, 0);
        if(ret != REG_NOMATCH){
            return grab->ignore ? NULL : grab;
        }
    }
    return NULL;
}

// return true if we decided to grab the device
static bool open_input(char *dev, grab_t *grabs, int *fd_out,
        grab_t **grab_out, bool verbose){
    *grab_out = NULL;
    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", dev, strerror(errno));
        return false;
    }

    char buf[256];
    ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
    grab_t *grab;
    if((grab = check_grabs(grabs, buf)) != NULL){
        if(verbose){
            printf("grabbing %s\n", buf);
        }
        int ret = ioctl(fd, EVIOCGRAB, 1);
        if (ret < 0) {
            fprintf(stderr, "%s: %s\n", dev, strerror(errno));
            close(fd);
            return false;
        } else {
            *fd_out = fd;
            *grab_out = grab;
            return true;
        }
    }
    if(verbose){
        printf("ignoring %s\n", buf);
    }
    return false;
}

void open_inputs(keyboard_t *kbs, int *n_kbs, grab_t *grabs, bool verbose){
    *n_kbs = 0;

    char dev[512];

    DIR *d = opendir("/dev/input");
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name != strstr(ent->d_name, "event"))
            continue;

        snprintf(dev, sizeof(dev), "/dev/input/%s", ent->d_name);

        if(*n_kbs < MAX_KBS){
            int fd;
            grab_t *grab;
            if(!open_input(dev, grabs, &fd, &grab, verbose))
                continue;

            keyboard_t kb;
            kb.fd = fd;
            kb.grab = grab;

            kbs[(*n_kbs)++] = kb;
        }
    }
    closedir(d);
}

void handle_inotify_events(int inot, keyboard_t *kbs, int* n_kbs,
        grab_t *grabs, bool verbose){
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
            int fd;
            grab_t *grab;
            if(!open_input(dev, grabs, &fd, &grab, verbose))
                continue;

            keyboard_t kb;
            kb.fd = fd;
            kb.grab = grab;

            kbs[(*n_kbs)++] = kb;
        }
    }
}

int open_inotify() {
  int inot = inotify_init();
  inotify_add_watch(inot, "/dev/input", IN_CREATE);

  return inot;
}
