#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <errno.h>

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

int open_input(){
  char dev[256];
  char buf[256];
  int input;
  int i, ret;
  for(i = 0;; i++){
    sprintf(dev, "/dev/input/event%d", i);
    input = open(dev, O_RDWR);
    if(input < 0){
      if(errno == ENOENT){
	puts("ran out of devices");
	return -1;
      }
      else{
	printf("%s: %s\n", dev, strerror(errno));
	close(input);
	continue;
      }
    }

    ioctl(input, EVIOCGNAME(sizeof(buf)), buf);
    DEBUG("%s\n", buf);
    if(strstr(buf, "keyboard"))
      break;
  }
  DEBUG("reading from %s (%s)\n", dev, buf);

  ret = ioctl(input, EVIOCGRAB, 1);
  if(ret < 0){
    close(input);
    puts(strerror(errno));
    return -1;
  }

  return input;
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
  sleep(1);
  int i;
  int ret;
  int in_fd = open_input();
  int out_fd = open_output();

  if(in_fd < 0){
    fputs("couldn't open input", stderr);
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

  i = 0;
  while(1){
    ret = read(in_fd, &ev, sizeof(struct input_event));
    proc_event(out_fd, ev);
  }
}
