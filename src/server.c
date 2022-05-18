#define IMPLEMENTS_IPV6
#define MULTITHREADED
// the original socket creation and accepting connection code is based on Comp30023 Lab 9 code.
#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
// for multithreaded server
#include <pthread.h>

#include "utils.h"

struct pthread_arg {
    int newsockfd;
    struct sockaddr_storage client_addr;
    char* root_path;
};

void* pthread_routine(void *arg);

int main(int argc, char** argv) {
	int sockfd, newsockfd, re, s;
	struct addrinfo hints, *res, *p;
	// struct sockaddr_storage client_addr;
	socklen_t client_addr_size;

    // pthread
    pthread_attr_t pthread_attr;
    struct pthread_arg *pthread_arg;
    pthread_t pthread;

	if (argc < 4) {
		fprintf(stderr, "ERROR, not enough arguments provided\n");
		exit(EXIT_FAILURE);
	}

    // create socket for given port number
	memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
    if (strcmp(argv[1], "4") == 0) {
        // initialise IPv4 address
	    hints.ai_family = AF_INET;
        s = getaddrinfo(NULL, argv[2], &hints, &res);
        if (s != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }
        // Create socket
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(argv[1], "6") == 0) {
        // initialise IPv6 address
        hints.ai_family = AF_INET6; 
        s = getaddrinfo(NULL, argv[2], &hints, &res);
        if (s != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }
        // Create socket for IPv6
        // based on week 8 lecture 2c code
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
        fprintf(stderr, "ERROR, provide a valid protocol number\n");
        exit(EXIT_FAILURE);
    }

	// Reuse port if possible
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// Bind address to the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(res);

	// Listen on socket - means we're ready to accept connections,
	// incoming connection requests will be queued, man 3 listen
	if (listen(sockfd, 5) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

    // initialise pthread attribute
    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("pthread_attr_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate");
        exit(EXIT_FAILURE);
    }

    // infinite loop so server don't close after handling single request
    // need to use SIGINT to close it
    for (;;) {
        // create pthread arg for each connection
        pthread_arg = (struct pthread_arg *)malloc(sizeof *pthread_arg);
        if (!pthread_arg) {
            perror("malloc");
            continue;
        }

        // Accept a connection
        client_addr_size = sizeof pthread_arg->client_addr;
        newsockfd = accept(sockfd, (struct sockaddr*)&pthread_arg->client_addr, &client_addr_size);
        if (newsockfd < 0) {
            perror("accept");
            free(pthread_arg);
            continue;
        }

        /* initialise pthread arg */
        pthread_arg->newsockfd = newsockfd;
        pthread_arg->root_path = getWebRootDir(argv[3]);

        if (pthread_create(&pthread, &pthread_attr, pthread_routine, (void*)pthread_arg) != 0) {
            perror("pthread create");
            free(pthread_arg);
            continue;
        }
    }

    // this won't be reached since program will only be terminated by calling SIGINT
	close(sockfd);

	return 0;
}

void* pthread_routine(void* arg) {
    struct pthread_arg* pthread_arg = (struct pthread_arg*)arg;

    int newsockfd = pthread_arg->newsockfd;
    // struct sockaddr_storage client_addr = pthread_arg->client_addr;
    char* root_path = pthread_arg->root_path;

    free(arg);

    char buffer[1024];
    char* file_path;
    char* full_path;
    const char* extension;
    int fd = 0;
    struct stat fstat;

    // variables to store http response
    char* http_status = malloc(sizeof(char) * 100);
    assert(http_status);
    char* content_type = malloc(sizeof(char) * 100);
    assert(content_type);
    char* response = NULL;

    // Read characters from the connection, then process
    // buffer is a http Req message
    int buf_len = 0; // total length
    int n = 0; // num of chars read
    while (1) {
        n = read(newsockfd, &buffer[buf_len], 255);
        if (n < 0) {
            perror("read");
            free(http_status);
            free(content_type);
            free(root_path);
            close(newsockfd);
            // exit(EXIT_FAILURE);
            return NULL;
        }
        buf_len += n;
        if (strcmp(&buffer[buf_len - 4], "\r\n\r\n") == 0) {
            break;
        }
    }

    // Null-terminate string
    buffer[buf_len] = '\0';

    // get the path of the requested file
    file_path = getGetReqPath(buffer);
    if (strcmp(file_path, "400") == 0) {
        free(file_path);
        free(http_status);
        free(content_type);
        free(root_path);
        close(newsockfd);
        // exit(EXIT_FAILURE);
        return NULL;
    } else if (strcmp(file_path, "/") == 0) {
        free(file_path);
        free(http_status);
        free(content_type);
        free(root_path);
        close(newsockfd);
        // exit(EXIT_FAILURE);
        return NULL;
    }

    full_path = addStrings(root_path, file_path);
    /* remove the ../ from the path to prevent going back directory */
    full_path = strRemove(full_path, "../");
    full_path = strRemove(full_path, "..\0");
    fd = open(full_path, O_RDONLY);

    // construct http response (RFC 1945)
    if (fd > 0) {
        // success
        strcpy(http_status, "HTTP/1.0 200 OK\r\n");

        extension = strchr(file_path, '.'); 
        if (!extension) {
            // unknown file type
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
        // http response (RFC 1945)
        response = addStrings(http_status, content_type);

    } else {
        perror("error");
        strcpy(http_status, "HTTP/1.0 404 Not Found\r\n\r\n");
        response = addStrings(http_status, "");
    }

    // Write message back
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
        // exit(EXIT_FAILURE);
        return NULL;
    }

    /* ---- Reason for using sendfile() over other alternatives ---- */
    // sendfile() copies data between two file descriptors and this copying 
    // is done in the kernel space.
    // it is therefore more efficient than using read() and write() which 
    // requires transferring data to and from user space.
    /* from Linux manual page */

    // send file only when the file exists
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
            // exit(EXIT_FAILURE);
            return NULL;
        }
    }

    free(file_path);
    free(full_path);
    free(response);
    free(http_status);
    free(content_type);
    free(root_path);

    close(newsockfd);

    return NULL;
}
