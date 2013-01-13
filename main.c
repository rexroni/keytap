#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#if 0
#define DEBUG(...) printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

int send_input_event(int fd, struct input_event ev){
  DEBUG("out: %d %d %d\n", ev.type, ev.code, ev.value);
  return write(fd, &ev, sizeof(ev));
}

int send_uinput_vals(int fd, int type, int code, int value){
  struct input_event ev;
  ev.type = type;
  ev.code = code;
  ev.value = value;
  return send_input_event(fd, ev);
}

int open_output(){
  int i, ret;
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if(fd < 0) {
    return -1;
  }

  ret = ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ret = ioctl(fd, UI_SET_EVBIT, EV_SYN);
  for(i = 0; i < 256; i++)
    ioctl(fd, UI_SET_KEYBIT, i);

  struct uinput_user_dev uidev;

  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-sample");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor  = 0x1234;
  uidev.id.product = 0xfedc;
  uidev.id.version = 1;

  ret = write(fd, &uidev, sizeof(uidev));
  ret = ioctl(fd, UI_DEV_CREATE);

  return fd;
}

int *open_inputs(int *n_kbs){
  static int MAX_KBS = 16;
  int *res = (int *)malloc(MAX_KBS * sizeof(int));
  int n = 0;

  char dev[256];
  char buf[256];
  int input;
  int ret;

  char devdir[] = "/dev/input";

  DIR *d = opendir(devdir);
  struct dirent *ent;
  while((ent = readdir(d))){
    if(ent->d_name != strstr(ent->d_name, "event"))
      continue;

    sprintf(dev, "%s/%s", devdir, ent->d_name);

    input = open(dev, O_RDWR);
    if(input < 0){
      fprintf(stderr, "%s: %s\n", dev, strerror(errno));
      close(input);
      continue;
    }

    ioctl(input, EVIOCGNAME(sizeof(buf)), buf);
    //DEBUG("%s\n", buf);
    if(n < MAX_KBS && strcasestr(buf, "keyboard")){
      ret = ioctl(input, EVIOCGRAB, 1);
      if(ret < 0){
        close(input);
        printf("%s: %s\n", dev, strerror(errno));
      }
      else{
        res[(n)++] = input;
        DEBUG("reading from %s (%s)\n", dev, buf);
      }
    }
  }

  *n_kbs = n;
  return res;
}

#define MAX_CODE 256
static int keymaps[MAX_CODE];

void init_keymaps(int maps[][2], int nmaps){
  memset(keymaps, 0, sizeof(keymaps));

  int i;
  for(i = 0; i < nmaps; i++)
    if(maps[i][0] > 0 && maps[i][0] < MAX_CODE)
      keymaps[maps[i][0]] = maps[i][1];
}

int proc_event(int out_fd, struct input_event ev){
  static int last_key = 0;
  DEBUG("in: %d %d %d\n", ev.type, ev.code, ev.value);
  if(ev.type == EV_SYN)
    send_input_event(out_fd, ev);
  else if(ev.type == EV_KEY){
    if(last_key){
      // last key released -> emit mapped key press+release
      if(ev.value == 0 && ev.code == last_key){
	send_uinput_vals(out_fd, EV_KEY, keymaps[last_key], 1);
	send_uinput_vals(out_fd, EV_KEY, keymaps[last_key], 0);
        last_key = 0;
      }
      // last key repeated -> revert to emit normal press
      else if(ev.value == 2 && ev.code == last_key){
        send_uinput_vals(out_fd, EV_KEY, last_key, 1);
        last_key = 0;
      }
      // other interesting key pressed -> emit original key and watch for other key
      else if(ev.value == 1 && ev.code < MAX_CODE && keymaps[ev.code]){
        send_uinput_vals(out_fd, EV_KEY, last_key, 1);
        last_key = ev.code;
      }
      // other key pressed -> emit original key press and other key
      else{
        send_uinput_vals(out_fd, EV_KEY, last_key, 1);
        send_input_event(out_fd, ev);
        last_key = 0;
      }
    }
    else{
      if(ev.value == 1 && ev.code < MAX_CODE && keymaps[ev.code]){
        last_key = ev.code;
      }
      else{
        send_input_event(out_fd, ev);
      }
    }
  }

  return 0;
}

int main(){
  usleep(250000);
  int i;
  int ret;
  int n_kbs = 0;
  int *in_fds = open_inputs(&n_kbs);
  int out_fd = open_output();

  if(n_kbs == 0){
    fputs("couldn't open any inputs", stderr);
    return 1;
  }
  if(out_fd < 0){
    fputs("couldn't open output", stderr);
    return 1;
  }

  struct input_event ev;

  int maps[][2] = {{KEY_CAPSLOCK, KEY_ESC},
                   {KEY_RIGHTSHIFT, KEY_BACKSPACE}};

  init_keymaps(maps, 2);

  int max_fd = 0;
  for(i = 0; i < n_kbs; i++)
    if(in_fds[i] > max_fd) max_fd = in_fds[i];

  fd_set fds;
  while(1){
    FD_ZERO(&fds);
    for(i = 0; i < n_kbs; i++)
      FD_SET(in_fds[i], &fds);
    select(1+max_fd, &fds, NULL, NULL, NULL);

    for(i = 0; i < n_kbs; i++){
      if(FD_ISSET(in_fds[i], &fds)){
        ret = read(in_fds[i], &ev, sizeof(struct input_event));
        proc_event(out_fd, ev);
      }
    }
  }
}
