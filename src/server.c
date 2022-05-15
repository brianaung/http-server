// the original socket creation and accepting connection code is based on Comp30023 Lab 9 code.
#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "utils.h"

int main(int argc, char** argv) {
	int sockfd, newsockfd, n, re, s;
	char buffer[256];
	struct addrinfo hints, *res;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;

	if (argc < 4) {
		fprintf(stderr, "ERROR, not enough arguments provided\n");
		exit(EXIT_FAILURE);
	}

	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
    // TODO: add IPv6 support
    if (strcmp(argv[1], "4") == 0) {
	    hints.ai_family = AF_INET; // IPv4
    } else if (strcmp(argv[1], "6") == 0) {
        // create socket for IPv6
        fprintf(stderr, "ERROR, IPv6 currently not supported\n");
        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "ERROR, provide a valid protocol number\n");
        exit(EXIT_FAILURE);
    }
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
	// node (NULL means any interface), service (port), hints, res
	s = getaddrinfo(NULL, argv[2], &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

    // get the web root path and verify that it exists
    char* root_path = getWebRootDir(argv[3]);
    verifyFilePath(root_path);

	// Create socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("socket");
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

	// Accept a connection - blocks until a connection is ready to be accepted
	// Get back a new file descriptor to communicate on
	client_addr_size = sizeof client_addr;
	newsockfd =
		accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
	if (newsockfd < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

    // Read characters from the connection, then process
    // buffer is a http Req message
    n = read(newsockfd, buffer, 255); // n is number of characters read
    if (n < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    // Null-terminate string
    buffer[n] = '\0';

    char* file_path = getGetReqPath(buffer);

    char* full_path = addStrings(root_path, file_path);
    verifyFilePath(full_path);

    // default http status (success)
    char* http_status = malloc(sizeof(char) * 100);
    assert(http_status);
    strcpy(http_status, "HTTP/1.0 200 OK\n");
    // default MIME type
    char* content_type = malloc(sizeof(char) * 100);
    assert(content_type);
    strcpy(content_type, "Content-Type: application/octet-stream\n");

    // get the correct MIME type based on file extension
    char* extension = strchr(file_path, '.');
    if (strcmp(extension, ".html") == 0) {
        strcpy(content_type, "Content-Type: text/html\n");
    } else if (strcmp(extension, ".jpeg") == 0) {
        strcpy(content_type, "Content-Type: image/jpeg\n");
    } else if (strcmp(extension, ".css") == 0) {
        strcpy(content_type, "Content-Type: text/css\n");
    } else if (strcmp(extension, ".js") == 0) {
        strcpy(content_type, "Content-Type: text/javascript\n");
    }

    // http response (RFC 1945)
    char* response = addStrings(http_status, content_type);
    strcat(response, "\n"); // headers should be CRLF terminated

    // Write message back
    // n = write(newsockfd, "I got your message", 18);
    n = send(newsockfd, response, strlen(response), 0);
    if (n < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    free(http_status);
    free(content_type);

    printf("DEBUG LOG:\n");
    printf("\n");
    printf("Here is the web root:\n%s\n", root_path);
    printf("\n");
    printf("Here is the req message:\n%s\n", buffer);
    printf("\n");
    printf("Here is the path:\n%s\n", file_path);

	close(sockfd);
	close(newsockfd);

    free(root_path);
    free(file_path);
    free(full_path);
    free(response);

	return 0;
}
