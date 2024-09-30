#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <string.h>

#include <lauxlib.h>

#include "config.h"
#include "names.h"

static int non_negative_idx(lua_State *L, int idx){
    return idx < 0 ? lua_gettop(L) + idx : idx;
}

int copy_to_key_action(lua_State *L, int idx, key_action_t *ka);

// An example lua allocator.
// from https://www.lua.org/manual/5.3/manual.html#lua_Alloc
static void *l_alloc (void *data, void *ptr, size_t osize, size_t nsize){
    (void)data;
    (void)osize;
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }else{
        return realloc(ptr, nsize);
    }
}

// the lua wrapper around key_action_free
int lua_key_action_gc(lua_State *L){
    key_action_t *ka = lua_touserdata(L, lua_gettop(L));
    key_action_free(ka);
    lua_pop(L, 1);
    return 0;
}

// fill maps of values based on other maps
void fill_map(key_action_t *tgt, key_action_t *base);

// fill individual values based on a map and a default value
void fill_action(key_action_t *tgt, key_action_t *base, key_action_t filler){
    switch(tgt->type){
        case KT_NONE:
            *tgt = filler;
            break;
        case KT_SIMPLE: break;
        case KT_MACRO: break;
        case KT_DUAL:
            fill_action(tgt->key.dual.tap, base, filler);
            fill_action(tgt->key.dual.hold, base, filler);
            break;
        case KT_MAP:
            fill_map(tgt, base);
            break;
    }
}

void fill_map(key_action_t *tgt, key_action_t *base){
    if(tgt->type != KT_MAP){
        fprintf(stderr, "fill_map: tgt is not a KT_MAP\n");
        return;
    }
    if(base && base->type != KT_MAP){
        fprintf(stderr, "fill_map: base is not a KT_MAP\n");
        return;
    }

    // fill all KT_NONE's with references (or KT_SIMPLE's for the root map)
    for(size_t i = 0; i < KEY_MAX; i++){
        key_action_t filler;
        if(base == NULL){
            filler.type = KT_SIMPLE;
            filler.key.simple = (int)i;
        }else{
            filler.type = KT_NONE;
            filler.key.ref = &base->key.map[i];
        }

        fill_action(&tgt->key.map[i], base, filler);
    }
}


// return 0/-1 on success/error
int extract_table_to_key_map(lua_State *L, int table_idx, key_action_t *map){
    // first key
    lua_pushnil(L);
    table_idx = non_negative_idx(L, table_idx);
    while(lua_next(L, table_idx) != 0){

        // key is at index -2, value is at index -1

        // get the key we will use in our internal hashmap
        int key;
        if(lua_isstring(L, -2)){
            const char *name = lua_tostring(L, -2);
            key = get_input_value(name);
            if(!key){
                fprintf(stderr, "no key with name %s\n", name);
                return -1;
            }
        }else if(lua_isinteger(L, -2)){
            lua_Integer n = lua_tointeger(L, -2);
            key = (int)n;
        }else{
            fprintf(stderr,
                    "invalid key type in keymap, must be string or number\n");
            return -1;
        }

        // duplicate the key action (this is a recursion)
        if(copy_to_key_action(L, -1, &map[key])){
            return -1;
        }

        // remove the value
        lua_pop(L, 1);
    }

    // lua_next() removes the key at the very end

    return 0;
}

// Copy a lua element, converting it into a key_action_t.  Return -1 on error.
int copy_to_key_action(lua_State *L, int idx, key_action_t *ka){
    *ka = (key_action_t){0};

    // nil objects are actually valid
    if(lua_isnil(L, idx)){
        ka->type = KT_NONE;
        return 0;
    }

    // simple key type?
    if(lua_isinteger(L, idx)){
        lua_Integer n = lua_tointeger(L, idx);

        // validate
        if(n < 0 || n >= KEY_MAX){
            fprintf(stderr, "Invalid key code: %d\n", (int)n);
            return -1;
        }

        ka->type = KT_SIMPLE;
        ka->key.simple = (int)n;

        return 0;
    }

    // key map?
    if(lua_istable(L, idx)){
        // alloc the map
        key_action_t *map = malloc(sizeof(*map) * KEY_MAX);
        if(!map) return -1;

        // zeroize
        for(size_t k = 0; k < KEY_MAX; k++){
            map[k] = (key_action_t){.type=KT_NONE};
        }

        // this may recurse.
        if(extract_table_to_key_map(L, idx, map)){
            // handle error: release everything we have allocated in map
            for(size_t k = 0; k < KEY_MAX; k++){
                key_action_free(&map[k]);
            }
            free(map);
            return -1;
        }

        ka->type = KT_MAP;
        ka->key.map = map;
        return 0;
    }

    // Is it already a key_action?
    if(lua_isuserdata(L, idx)){
        key_action_t *copyme = lua_touserdata(L, idx);
        return key_action_dup(copyme, ka);
    }

    // No other types are allowed
    fprintf(stderr, "unallowed type in copy_to_key_action()\n");
    return -1;
}

