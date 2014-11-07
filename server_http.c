
#include "server_http.h"

#include "http_request.h"

#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>

/* How big the request buffer is initially */
#define BUFFER_INITIAL 1024*2 /* 2 KB */


/* States for the incremental request parser to be in */
#define RECV_STATE_READY  0
#define RECV_STATE_TEXT   1
#define RECV_STATE_EOF    3


/* Private function forwards declarations */
void format_date(char *buffer, size_t len);
void http_response_const(struct server_filesystem *fs, int connection_fd, 
	const char* resp[2]);
void http_response_log(struct server_filesystem *fs, char *addr, 
	struct http_method *method, char *date, char *response);
void http_response_dispatch(struct server_filesystem *fs, int connection_fd,
	char *addr, struct http_method *method);


/*
 * Format a date in the format that we want to use for HTTP responses from
 * this server. Formats into a buffer with a given length provided as 
 * arguments.
 */
void format_date(char *buffer, size_t len) {
	/*
	 * Code taken from example on:
	 *  http://linux.die.net/man/3/strftime
	 * And modified
	 */
	time_t t;
	struct tm *tmp;
	t = time(NULL);
	tmp = gmtime(&t);

	/* Format into our date format */
	if (0 == strftime(buffer, len, "%a %d %b %Y %T GMT", tmp)) {
		/* 
		 * Failed, just write a null terminator into the buffer to
		 * make it a valid C-string.
		 */
		if (len > 0) {
			buffer[0] = '\0';
		}
	}
}


/* Bad Request */
const char *response_400[2] = {
	"HTTP/1.1 400 Bad Request\n"
	"Date: %s\n"
	"Content‐Type: text/html\n"
	"Content‐Length: %d\n"
	"\n"
	,
	"<html><body>\n"
	"<h2>Malformed Request</h2>\n"
	"Your browser sent a request I could not understand.\n"
	"</body></html>"
};

/* Forbidden */
const char *response_403[2] = {
	"HTTP/1.1 403 Forbidden\n"
	"Date: %s\n"
	"Content-Type: text/html\n"
	"Content-Length: %d\n"
	"\n"
	,
	"<html><body>\n"
	"<h2>Permission Denied</h2>\n"
	"You asked for a document you are not permitted to see. It sucks to be you.\n" 
	"</body></html>"
};

/* Not Found */
const char *response_404[2] = {
	"HTTP/1.1 404 Not Found\n"
	"Date: %s\n"
	"Content‐Type: text/html\n"
	"Content‐Length: %d\n"
	"\n"
	,
	"<html><body>\n"
	"<h2>Document not found</h2>\n"
	"You asked for a document that doesn't exist. That is so sad.\n"
	"</body></html>"
};

/* Method Not Allowed */
const char *response_405[2] = {
	"HTTP/1.1 405 Method Not Allowed\n"
	"Date: %s\n"
	"Content-Type: text/html\n"
	"Content-Length: %d\n"
	"\n"
	,
	"<html><body>\n"
	"<h2>Oops. That Didn't work</h2>\n"
	"I had some sort of problem dealing with your request. Sorry, I'm lame.\n"
	"</body></html>"
};

/* Internal Server Error */
const char *response_500[2] = {
	"HTTP/1.1 500 Internal Server Error\n"
	"Date: %s\n"
	"Content-Type: text/html\n"
	"Content-Length: %d\n"
	"\n"
	,
	"<html><body>\n"
	"<h2>Oops. That Didn't work</h2>\n"
	"I had some sort of problem dealing with your request. Sorry, I'm lame.\n"
	"</body></html>"
};


/*
 * Serve a response which has fixed predefined contents other than the 
 * date in it's header.
 * Takes any |resp| from the above defined responses_<status>s.
 */
void http_response_const(struct server_filesystem *fs, int connection_fd, 
	const char* resp[2])
{
	char date[200];
	size_t length;

	/* Get date */
	format_date(date, 200);

	/* Length */
	length = strlen(resp[1]);

	/* Write the response */
	dprintf(connection_fd, resp[0], date, length);
	write(connection_fd, resp[1], strlen(resp[1]));
}

/* Okay header fragment */
const char *response_200 =
	"HTTP/1.1 200 OK\n"
	"Date: %s\n"
	"Content-Type: text/html\n"
	"Content-Length: %d\n"
	"\n";


/*
 * Write to the log file in the log format that we want 
 */
void http_response_log(struct server_filesystem *fs, char *addr, 
	struct http_method *method, char *date, char *response)
{
	server_fs_log(fs, "%s\t%s\t%.*s %.*s %.*s\t%s\n",
		date,
		addr,
		method->method.length, method->method.ptr,
		method->url.length, method->url.ptr,
		method->version.length, method->version.ptr,
		response);
}


/* 
 * Decide what kind of response a given method needs, and dispatch that
 * response to the client via that method.
 */
