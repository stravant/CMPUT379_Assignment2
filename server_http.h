
#ifndef SERVER_HTTP_H_
#define SERVER_HTTP_H_

#include "server_filesystem.h"

/*
 * Handle a request on a given connection, as a file descriptor, using a given 
 * server_filesystem to serve from.
 */
void handle_http_request(struct server_filesystem *fs, int connection_fd);

#endif