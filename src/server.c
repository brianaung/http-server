#define IMPLEMENTS_IPV6
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

int main(int argc, char** argv) {
	int sockfd, newsockfd, n, re, s;
	char buffer[256];
	struct addrinfo hints, *res, *p;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;

    char* root_path = NULL;  // web root path
    char* file_path = NULL;  // path to file from get request
    char* full_path = NULL;
    const char* extension;
    int fd = 0;
    struct stat fstat;

    // variables to store http response
    char* http_status = malloc(sizeof(char) * 100);
    assert(http_status);
    char* content_type = malloc(sizeof(char) * 100);
    assert(content_type);
    char* response = NULL;

	if (argc < 4) {
		fprintf(stderr, "ERROR, not enough arguments provided\n");
		exit(EXIT_FAILURE);
	}

	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
    if (strcmp(argv[1], "4") == 0) {
	    hints.ai_family = AF_INET;  // IPv4
                                         
	    // node (NULL means any interface), service (port), hints, res
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
        hints.ai_family = AF_INET6;  // IPv6
                                         
	    // node (NULL means any interface), service (port), hints, res
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

    // get the web root path and verify that it exists
    root_path = getWebRootDir(argv[3]);

    // infinite loop so server don't close after handling single request
    // need to use SIGINT to close it
    for (;;) {

        // Accept a connection - blocks until a connection is ready to be accepted
        // Get back a new file descriptor to communicate on
        client_addr_size = sizeof client_addr;
        newsockfd =
            accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
        if (newsockfd < 0) {
            perror("accept");
            continue;
        }

        // Read characters from the connection, then process
        // buffer is a http Req message
        n = read(newsockfd, buffer, 255); // n is number of characters read
        if (n < 0) {
            perror("read");
            close(newsockfd);
            continue;
        }
        // Null-terminate string
        buffer[n] = '\0';

        // get the path of the requested file
        file_path = getGetReqPath(buffer);
        if (strcmp(file_path, "400") == 0) {
            close(newsockfd);
            continue;
        }
        full_path = addStrings(root_path, file_path);
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
            close(newsockfd);
            continue;
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
            if (n < 0) {
                perror("write");
                close(newsockfd);
                continue;
            }
        }

        free(file_path);
        free(full_path);
        free(response);

        close(fd);
	    close(newsockfd);
    }

	close(sockfd);

    free(root_path);
    free(http_status);
    free(content_type);

	return 0;
}
