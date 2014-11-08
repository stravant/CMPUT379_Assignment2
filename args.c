
#include "args.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>


void print_usage(char *prog_name) {
	printf("Usage: %s port rootdir logfile\n", prog_name);
}


int parse_args(struct server_args *result, int argc, char *argv[]) {
	/* Must have exactly 4 arguments */
	if (argc != 4) {
		return ARGS_ERROR;
	}

	/* Get the port */
	char *endptr;
	result->port = strtol(argv[1], &endptr, 10);
	if (strlen(argv[1]) == 0 || *endptr != '\0') {
		/* 
		 * Bad port
		 * Note: Should just use strtonum... but the lab machines don't have
		 * that function for some reason, so this serves as a roundabout way of
		 * determining if the first arg was a valid number.
		 */
		return ARGS_ERROR;
	}

	/* Get the paths */
	result->server_root = argv[2];
	result->log_file = argv[3];

	return ARGS_OKAY;
}