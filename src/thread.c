#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#include "utils.h"
#include "thread.h"

void* thread_connection(void* arg) {
    /* localised pthread arguments and free it */
    struct thread_args* thread_args = (struct thread_args*)arg;
    int newsockfd = thread_args->newsockfd;
    char* root_path = thread_args->root_path;
    free(arg);
    /* for path name extraction and opening requested file */
    char buffer[STRLEN_L];
    char* file_path;
    char* full_path;
    const char* extension;
    struct stat fstat;
    int fd = 0;
    /* to store http response */
    char* response = NULL;
    char http_status[STRLEN_S];
    char content_type[STRLEN_S];

    int buf_len = 0; /* total length */
    int n = 0; /* num of chars read */
    /* wait for request to be over - detect crlf */
    while (1) {
        n = read(newsockfd, &buffer[buf_len], STRLEN_L);
        if (n < 0) {
            perror("read");
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
        free(root_path);
        close(newsockfd);
        return NULL;
    } else if (strcmp(file_path, "/") == 0) {
        free(file_path);
        free(root_path);
        close(newsockfd);
        return NULL;
    }

    /* get the full path name (also handle path_escape) */
    full_path = addStrings(root_path, file_path);
    /* base case: 404 not found error response */
    strcpy(http_status, "HTTP/1.0 404 Not Found\r\n\r\n");
    response = addStrings(http_status, "");
    /* check for double dot segments */
    char* x;
    if ((x = strstr(full_path, "../")) == NULL ||
            (x = strstr(full_path, "..\0")) == NULL) {
        /* construct http response on existing file (RFC 1945) */
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
        }
    }

    /* Write message back */
    n = send(newsockfd, response, strlen(response), 0);
    if (n < 0) {
        perror("write");
        free(file_path);
        free(root_path);
        free(full_path);
        free(response);
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
            free(root_path);
            free(full_path);
            free(response);
            close(newsockfd);
            return NULL;
        }
    }

    /* clean up */
    free(file_path);
    free(root_path);
    free(full_path);
    free(response);

    close(newsockfd);

    return NULL;
}
