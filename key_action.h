#ifndef KEY_ACTION_H
#define KEY_ACTION_H

struct key_action_t;
typedef struct key_action_t key_action_t;

struct key_macro_t;
typedef struct key_macro_t key_macro_t;

struct key_macro_t{
    int code;
    bool press;
    key_macro_t *next;
};

typedef enum {
    // rollover waveform triggers the "tap" key action
    DUAL_MODE_TAP_ON_ROLLOVER = 0,
    // rollover waveform triggers the "hold" key action
    DUAL_MODE_HOLD_ON_ROLLOVER = 1,
    // only the timeout can trigger the "hold" key action
    DUAL_MODE_TIMEOUT_ONLY = 2,
} dual_key_mode_t;

typedef struct {
    // members must not be addtional key_dual_t's
    key_action_t *tap;
    key_action_t *hold;
    dual_key_mode_t mode;
    long hold_ms;
    long double_tap_ms;
} key_dual_t;

enum key_type {
    KT_NONE,
    KT_SIMPLE,
    KT_MACRO,
    KT_DUAL,
    KT_MAP,
};

union key_union {
    int simple;
    key_macro_t *macro;
    key_dual_t dual;
    key_action_t *map; // always allocated to length of 256
    key_action_t *ref; // non-root keymaps with KT_NONE will have this filled
};

struct key_action_t {
    enum key_type type;
    union key_union key;
};

key_macro_t *key_macro_new(int code, bool press);
void key_macro_free(key_macro_t *macro);
key_macro_t *key_macro_dup(key_macro_t *macro);

void key_action_free(key_action_t *ka);
int key_action_dup(const key_action_t *in, key_action_t *out);

// helper function for dereferencing key actions from a key action map
key_action_t *key_action_get(key_action_t *ka, int i);

#endif // KEY_ACTION_H

