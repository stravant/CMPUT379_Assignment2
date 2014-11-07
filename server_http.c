
#include "server_http.h"

#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <time.h>

#define BUFFER_INITIAL 20

#define RECV_STATE_READY  0
#define RECV_STATE_TEXT   1
#define RECV_STATE_EOF    3

struct str_buffer_ptr {
	char *ptr;
	size_t length;
};

struct http_header {
	struct str_buffer_ptr label;
	struct str_buffer_ptr value;
	struct http_header *prev;
};

struct http_method {
	struct str_buffer_ptr method;
	struct str_buffer_ptr url;
	struct str_buffer_ptr version;
};

int parse_method(struct http_method *method,
	char *source_buffer, size_t source_length)
{
	/* Set up parsing variables */
	char *after = source_buffer + source_length;
	char *ptr = source_buffer;
	char *start = ptr;

	/* Get method */
	while ((ptr < after) && (*ptr != ' '))
		++ptr;
	method->method.ptr = start;
	method->method.length = (ptr - start);

	/* Next block */
	while ((ptr < after) && (*ptr == ' '))
		++ptr;
	start = ptr;

	/* Get url */
	while ((ptr < after) && (*ptr != ' '))
		++ptr;
	method->url.ptr = start;
	method->url.length = (ptr - start);

	/* Next block */
	while ((ptr < after) && (*ptr == ' '))
		++ptr;
	start = ptr;

	/* Get version */
	while ((ptr < after) && (*ptr != '\n') && (*ptr != '\r'))
		++ptr;
	method->version.ptr = start;
	method->version.length = (ptr - start);

	/* Get the \n and \r */
	while ((ptr < after) && ((*ptr == '\n') || (*ptr == '\r')))
		++ptr;

	return ((method->method.length  > 0) && 
	        (method->url.length     > 0) &&
	        (method->version.length > 0));
}

int parse_header(struct http_header *header, 
	char *source_buffer, size_t source_length) 
{
	/* Set up parsing variables */
	char *after = source_buffer + source_length;
	char *ptr = source_buffer;
	char *start = ptr;

	/* Get header label */
	while ((ptr < after) && (*ptr != ':'))
		++ptr;
	header->label.ptr = start;
	header->label.length = (ptr - start);

	/* Get the : separator if found */
	if ((header->label.length > 0) && (ptr < after) && (*ptr == ':')) {
		/* consume : and space */
		ptr += 2;

		/* Get the header value */
		start = ptr;
		while ((ptr < after) && (*ptr != '\n') && (*ptr != '\r'))
			++ptr;
		header->value.ptr = start;
		header->value.length = (ptr - start);

		/* Consume the \n and \r */
		if ((ptr < after) && ((*ptr == '\n') || (*ptr == '\r')))
			++ptr;

		/* Return success, the value may have a zero length */
		return 1;
	} else {
		/* No separator, bad header */
		return 0;
	}
}

/* Returns -> Boolean success */
int get_value_size_t(struct http_header *header, size_t *len, size_t max_len) {
	/* Must have a value */
	if (header->value.length == 0)
		return 0;

	/* Parse value */
	long n = 1;
	size_t value = 0;
	int i;
	for (i = header->value.length-1; i >= 0; --i) {
		char c = header->value.ptr[i];
		if (c < '0' || c > '9')
			return 0;
		value += (c - '0')*n;
		n *= 10;
	}

	/* Check value */
	if (value > max_len)
		return 0;

	/* Return */
	*len = value;
	return 1;
}

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
	strftime(buffer, len, "%a %d %b %Y %T GMT", tmp);
}

/* bad request */
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

/* not found */
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

/* forbidden */
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

/* internal server error */
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

/* Method not allowed */
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

