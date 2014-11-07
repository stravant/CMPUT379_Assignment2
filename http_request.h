
#include <stdlib.h>

/* 
 * A non-(null terminated) string which is a pointer into a character
 * buffer, used in buffer parsing routines.
 */
struct str_buffer_ptr {
	char *ptr;
	size_t length;
};

/*
 * An Http request header, which consists of a label, and a value, 
 * stored as str_buffer_ptrs.
 * Contains a link to the next http_header for use in a linked list of
 * http_header structures.
 */
struct http_header {
	struct str_buffer_ptr label;
	struct str_buffer_ptr value;
	struct http_header *prev;
};

/*
 * An Http request method, which stores the method, url, and http version
 * of an Http request as str_buffer_ptrs
 */
struct http_method {
	struct str_buffer_ptr method;
	struct str_buffer_ptr url;
	struct str_buffer_ptr version;
};

/*
 * Parse an http_method structure out of a piece of a given character buffer.
 * Returns: 
 *   0 -> The parse failed, the method was malformed
 *   1 -> The parse succeeded, the method struct was filled with the data
 */
int parse_method(struct http_method *method,
	char *source_buffer, size_t source_length);

/*
 * Parse an http_header structure out of a piece of a given character buffer.
 * Returns:
 *   0 -> The parse failed, the header was malformed
 *   1 -> The parse succeeded, the header struct was filled with the data
 */
int parse_header(struct http_header *header, 
	char *source_buffer, size_t source_length);

/*
 * Parse a header's value converted to a size_t, with a maximum value allowed.
 * Returns:
 *   0 -> The parse failed, either the value was not convertable to a size_t,
 *        or the value was greater than the max_value requested.
 *   1 -> The parse succeeded, and the value was written into len.
 */
int header_value_as_size_t(struct http_header *header, 
	size_t *len, size_t max_value);