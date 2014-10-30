
#include "server_http.h"

#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define BUFFER_INITIAL 20

#define RECV_STATE_READY  0
#define RECV_STATE_TEXT   1
#define RECV_STATE_EOF    3

struct http_header {
	char *label_ptr;
	size_t label_length;
	
};

void handle_http_request(struct server_filesystem *fs, int connection_fd) {
	size_t buffer_capacity = BUFFER_INITIAL;
	size_t buffer_size = 0;
	char *buffer = malloc(buffer_capacity + 1);
	size_t buffer_index = 0;

	int lex_state = RECV_STATE_READY;

	int line_count = 0;

	/* Read the request headers */
	for (;;) {
		/* Read a new chunk into the buffer */
		int received = recv(connection_fd, 
			buffer + buffer_size,
			buffer_capacity - buffer_size,
			0);
		buffer_size += received;

		/* Lex to see if we reached the end of the packet. EOF condition:
		 * \r\n or \n alone on a line
		 */
		while (buffer_index < buffer_size) {
			char c = buffer[buffer_index];
			if (c == '\r') {
				/* Remain in the same state */
			} else if (c == '\n') {
				if (lex_state == RECV_STATE_READY) {
					/* empty line, break out */
					lex_state = RECV_STATE_EOF;
					break;
				} else {
					/* New line, become ready again */
					lex_state = RECV_STATE_READY;
				}
				++line_count;
			} else {
				/* Other text -> text state */
				lex_state = RECV_STATE_TEXT;
			}

			/* to next character */
			++buffer_index;
		}

		/* Now, if we are at EOF, break out. Otherwise, possibly resize the 
		 * buffer, and go on to read more chunks.
		 */
		if (lex_state == RECV_STATE_EOF) {
			break;
		} else {
			/* Double on: buffer is more than half full */
			if ((buffer_capacity - buffer_size) < buffer_capacity/2) {
				buffer_capacity *= 2;
				buffer = realloc(buffer, buffer_capacity + 1);
			}
		}
	}

	/* Note: Safe since we allocate one additional byte on top of the
	 * capacity that we are working with when maniuplating the buffer, as
	 * space to put this null terminator at. */
	buffer[buffer_index] = '\0';

	/* Parse the request headers */


	/* Log the request */
	/* todo: */


	/* debug print request */
	printf("Request<%d>: %s\n", buffer_index, buffer);


	/*server_fs_log(fs, ">REQUEST<%d>: %s\n", buffer_index, buffer);*/
	const char *response =
		"HTTP/1.1 200 OK\n"
		"\n"
		"Hello, World!\n";

	/* Write out a response */
	write(connection_fd, response, strlen(response));

	/* Free the buffer we used */
	/* free(buffer); */
}