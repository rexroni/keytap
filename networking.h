#ifndef NETWORKING_H
#define NETWORKING_H

#include <stdbool.h>

int gai_open(const char* host, const char* service, bool server_side);

#endif // NETWORKING_H
