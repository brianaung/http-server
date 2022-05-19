#include <netdb.h>

/* store thread arguments */
struct thread_args {
    int newsockfd;
    struct sockaddr_storage client_addr;
    char* root_path;
};

void* thread_connection(void *arg);
