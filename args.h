
#ifndef ARGS_H_
#define ARGS_H_


/* Status codes returned by parse_args */
#define ARGS_OKAY   0
#define ARGS_ERROR -1


/*
 * A structure representing the arguments passed to our server.
 */
struct server_args {
	int port;
	char *server_root;
	char *log_file;
};


/*
 * Print out the correct argument usage for the program
 */
void print_usage(char *prog_name);


/*
 * Parse in a set of arguments passed to an invokation of the program, and
 * write out the parsed args into a server_args structure.
 * Returns:
 *   ARGS_OKAY on success, and the results are written into |result|
 *   ARGS_ERROR on failure
 */
int parse_args(struct server_args *result, int argc, char *argv[]);


#endif