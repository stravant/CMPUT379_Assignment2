
#include "server_filesystem.h"

#include <stdarg.h>
#include <stdio.h>

int server_fs_create(struct server_filesystem *fs, char *rootDirectory, 
	char *logDirectory)
{
	return FS_OKAY;
}

int server_fs_open(struct server_filesystem *fs, char *path) {
	return FS_EFILE_NOTFOUND;
}

void server_fs_destroy(struct server_filesystem *fs) {

}

void server_fs_log(struct server_filesystem *fs, char* format, ...) {
	va_list arglist;
	va_start(arglist, format);
	vprintf(format, arglist);
	va_end(arglist);
}