int append_simple_key_to_macro(int n, key_macro_t **tail){
    // validate
    if(n < 0 || n >= KEY_MAX){
        fprintf(stderr, "Invalid key code: %d\n", (int)n);
        return -1;
    }

    // append a key press
    *tail = key_macro_new(n, true);
    if(!*tail){
        return -1;
    }

    // append a key release
    (*tail)->next = key_macro_new(n, false);
    if(!(*tail)->next){
        return -1;
    }

    return 0;
}

// Copy a lua element, converting it to a key macro.  Return -1 on error.
int append_copy_to_macro(lua_State *L, int idx, key_macro_t **tail){

    // simple key type?
    if(lua_isinteger(L, idx)){
        lua_Integer n = lua_tointeger(L, idx);
        return append_simple_key_to_macro(n, tail);
    }

    // is it a key_action?
    if(lua_isuserdata(L, idx)){
        key_action_t *ka = lua_touserdata(L, idx);
        switch(ka->type){
            case KT_SIMPLE:
                return append_simple_key_to_macro(ka->key.simple, tail);
            case KT_MACRO:
                *tail = key_macro_dup(ka->key.macro);
                if(!*tail) return -1;
                return 0;
            default:
                break;
        }
    }

    // No other types are allowed
    fprintf(stderr, "unallowed type in append_copy_to_macro()\n");
    return -1;
}

int lua_key_action_tostring(lua_State *L){
    key_action_t *ka = lua_touserdata(L, lua_gettop(L));
    lua_pop(L, 1);
    switch(ka->type){
        case KT_NONE:
            lua_pushfstring(L, "NONE");
            break;
        case KT_SIMPLE:
            lua_pushfstring(L, "%s", get_input_name(ka->key.simple));
            break;
        case KT_MACRO:
            lua_pushliteral(L, "MACRO");
            break;
        case KT_DUAL:
            lua_pushliteral(L, "MAP");
            break;
        case KT_MAP:
            lua_pushliteral(L, "MAP");
            break;
        default:
            lua_pushliteral(L, "invalid key action");
            break;
    }
    return 1;
}

// push an allocated key action onto the stack. return -1 on error, 0 otherwise
int lua_new_key_action(lua_State *L){
    key_action_t *ka = lua_newuserdata(L, sizeof(*ka));
    if(!ka) return -1;
    *ka = (key_action_t){0};

    int ka_idx = lua_gettop(L);

    // push a table to the stack, and treat it as a metatable
    lua_newtable(L);
    int table_idx = lua_gettop(L);

    // set __tostring
    lua_pushliteral(L, "__tostring");
    lua_pushcfunction(L, lua_key_action_tostring);
    lua_rawset(L, table_idx);

    // set __gc
    lua_pushliteral(L, "__gc");
    lua_pushcfunction(L, lua_key_action_gc);
    lua_rawset(L, table_idx);

    // set the metatable for the key_action object
    lua_setmetatable(L, ka_idx);

    return 0;
}

// build a chain of key events to be sent as a single key action
int lua_macro(lua_State *L){
    // check the number of arguments
    int nargs = lua_gettop(L);
    if(nargs == 0){
        lua_pushliteral(L, "macro() requires at least one argument");
        goto fail;
    }

    // build a macro chain from all the arguments
    key_macro_t *macro = NULL;
    key_macro_t **tail = &macro;
    for(int i = 1; i <= nargs; i++){
        // append a copy of the argument (as a macro) to the tail
        int ret = append_copy_to_macro(L, i, tail);
        if(ret < 0){
            lua_pushliteral(L, "macro() failed to copy an argument");
            goto fail_macro;
        }

        // find the new tail
        while(*tail){
            tail = &(*tail)->next;
        }
    }

    // done with args
    lua_pop(L, nargs);

    // push a new key_action to the stack
    if(lua_new_key_action(L)){
        lua_pushliteral(L, "macro() failed to allocate memory");
        goto fail_macro;
    }
    int ka_idx = lua_gettop(L);
    key_action_t *ka = lua_touserdata(L, ka_idx);

    ka->type = KT_MACRO;
    ka->key.macro = macro;

    return 1;

fail_macro:
    key_macro_free(macro);
fail:
    return lua_error(L);
}

