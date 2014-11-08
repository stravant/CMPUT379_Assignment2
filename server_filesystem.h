
#ifndef SERVER_FILESYSTEM_H_
#define SERVER_FILESYSTEM_H_


#include <pthread.h>


/* fs_open status codes */
#define FS_OKAY             0
#define FS_BADROOT         -4 /* Bad root directory to serve from */
#define FS_BADLOG          -5 /* Bad log directory to log to */
#define FS_INITERROR       -6 /* Error durring initialization */
#define FS_EFILE_NOTFOUND  -1 /* File does not exist */
#define FS_EFILE_FORBIDDEN -2 /* Use of /.. or other forbidden constructs */
#define FS_EFILE_INTERNAL  -3 /* Internal problem accessing the file */

/*
 * A structure representing an opened server file system.
 */
struct server_filesystem {
	char *root_dir;
	char *log_dir;
	int log_fd;
	int useflock;
	pthread_mutex_t log_pthread_lock;
};


/*
 * Initialize a server file system to read from a given root directory, and
 * log to another given directory.
 * If useflock is 1, then flock is used to lock the log file
 * If useflock is 0, then a pthread_mutex is used to lock the log file
 * Returns: FS_OKAY on success, or one of FS_BADROOT or FS_BADLOG on failure.
 */
int server_fs_create(struct server_filesystem *fs, char *rootDirectory, 
	char *logDirectory, int useflock);


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
 * Should only be used on a server_filesystem that was successfully
 * server_fs_create'd.
 */
void server_fs_destroy(struct server_filesystem *fs);


/*
 * Append text to the log file for a given server_filesystem
 */
void server_fs_log(struct server_filesystem *fs, char* format, ...);


#endif