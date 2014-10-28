
#include "server_http.h"

#include <string.h>
#include <sys/socket.h>

const char *response =
	"HTTP/1.1 200 OK\n"
	"\n"
	"Hello, World!\n";

void handle_http_request(struct server_filesystem *fs, int connection_fd) {
	send(connection_fd, response, strlen(response), 0);
}