// build a macro chain, wrapping it in SHIFT_PRESS ... SHIFT_RELEASE
// build a macro chain, wrapping it in MOD_PRESS ... MOD_RELEASE
#define DEF_LUA_MOD(name, num) \
int lua_##name(lua_State *L){ \
    /* check the number of arguments */ \
    int nargs = lua_gettop(L); \
    if(nargs == 0){ \
        lua_pushliteral(L, #name "() requires at least one argument"); \
        goto fail; \
    } \
\
    /* build a macro chain from all the arguments, starting with mod press */ \
    key_macro_t *macro = key_macro_new(num, true); \
    if(!macro){ \
        lua_pushliteral(L, #name "() failed to allocate memory"); \
        goto fail; \
    } \
\
    key_macro_t **tail = &macro->next; \
    for(int i = 1; i <= nargs; i++){ \
        /* append a copy of the argument (as a macro) to the tail */ \
        int ret = append_copy_to_macro(L, i, tail); \
        if(ret < 0){ \
            lua_pushliteral(L, #name "() failed to copy an argument"); \
            goto fail_macro; \
        } \
\
        /* find the new tail */ \
        while(*tail){ \
            tail = &(*tail)->next; \
        } \
    } \
\
    /* end by releasing the mod key */ \
    *tail = key_macro_new(num, false); \
    if(!*tail){ \
        lua_pushliteral(L, #name "() failed to allocate memory"); \
        goto fail; \
    } \
\
    /* done with args */ \
    lua_pop(L, nargs); \
\
    /* push a new key_action to the stack */ \
    if(lua_new_key_action(L)){ \
        lua_pushliteral(L, #name "() failed to allocate memory"); \
        goto fail_macro; \
    } \
    int ka_idx = lua_gettop(L); \
    key_action_t *ka = lua_touserdata(L, ka_idx); \
\
    ka->type = KT_MACRO; \
    ka->key.macro = macro; \
\
    return 1; \
\
fail_macro: \
    key_macro_free(macro); \
fail: \
    return lua_error(L); \
}
DEF_LUA_MOD(shift, 42)
DEF_LUA_MOD(ctrl, 29)
DEF_LUA_MOD(alt, 56)
DEF_LUA_MOD(meta, 125)

#define DUAL_KEY_CONFIG_BAD_TYPE -1
#define DUAL_KEY_CONFIG_BAD_TYPE_MSG \
    "dual_key() argument 3 must a config table. Allowed keys are\n" \
    " MODE, HOLD_MS, and DOUBLE_TAP_MS."

#define DUAL_KEY_INVALID_MODE -2
#define DUAL_KEY_INVALID_MODE_MSG \
    "CONFIG.MODE in dual_key() must have a value which is one of\n" \
    "TAP_ON_ROLLOVER, HOLD_ON_ROLLOVER, or TIMEOUT_ONLY."

#define DUAL_KEY_INVALID_HOLD_MS -3
#define DUAL_KEY_INVALID_HOLD_MS_MSG \
    "CONFIG.HOLD_MS in dual_key() must be a positive integer."

#define DUAL_KEY_INVALID_DOUBLE_TAP_MS -4
#define DUAL_KEY_INVALID_DOUBLE_TAP_MS_MSG \
    "CONFIG.DOUBLE_TAP_MS in dual_key() must be a positive integer or 0 or -1"

// return a negative error code on failure, 0 on success
int read_dual_key_config(
    lua_State *L,
    int config_idx,
    dual_key_mode_t *mode_out,
    long *hold_ms_out,
    long *double_tap_ms_out
){
    int error_code = DUAL_KEY_CONFIG_BAD_TYPE;

    // deprecated support for MODE as the third arg
    if(lua_isinteger(L, 3)){
        lua_Integer mode = lua_tointeger(L, 3);
        switch(mode){
            case DUAL_MODE_TAP_ON_ROLLOVER:
            case DUAL_MODE_HOLD_ON_ROLLOVER:
            case DUAL_MODE_TIMEOUT_ONLY:
                *mode_out = mode;
                return 0;
        }
        // invalid number as deprecated MODE argument
        return error_code;
    }

    // CONFIG arg must be a table
    if(!lua_istable(L, 3)) return error_code;

    // iterate through keys in the table
    lua_pushnil(L);
    config_idx = non_negative_idx(L, config_idx);
    while(lua_next(L, config_idx) != 0){

        // key is at index -2, value is at index -1

        if(!lua_isstring(L, -2)){
            // all keys must be strings
            error_code = DUAL_KEY_CONFIG_BAD_TYPE;
            goto fail_iter;
        }

        size_t keylen;
        const char *key = lua_tolstring(L, -2, &keylen);
        if(strncmp("MODE", key, keylen) == 0){
            if(!lua_isinteger(L, -1)){
                error_code = DUAL_KEY_INVALID_MODE;
                goto fail_iter;
            }
            lua_Integer n = lua_tointeger(L, -1);
            switch(n){
                case DUAL_MODE_TAP_ON_ROLLOVER:
                case DUAL_MODE_HOLD_ON_ROLLOVER:
                case DUAL_MODE_TIMEOUT_ONLY:
                    *mode_out = n;
                    break;
                default:
                    error_code = DUAL_KEY_INVALID_MODE;
                    goto fail_iter;
            }
        }else if(strncmp("HOLD_MS", key, keylen) == 0){
            if(!lua_isinteger(L, -1)){
                error_code = DUAL_KEY_INVALID_HOLD_MS;
                goto fail_iter;
            }
            lua_Integer n = lua_tointeger(L, -1);
            // hold_ms must be positive
            if(n < 1){
                error_code = DUAL_KEY_INVALID_HOLD_MS;
                goto fail_iter;
            }
            *hold_ms_out = n;
        }else if(strncmp("DOUBLE_TAP_MS", key, keylen) == 0){
            if(!lua_isinteger(L, -1)){
                error_code = DUAL_KEY_INVALID_DOUBLE_TAP_MS;
                goto fail_iter;
            }
            lua_Integer n = lua_tointeger(L, -1);
            // hold_ms must be positive, 0, or -1
            if(n < -1){
                error_code = DUAL_KEY_INVALID_DOUBLE_TAP_MS;
                goto fail_iter;
            }
            *double_tap_ms_out = n;
        }else{
            // unrecognized key
            goto fail_iter;
        }

        // remove the value
        lua_pop(L, 1);
        continue;

    fail_iter:
        lua_pop(L, 2);
        return error_code;
    }
    // lua_next() removes the key at the very end

    return 0;
}

int lua_dual_key(lua_State *L){
    // check the number of arguments
    int nargs = lua_gettop(L);
    if(nargs != 2 && nargs != 3){
        lua_pushliteral(L, "dual_key() requires two or three arguments");
        goto fail;
    }

    // copy the args
    key_action_t tap;
    if(copy_to_key_action(L, 1, &tap)){
        lua_pushliteral(L, "dual_key() failed to copy argument 1");
        goto fail;
    }
    key_action_t hold;
    if(copy_to_key_action(L, 2, &hold)){
        lua_pushliteral(L, "dual_key() failed to copy argument 2");
        goto fail_tap;
    }

    // validate args
    if(tap.type == KT_DUAL || tap.type == KT_MAP){
        lua_pushliteral(L,
            "dual_key() argument 1 cannot be another dual_key or a keymap"
        );
        goto fail_hold;
    }
    if(hold.type == KT_DUAL){
        lua_pushliteral(L, "dual_key() argument 2 cannot be another dual_key");
        goto fail_hold;
    }

    // defaults
    dual_key_mode_t mode = DUAL_MODE_TAP_ON_ROLLOVER;
    long hold_ms = 200;
    long double_tap_ms = 300;

    if(nargs > 2){
        int e = read_dual_key_config(L, 3, &mode, &hold_ms, &double_tap_ms);
        switch(e){
            case 0: break;

            case DUAL_KEY_INVALID_MODE:
                lua_pushliteral(L, DUAL_KEY_INVALID_MODE_MSG);
                goto fail_hold;

            case DUAL_KEY_INVALID_HOLD_MS:
                lua_pushliteral(L, DUAL_KEY_INVALID_HOLD_MS_MSG);
                goto fail_hold;

            case DUAL_KEY_INVALID_DOUBLE_TAP_MS:
                lua_pushliteral(L, DUAL_KEY_INVALID_DOUBLE_TAP_MS_MSG);
                goto fail_hold;

            case DUAL_KEY_CONFIG_BAD_TYPE:
            default:
                lua_pushliteral(L, DUAL_KEY_CONFIG_BAD_TYPE_MSG);
                goto fail_hold;

        }
    }

    // done with args
    lua_pop(L, nargs);

    // push a new key_action to the stack
    if(lua_new_key_action(L)){
        lua_pushliteral(L, "dual_key() failed to allocate memory");
        goto fail_hold;
    }

    int ka_idx = lua_gettop(L);
    key_action_t *ka = lua_touserdata(L, ka_idx);
    ka->type = KT_DUAL;

    // allocate for tap
    ka->key.dual.tap = malloc(sizeof(*ka->key.dual.tap));
    if(!ka->key.dual.tap){
        lua_pushliteral(L, "dual_key() failed to allocate memory");
        goto fail_hold;
    }
    *ka->key.dual.tap = (key_action_t){0};

    // allocate for hold
    ka->key.dual.hold = malloc(sizeof(*ka->key.dual.hold));
    if(!ka->key.dual.hold){
        lua_pushliteral(L, "dual_key() failed to allocate memory");
        goto fail_hold;
    }
    *ka->key.dual.hold = (key_action_t){0};

    // copy args into place
    *ka->key.dual.tap = tap;
    *ka->key.dual.hold = hold;
    ka->key.dual.mode = mode;
    ka->key.dual.hold_ms = hold_ms;
    ka->key.dual.double_tap_ms = double_tap_ms;

    return 1;

fail_hold:
    key_action_free(&hold);
fail_tap:
    key_action_free(&tap);
fail:
    return lua_error(L);
}

int lua_print(lua_State *L){
    int nargs = lua_gettop(L);
    for(int i = 0; i < nargs; i++){
        // get the string
        size_t len;
        const char *str = luaL_tolstring(L, i+1, &len);
        printf("%.*s\t", (int)len, str);
        lua_pop(L, 1);
    }
    printf("\n");

    // pop all the args too
    lua_pop(L, nargs);

    // return nothing on the stack
    return 0;
}

// function grab_keyboard(pattern: string, keymap: table):
int lua_grab_keyboard(lua_State *L){
    // check the number of arguments
    if(lua_gettop(L) != 2){
        lua_pushliteral(L, "grab_keyboard() requires two arguments");
        goto fail;
    }

    if(!lua_isstring(L, 1)){
        lua_pushliteral(L, "argument 1 of grab_keyboard must be a string");
        goto fail;
    }

    // allocate a new grab object
    grab_t *grab = malloc(sizeof(*grab));
    if(!grab){
        lua_pushliteral(L, "malloc failed");
        goto fail;
    }
    *grab = (grab_t){.ignore=false};

    // copy the second argument
    if(copy_to_key_action(L, 2, &grab->map)){
        lua_pushliteral(L, "grab_keyboard() failed to copy arg 2");
        goto fail_grab;
    }

    // confirm type is valid
    if(grab->map.type != KT_MAP){
        lua_pushliteral(L, "argument 2 of grab_keyboard must be a table");
        goto fail_map;
    }

    // fill in the KT_NONE values (which should have fall-through behavior)
    fill_map(&grab->map, NULL);

    // compile the regex pattern
    size_t len;
    const char *pattern = lua_tolstring(L, 1, &len);
    int ret = regcomp(&grab->regex, pattern, REG_EXTENDED | REG_ICASE);
    if(ret){
        char err[1024];
        size_t errlen = regerror(ret, &grab->regex, err, sizeof(err));
        lua_pushfstring(L, "failed to compile regex: %.*s", (int)errlen, err);
        goto fail_map;
    }

    // get the config from the lua_State
    lua_getglobal(L, "__config");
    config_t *config = lua_touserdata(L, lua_gettop(L));

    // append the new grab to the config state
    grab_t **last = &config->grabs;
    while(*last)
        last = &(*last)->next;
    *last = grab;

    // pop the args and the __config
    lua_pop(L, 3);
    return 0;

fail_map:
    key_action_free(&grab->map);
fail_grab:
    free(grab);
fail:
    return lua_error(L);
}

// function ignore_keyboard(pattern: string):
int lua_ignore_keyboard(lua_State *L){
    // check the number of arguments
    if(lua_gettop(L) != 1){
        lua_pushliteral(L, "ignore_keyboard() requires one argument");
        goto fail;
    }

    if(!lua_isstring(L, 1)){
        lua_pushliteral(L, "argument 1 of ignore_keyboard must be a string");
        goto fail;
    }

    // allocate a new grab object
    grab_t *grab = malloc(sizeof(*grab));
    if(!grab){
        lua_pushliteral(L, "malloc failed");
        goto fail;
    }
    *grab = (grab_t){.ignore=true};

    // compile the regex pattern
    size_t len;
    const char *pattern = lua_tolstring(L, 1, &len);
    int ret = regcomp(&grab->regex, pattern, REG_EXTENDED | REG_ICASE);
    if(ret){
        char err[1024];
        size_t errlen = regerror(ret, &grab->regex, err, sizeof(err));
        lua_pushfstring(L, "failed to compile regex: %.*s", (int)errlen, err);
        goto fail_grab;
    }

    // get the config from the lua_State
    lua_getglobal(L, "__config");
    config_t *config = lua_touserdata(L, lua_gettop(L));

    // append the new grab to the config state
    grab_t **last = &config->grabs;
    while(*last) last = &(*last)->next;
    *last = grab;

    // pop the arg and the __config
    lua_pop(L, 2);
    return 0;

fail_grab:
    free(grab);
fail:
    return lua_error(L);
}

void grab_free(grab_t *grab){
    if(!grab) return;

    regfree(&grab->regex);
    key_action_free(&grab->map);
    grab_free(grab->next);
    free(grab);
}

void add_global(void *arg, const char *name, uint16_t val){
    lua_State *L = arg;
    // map a key_name to its numeric value as a lua global variable
    lua_pushinteger(L, val);
    lua_setglobal(L, name);
}

int set_lua_globals(config_t *config){
    lua_State *L = config->L;

    // add a lua reference to our config object
    lua_pushlightuserdata(L, config);
    lua_setglobal(L, "__config");

    // add c functions
    lua_pushcfunction(L, lua_print);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, lua_macro);
    lua_setglobal(L, "macro");

    lua_pushcfunction(L, lua_shift);
    lua_setglobal(L, "shift");

    lua_pushcfunction(L, lua_ctrl);
    lua_setglobal(L, "ctrl");

    lua_pushcfunction(L, lua_alt);
    lua_setglobal(L, "alt");

    lua_pushcfunction(L, lua_meta);
    lua_setglobal(L, "meta");

    lua_pushcfunction(L, lua_dual_key);
    lua_setglobal(L, "dual_key");

    // make dual_key modes available
    lua_pushinteger(L, DUAL_MODE_TAP_ON_ROLLOVER);
    lua_setglobal(L, "TAP_ON_ROLLOVER");
    lua_pushinteger(L, DUAL_MODE_HOLD_ON_ROLLOVER);
    lua_setglobal(L, "HOLD_ON_ROLLOVER");
    lua_pushinteger(L, DUAL_MODE_TIMEOUT_ONLY);
    lua_setglobal(L, "TIMEOUT_ONLY");

    lua_pushcfunction(L, lua_grab_keyboard);
    lua_setglobal(L, "grab_keyboard");

    lua_pushcfunction(L, lua_ignore_keyboard);
    lua_setglobal(L, "ignore_keyboard");

    // add the standard lua libraries (nah... too dangerous)
    // luaL_openlibs(L);

    // make a variable for every key_name with its numeric value
    for_each_name(add_global, L);

    return 0;
}

config_t *config_new(const char* config_file){
    config_t *config = malloc(sizeof(*config));
    if(!config) return NULL;
    *config = (config_t){0};

    lua_State *L = lua_newstate(l_alloc, NULL);
    if(!L) goto fail;
    config->L = L;

    if(set_lua_globals(config)){
        goto fail;
    }

    // load the config file
    int ret = luaL_loadfile(L, config_file);
    if(ret != 0){
        size_t len;
        const char *err = lua_tolstring(L, lua_gettop(L), &len);
        fprintf(stderr, "%.*s\n", (int)len, err);
        goto fail;
    }

    // execute the config file
    ret = lua_pcall(L, 0, 1, 0);
    if(ret != 0){
        size_t len;
        const char *err = lua_tolstring(L, lua_gettop(L), &len);
        fprintf(stderr, "%.*s\n", (int)len, err);
        goto fail;
    }

    return config;

fail:
    lua_close(config->L);
    free(config);
    return NULL;
}


void config_free(config_t *config){
    if(!config) return;

    lua_close(config->L);
    grab_free(config->grabs);
    free(config);
}
