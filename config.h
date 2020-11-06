#ifndef CONFIG_H
#define CONFIG_H

#include <regex.h>
#include <lua.h>

#include "key_action.h"
#include "resolver.h"

// a linked list of keyboard grabs or ignores passed by the user
typedef struct grab_t {
    // a compliled regex pattern
    regex_t regex;
    bool ignore;
    // except when ignore==true, map will always have type == KT_MAP:
    key_action_t map;
    struct grab_t *next;
    // initialized when the send_t is known, well after the config is read
    struct resolver resolver;
} grab_t;

typedef struct {
    lua_State *L;
    grab_t *grabs;
} config_t;

config_t *config_new(const char* config_file);
void config_free(config_t *config);

#endif // CONFIG_H