void http_response_dispatch(struct server_filesystem *fs, int connection_fd,
	char *addr, struct http_method *method) 
{
	char *filename;
	char date[200];
	int fd;
	ssize_t fsize;
	ssize_t status;
	char dataBuffer[1024];
	size_t total_written;
	ssize_t len;

	/* Null terminate the file to get name */
	filename = malloc(method->url.length + 1);
	memcpy(filename, method->url.ptr, method->url.length);
	filename[method->url.length] = '\0';

	/* Get date */
	format_date(date, 200);

	/* Check the method */
	if (strncmp("GET", method->method.ptr, method->method.length)) {
		/* Request is not a get, issue 405 bad method */
		http_response_const(fs, connection_fd, response_405);
		http_response_log(fs, addr, method, date, "403 Method Not Allowed");
		return;
	}

	/* Path must start with a slash */
	if (filename[0] != '/') {
		http_response_const(fs, connection_fd, response_400);
		http_response_log(fs, addr, method, date, "400 Bad Request");	
	}

	/* Open file */
	fd = server_fs_open(fs, filename);
	free(filename);
	if (fd < 0) {
		/* Problem opening the file for response */
		if (fd == FS_EFILE_FORBIDDEN) {
			http_response_const(fs, connection_fd, response_403);
			http_response_log(fs, addr, method, date, "403 Forbidden");
		} else if (fd == FS_EFILE_NOTFOUND) {
			http_response_const(fs, connection_fd, response_404);
			http_response_log(fs, addr, method, date, "404 Not Found");
		} else {
			http_response_const(fs, connection_fd, response_500);
			http_response_log(fs, addr, method, date, 
				"500 Internal Server Error");
		}
		return;
	}

	/* The file exists, try to respond with the contents */

	/* Get the file size (seek to end and back to start) */
	fsize = lseek(fd, 0, SEEK_END);
	status = lseek(fd, 0, SEEK_SET);

	/* Failed to get file length? */
	if (fsize < 0 || status < 0) {
		http_response_const(fs, connection_fd, response_500);
		http_response_log(fs, addr, method, date, "500 Internal Server Error");
		return;
	}

	/* Ready to send contents, emit a 200 OK response type header */

	/* Write headers */
	if (dprintf(connection_fd, response_200, date, fsize) < 0) {
		/* At this point, we may have sent some of the header already, so
		 * the only option is to stop sending and fail; we can't start a 
		 * 500 Internal Server Error at this point.
		 */
		http_response_log(fs, addr, method, date, 
			"Connection unexpectedly terminated while "
			"sending response header.");
		return;
	}

	/* Write contents in 1KB chunks */
	total_written = 0;

	/* While we read data, and didn't encounter an error */
	while ((len = read(fd, dataBuffer, 1024)) > 0) {
		ssize_t written;

		/* Try to write out the data chunk that we read */
		written = write(connection_fd, dataBuffer, len);
		if (written < 0) {
			/* Error writing occurred */
			break;
		} else if (written < len) {
			/* 
			 * Couldn't write all of the data, connection was probably
			 * closed by the client, so break out.
			 */
			total_written += written;
			break;
		} else {
			/* Write succeeded */
			total_written += written;
		}
	}

	/* Log how the 200 OK response went (how much of the data we
	 * managed to send out of the total file size.
	 */
	server_fs_log(fs, "%s\t%s\t%.*s %.*s %.*s\t200 OK %d/%d\n",
		date,
		addr,
		method->method.length, method->method.ptr,
		method->url.length, method->url.ptr,
		method->version.length, method->version.ptr,
		total_written,
		fsize);	
}


