#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

int main(int argc, char *argv[]) {
	// Argument should be the number of transactions to do
	if (argc < 2) {
		return 1;
	}

	int number_of_transactions = strtol(argv[1], NULL, 10);
	if (errno != 0) {
		perror("client: strtol");
		return 1;
	}

	int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		perror("client: socket");
		return 1;
	}

	struct sockaddr_un socket_addr;
	memset(&socket_addr, '\0', sizeof(socket_addr));
	socket_addr.sun_family = AF_UNIX;
	strcpy(socket_addr.sun_path, SOCKET_NAME);

	int err = connect(socket_fd, (struct sockaddr *)&socket_addr,
	                  sizeof(socket_addr));
	if (err < 0) {
		perror("client: connect");
		return 1;
	}

	srand(time(NULL));
	int r = rand() % 1001 + (-500); // Inclusive range -500 to 500

	char buffer[BUFFER_SIZE];
	snprintf(buffer, BUFFER_SIZE, "%d", r);
	write(socket_fd, &buffer, sizeof(buffer));

	close(socket_fd);
	return 0;
}
