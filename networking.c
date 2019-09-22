#include "networking.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

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
        return 1;
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
            ret = listen(out_fd, 5);
            if(ret != 0){
                perror("bind");
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
