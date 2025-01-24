#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include "cr.h"


static int connections[CONNECTIONS_MAX];
static sem_t available_connections;


int setup_server_socket() {
    int status, yes = 1, backlog = 5;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *addrs; // free
    if ((status = getaddrinfo(NULL, PORT, &hints, &addrs)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    /* bind socket */
    int sd;
    for (struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
        if ((sd = socket(addr->ai_family,  addr->ai_socktype, addr->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sd, addr->ai_addr, addr->ai_addrlen) == -1) {
            perror("bind");
            continue;
        }

        break;
    }
    freeaddrinfo(addrs);

    if (listen(sd, backlog) == -1) {
        perror("listen");
        exit(1);
    }

    return sd;
}


void send_data(Connection *sender, Opcode op, char *txt, Transmission_type t_type) {
    Data *data = calloc(DATA_SZ_MAX, sizeof(char));
    data->op = op;

    data->txt_sz = strlen(txt);
    if (data->txt_sz > TXT_SZ_MAX) { // TODO: proper handling
        fprintf(stderr, "message of size %u too long, set to empty", data->txt_sz);
        txt = "";
        data->txt_sz = 1;
    }
    memcpy(data->txt, txt, data->txt_sz);
    size_t data_sz = sizeof(Opcode) + ALIAS_SZ_MAX + sizeof(uint32_t) + data->txt_sz;

    switch (t_type) {
        case UNICAST: // used solely to communicate with sender
            if (send(*sender->sd_loc, data, data_sz, 0) == -1) {
                perror("send");
            }
            break;
        case BROADCAST: // send to all but sender
            for (size_t i=0; i<CONNECTIONS_MAX; i++) {
                if (connections[i] != 0 && connections[i] != *sender->sd_loc) {
                    if (send(connections[i], data, data_sz, 0) == -1) {
                        perror("send");
                    }
                }
            }
            break;
    }
    free(data);
}


/* Assumptions
 * - if opcode is formatted correctly, so is everything else
 */
void handle_data(Connection *connection, Data *data_r, size_t data_sz) {
    char *message;
    switch (data_r->op) {
        case HELLO: // handshake
            message = "hi";
            send_data(connection, HELLO, message, UNICAST);
            break;
        case TEXT: // send message
            send_data(connection, TEXT, data_r->txt, BROADCAST);
            printf("msg: %s\n", data_r->txt);
            break;
        case ALIAS: // set alias
            /* assumptions
             * - if alias is successfully set, send alias
             *   otherwise send empty buffer
             */
            if (data_r->txt_sz > ALIAS_SZ_MAX) {
                // too big
                message = "";
            } else {
                memcpy(connection->alias, data_r->txt, data_r->txt_sz);
                message = data_r->txt;
            }
            send_data(connection, ALIAS, message, UNICAST);
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


void *recv_handler(void *connection) {
    Connection *con_info = (Connection *) connection;
    int sd = *con_info->sd_loc;

    printf("client connected\n");

    char data[DATA_SZ_MAX];
    int bytes_read = 1;

    while (1) {
        memset(data, 0, DATA_SZ_MAX);
        bytes_read = recv(sd, data, DATA_SZ_MAX, 0);
        switch (bytes_read) {
            case 0: // client disconnected
                *con_info->sd_loc = 0; // TODO: shared resource, maybe mutex needed (probably not)
                sem_post(&available_connections);
                free(connection);
                close(sd);
                pthread_exit(0);
            case -1: // err
                perror("recv");
                continue;
            default:
                handle_data(con_info, (Data *) data, bytes_read);
                continue;
        }
    }
}


int main() {
    int sd_server = setup_server_socket();
    sem_init(&available_connections, 0, CONNECTIONS_MAX);

    /* main loop for accepting connections  */
    while (1) {
        sem_wait(&available_connections);

        int sd_con;
        Connection *cur_con = malloc(sizeof(Connection)); // to be freed by thread
        if ((sd_con = accept(sd_server, (struct sockaddr *)&cur_con->addr, &cur_con->addr_sz)) == -1) {
            perror("accept");
        }

        // find spot in connection list
        for (size_t i=0; i<CONNECTIONS_MAX; i++) {
            if (connections[i] == 0) {
                connections[i] = sd_con;
                cur_con->sd_loc = &connections[i];
                break;
            }
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, recv_handler, cur_con) != 0) {
            perror("pthread_create");
            exit(1);
        }
        pthread_detach(tid);
    } /* end main loop */
    sem_destroy(&available_connections);
}