void handle_http_request(struct server_filesystem *fs, int connection_fd,
	char *addr) 
{
	size_t buffer_capacity;
	size_t buffer_size;
	char *buffer;
	size_t buffer_index;
	size_t line_start_index;
	int lex_state;
	int line_count;
	struct http_method method;
	struct http_header *header_list;
	char *request_content;

	/* Set up the growable buffer that we read the request into */
	buffer_capacity = BUFFER_INITIAL;
	buffer_size = 0;
	buffer = malloc(buffer_capacity + 1); /* +1 -> room for trailing '\0' */
	buffer_index = 0;
	line_start_index = 0;

	/* Set the initial lex state */
	lex_state = RECV_STATE_READY;

	/* Current line count */
	line_count = 0;

	/* The http method */
	memset(&method, 0x0, sizeof(method));

	/* A linked list of http_header-s */
	header_list = NULL;

	/* Storage for the request content if any */
	request_content = NULL;

	/* Read the request headers */
	for (;;) {
		ssize_t received;

		/* Read a new chunk into the buffer */
		received = recv(connection_fd, 
			buffer + buffer_size,
			buffer_capacity - buffer_size,
			0);

		/* Error: recv failed */
		if (received == -1) {
			goto badrequest;
		}

		/* Add the content */
		buffer_size += received;

		/* Lex to see if we reached the end of the packet. EOF condition:
		 * \r\n or \n alone on a line
		 */
		while (buffer_index < buffer_size) {
			char c;

			/* Get the current character */
			c = buffer[buffer_index];

			/* Lex the character given the current lex state */
			if (c == '\r') {
				/* Remain in the same state */
			} else if (c == '\n') {
				if (lex_state == RECV_STATE_READY) {
					/* empty line, break out, the request is complete */
					lex_state = RECV_STATE_EOF;
					break;
				} else {
					/* New line, become ready again */
					lex_state = RECV_STATE_READY;
				}

				/* Complete line has been read in */
				++line_count;

				/* Process this line */
				if (line_count == 1) {
					/* Line number 1 is the request method */
					int result;

					/* Process method */
					result = parse_method(&method, 
						buffer + line_start_index,
						buffer_index - line_start_index + 1);

					/* Failed to process method */
					if (result == 0) {
						/* Error malformed method */
						goto badrequest;
					}
				} else {
					/* Other lines are request headers */
					struct http_header header;
					int result;

					/* Process a header */
					result = parse_header(&header,
						buffer + line_start_index,
						buffer_index - line_start_index + 1);

					/* Failed to process header */
					if (result == 0)
						/* Error malformed header */
						goto badrequest;
					else {
						struct http_header *node;
						/* 
						 * Otherwise, allocate a linked list node for the
						 * header, and insert it into the header list. 
						 */
						node = malloc(sizeof(struct http_header));
						memcpy(node, &header, sizeof(struct http_header));
						node->prev = header_list;
						header_list = node;
					}
				}

				/* Line processed, reset the startofline tag to current ptr */
				line_start_index = (buffer_index + 1);
			} else {
				/* Other text -> "line has text" state */
				lex_state = RECV_STATE_TEXT;
			}

			/* Character processed, to next character */
			++buffer_index;
		}

		/* 
		 * Now, if we got the complete request, break out. Otherwise, possibly 
		 * resize the buffer, and go on to read more chunks.
		 */
		if (lex_state == RECV_STATE_EOF) {
			break;
		} else {
			ptrdiff_t delta;
			struct http_header *curHeader;

			/* Double the buffer when it is more than half full */
			if ((buffer_capacity - buffer_size) < buffer_capacity/2) {
				char *oldBuffer;

				/* Double teh buffer size via realloc */
				buffer_capacity *= 2;
				oldBuffer = buffer;
				buffer = realloc(buffer, buffer_capacity + 1);
				delta = buffer - oldBuffer;

				/* 
				 * Patch the method and/or headers's pointers into the buffer
				 * that we realocated.
				 */
				if (method.method.ptr != 0) {
					method.method.ptr  += delta;
					method.url.ptr     += delta;
					method.version.ptr += delta;
				}
				for (curHeader = header_list; curHeader; 
					curHeader = curHeader->prev) 
				{
					curHeader->label.ptr += delta;
					curHeader->value.ptr += delta;
				}
			}
		}
	}

	/* Note: Safe since we allocate one additional byte on top of the
	 * capacity that we are working with when maniuplating the buffer, as
	 * space to put this null terminator at. */
	buffer[buffer_index] = '\0';

	/* 
	 * If there is a request body (Content-Length header exists), we need to 
	 * read that in. 
	 */
	struct http_header *curheader;
	for (curheader = header_list; curheader; curheader = curheader->prev) {
		if (!strncmp(curheader->label.ptr, "Content-Length", 
			curheader->label.length)) 
		{
			size_t length;

			/* limit to 100MB */
			if (header_value_as_size_t(curheader, &length, 100*1024*1024)) {
				size_t data_read;
				ssize_t received;

				/* Read in the data */
				data_read = 0;
				request_content = malloc(length);
				while (data_read < length) {
					received = recv(connection_fd, 
						request_content + data_read,
						length - data_read,
						0);

					/* Error: Failed to recieve body */
					if (received < 0) {
						goto badrequest;
					}

					data_read += received;
				}
			} else {
				/* Error too long */
				goto badrequest;
			}

			/* There can only be one Content-Length */
			break;
		}
	}

	/* Serve the response */
	http_response_dispatch(fs, connection_fd, addr, &method);

	/* Completed successfully, skip bad request block */
	goto cleanup;


badrequest: 
	/* 
	 * Block that writes out a bad request response to the header 
	 * This is done as a goto-block rather than a function since it can be
	 * reached by breaking out of deeply nested logic above.
	 */
	{
		char date[200];

		/* Get date */
		format_date(date, 200);

		/* Output response */
		http_response_const(fs, connection_fd, response_400);
		http_response_log(fs, addr, &method, date, 
			"400 Bad Request");
	}


cleanup:
	/* 
	 * Main cleanup for the buffers & structures allocated during handling 
	 * of the request.
	 */
	{
		struct http_header *header;

		/* Free the request content */
		if (request_content)
			free(request_content);

		/* Free the header linked list */
		for (header = header_list; header;) {
			struct http_header *prev = header->prev;
			free(header);
			header = prev;
		}

		/* Free the buffer we used */
		free(buffer);
	}
}