

#include "args.h"
#include "server_filesystem.h"
#include "server_common.h"
#include "server_http.h"

#include <stdio.h>
#include <unistd.h>
#include <setjmp.h>
#include <memory.h>
#include <stdlib.h>
#include <pthread.h>

/* Forward declarations of functions */
void sig_child_handler(int);
void sig_int_handler(int);
void install_sig_handler();
void uninstall_sig_handler();
void *serve_single_request(void *arg);
void serve_requests(struct server_filesystem*, struct server_state*);

/* 
 * The PID of the main process, so that children can tell to do nothing
 * if they get a SIGINT signal after a fork but before they have managed to 
 * clear the SIGINT handler.
 */ 
pid_t main_process_pid;

/* Loctation to jump to on inturrepted */
sigjmp_buf before_exit;

/* 
 * Our interrupt handlers
 * We handle SIGINT for breaking out of the handler loop when not in daemonized
 * mode, and SIGCHLD to reap completed child processes. 
 */
struct sigaction server_child_sigaction;
struct sigaction server_int_sigaction;


/*
 * The state that a request needs to operate
 */
struct request_state {
	pthread_t thread;
	struct server_filesystem *fs;
	char *addr;
	int connectionfd;
};


/* Signal handler for SIGCHLD */
void sig_child_handler(int sig) {
	/* 
	 * If this is not main process (May happen if a signal arrives right after
	 * a fork, but before the child has removed this signal handler), then do
	 * nothing.
	 */
	if (getpid() != main_process_pid)
		return;

	/* Reap a child process if one has exited. */
	waitpid(WAIT_ANY, NULL, WNOHANG);
}


/* Signal handler for SIGINT */
void sig_int_handler(int sig) {
	/* 
	 * If this is not main process (May happen if a signal arrives right after
	 * a fork, but before the child has removed this signal handler), then do
	 * nothing.
	 */
	if (getpid() != main_process_pid)
		return;

	/* On inturrupted, break out to the break-out-of-handler-loop jump point */
	siglongjmp(before_exit, 1);
}


/* Install the signal handlers */
void install_sig_handler() {
	/* Get the main process PID for the signals to use */
	main_process_pid = getpid();

	/* Install SIGCHLD */
	memset(&server_child_sigaction, 0x0, sizeof(sigaction));
	server_child_sigaction.sa_handler = sig_child_handler;
	server_child_sigaction.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &server_child_sigaction, NULL);

	/* Install SIGINT */
	memset(&server_int_sigaction, 0x0, sizeof(sigaction));
	server_int_sigaction.sa_handler = sig_int_handler;
	server_int_sigaction.sa_flags = SA_RESTART;
	sigaction(SIGINT, &server_int_sigaction, NULL);
}


/* Uninstall the signal handlers */
void uninstall_sig_handler() {
	/* Uninstall SIGCHLD */
	sigaction(SIGCHLD, NULL, NULL);

	/* Uninstall SIGINT */
	sigaction(SIGINT, NULL, NULL);
}


/*
 * pthread Entry point for theads handling requests
 * Parameters: A pointer to a request_state structure
 *   Note: The thread owns this request_state structure, and must
 *         free it before exiting.
 */
void *serve_single_request(void *arg) {
	struct request_state *state;

	/* Get the request state */
	state = (struct request_state*)arg;

	/* Call off to handle the request */
	handle_http_request(state->fs, state->connectionfd, state->addr);

	/* Shut down the connection */
	shutdown(state->connectionfd, SHUT_RDWR);

	/* Free the request_state structure */
	free(state->addr);
	free(state);

	/* Done, exit */
	pthread_exit(0);
}


/*
 * Main function to serve requests to the client, using a given server_state
 * serving documents from a given server_filesystem.
 * Each request is processed in a new child process, which terminates once
 * the request has been completed.
 */
void serve_requests(struct server_filesystem *fs, struct server_state *state) {
	for (;;) {
		char *addr;
		int fd;

		/* Do the listen */
		if ((fd = server_listen(state, &addr)) > 0) {
			struct request_state *req;

			/* Init a request structure for the request */
			req = malloc(sizeof(struct request_state));
			req->fs = fs;
			req->addr = malloc(strlen(addr) + 1);
			strcpy(req->addr, addr);
			req->connectionfd = fd;

			/* Start a new thread to handle it */
			if (0 != pthread_create(&(req->thread), NULL, 
				serve_single_request, (void*)req))
			{
				/* Thread creation failed, free req and stop */
				free(req->addr);
				free(req);
			}

			/* The thread takes ownership of req, we don't need to free it */
		} else {
			/* There was an error, exit */
			printf("Error trying to accept a connection, terminating...\n");
			break;
		}
	}
}


/* Main program entry point */
int main(int argc, char *argv[]) {
	struct server_args args;
	struct server_filesystem fs;
	int fs_status;
	struct server_state server;

	/* Get the server arguments */
	if (parse_args(&args, argc, argv) != ARGS_OKAY) {
		print_usage("server_f");
		return -1;
	}

	/* Open the server filesystem (0 -> don't use flock) */
	if ((fs_status = server_fs_create(&fs, args.server_root, args.log_file, 0)) 
		!= FS_OKAY) 
	{
		/* Failed to open the server filesystem, report and exit */
		switch (fs_status) {
		case FS_BADROOT:
			printf("Could not access server root directory.\n");
			break;
		case FS_BADLOG:
			printf("Could not open log file for writing.\n");
			break;
		case FS_INITERROR:
			printf("Error initializing the file system access.\n");
			break;
		default:
			printf("Unknown Error during startup.\n");
		}
		return -1;
	}

	/* Create the server state */
	if (server_create(&server, args.port) != SERVER_OKAY) {
		/* 
		 * Failed to create the server on the port requested, report 
		 * and exit
		 */
		printf("Could not start the server on port %d.\n", args.port);

		/* 
		 * We already opened the filesystem, so before exiting, destroy 
		 * destroy the server_fs 
		 */
		server_fs_destroy(&fs);

		return -1;
	}

	/* Listen and serve new connections */
	if (sigsetjmp(before_exit, 1) == 0) {
		/* 
		 * With the jump point installed, now we can safely install the 
		 * signal handlers.
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