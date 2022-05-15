#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>

/* adapted from stackoverflow post */
char* getGetReqPath(const char* buffer) {
    const char* start = buffer;
    const char* end;

    char* path = NULL;
    size_t path_len;

    /* make sure GET is at the start of the path */
    if (strncmp("GET ", start, 4)) {
		fprintf(stderr, "Parse Error: GET is missing\n");
		exit(EXIT_FAILURE);
    }

    /* move the start pointer to get rid of "GET<whitespace>" */
    start += 4;

    end = start;
    while (*end && !isspace(*end)) ++end;
    path_len = (end - start);
    path = malloc(path_len + 1);
    assert(path);

    // copy path
    memcpy(path, start, path_len);

    // null terminate string
    path[path_len] = '\0';

    return path;
}

char* getWebRootDir(char* input_path) {
    // check if the file path exists
    if (access(input_path, F_OK) != 0) {
		fprintf(stderr, "404 Bad Request, path does not exist\n");
		exit(EXIT_FAILURE);
    }

    // if exists
    char* root_path = malloc(strlen(input_path));
    assert(root_path);
    strcpy(root_path, input_path);

    return root_path;
}
