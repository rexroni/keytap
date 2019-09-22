#ifndef DEVICES_H
#define DEVICES_H

#include <stdbool.h>

#define MAX_KBS 16

int open_output();
bool device_name_check(const char *name);
int open_inputs(int *res);
void handle_inotify_events(int inot, int *in_fds, int* n_kbs);
int open_inotify();

#endif // DEVICES_H
