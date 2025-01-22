#include <bits/types/struct_timeval.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "cr.h"


int setup_client_socket() {
    int status, backlog = 5;

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

void *recv_handler(void *args) {
    int sd = *(int *) args;

    char message[MSG_LEN_MAX];
    int bytes_read = 1;

    while (1) {
        memset(message, 0, MSG_LEN_MAX);
        bytes_read = recv(sd, message, MSG_LEN_MAX, 0);
        switch (bytes_read) {
            case 0:
                close(sd);
                pthread_exit(0);
            case -1:
                perror("recv");
                continue;
            default:
                printf("msg: %s\n", message);
                continue;
        }
    }


    return 0;
}

int main() {
    int sd = setup_client_socket();
    printf("connected to server\n");

    /* recv messages */
    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_handler, &sd) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_detach(tid);

    /* send messages */
    while (1) {
        printf("> ");
        char *msg = malloc(MSG_LEN_MAX);
        msg = fgets(msg, MSG_LEN_MAX, stdin);
        if (msg == NULL) {
            // note: triggers when C-d is hit on empty line
            perror("fgets");
            exit(1);
        }

        int msg_len = strlen(msg);
        int bytes_sent;
        // remove newline
        if (msg[msg_len-1] == '\n') {
            msg[msg_len-1] = '\0';
        }
        if ((bytes_sent = send(sd, msg, msg_len, 0)) == -1) {
            perror("send");
            continue;
        }
    }
}
