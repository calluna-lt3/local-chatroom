#ifndef PRIMARY_H
#define PRIMARY_H

#include <stdint.h>
#include <sys/socket.h>

#define ADDRESS "10.20.8.9" // server address
#define PORT "8018"
#define CONNECTIONS_MAX 2
#define DATA_SZ_MAX 128
#define ALIAS_SZ_MAX 16

/* can probably be defined at runtime, based on DATA_SZ_MAX and ALIAS_SZ_MAX */
#define TXT_SZ_MAX 104

typedef enum {
    HELLO = 0x0, // handshake
    TEXT  = 0x1, // send message
    ALIAS = 0x2, // set alias
    END   = 0x3, // disconnect from server
} Opcode;

typedef enum {
    UNICAST   = 0x0,
    BROADCAST = 0x1,
} Transmission_type;

typedef struct {               // 128 bytes
    Opcode op;                 //  4
    char alias[ALIAS_SZ_MAX];  //  16
    uint32_t txt_sz;           //  4
    char txt[TXT_SZ_MAX];      //  104
} Data;

typedef struct {
    int* sd_loc;
    char alias[ALIAS_SZ_MAX];
    struct sockaddr_storage addr;
    socklen_t addr_sz;
} Connection;

#endif /* PRIMARY_H */
