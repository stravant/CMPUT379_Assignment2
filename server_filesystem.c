
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

/* Private function forward declarations */
int check_path(char *path);

/* 
 * Check that a path does not "ascend" upwards past it's root using `..`
 * expressions. That is, the resulting directory for the path should not be
 * higher up in the hierarchy than the base directory for the path.
 * Also changes backslashes -> forward slashes in the path
 * Returns 0 -> path does ascend too far
 *         1 -> path is okay
 */
int check_path(char *path) {
	int ptr;
	size_t pathlen;
	int depth;

	pathlen = strlen(path);
	depth = -1;
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
		} else if (path[ptr] == '.') {
			if ((ptr+1 < pathlen) && (path[ptr+1] == '.')) {
				/* ascend */
				--depth;

				if (depth < 0) {
					/* Ascended up past the root dir!! Not allowed */
					return 0;
				}
			}
		}
	}	

	return 1;
}


int server_fs_create(struct server_filesystem *fs, char *rootDirectory, 
	char *logFile, int useflock)
{
	struct stat st_buf;
	int status;

	/*
	 * Check that the rootDirectory is a valid folder
	 * Code from:
	 *  http://stackoverflow.com/questions/1036625/differentiate-
	 *   between-a-unix-directory-and-file-in-c
	 */
	status = stat(rootDirectory, &st_buf);
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

	/* 
	 * If we are using pthread locks on the log file, then init the pthread
	 * lock.
	 */
	fs->useflock = useflock;
	if (useflock == 0) {
		if (0 != pthread_mutex_init(&fs->log_pthread_lock, NULL)) {
			return FS_INITERROR;
		}
	}

	/* Done setup, all good */
	fs->root_dir = rootDirectory;
	fs->log_dir = logFile;
	return FS_OKAY;
}


int server_fs_open(struct server_filesystem *fs, char *path) {
	size_t totallen;
	char *fullpath;
	struct stat st_buf;
	int status;
	int fd;

	/* 
	 * Check for /../blah shenanigans in the path: Don't let the user
	 * use ..s to ascend up past the root directory and access file that
	 * we don't want to be serving.
	 */
	if (!check_path(path)) {
		return FS_EFILE_FORBIDDEN;
	}

	/* 
	 * Concatenate the path with the server root directory into a single 
	 * path entry.
	 */
	totallen = strlen(fs->root_dir) + strlen(path);
	fullpath = malloc(totallen + 1);
	strcpy(fullpath, fs->root_dir);
	strcpy(fullpath + strlen(fs->root_dir), path);
	fullpath[totallen] = '\0';

	/* Check that the file is a file and not a directory */
	status = stat(fullpath, &st_buf);
	if (status == -1) {
		return FS_EFILE_NOTFOUND;
	}
	if (!S_ISREG(st_buf.st_mode)) {
		return FS_EFILE_FORBIDDEN;
	}

	/* Try to open the file */
	fd = open(fullpath, O_RDONLY);
	free(fullpath);

	/* Return translated status */
	if (fd == -1)
		return FS_EFILE_NOTFOUND;
	else
		return fd;
}


void server_fs_destroy(struct server_filesystem *fs) {
	/* Close the log file */
	close(fs->log_fd);

	/* Maybe destroy the lock */
	if (!fs->useflock) {
		pthread_mutex_destroy(&fs->log_pthread_lock);
	}
}


void server_fs_log(struct server_filesystem *fs, char* format, ...) {

	/* Log the log file */
	if (fs->useflock)
		flock(fs->log_fd, LOCK_EX);
	else
		pthread_mutex_lock(&fs->log_pthread_lock);

	/* Print out the variable arguments */
	/* 
	 * Note: va_list can't be at start of function, it must be declared
	 * immediately before it's usage.
	 */
	va_list arglist; 
	va_start(arglist, format);
	vdprintf(fs->log_fd, format, arglist);
	va_end(arglist);

	/* Unlock the log file */
	if (fs->useflock)
		flock(fs->log_fd, LOCK_UN);
	else
		pthread_mutex_unlock(&fs->log_pthread_lock);
}