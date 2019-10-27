#ifndef NAMES_H
#define NAMES_H

#include <stdint.h>
#include <linux/input.h>

const char *get_input_name(uint16_t value);

// returns 0 if name not found
uint16_t get_input_value(const char *name);

// returns 0/-1 on success/error
int names_init(void);
void names_free(void);


typedef void (*name_hook_t)(void *arg, const char *name, uint16_t val);

void for_each_name(name_hook_t hook, void* arg);
void for_each_value(name_hook_t hook, void* arg);

#endif // NAMES_H
