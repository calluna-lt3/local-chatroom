#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>
#include "cr.h"


static char alias[ALIAS_SZ_MAX];
static sem_t alias_ackd;
static size_t valid_alias = 0;



int setup_client_socket() {
    int status;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *addrs; // free
    if ((status = getaddrinfo(ADDRESS, PORT, &hints, &addrs)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    int sd;
    for (struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
        if ((sd = socket(addr->ai_family,  addr->ai_socktype, addr->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        break;
    }

    if ((status = connect(sd, addrs->ai_addr, addrs->ai_addrlen)) == -1) {
        perror("connect");
        exit(1);
    }

    freeaddrinfo(addrs);

    return sd;
}


void send_data(int sd, Opcode op, char *txt) {
    Data *data = calloc(DATA_SZ_MAX, sizeof(char));
    data->op = op;
    // maybe memset?
    memcpy(data->alias, alias, ALIAS_SZ_MAX);

    data->txt_sz = strlen(txt);
    if (data->txt_sz > TXT_SZ_MAX) { // TODO: proper handling
        fprintf(stderr, "message of size %u too long, set to empty", data->txt_sz);
        txt = "";
        data->txt_sz = 1;
    }
    memcpy(data->txt, txt, data->txt_sz);
    size_t data_sz = sizeof(Opcode) + ALIAS_SZ_MAX + sizeof(uint32_t) + data->txt_sz;

    if (send(sd, data, data_sz, 0) == -1) {
        perror("send");
    }
}


/* Assumptions
 * - if opcode is formatted correctly, so is everything else
 */
void handle_data(Data *data_r, size_t data_sz) {
    switch (data_r->op) {
        case HELLO: // recv handshake
          // TODO: ack connection was attempted, but is at limit for max connections so retry later
          printf("successfully connected to server\n");
          break;
        case TEXT: // print message
            printf("%s: %s\n", data_r->alias, data_r->txt);
            break;
        case ALIAS: // set alias
            if (data_r->txt_sz == 0) {
                printf("alias rejected by server\n");
            } else {
                valid_alias = 1;
                memcpy(alias, data_r->txt, data_r->txt_sz);
            }
            sem_post(&alias_ackd);
            break;
        case END: // TODO: disconnect client, maybe unnecessary
            break;
        default:
            fprintf(stderr, "data not properly formatted\nHEXDUMP:\n");
            for (size_t i=0; i<data_sz; i++) {
                fprintf(stderr, "0x%x ", ((char *) data_r)[i]);
            }
            printf("\n");
            break;
    }
}


// there is client length verification, and server length verification
//  maybe unnecessary ?
void handle_alias(int sd) {
    sem_init(&alias_ackd, 0, 0);
    do {
        printf("enter alias: \n> ");
        char *alias = malloc(ALIAS_SZ_MAX);
        alias = fgets(alias, ALIAS_SZ_MAX, stdin);
        if (alias == NULL) {
            perror("fgets");
            exit(1);
        }

        int alias_len = strlen(alias);
        if (alias_len == 1) { // empty alias (only newline)
            fprintf(stderr, "invalid alias\n");
            free(alias);
            continue;
        }

        if (alias[alias_len-1] == '\n') {
            alias[alias_len-1] = '\0';
        }

        send_data(sd, ALIAS, alias);
        free(alias);
        sem_wait(&alias_ackd);
    } while (!valid_alias);
    sem_destroy(&alias_ackd);
    printf("alias successfully set to: %s\n", alias);
}


void *recv_handler(void *args) {
    int sd = *(int *) args;

    char data_raw[DATA_SZ_MAX];
    int bytes_read = 1;

    while (1) {
        memset(data_raw, 0, DATA_SZ_MAX);
        bytes_read = recv(sd, data_raw, DATA_SZ_MAX, 0);
        switch (bytes_read) {
            case 0:
                close(sd);
                pthread_exit(0);
            case -1:
                perror("recv");
                continue;
            default:
                /* prints bytes of receieved message */
                handle_data((Data *)data_raw, bytes_read);
                continue;
        }
    }
}


// possibility: alarm as a timeout length for handshake
int main() {
    int sd = setup_client_socket();

    /* recv messages */
    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_handler, &sd) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_detach(tid);

    // handshake
    send_data(sd, HELLO, "pls respond");

    // without this delay, the bytes get batched (bad)
    sleep(1);

    // alias
    handle_alias(sd);

    /* send messages */
    while (1) {
        printf("> ");
        char *msg = malloc(DATA_SZ_MAX);
        msg = fgets(msg, DATA_SZ_MAX, stdin);
        if (msg == NULL) {
            // note: triggers when C-d is hit on empty line
            perror("fgets");
            exit(1);
        }

        int msg_len = strlen(msg);
        if (msg[msg_len-1] == '\n') {
            msg[msg_len-1] = '\0';
        }
        send_data(sd, TEXT, msg);
    }
}
