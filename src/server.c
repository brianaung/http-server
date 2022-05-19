/* flag completion of task 3 and 4 */
#define IMPLEMENTS_IPV6
#define MULTITHREADED

/* Original TCP server implementation is based on server.c
 * by COMP30023 Week9 Practical */
/* Thread creation code is influenced by 
 * https://github.com/RedAndBlueEraser/c-multithreaded-client-server/blob/master/server.c */
#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
/* for multithreaded server */
#include <pthread.h>

#include "utils.h"
// #include "thread.h"

#define CONN_IN 10

struct thread_args {
    int newsockfd;
    struct sockaddr_storage client_addr;
    char* root_path;
};

void* thread_connection(void *arg);

int main(int argc, char** argv) {

	int sockfd, newsockfd, re, s;
	struct addrinfo hints, *res, *p;
	socklen_t client_addr_size;
    /* for threads */
    pthread_t tid;
    // pthread_attr_t pthread_attr;
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

    /* initialise pthread attribute */
    // if (pthread_attr_init(&pthread_attr) != 0) {
    //     perror("pthread_attr_init");
    //     exit(EXIT_FAILURE);
    // }
    // if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
    //     perror("pthread_attr_setdetachstate");
    //     exit(EXIT_FAILURE);
    // }

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
        if (pthread_create(&tid, NULL, thread_connection, 
                    (void*)thread_args) != 0) {
            perror("pthread");
            free(thread_args);
            continue;
        }
        if (pthread_join(tid, NULL)) {
            perror("pthread");
            continue;
        }
    }

	close(sockfd);
	return 0;
}

void* thread_connection(void* arg) {
    /* localised pthread arguments and free it */
    struct thread_args* thread_args = (struct thread_args*)arg;
    int newsockfd = thread_args->newsockfd;
    char* root_path = thread_args->root_path;
    free(arg);
    /* for path name extraction and opening requested file */
    char buffer[1024];
    char* file_path;
    char* full_path;
    const char* extension;
    struct stat fstat;
    int fd = 0;
    /* to store http response */
    char* response = NULL;
    char* http_status = malloc(sizeof(char) * 100);
    assert(http_status);
    char* content_type = malloc(sizeof(char) * 100);
    assert(content_type);

    int buf_len = 0; /* total length */
    int n = 0; /* num of chars read */
    /* wait for request to be over - detect crlf */
    while (1) {
        n = read(newsockfd, &buffer[buf_len], 255);
        if (n < 0) {
            perror("read");
            free(http_status);
            free(content_type);
            free(root_path);
            close(newsockfd);
            return NULL;
        }
        buf_len += n;
        if (strcmp(&buffer[buf_len - 4], "\r\n\r\n") == 0) {
            break;
        }
    }

    /* Null-terminate string */
    buffer[buf_len] = '\0';

    /* get the path of the requested file */
    file_path = getGetReqPath(buffer);
    if (strcmp(file_path, "400") == 0) {
        free(file_path);
        free(http_status);
        free(content_type);
        free(root_path);
        close(newsockfd);
        return NULL;
    } else if (strcmp(file_path, "/") == 0) {
        free(file_path);
        free(http_status);
        free(content_type);
        free(root_path);
        close(newsockfd);
        return NULL;
    }

    /* get the full path name (also handle path_escape) */
    full_path = addStrings(root_path, file_path);
    full_path = removeSubStr(full_path, "../");
    full_path = removeSubStr(full_path, "..\0");

    /* construct http response (RFC 1945) */
    if ((fd = open(full_path, O_RDONLY)) > 0) {

        strcpy(http_status, "HTTP/1.0 200 OK\r\n");

        extension = strchr(file_path, '.'); 
        if (!extension) {
            /* unknown file type */
            strcpy(content_type, "Content-Type: application/octet-stream\r\n\r\n");
        } else if (strcmp(extension, ".html") == 0) {
            strcpy(content_type, "Content-Type: text/html\r\n\r\n");
        } else if (strcmp(extension, ".jpg") == 0) {
            strcpy(content_type, "Content-Type: image/jpeg\r\n\r\n");
        } else if (strcmp(extension, ".css") == 0) {
            strcpy(content_type, "Content-Type: text/css\r\n\r\n");
        } else if (strcmp(extension, ".js") == 0) {
            strcpy(content_type, "Content-Type: text/javascript\r\n\r\n");
        }
        response = addStrings(http_status, content_type);
    } else {
        perror("read");
        strcpy(http_status, "HTTP/1.0 404 Not Found\r\n\r\n");
        response = addStrings(http_status, "");
    }

    /* Write message back */
    n = send(newsockfd, response, strlen(response), 0);
    if (n < 0) {
        perror("write");
        free(file_path);
        free(full_path);
        free(response);
        free(http_status);
        free(content_type);
        free(root_path);
        close(newsockfd);
        return NULL;
    }

    /* ---- Reason for using sendfile() over other alternatives ---- */
    /* sendfile() copies data between two file descriptors and this copying 
     * is done in the kernel space.
     * it is therefore more efficient than using read() and write() which 
     * requires transferring data to and from user space. */
    /* from Linux manual page */

    /* send file only when the file exists */
    if (fd > 0 && (stat(full_path, &fstat) == 0)) {
        n = sendfile(newsockfd, fd, NULL, fstat.st_size);
        close(fd);
        if (n < 0) {
            perror("write");
            free(file_path);
            free(full_path);
            free(response);
            free(http_status);
            free(content_type);
            free(root_path);
            close(newsockfd);
            return NULL;
        }
    }

    /* clean up */
    free(file_path);
    free(full_path);
    free(response);
    free(http_status);
    free(content_type);
    free(root_path);

    close(newsockfd);

    return NULL;
}
