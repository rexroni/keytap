#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

// user must be null-terminated, but it can be a uid or a username
static int get_uid_from_string(char *user, uid_t *out){
    // check if user is a UID already
    char *endptr = user;
    long int luid = strtol(user, &endptr, 0);

    // was it a valid number?
    if(user[0] != '\0' && endptr[0] == '\0'){
        // user was a UID
        if(luid > UINT_MAX){
            fprintf(stderr, "UID %ld too big\n", luid);
            return -1;
        }
        if(luid < 0){
            fprintf(stderr, "UID %ld cannot be negative\n", luid);
            return -1;
        }
        *out = (uid_t)luid;
        return 0;
    }

    // otherwise, assume user is a name string
    struct passwd pwd;
    struct passwd *pwd_result;
    char namebuf[1024];
    errno = 0;
    int ret = getpwnam_r(user, &pwd, namebuf, sizeof(namebuf), &pwd_result);
    // check for error
    if(ret != 0){
        perror("getpwnam_r");
        return -1;
    }
    // check for name not found
    if(pwd_result == NULL){
        fprintf(stderr, "user %s not found\n", user);
        return -1;
    }

    *out = pwd_result->pw_uid;

    return 0;
}

// group must be null-terminated, but it can be a gid or a username
static int get_gid_from_string(char *group, gid_t *out){
    // check if group is a GID already
    char *endptr = group;
    long int lgid = strtol(group, &endptr, 0);

    // was it a valid number?
    if(group[0] != '\0' && endptr[0] == '\0'){
        // group was a GID
        if(lgid > UINT_MAX){
            fprintf(stderr, "GID %ld too big\n", lgid);
            return -1;
        }
        if(lgid < 0){
            fprintf(stderr, "GID %ld cannot be negative\n", lgid);
            return -1;
        }
        *out = (gid_t)lgid;
        return 0;
    }

    // otherwise, assume group is a name string
    struct group grp;
    struct group *grp_result;
    char namebuf[1024];
    errno = 0;
    int ret = getgrnam_r(group, &grp, namebuf, sizeof(namebuf), &grp_result);
    // check for error
    if(ret != 0){
        perror("getgrnam_r");
        return -1;
    }
    // check for name not found
    if(grp_result == NULL){
        fprintf(stderr, "group %s not found\n", group);
        return -1;
    }

    *out = grp_result->gr_gid;

    return 0;
}

int set_file_perms(char *file, char *user, char *group, char *mode){
    int ret;
    if(user != NULL || group != NULL){
        if(user == NULL || group == NULL){
            fprintf(stderr, "user and group must both be NULL or non-NULL\n");
            return -1;
        }
        uid_t uid;
        ret = get_uid_from_string(user, &uid);
        if(ret != 0){
            return -1;
        }

        gid_t gid;
        ret = get_gid_from_string(group, &gid);
        if(ret != 0){
            return -1;
        }

        printf("chown\n");
        ret = chown(file, uid, gid);
        if(ret != 0){
            perror(file);
            return -1;
        }
    }

    if(mode != NULL){
        // parse the string as octal
        char *endptr = mode;
        long int lmode = strtol(mode, &endptr, 8);
        if(mode[0] == '\0' || endptr[0] != '\0'){
            fprintf(stderr, "invalid permissions: %s\n", mode);
            return -1;
        }
        if(lmode > UINT_MAX){
            fprintf(stderr, "mode %ld too big\n", lmode);
            return -1;
        }
        if(lmode < 0){
            fprintf(stderr, "mode %ld cannot be negative\n", lmode);
            return -1;
        }

        ret = chmod(file, (mode_t)lmode);
        if(ret != 0){
            perror(file);
            return -1;
        }
    }
    return 0;
}
