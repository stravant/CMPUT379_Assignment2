
#include "args.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

void print_usage() {
	printf("Usage: server_f port rootdir logdir\n");
}

int parse_args(struct server_args *result, int argc, char *argv[]) {
	if (argc != 4) {
		return ARGS_ERROR;
	}

	/* Get the port */
	char *endptr;
	result->port = strtol(argv[1], &endptr, 10);
	if (strlen(argv[1]) == 0 || *endptr != '\0') {
		/* 
		 * Note: Should just use strtonum... but the lab machines don't have
		 * that function for some reason, so this serves as a roundabout way of
		 * determining if the first arg was a valid number.
		 */
		return ARGS_ERROR;
	}

	/* Get the paths */
	/* TODO: Error check */
	result->server_root = argv[2];
	result->log_file = argv[3];

	return ARGS_OKAY;
}