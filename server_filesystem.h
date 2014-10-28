
#ifndef SERVER_FILESYSTEM_H_
#define SERVER_FILESYSTEM_H_

/* fs_open status codes */
#define FS_OKAY             0
#define FS_BADROOT         -4 /* Bad root directory to serve from */
#define FS_BADLOG          -5 /* Bad log directory to log to */
#define FS_EFILE_NOTFOUND  -1 /* File does not exist */
#define FS_EFILE_FORBIDDEN -2 /* Use of /.. or other forbidden constructs */
#define FS_EFILE_INTERNAL  -3 /* Internal problem accessing the file */

struct server_filesystem {
	char *root_dir;
	char *log_dir;
	int log_fd;
};

/*
 * Initialize a server file system to read from a given root directory, and
 * log to another given directory.
 * Returns: FS_OKAY on success, or one of FS_BADROOT or FS_BADLOG on failure.
 */
int server_fs_create(struct server_filesystem *fs, char *rootDirectory, 
	char *logDirectory);

/*
 * Open the file with the given path on the server, relative to the root
 * specified. Prevents any accesses to super-root directories through /../.. 
 * and other such shinanegans.
 * Returns: (positive) A file descriptor reference to the file.
 *          (negative) An fs_open status code from above.
 */
int server_fs_open(struct server_filesystem *fs, char *path);

/*
 * Destroy a server_filesystem struct
 */
void server_fs_destroy(struct server_filesystem *fs);

/*
 * Append text to the log file
 */
void server_fs_log(struct server_filesystem *fs, char* message);

#endif