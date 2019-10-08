#include "time_util.h"
#include "resolver.h"
// #include "names.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void resolver_init(struct resolver *r, key_action_t *root_keymap,
        send_t send, void *send_data){
    *r = (struct resolver){0};
    r->send = send;
    r->send_data = send_data;
    r->root_keymap = root_keymap;
    r->current_keymap = root_keymap;
}

/* Given a pressed dual-key X, check unresolved events to decide tap or hold.
   The first condition met from the list below indicates the correct mode:
     - X has timed out (hold mode)
     - X has been released (tap mode)
     - another key has been pressed and released (hold mode)
     - actually, neither has happened yet (resolvable time will be set) */
enum waveform {
    WAVEFORM_TAP,
    WAVEFORM_HOLD,
    WAVEFORM_NONE_YET,
};
enum waveform check_waveform(const struct resolver *r, struct input_event ev,
        dual_key_mode_t mode){
    struct timeval now = timeval_now();

    // is the keypress old enough that we know it is a modifier?
    if(msec_diff(now, ev.time) > 200){
        return WAVEFORM_HOLD;
    }
    // in TIMEOUT_ONLY, we don't have to check any further
    else if(mode == DUAL_MODE_TIMEOUT_ONLY){
        return WAVEFORM_NONE_YET;
    }

    int keys_pressed[256] = {0};
    for(size_t i = 1; i < r->ur_len; i++){
        struct input_event ev2 = r->unresolved[(r->ur_start + i) % URMAX];
        // was the main key released?
        if(ev2.value == 0 && ev2.code == ev.code){
            return WAVEFORM_TAP;
        }
        // in HOLD_ON_ROLLOVER mode, any key event but main-key-release is HOLD
        else if(mode == DUAL_MODE_HOLD_ON_ROLLOVER && ev2.type == EV_KEY){
            return WAVEFORM_HOLD;
        }
        // record the pressed state of the key
        else if(ev2.value == 1 && ev2.code < MAX_CODE){
            keys_pressed[ev2.code] = 1;
        }
        // some other key was pressed and released, main key is a HOLD
        else if(ev2.value == 0 && ev2.code < MAX_CODE && keys_pressed[ev2.code]){
            return WAVEFORM_HOLD;
        }
    }

    return WAVEFORM_NONE_YET;
}

void do_keypress(struct resolver *r, struct input_event ev, key_action_t *ka){
    switch(ka->type){
        case KT_DUAL:
            fprintf(stderr, "can't call do_keypress() on a dual-mode key\n");
            exit(1);
            break;
        case KT_NONE:
            fprintf(stderr, "can't call do_keypress() on a none-type key\n");
            exit(1);
            break;
        case KT_SIMPLE:
            // remember how to release the key
            r->release_map[ev.code] = ka->key.simple;
            // send the modified key
            ev.code = ka->key.simple;
            r->send(r->send_data, ev);
            break;
        case KT_MAP:
            // set the keymap
            r->current_keymap = ka;
            // reset the keymap on release
            r->release_map[ev.code] = RESET_KEYMAP;
            // send nothing
            break;
    }
}

/* helper function which tries to resolve the oldest key event.  Returns false
   if it deems the event unresolvable, setting the resolver.resolve_time as
   appropriate. */