/* any of the above constant responses */
void http_response_const(struct server_filesystem *fs, int connection_fd, 
	const char* resp[2])
{
	/* Get date */
	char date[200];
	format_date(date, 200);

	/* Length */
	size_t length = strlen(resp[1]);

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

/* Decide what kind of response a given method needs */
void http_response_dispatch(struct server_filesystem *fs, int connection_fd,
	char *addr, struct http_method *method) 
{
	/* Null terminate the file to get name */
	char *filename = malloc(method->url.length + 1);
	memcpy(filename, method->url.ptr, method->url.length);
	filename[method->url.length] = '\0';

	/* Get date */
	char date[200];
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
	int fd = server_fs_open(fs, filename);
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

	/* The file exists, do a 200 OK response */

	/* Get the file size (seek to end and back to start) */
	size_t fsize = lseek(fd, 0, SEEK_END);
	size_t status = lseek(fd, 0, SEEK_SET);

	/* Failed to get file length? */
	if (fsize < 0 || status < 0) {
		http_response_const(fs, connection_fd, response_500);
		http_response_log(fs, addr, method, date, "500 Internal Server Error");
		return;
	}

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

	/* Write contents in chunks */
	char dataBuffer[1024];
	size_t total_written = 0;
	ssize_t len;
	while ((len = read(fd, dataBuffer, 1024))) {
		if (len < 0) {
			/* An error ocurred reading file */
			break;
		}
		ssize_t written = write(connection_fd, dataBuffer, len);
		if (written < 0) {
			/* Error writing occurred */
			break;
		} else if (written < len) {
			/* Couldn't write all of the data, connection was probably
			 * closed by the client
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
	size_t buffer_capacity = BUFFER_INITIAL;
	size_t buffer_size = 0;
	char *buffer = malloc(buffer_capacity + 1);
	size_t buffer_index = 0;
	size_t line_start_index = 0;

	int lex_state = RECV_STATE_READY;

	/* Current line count */
	int line_count = 0;

	/* The http method */
	struct http_method method;
	memset(&method, 0x0, sizeof(method));

	/* A linked list of http_header-s */
	struct http_header *header_list = 0;

	/* Storage for the request content if any */
	char *request_content = 0;

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

				/* Process this line */
				if (line_count == 1) {
					/* Process method */
					int result = parse_method(&method, 
						buffer + line_start_index,
						buffer_index - line_start_index + 1);

					/* Failed to process method */
					if (result == 0) {
						/* TODO: Error malformed method */
						goto cleanup;
					}
				} else {
					/* Process a header */
					struct http_header header;
					int result = parse_header(&header,
						buffer + line_start_index,
						buffer_index - line_start_index + 1);

					/* Failed to process header */
					if (result == 0)
						/* TODO: Error malformed header */
						goto cleanup;
					else {
						/* Otherwise, add to linked list */
						struct http_header *node = 
							malloc(sizeof(struct http_header));
						memcpy(node, &header, sizeof(struct http_header));
						node->prev = header_list;
						header_list = node;
					}
				}

				/* Go to the next line */
				line_start_index = (buffer_index + 1);
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
			ptrdiff_t delta;
			if ((buffer_capacity - buffer_size) < buffer_capacity/2) {
				buffer_capacity *= 2;
				char *oldBuffer = buffer;
				buffer = realloc(buffer, buffer_capacity + 1);
				delta = buffer - oldBuffer;
			}

			/* Patch the method and/or headers's pointers into the buffer
			 * that we realocated.
			 */
			if (method.method.ptr != 0) {
				method.method.ptr  += delta;
				method.url.ptr     += delta;
				method.version.ptr += delta;
			}
			struct http_header *curHeader;
			for (curHeader = header_list; curHeader; 
				curHeader = curHeader->prev) 
			{
				curHeader->label.ptr += delta;
				curHeader->value.ptr += delta;
			}
		}
	}

	/* Note: Safe since we allocate one additional byte on top of the
	 * capacity that we are working with when maniuplating the buffer, as
	 * space to put this null terminator at. */
	buffer[buffer_index] = '\0';

	/* If there is a request body (content-length), we need to read that in */
	struct http_header *curheader;
	for (curheader = header_list; curheader; curheader = curheader->prev) {
		if (!strncmp(curheader->label.ptr, "Content-Length", 
			curheader->label.length)) 
		{
			size_t length;
			/* limit to 100MB */
			if (get_value_size_t(curheader, &length, 100*1024*1024)) {
				/* Read in the data */
				size_t data_read = 0;
				request_content = malloc(length);
				while (data_read < length) {
					size_t received = recv(connection_fd, 
						request_content + data_read,
						length - data_read,
						0);
					data_read += received;
				}
			} else {
				/* TODO: Error requst too long */
				goto cleanup;
			}

			/* There can only be one Content-Length */
			break;
		}
	}

	/* Serve the response */
	http_response_dispatch(fs, connection_fd, addr, &method);

cleanup:
	/* Free the request content */
	if (request_content)
		free(request_content);

	/* Free the header linked list */
	struct http_header *header = header_list;
	while (header) {
		struct http_header *prev = header->prev;
		free(header);
		header = prev;
	}

	/* Free the buffer we used */
	free(buffer);
}