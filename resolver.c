#include "time_util.h"
#include "resolver.h"

#include <stdbool.h>

/* Given a pressed key X, check unresolved events to deterimine which is first:
     - X has timed out (X is a modifier)
     - X has been released (X is not a modifier)
     - another key has been pressed and released (X is a modifier)
     - actually, neither has happened yet (resolvable time will be set) */
enum waveform {
    WAVEFORM_TIMEOUT,
    WAVEFORM_MAIN_KEY_RELEASED,
    WAVEFORM_ANOTHER_KEY,
    WAVEFORM_NONE_YET,
};
enum waveform check_waveform(const struct resolver *r, struct input_event ev){
    struct timeval now = timeval_now();

    // is the keypress old enough that we know it is a modifier?
    if(msec_diff(now, ev.time) > 200){
        return WAVEFORM_TIMEOUT;
    }

    int keys_pressed[256] = {0};
    for(size_t i = 1; i < r->ur_len; i++){
        struct input_event ev2 = r->unresolved[(r->ur_start + i) % URMAX];
        if(ev2.value == 0 && ev2.code == ev.code){
            // the main key was released first, it's just its normal self
            return WAVEFORM_MAIN_KEY_RELEASED;
        }else if(ev2.value == 1 && ev2.code < MAX_CODE){
            keys_pressed[ev2.code] = 1;
        }else if(ev2.value == 0 && ev2.code < MAX_CODE && keys_pressed[ev2.code]){
            // some other key was pressed and released, main key is a modifier
            return WAVEFORM_ANOTHER_KEY;
        }
    }

    return WAVEFORM_NONE_YET;
}

/* helper function which tries to resolve the oldest key event.  Returns false
   if it deems the event unresolvable, setting the resolver.resolve_time as
   appropriate. */
bool resolve(struct resolver *r, int *fmap_exp){
    if(r->ur_len == 0){
        return false;
    }

    // grab the oldest event
    struct input_event ev = r->unresolved[r->ur_start % URMAX];

    bool resolved = false;
    r->use_resolvable_time = false;

    if(ev.type == EV_KEY){
        // key released
        if(ev.value == 0){
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            if(ev.code < MAX_CODE){
                ev.code = r->release_map[ev.code];
                r->release_map[ev.code] = 0;
            }
            switch(ev.code){
                case KEYMAP_F:
                    // disable KEYMAP_F if it is the current one
                    if(r->current_keymap == KEYMAP_F)
                        r->current_keymap = KEYMAP_NONE;
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
            // F key changes the keymap
            if(ev.code == KEY_F){
                enum waveform waveform = check_waveform(r, ev);
                switch(waveform){
                    case WAVEFORM_MAIN_KEY_RELEASED:
                        resolved = true;
                        // a normal F
                        r->send(r->send_data, ev);
                        r->release_map[KEY_F] = KEY_F;
                        break;
                    case WAVEFORM_TIMEOUT:
                    case WAVEFORM_ANOTHER_KEY:
                        resolved = true;
                        // don't emit F; switch keymaps
                        r->current_keymap = KEYMAP_F;
                        // when "F" is released, disable the keymap
                        r->release_map[KEY_F] = KEYMAP_F;
                        break;
                    case WAVEFORM_NONE_YET:
                        r->resolvable_time = msec_after(ev.time, 200);
                        r->use_resolvable_time=true;
                        break;
                }
            // D key behaves like a meta key
            }else if(ev.code == KEY_D){
                enum waveform waveform = check_waveform(r, ev);
                switch(waveform){
                    case WAVEFORM_MAIN_KEY_RELEASED:
                        resolved = true;
                        // a normal D
                        r->send(r->send_data, ev);
                        r->release_map[KEY_D] = KEY_D;
                        break;
                    case WAVEFORM_TIMEOUT:
                    case WAVEFORM_ANOTHER_KEY:
                        ev.code = KEY_LEFTMETA;
                        // when "D" is released, release the meta key
                        r->release_map[KEY_D] = KEY_LEFTMETA;
                        r->send(r->send_data, ev);
                        resolved = true;
                        break;
                    case WAVEFORM_NONE_YET:
                        r->resolvable_time = msec_after(ev.time, 200);
                        r->use_resolvable_time=true;
                        break;
                }
            }else{
                int original_code = ev.code;
                // normal key was pressed
                switch(r->current_keymap){
                    case KEYMAP_NONE: break;
                    case KEYMAP_F: ev.code = fmap_exp[original_code]; break;
                }
                // remember how to release the key
                if(original_code < MAX_CODE){
                    r->release_map[original_code] = ev.code;
                }
                r->send(r->send_data, ev);
                resolved = true;
            }
        }
        // key repeated
        else if(ev.value == 2){
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            if(ev.code < MAX_CODE){
                ev.code = r->release_map[ev.code];
            }
            switch(ev.code){
                case KEYMAP_F:
                    // ignore repeats of keymap modifiers
                    break;
                default:
                    r->send(r->send_data, ev);
            }
            resolved = true;
        }
    }else{
        // non EV_KEY events are passed through unchanged
        if (ev.type == EV_SYN)
            r->send(r->send_data, ev);
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
        if(ev.value == 0){
            /* make the code look like whatever we mapped it to when we
               resolved the initial keypress */
            if(ev.code < MAX_CODE){
                ev.code = r->release_map[ev.code];
            }
            switch(ev.code){
                case KEYMAP_F:
                    // disable KEYMAP_F if it is the current one
                    if(r->current_keymap == KEYMAP_F)
                        r->current_keymap = KEYMAP_NONE;
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