bool resolve(struct resolver *r){
    if(r->ur_len == 0){
        return false;
    }

    // grab the oldest event
    struct input_event ev = r->unresolved[r->ur_start % URMAX];

    bool resolved = false;
    r->use_resolvable_time = false;

    if(ev.type == EV_KEY){
        // invalid key code
        if(ev.code > 255){
            fprintf(stderr, "Dropping too-high keycode %d\n", ev.code);
            resolved = true;
        }
        // key released
        else if(ev.value == 0){
            // printf("%.10s of %.10s\n", "release", key_names[ev.code]);
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            ev.code = r->release_map[ev.code];
            r->release_map[ev.code] = 0;
            switch(ev.code){
                case RESET_KEYMAP:
                    r->current_keymap = r->root_keymap;
                    break;
                case 0:
                    // we must have sent this key release early; do nothing.
                    break;
                default:
                    r->send(r->send_data, ev);
            }
            resolved = true;
        }
        // key pressed
        else if(ev.value == 1){
            // printf("%.10s of %.10s\n", "press", key_names[ev.code]);
            // get the key action from the map
            key_action_t *ka = key_action_get(r->current_keymap, ev.code);
            switch(ka->type){
                case KT_MAP:
                case KT_SIMPLE:
                    do_keypress(r, ev, ka);
                    resolved = true;
                    break;
                case KT_DUAL:
                    switch(check_waveform(r, ev, ka->key.dual.mode)){
                        // .tap and .hold must not be KT_DUALs
                        case WAVEFORM_TAP:
                            do_keypress(r, ev, ka->key.dual.tap);
                            resolved = true;
                            break;
                        case WAVEFORM_HOLD:
                            do_keypress(r, ev, ka->key.dual.hold);
                            resolved = true;
                            break;
                        case WAVEFORM_NONE_YET:
                            // wait for a timeout to resolve this event
                            r->resolvable_time = msec_after(ev.time, 200);
                            r->use_resolvable_time=true;
                            break;
                    }
                    break;
                case KT_NONE:
                default:
                    fprintf(stderr, "invalid key action in resolver\n");
                    exit(1);
            }
        }
        // key repeated
        else if(ev.value == 2){
            // printf("%.10s of %.10s\n", "repeat", key_names[ev.code]);
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            ev.code = r->release_map[ev.code];
            if(ev.code != 0 && ev.code != RESET_KEYMAP){
                r->send(r->send_data, ev);
            }
            resolved = true;
        }
    }else{
        // non EV_KEY events are passed through unchanged
        if (ev.type == EV_SYN){
            // printf("EV_SYN\n");
            r->send(r->send_data, ev);
        }
        resolved = true;
    }

    if(resolved){
        // one less element
        r->ur_len--;
        // but we start one later
        r->ur_start = (r->ur_start + 1) % URMAX;
    }else{
        /* if we decided we couldn't resolve the oldest event, but the newest
           event is a key release, emit the key release immediately.  We know
           that the initial press must have been resolved because if the
           initial press had come after the unresolvable key, then now that
           we have the release the unresolvable key would be resolvable. */
        ev = r->unresolved[(r->ur_start + r->ur_len - 1) % URMAX];
        if(ev.value == 0 && ev.code < 256){
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            ev.code = r->release_map[ev.code];
            switch(ev.code){
                case RESET_KEYMAP:
                    // return to root keymap
                    r->current_keymap = r->root_keymap;
                    // one less element
                    r->ur_len--;
                    break;
                case KEY_LEFTALT:
                case KEY_RIGHTALT:
                case KEY_LEFTCTRL:
                case KEY_RIGHTCTRL:
                case KEY_LEFTMETA:
                case KEY_RIGHTMETA:
                case KEY_LEFTSHIFT:
                case KEY_RIGHTSHIFT:
                    // modifier keys don't get resolved early
                    break;
                default:
                    r->send(r->send_data, ev);
                    // mark the end of an input packet of keypresses
                    struct input_event syn_ev = {
                        .type = EV_SYN,
                        .code = SYN_REPORT,
                        .value = 0,
                    };
                    r->send(r->send_data, syn_ev);
                    // one less element
                    r->ur_len--;
            }
        }
    }
    return resolved;
}

// returns the timeout to be used for select(), which is either out or NULL
struct timeval *select_timeout(struct resolver *r, struct timeval *out){
    // don't pass a timeout if none is valid.
    if(!r->use_resolvable_time){
        return NULL;
    }

    struct timespec now_timespec;
    clock_gettime(CLOCK_REALTIME, &now_timespec);
    struct timeval now;
    TIMESPEC_TO_TIMEVAL(&now, &now_timespec);

    long mdiff = msec_diff(r->resolvable_time, now);
    if(mdiff < 0){
        // somehow the timeout has expired, zero length timeout
        out->tv_sec = 0;
        out->tv_usec = 0;
    }else{
        out->tv_sec = mdiff / 1000;
        out->tv_usec = (mdiff % 1000) * 1000;
    }
    return out;
}
