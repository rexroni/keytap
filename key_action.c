#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "key_action.h"
#include "names.h"

key_macro_t *key_macro_new(int code, bool press){
    key_macro_t *macro = malloc(sizeof(*macro));
    if(!macro) return NULL;
    *macro = (key_macro_t){0};
    macro->code = code;
    macro->press = press;
    return macro;
}

void key_macro_free(key_macro_t *macro){
    key_macro_t *next;
    for(; macro != NULL; macro = next){
        next = macro->next;
        free(macro);
    }
}

// This is not remotely efficient but it is definitely valid with lua GC'ing
key_macro_t *key_macro_dup(key_macro_t *macro){
    key_macro_t *copy = key_macro_new(macro->code, macro->press);
    if(!copy) return NULL;

    key_macro_t *old = macro;
    key_macro_t *new = copy;
    while(old->next){
        new->next = key_macro_new(old->next->code, old->next->press);
        if(!new->next){
            key_macro_free(copy);
            return NULL;
        }
        old = old->next;
        new = new->next;
    }

    return copy;
}

void key_action_free(key_action_t *ka){
    if(!ka) return;

    switch(ka->type){
        case KT_NONE:   break;
        case KT_SIMPLE: break;
        case KT_MACRO:
            key_macro_free(ka->key.macro);
            break;
        case KT_DUAL:
            key_action_free(ka->key.dual.tap);
            free(ka->key.dual.tap);
            key_action_free(ka->key.dual.hold);
            free(ka->key.dual.hold);
            break;
        case KT_MAP:
            for(size_t i = 0; i < KEY_MAX; i++){
                key_action_free(&ka->key.map[i]);
            }
            free(ka->key.map);
            break;
    }
    *ka = (key_action_t){0};
}

int key_action_dup(const key_action_t *in, key_action_t *out){
    switch(in->type){
        case KT_NONE:   *out = *in; break;
        case KT_SIMPLE: *out = *in; break;
        case KT_MACRO:
            out->key.macro = key_macro_dup(in->key.macro);
            if(!out->key.macro) goto fail;
            out->type = KT_MACRO;
            break;
        case KT_DUAL:
            // match type and mode
            out->type = in->type;
            out->key.dual.mode = in->key.dual.mode;
            out->key.dual.hold_ms = in->key.dual.hold_ms;
            out->key.dual.double_tap_ms = in->key.dual.double_tap_ms;
            // match tap
            out->key.dual.tap = malloc(sizeof(*out->key.dual.tap));
            if(!out->key.dual.tap) goto fail;
            if(key_action_dup(in->key.dual.tap, out->key.dual.tap)){
                free(out->key.dual.tap);
                goto fail;
            }
            // match hold
            out->key.dual.hold = malloc(sizeof(*out->key.dual.hold));
            if(!out->key.dual.hold){
                free(out->key.dual.tap);
                goto fail;
            }
            if(key_action_dup(in->key.dual.hold, out->key.dual.hold)){
                free(out->key.dual.tap);
                free(out->key.dual.hold);
                goto fail;
            }
            break;
        case KT_MAP:
            // match type
            out->type = in->type;
            // alloc map
            out->key.map = malloc(sizeof(*out->key.map) * KEY_MAX);
            if(!out->key.map) goto fail;
            // duplicate map elements
            for(size_t i = 0; i < KEY_MAX; i++){
                if(key_action_dup(&in->key.map[i], &out->key.map[i])){
                    for(size_t ii = 0; ii < i; i++){
                        key_action_free(&out->key.map[ii]);
                    }
                    free(out->key.map);
                    goto fail;
                }
            }
            break;
    }
    return 0;

fail:
    *out = (key_action_t){0};
    return -1;
}

key_action_t *key_action_get(key_action_t *ka, int i){
    for(size_t tries = 0; tries < 32; tries++){
        switch(ka->type){
            case KT_SIMPLE:
            case KT_MACRO:
            case KT_DUAL:
                return ka;
            case KT_MAP:
                return &ka->key.map[i];
            case KT_NONE:
                // try again with another deref
                ka = ka->key.ref;
                break;
            default:
                fprintf(stderr, "invalid key action in key_action_get()\n");
                exit(1);
        }
    }

    fprintf(stderr, "detected probable infinite recursion in keymap\n");
    exit(1);
    return NULL;
}
