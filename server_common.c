
#include "server_common.h"

#include <memory.h>
#include <stdio.h>

#define MAX_REQUESTS 3

int server_create(struct server_state *state, int port) {
	int err;
	int reuseOpt;

	/* Create a new internet socket */
	if ((state->socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { /* 0 -> IP */
		printf("socket() error\n");
		return SERVER_ERROR;
	}

	/* Change the socket to reuse connections mode, so that we don't run
	 * lack of connections problems when restarting the server.
	 */
	reuseOpt = 1; /* 1 -> TRUE */
	setsockopt(state->socketfd, 
		SOL_SOCKET, SO_REUSEADDR, 
		&reuseOpt, sizeof(reuseOpt));

	/* Bind the socket to the port */
	memset(&state->addr, 0x0, sizeof(state->addr));
	state->addr.sin_family = AF_INET;
	state->addr.sin_port = htons(port);
	state->addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if ((err = bind(state->socketfd, (struct sockaddr*)&state->addr, 
		sizeof(state->addr)))) 
	{
		printf("bind() error: %s\n", strerror(err));
		return SERVER_ERROR;
	}

	/* Start listening */
	if (listen(state->socketfd, MAX_REQUESTS)) {
		printf("listen() error\n");
		return SERVER_ERROR;
	}

	return SERVER_OKAY;
}
 
int server_listen(struct server_state *state) {
	/* Set up listener info */
	struct sockaddr_in connection_addr;
	socklen_t connection_len = sizeof(connection_addr);
	memset(&connection_addr, 0x0, connection_len);

	/* Wait for an incomming connection */
	int connectionfd = accept(state->socketfd, 
		(struct sockaddr*)&connection_addr, &connection_len);

	/* Return the connection if sucessfull */
	if (connectionfd < 0) {
		return SERVER_ERROR;
	} else {
		return connectionfd;
	}
}

void server_destroy(struct server_state *state) {
	/* Close the listener */
	close(state->socketfd);
}