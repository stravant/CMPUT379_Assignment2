
#include <stdio.h>

#include <unistd.h>

#include <setjmp.h>
#include <memory.h>
#include <stdlib.h>

#include "args.h"
#include "server_filesystem.h"
#include "server_common.h"
#include "server_http.h"

void serve_requests(struct server_filesystem *fs, struct server_state *state) {
	for (;;) {
		int fd = server_listen(state);
		printf("Got request\n");
		if (fd > 0) {
			/* Good request, fork off to a child process to handle it in */
			if (fork() == 0) {
				/* Serve the request as an http request */
				handle_http_request(fs, fd);

				/* Shutdown communications on the fd and close the fd handle */
				shutdown(fd, SHUT_RDWR);

				printf("Done request\n");

				/* This fork has completed, end the process here */
				exit(0);
			}
		} else {
			/* There was an error, exit */
			printf("Listen failed\n");
			break;
		}
	}
}

/* Loctation to jump to on inturrepted */
sigjmp_buf before_exit;

/* Our interrupt handlers 
 * We handle SIGINT for breaking out of the handler loop when not in daemonized
 * mode, and SIGCHLD to reap completed child processes. 
 */
struct sigaction server_child_sigaction;
struct sigaction server_int_sigaction;

void sig_child_handler(int sig) {
	/* Reap a child process if one has exited. */
	waitpid(WAIT_ANY, NULL, WNOHANG);
}

void sig_int_handler(int sig) {
	/* On inturrupted, break out to the break-out-of-handler-loop jump point */
	siglongjmp(before_exit, 1);
}

/* Install the signal handlers */
void install_sig_handler() {
	memset(&server_child_sigaction, 0x0, sizeof(sigaction));
	server_child_sigaction.sa_handler = sig_child_handler;
	server_child_sigaction.sa_flags = SA_RESTART;

	memset(&server_int_sigaction, 0x0, sizeof(sigaction));
	server_int_sigaction.sa_handler = sig_int_handler;
	server_int_sigaction.sa_flags = SA_RESTART;

	sigaction(SIGCHLD, &server_child_sigaction, NULL);
	sigaction(SIGINT, &server_int_sigaction, NULL);
}

int main(int argc, char *argv[]) {
	/* Get the server arguments */
	struct server_args args;
	if (parse_args(&args, argc, argv) != ARGS_OKAY) {
		print_usage();
		return -1;
	}

	/* Open the server filesystem */
	struct server_filesystem fs;
	if (server_fs_create(&fs, args.server_root, args.log_file) != FS_OKAY) {
		printf("Bad paths to serve from / log to.\n");
		return -1;
	}

	/* Create the server state */
	struct server_state server;
	if (server_create(&server, args.port) != SERVER_OKAY) {
		printf("Could not start the server on port %d.\n", args.port);
		/* destroy the server_fs */
		server_fs_destroy(&fs);
		return -1;
	}

	/* Listen and serve new connections */
	if (sigsetjmp(before_exit, 1) == 0) {
		/* With the jump point installed, now we can safely install the signal
		 * handlers.
		 */
		install_sig_handler();

		/* Go into the main handler loop */
		serve_requests(&fs, &server);
	} else {
		/* User Ctrl-C requested exit (if not daemonized) */
		printf("\nShutdown Requested, terminating...\n");
	}

	/* Close the server and fs */
	server_destroy(&server);
	server_fs_destroy(&fs);

	/* Done */
	return 0;
}