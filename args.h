
#ifndef ARGS_H_
#define ARGS_H_

#define ARGS_OKAY   0
#define ARGS_ERROR -1

struct server_args {
	int port;
	char *server_root;
	char *log_file;
};

void print_usage();

int parse_args(struct server_args *result, int argc, char *argv[]);

#endif