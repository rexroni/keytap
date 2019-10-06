#ifndef DEVICES_H
#define DEVICES_H

#include <stdbool.h>

#include "resolver.h"

#define MAX_KBS 16

// Every keyboard maintains its own resolver state
typedef struct {
    int fd;
    struct resolver resolv;
} keyboard_t;

int open_output();
bool device_name_check(const char *name);
void open_inputs(keyboard_t *kbs, int *n_kbs, grab_t *grabs, send_t send,
        void *send_data);
void handle_inotify_events(int inot, keyboard_t *kbs, int* n_kbs,
        grab_t *grabs, send_t send, void *send_data);
int open_inotify();

#endif // DEVICES_H
