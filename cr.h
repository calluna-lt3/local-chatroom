#ifndef PRIMARY_H
#define PRIMARY_H

#include <sys/socket.h>

#define ADDRESS "10.20.8.9" // server address
#define PORT "8018"
#define MSG_LEN_MAX 64
#define CONNECTIONS_MAX 16

typedef struct {
    int* sd_loc;
    struct sockaddr_storage addr;
    socklen_t addr_sz;
} Connection;

#endif /* PRIMARY_H */
