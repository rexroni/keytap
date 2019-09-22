#ifndef RESOLVER_H
#define RESOLVER_H

#include <linux/input.h>
#include <stdbool.h>
#include <time.h>

// maximum code that can represent a key
#define MAX_CODE 256

// the maximum number of unresolved events before we start dropping events
#define URMAX 32

/* if you find one of these values in the release_map, then that means
   "instead of sending a keyrelease event, just stop using this keymap" */
enum keymap {
    KEYMAP_NONE = 0,
    KEYMAP_F = 256,
};

// the state of the resolver thread, which decides how to interpret keys
struct resolver {
    // We can either send to a local keyboard device or to a network socket
    int (*send)(void *send_data, struct input_event ev);
    void *send_data;
    /* key events received, but we haven't decided how to treat them.  No key
       can be resolved until all of the keys before it are resolved. */
    struct input_event unresolved[URMAX];
    size_t ur_len;
    size_t ur_start;
    /* when we decide how to treat a keypress, we have to remember what key to
       release.  This also implicitly maps out which keys are pressed. */
    int release_map[256];
    /* If we have an unresolvable event, we mark the time that it will become
       resolvable by timeout */
    struct timeval resolvable_time;
    bool use_resolvable_time;
    enum keymap current_keymap;
};

bool resolve(struct resolver *r, int *fmap_exp);

// returns the timeout to be used for select(), which is either out or NULL
struct timeval *select_timeout(struct resolver *r, struct timeval *out);

#endif // RESOLVER_H
