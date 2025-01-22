#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include "cr.h"


static int connections[CONNECTIONS_MAX];
static int connections_count = 0;


/* todo
 *  - implement some sort of protocol so that you can send diff types of
 *    messages, instead of sending chars then printing chars
 *  - fix formatting when sending/receiving messages
 *      - show who sent what message
 *      - allow clients to set an alias
 */
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


/* send message to all currently connected clients except the sender
 * parameters
 *  sender: socket descriptor that sent the message (can specify 0 to broadcast to all)
 *  msg: message of size msg_len
 *  msg_len: size of message in bytes
 *
 */
void broadcast(int sender, char *msg, size_t msg_len) {
    if (msg_len > MSG_LEN_MAX) {
        fprintf(stderr, "broadcast: message of size %lu is too long, not sent", msg_len);
        return;
    }
    for (size_t i=0; i<CONNECTIONS_MAX; i++) {
        if (connections[i] != 0 && connections[i] != sender) {
            send(connections[i], msg, msg_len, 0);
        }
    }
}

char* get_alias() {
    return 0;
}


/* recvs messages, broadcasts recvd messages */
void *recv_handler(void *connection) {
    Connection *con_info = (Connection *) connection;
    int sd = *con_info->sd_loc;

    printf("client connected\n");

    char message[MSG_LEN_MAX];
    int bytes_read = 1;

    /* get alias
     *  want get alias from user, make struct for message type now
     *  or differentiate receiving messages from broadcasting
     * */
    while (1) {
        memset(message, 0, MSG_LEN_MAX);
        bytes_read = recv(sd, message, MSG_LEN_MAX, 0);
        switch (bytes_read) {
            case 0:
                *con_info->sd_loc = 0;
                connections_count--;
                free(connection);
                close(sd);
                pthread_exit(0);
            case -1:
                perror("recv");
                continue;
            default:
                printf("msg: %s\n", message);
                broadcast(sd, message, bytes_read);
                continue;
        }
    }
}


int main() {
    int sd_server = setup_server_socket();

    /* main loop for accepting connections  */
    while (1) {
        while (connections_count == CONNECTIONS_MAX); /* wait for someone to disconnect */
        connections_count++; // RACE CONDITION: use semaphore

        int sd_con;
        Connection *cur_con = malloc(sizeof(Connection)); // to be freed in thread
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
}

