
#include "server_filesystem.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int server_fs_create(struct server_filesystem *fs, char *rootDirectory, 
	char *logFile)
{
	/*
	 * Check that the rootDirectory is a valid folder
	 * Code from:
	 *  http://stackoverflow.com/questions/1036625/differentiate-
	 *   between-a-unix-directory-and-file-in-c
	 */
	struct stat st_buf;
	int status = stat(rootDirectory, &st_buf);
	if (status == -1 || !S_ISDIR(st_buf.st_mode)) {
		return FS_BADROOT;
	} 

	/* 
	 * Check that the logFile is a valid file
	 * either it must not exist, so that we can make it, or
	 * it must not be a folder (That is, it is a file, or a device such as
	 * /dev/null). 
	 */
	status = stat(logFile, &st_buf);
	if (status == 0 && S_ISDIR(st_buf.st_mode)) {
		return FS_BADLOG;
	}

	/* Open the log file for writing or create it if it doesn't exist */
	if ((fs->log_fd = open(logFile, O_CREAT | O_APPEND | O_WRONLY, 0777)) == -1) {
		return FS_BADLOG;
	}

	/* Done setup, all good */
	fs->root_dir = rootDirectory;
	fs->log_dir = logFile;
	return FS_OKAY;
}

int server_fs_open(struct server_filesystem *fs, char *path) {
	/* 
	 * Check for /../blah shenanigans in the path: Don't let the user
	 * use ..s to ascend up past the root directory and access file that
	 * we don't want to be serving. 
	 */
	int ptr;
	size_t pathlen = strlen(path);
	int depth = -1;
	for (ptr = 0; ptr < pathlen; ++ptr) {
		/* 
		 * Fix backslashes while we're at it, most we servers will
		 * accept backslashes as though they were slashes. 
		 */
		if (path[ptr] == '\\')
			path[ptr] = '/';

		/* Check for ascend / descend */
		if (path[ptr] == '/') {
			/* decend */
			++depth;
			printf("Descend -> %d\n", depth);
		} else if (path[ptr] == '.') {
			if ((ptr+1 < pathlen) && (path[ptr+1] == '.')) {
				/* ascend */
				--depth;
				printf("Ascend -> %d\n", depth);

				if (depth < 0) {
					/* Ascended up past the root dir!! Not allowed */
					return FS_EFILE_FORBIDDEN;
				}
			}
		}
	}

	/* 
	 * Concatenate the path with the server root directory into a single 
	 * path entry.
	 */
	size_t totallen = strlen(fs->root_dir) + strlen(path);
	char *fullpath = malloc(totallen + 1);
	strcpy(fullpath, fs->root_dir);
	strcpy(fullpath + strlen(fs->root_dir), path);
	fullpath[totallen] = '\0';

	/* Try to open the file */
	printf("Opening path: `%s`\n", fullpath);
	int fd = open(fullpath, O_RDONLY);
	free(fullpath);
	if (fd == -1)
		return FS_EFILE_NOTFOUND;
	else
		return fd;
}

void server_fs_destroy(struct server_filesystem *fs) {
	close(fs->log_fd);
}

void server_fs_log(struct server_filesystem *fs, char* format, ...) {
	flock(fs->log_fd, LOCK_EX);
	va_list arglist;
	va_start(arglist, format);
	vdprintf(fs->log_fd, format, arglist);
	va_end(arglist);
	flock(fs->log_fd, LOCK_UN);
}