#ifndef DEVICES_H
#define DEVICES_H

#include <stdbool.h>
#include "config.h"

#define MAX_KBS 16

typedef struct {
    int fd;
    grab_t *grab;
} keyboard_t;

int open_output(void);
bool device_name_check(const char *name);
void open_inputs(keyboard_t *kbs, int *n_kbs, grab_t *grabs, bool verbose);
void handle_inotify_events(int inot, keyboard_t *kbs, int* n_kbs,
        grab_t *grabs, bool verbose);
int open_inotify();

#endif // DEVICES_H
