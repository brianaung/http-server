#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "utils.h"

/* given a http request buffer, extract out the path name */
char* getGetReqPath(const char* buffer) {
    const char* start = buffer;
    const char* end;

    char* path = NULL;
    size_t path_len;

    /* make sure GET is at the start of the path */
    if (strncmp("GET ", start, 4)) {
		fprintf(stderr, "Parse Error: GET is missing\n");
        path = malloc(strlen("400"));
        strcpy(path, "400");
        return path;
    }

    /* move the start pointer to get rid of "GET<whitespace>" */
    start += 4;

    end = start;
    while (*end && !isspace(*end)) {
        ++end;
    }
    path_len = (end - start);
    path = malloc(path_len + 1);
    assert(path);

    /* copy path */
    memcpy(path, start, path_len);
    path[path_len] = '\0';

    return path;
}

/* from the program argument, get the absolute root web directory */
char* getWebRootDir(char* input_path) {
    char* root_path = malloc(strlen(input_path) + 1);
    assert(root_path);

    strcpy(root_path, input_path);

    return root_path;
}

/* add 2 given strings together and return a new string */
char* addStrings(char* str1, char* str2) {
    char* ret = malloc(strlen(str1) + strlen(str2) + 1);
    assert(ret);

    /* copy first string to ret */
    strcpy(ret, str1);

    /* concatenate second string to ret (first) */
    strcat(ret, str2);

    return ret;
}
