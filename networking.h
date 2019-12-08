#ifndef NETWORKING_H
#define NETWORKING_H

#include <stdbool.h>

int gai_open(const char* host, const char* service, bool server_side);

// obtain a file lock and bind to the socket path (does not listen)
int unix_socket_open(char *sock, char *lock, int *lockfd);

// close the socket then unlock and close the lockfd
void unix_socket_close(int sockfd, int lockfd);

#endif // NETWORKING_H
