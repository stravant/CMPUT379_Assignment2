
#include "http_request.h"


int parse_method(struct http_method *method,
	char *source_buffer, size_t source_length)
{
	char *after;
	char *ptr;
	char *start;

	/* Set up parsing variables */
	after = source_buffer + source_length;
	ptr = source_buffer;
	start = ptr;

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
	char *after;
	char *ptr;
	char *start;

	/* Set up parsing variables */
	after = source_buffer + source_length;
	ptr = source_buffer;
	start = ptr;

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


int header_value_as_size_t(struct http_header *header, 
	size_t *len, size_t max_len) 
{
	long n;
	size_t value;
	int i;

	/* Must have a value */
	if (header->value.length == 0)
		return 0;

	/* Parse value */
	n = 1;
	value = 0;
	for (i = header->value.length-1; i >= 0; --i) {
		char c;

		/* Get the character and handle it */
		c = header->value.ptr[i];
		if (c < '0' || c > '9') {
			/* Bad character */
			return 0;
		}
		
		/* Add to value */
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