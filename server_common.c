
#include "server_common.h"

#include <memory.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
 
int server_listen(struct server_state *state, char **addr) {
	struct sockaddr_in connection_addr;
	socklen_t connection_len;
	int connectionfd;
	struct timeval recv_timeout;
	struct timeval send_timeout;

	/* Set up listener info (zero it) */
	connection_len = sizeof(connection_addr);
	memset(&connection_addr, 0x0, connection_len);

	/* Wait for an incomming connection */
	connectionfd = accept(state->socketfd, 
		(struct sockaddr*)&connection_addr, &connection_len);

	/* Check that the connection succeeded */
	if (connectionfd < 0) {
		return SERVER_ERROR;
	}

	/* 
	 * Configure the read / write timeout on a socket
	 * Read Timeout: 10 seconds
	 * Write Timeout: 10 seconds
	 * Code From: 
	 *   http://stackoverflow.com/questions/2876024/linux-is-there-
	 *     a-read-or-recv-from-socket-with-timeout
	 */
	recv_timeout.tv_sec = 10;
	recv_timeout.tv_usec = 0;
	if (-1 == setsockopt(connectionfd, SOL_SOCKET, SO_RCVTIMEO, 
		(char*)&recv_timeout, sizeof(struct timeval))) 
	{
		/* Failed to configure, close connection and return failure */
		close(connectionfd);
		return SERVER_ERROR;
	}

	send_timeout.tv_sec = 10;
	send_timeout.tv_usec = 0;
	if (-1 == setsockopt(connectionfd, SOL_SOCKET, SO_SNDTIMEO,
		(char*)&send_timeout, sizeof(struct timeval)))
	{
		/* Failed to configure, close connection and return failure */
		close(connectionfd);
		return SERVER_ERROR;
	}

	/* Get the source IP as a string. */
	*addr = inet_ntoa(connection_addr.sin_addr);

	/* Successful, return the connection */
	return connectionfd;
}

void server_destroy(struct server_state *state) {
	/* Close the listener */
	close(state->socketfd);
}