#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>

#include <lauxlib.h>

#include "config.h"
#include "names.h"

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

void key_action_free(key_action_t *ka){
    if(!ka) return;

    switch(ka->type){
        case KT_NONE:   break;
        case KT_SIMPLE: break;
        case KT_DUAL:
            key_action_free(ka->key.dual.tap);
            free(ka->key.dual.tap);
            key_action_free(ka->key.dual.hold);
            free(ka->key.dual.hold);
            break;
        case KT_MAP:
            for(size_t i = 0; i < 256; i++){
                key_action_free(&ka->key.map[i]);
            }
            free(ka->key.map);
            break;
    }
    *ka = (key_action_t){0};
}

// the lua wrapper around key_action_free
int lua_key_action_gc(lua_State *L){
    key_action_t *ka = lua_touserdata(L, lua_gettop(L));
    key_action_free(ka);
    lua_pop(L, 1);
    return 0;
}

int key_action_dup(const key_action_t *in, key_action_t *out){
    switch(in->type){
        case KT_NONE:   *out = *in; break;
        case KT_SIMPLE: *out = *in; break;
        case KT_DUAL:
            // match type
            out->type = in->type;
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
            out->key.map = malloc(sizeof(*out->key.map) * 256);
            if(!out->key.map) goto fail;
            // duplicate map elements
            for(size_t i = 0; i < 256; i++){
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

// fill maps of values based on other maps
void fill_map(key_action_t *tgt, key_action_t *base);

// fill individual values based on a map and a default value
void fill_action(key_action_t *tgt, key_action_t *base, key_action_t filler){
    switch(tgt->type){
        case KT_SIMPLE: break;
        case KT_NONE:
            *tgt = filler;
            break;
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
    for(size_t i = 0; i < 256; i++){
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

// Copy a lua element, converting it into a key_action_t.  Return -1 on error.
int copy_to_key_action(lua_State *L, int idx, key_action_t *ka){
    *ka = (key_action_t){0};

    // nil objects are actually valid
    if(lua_isnil(L, idx)){
        ka->type = KT_NONE;
        return 0;
    }

    // simple key type?
    if(lua_isnumber(L, idx)){
        lua_Number n = lua_tonumber(L, idx);

        // validate
        if(n < 0 || n >= 256){
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
        key_action_t *map = malloc(sizeof(*map) * 256);
        if(!map) return -1;

        // extract all the values from the table
        for(size_t k = 0; k < 256; k++){
            if(k < sizeof(key_names)/sizeof(*key_names)){
                // look up this key name in the table
                lua_pushstring(L, key_names[k]);
                lua_gettable(L, idx);
            }else{
                // codes without names are passed unchanged
                lua_pushnumber(L, (lua_Number)k);
            }
            // recurse
            int ret = copy_to_key_action(L, lua_gettop(L), &map[k]);
            // remove the table entry from the stack
            lua_pop(L, 1);
            // handle error: release everything we have allocated in map
            if(ret){
                for(size_t kk = 0; kk < k; kk++){
                    key_action_free(&map[kk]);
                }
                free(map);
                return -1;
            }
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

int lua_key_action_tostring(lua_State *L){
    key_action_t *ka = lua_touserdata(L, lua_gettop(L));
    lua_pop(L, 1);
    switch(ka->type){
        case KT_NONE:
            lua_pushfstring(L, "NONE");
            break;
        case KT_SIMPLE:
            lua_pushfstring(L, "%d", ka->key.simple);
            break;
        case KT_DUAL:
            lua_pushfstring(L, "DUAL");
            break;
        case KT_MAP:
            lua_pushfstring(L, "MAP");
            break;
        default:
            lua_pushfstring(L, "invalid key action");
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

int lua_dual_key(lua_State *L){
    // check the number of arguments
    if(lua_gettop(L) != 2){
        lua_pushliteral(L, "dual_key() requires two arguments");
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
    if(tap.type == KT_DUAL){
        lua_pushliteral(L, "dual_key() argument 1 cannot be another dual_key");
        goto fail_hold;
    }
    if(hold.type == KT_DUAL){
        lua_pushliteral(L, "dual_key() argument 2 cannot be another dual_key");
        goto fail_hold;
    }

    // done with args
    lua_pop(L, 2);

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

    return 1;

fail_hold:
    key_action_free(&hold);
fail_tap:
    key_action_free(&tap);
fail:
    return lua_error(L);
}

int lua_print(lua_State *L){
    while(lua_gettop(L) > 0){
        // get the string
        size_t len;
        const char *str = luaL_tolstring(L, lua_gettop(L), &len);
        printf("%.*s\t", (int)len, str);
        // have to pop twice after luaL_tolstring
        lua_pop(L, 2);
    }
    printf("\n");

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

int set_lua_globals(config_t *config){
    lua_State *L = config->L;

    // add a lua reference to our config object
    lua_pushlightuserdata(L, config);
    lua_setglobal(L, "__config");

    // add c functions
    lua_pushcfunction(L, lua_print);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, lua_dual_key);
    lua_setglobal(L, "dual_key");

    lua_pushcfunction(L, lua_grab_keyboard);
    lua_setglobal(L, "grab_keyboard");

    lua_pushcfunction(L, lua_ignore_keyboard);
    lua_setglobal(L, "ignore_keyboard");

    // add the standard lua libraries (nah... too dangerous)
    // luaL_openlibs(L);

    // make a variable for every key_name with its numeric value
    for(size_t i = 0; i < sizeof(key_names) / sizeof(*key_names); i++){
        // map a key_name to its numeric value as a lua global variable
        lua_pushnumber(L, (lua_Number)i);
        lua_setglobal(L, key_names[i]);
    }

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

key_action_t *key_action_get(key_action_t *ka, int i){
    for(size_t tries = 0; tries < 32; tries++){
        switch(ka->type){
            case KT_SIMPLE:
            case KT_DUAL:
                return ka;
            case KT_MAP:
                return &ka->key.map[i];
            case KT_NONE:
                // try again with another deref
                ka = ka->key.ref;
            default:
                fprintf(stderr, "invalid key action in key_action_get()\n");
                exit(1);
        }
    }

    fprintf(stderr, "detected probable infinite recursion in keymap\n");
    exit(1);
    return NULL;
}
