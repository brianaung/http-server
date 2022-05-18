/* given a http request buffer, extract out the path name */
char* getGetReqPath(const char* buffer);

/* from the program argument, get the absolute root web directory */
char* getWebRootDir(char* input_path);

/* add 2 given strings together and return a new string */
char* addStrings(char* str1, char* str2);

/* remove substring from a given string */
char* removeSubStr(char* str, const char* sub);
