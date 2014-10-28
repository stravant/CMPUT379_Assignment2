
#include <memory.h>

#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

const char *response =
	"HTTP/1.1 200 OK\n"
	"\n"
	"Hello, World!\n";

/* Args: port, dir, logFile */
int main(int argc, char *argv[]) {
	int port = 8000;

	int socketfd = socket(AF_INET, SOCK_STREAM, 0); /* 0 -> IP */
	if (socketfd < 0) {
		printf("socket() error\n");
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(socketfd, (struct sockaddr*)&addr, sizeof(addr))) {
		printf("bind() error\n");
		return -1;
	}

	if (listen(socketfd, 3)) {
		printf("listen() error\n");
		return -1;
	}

	struct sockaddr_in connection_addr;
	socklen_t connection_len = sizeof(connection_addr);
	memset(&connection_addr, 0, connection_len);
	printf("waiting for connection...\n");
	int connectionfd = 
		accept(socketfd, (struct sockaddr*)&connection_addr, &connection_len);
	if (connectionfd < 0) {
		printf("accept() error\n");
		return -1;
	}

	printf("Accepted connection from: %d\n", connection_addr.sin_addr.s_addr);

	/* read in everything */
	printf("Reading...\n");
	char buff[400];
	int v = recv(connectionfd, buff, 400, 0);
	printf("Recieved:\n%d\n", v);
	printf("Body: %s\n", buff);

	send(connectionfd, response, strlen(response), 0);

	shutdown(connectionfd, SHUT_RDWR);
	close(connectionfd);

	close(socketfd);

	return 0;
}