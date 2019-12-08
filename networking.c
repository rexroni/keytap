#include "networking.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <linux/un.h>
#include <fcntl.h>

int gai_open(const char* host, const char* service, bool server_side){
    int out_fd;

    // prepare for getaddrinfo
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    //hints.ai_flags = server_side ? AI_PASSIVE : 0;

    // get address of host
    struct addrinfo* ai;
    int ret = getaddrinfo(host, service, &hints, &ai);
    if(ret != 0){
        return -1;
    }
    // reset error
    errno = 0;

    // connect to the host
    struct addrinfo* p;
    for(p = ai; p != NULL; p = p->ai_next){
        struct sockaddr_in* sin = (struct sockaddr_in*)p->ai_addr;
        printf("%s to ip addr %s\n", server_side ? "Binding" : "Connecting",
                inet_ntoa(sin->sin_addr));
        // create a socket
        out_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(out_fd < -1){
            perror("socket");
            continue;
        }
        // bind/listen or connect, depending
        if(server_side){
            ret = bind(out_fd, p->ai_addr, p->ai_addrlen);
            if(ret != 0){
                perror("bind");
                close(out_fd);
                continue;
            }
            // set RESUSEADDR for the bind socket
            int enable = 1;
            ret = setsockopt(out_fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                    sizeof(enable));
            if(ret != 0){
                perror("setsockopt");
                close(out_fd);
                continue;
            }
            // start listening
            ret = listen(out_fd, 5);
            if(ret != 0){
                perror("listen");
                close(out_fd);
                continue;
            }
        }else{
            printf("connecting!\n");
            ret = connect(out_fd, p->ai_addr, p->ai_addrlen);
            if(ret != 0){
                perror("connect");
                close(out_fd);
                continue;
            }
        }
        // if we made it here, we connected successfully
        break;
    }
    // make sure we found something
    if(p == NULL){
        fprintf(stderr, "failed all attempts\n");
        out_fd = -1;
    }

    freeaddrinfo(ai);

    return out_fd;
}

int unix_socket_open(char *sock, char *lock, int *lockfd){
    if(strlen(sock) >= UNIX_PATH_MAX){
        size_t diff = strlen(sock) - UNIX_PATH_MAX - 1;
        fprintf(stderr, "socket path is %zu bytes too long: %s\n", diff, sock);
        return -1;
    }

    // create the lock file if it doesn't exist
    *lockfd = open(lock, O_WRONLY | O_CREAT, 0666);
    if(*lockfd < 0){
        perror(lock);
        return -1;
    }

    // obtain the file lock (F_SETLK doesn't block, but F_SETLKW would)
    struct flock flock = {.l_type = F_WRLCK};
    int ret = fcntl(*lockfd, F_SETLK, &flock);
    if(ret == -1){
        perror("unable to lock file");
        goto fail_lockfd;
    }

    // now that we have the file, delete the socket if it exists
    unlink(sock);

    // create a unix socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        goto fail_lock;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    // we already checked the length of socket to ensure a null byte will fit
    strncpy(addr.sun_path, sock, UNIX_PATH_MAX);

    // bind will create the sock file
    ret = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret == -1){
        perror(sock);
        goto fail_sockfd;
    }

    return sockfd;

fail_sockfd:
    close(sockfd);
fail_lock:
    flock.l_type = F_UNLCK;
    if(fcntl(*lockfd, F_SETLK, &flock) == -1){
        fprintf(stderr, "warning: failed to release lock on %s\n", lock);
    }

fail_lockfd:
    close(*lockfd);

    return -1;
}

void unix_socket_close(int sockfd, int lockfd){
    // close socket before unlocking
    close(sockfd);

    // unlock then close lock file
    struct flock flock = {.l_type = F_UNLCK};
    if(fcntl(lockfd, F_SETLK, &flock) == -1){
        fprintf(stderr, "warning: failed to release file lock\n");
    }

    close(lockfd);
}
