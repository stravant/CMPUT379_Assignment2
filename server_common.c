
#include "server_common.h"

#include <memory.h>

#define MAX_REQUESTS 3

int server_create(struct server_state *state, int port) {
	/* Create a new internet socket */
	if ((state->socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { /* 0 -> IP */
		return SERVER_ERROR;
	}

	/* Bind the socket to the port */
	memset(&state->addr, 0x0, sizeof(state->addr));
	state->addr.sin_family = AF_INET;
	state->addr.sin_port = htons(port);
	state->addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(state->socketfd, (struct sockaddr*)&state->addr, 
		sizeof(state->addr))) 
	{
		return SERVER_ERROR;
	}

	/* Start listening */
	if (listen(state->socketfd, MAX_REQUESTS)) {
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