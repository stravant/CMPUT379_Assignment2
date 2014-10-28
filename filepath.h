
#ifndef FILEPATH_H_
#define FILEPATH_H_

#define FILEPATH_INVALID   1
#define FILEPATH_FILE      2
#define FILEPATH_DIRECTORY 3

/*
 * Inspect a given file path. The return value is a status code
 * That either specifies that the path is invalid, or if it is valid,
 * whether it is a file or a folder at the path location.
 */
int filepath_info(char *buffer, size_t length);

#endif

