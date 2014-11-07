
#ifndef SERVER_COMMON_H_
#define SERVER_COMMON_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define SERVER_OKAY   0
#define SERVER_ERROR -1

struct server_state {
	int socketfd;
	struct sockaddr_in addr;
};

/*
 * Initialize a server state to be listening on the given port.
 * Parameters:
 *   state: The server state to initialize
 *   port: The port to listen on
 * Returns:
 *   A status code representing whether the operation was sucessfull
 */
int server_create(struct server_state *state, int port);

/*
 * Wait for and accept an incomming connection.
 * Parameters:
 *   state: The server state to listen on
 *   addr:  Pointer to the address that the connection was accepted from
 * Returns:
 *   (positive) A file descriptor representing the opened connection.
 *   (negative) An error code from above
 */ 
int server_listen(struct server_state *state, char **addr);

/*
 * Destroy a server
 */
void server_destroy(struct server_state *state);

#endif