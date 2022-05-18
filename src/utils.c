#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>

#include "utils.h"

/* adapted from stackoverflow post */
char* getGetReqPath(const char* buffer) {
    const char* start = buffer;
    const char* end;

    char* path = NULL;
    size_t path_len;

    /* make sure GET is at the start of the path */
    if (strncmp("GET ", start, 4)) {
		// fprintf(stderr, "Parse Error: GET is missing\n");
        path = malloc(strlen("400"));
        strcpy(path, "400");
        return path;
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
    char* root_path = malloc(strlen(input_path) + 1);
    assert(root_path);

    strcpy(root_path, input_path);

    return root_path;
}

void prepend(char* s, const char* t) {
    size_t len = strlen(t);
    memmove(s + len, s, strlen(s) + 1);
    memcpy(s, t, len);
}

char* addStrings(char* str1, char* str2) {
    char* res = malloc(strlen(str1) + strlen(str2) + 1);
    assert(res);

    // copy first string to res
    strcpy(res, str1);

    // concatenate second string to res (first)
    strcat(res, str2);

    return res;
}

char* strRemove(char* str, const char* sub) {
    char *p, *q, *r;

    if (*sub && (q = r = strstr(str, sub)) != NULL) {
        size_t len = strlen(sub);
        while ((r = strstr(p = r + len, sub)) != NULL) {
            while (p < r) { *q++ = *p++; }
        }
        while ((*q++ = *p++) != '\0') { continue; }
    }

    return str;
}
