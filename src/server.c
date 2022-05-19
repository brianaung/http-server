/* flag completion of task 3 and 4 */
#define IMPLEMENTS_IPV6
#define MULTITHREADED

/* Original TCP server implementation is based on server.c
 * by COMP30023 Week9 Practical */
#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
/* for multithreaded server */
#include <pthread.h>

#include "utils.h"
#include "thread.h"

#define CONN_IN 10


int main(int argc, char** argv) {

	int sockfd, newsockfd, re, s;
	struct addrinfo hints, *res, *p;
	socklen_t client_addr_size;
    /* for threads */
    pthread_t tid;
    pthread_attr_t pthread_attr;
    struct thread_args *thread_args;

	if (argc < 4) {
		fprintf(stderr, "ERROR, not enough arguments provided\n");
		exit(EXIT_FAILURE);
	}

    /* create socket for given port number */
	memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (strcmp(argv[1], "6") == 0) {
        /* initialise IPv6 address and create socket */
        hints.ai_family = AF_INET6; 
        s = getaddrinfo(NULL, argv[2], &hints, &res);
        if (s != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }
        /* based on IPv6 socket creation code 
         * by COMP30023 Week8 Lec2c */
        for (p = res; p != NULL; p = p->ai_next) {
            if (p->ai_family == AF_INET6 &&
                    (sockfd = socket(p->ai_family,
                                   p->ai_socktype,
                                   p->ai_protocol)) < 0) {
                perror("socket");
                exit(EXIT_FAILURE);
            }
        }
    } else {
        /* fall back to IPv4 on other argv[2] values */
        /* initialise IPv4 address and create socket */
	    hints.ai_family = AF_INET;
        s = getaddrinfo(NULL, argv[2], &hints, &res);
        if (s != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }
    }

	/* Reuse port if possible */
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	/* Bind address to the socket */
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(res);

    /* Listen on socket */
	if (listen(sockfd, CONN_IN) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

    /* Thread creation code is influenced by 
     * https://github.com/RedAndBlueEraser/c-multithreaded-client-server/blob/master/server.c */
    /* initialise pthread attribute */
    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("pthread_attr_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate");
        exit(EXIT_FAILURE);
    }

    /* infinite loop so server don't close after handling single request */
    for (;;) {

        /* create thread_args for each incoming connection */
        /* TODO: fix the memory leak here */
        thread_args = (struct thread_args *)malloc(sizeof *thread_args);
        if (!thread_args) {
            perror("malloc");
            continue;
        }

        /* Accept a connection */
        client_addr_size = sizeof thread_args->client_addr;
        newsockfd = accept(sockfd, (struct sockaddr*)&thread_args->client_addr, 
                &client_addr_size);
        if (newsockfd < 0) {
            perror("accept");
            free(thread_args);
            continue;
        }

        /* initialise thread arguments and create thread */
        thread_args->newsockfd = newsockfd;
        thread_args->root_path = getWebRootDir(argv[3]);
        if (pthread_create(&tid, &pthread_attr, thread_connection, 
                    (void*)thread_args) != 0) {
            perror("pthread");
            free(thread_args);
            continue;
        }
    }

	close(sockfd);
	return 0;
}
