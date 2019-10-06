#ifndef RESOLVER_H
#define RESOLVER_H

#include <linux/input.h>
#include <stdbool.h>
#include <time.h>

#include "app.h"
#include "config.h"

// maximum code that can represent a key
#define MAX_CODE 256

// the maximum number of unresolved events before we start dropping events
#define URMAX 32

// a special value in release_map which indicates we should reset the keymap
#define RESET_KEYMAP 256

// the state of the resolver thread, which decides how to interpret keys
struct resolver {
    // We can either send to a local keyboard device or to a network socket
    send_t send;
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

    key_action_t *root_keymap;
    /* for now, we will only track a single current keymap, and releasing any
       keyamp will send you back to the root keymap.  This isn't correct
       behavior, but correct behavior seems complex and I can't quite decide on
       what is correct behavior anyway.  For now, the simple solution is
       sufficient. */
    key_action_t *current_keymap;
};

void resolver_init(struct resolver *r, key_action_t *root_keymap,
        send_t send, void *send_data);

bool resolve(struct resolver *r);

// returns the timeout to be used for select(), which is either out or NULL
struct timeval *select_timeout(struct resolver *r, struct timeval *out);

#endif // RESOLVER_H
