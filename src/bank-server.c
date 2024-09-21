#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"

int main(void) {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("server: socket");
		return 1;
	}

	struct sockaddr_un sock_addr;
	memset(&sock_addr, '\0', sizeof(sock_addr));
	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, SOCKET_NAME);

	int err = bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
	if (err < 0) {
		perror("server: bind");
		return 1;
	}

	listen(sockfd, 10);

	struct sockaddr_un client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	int client_socket_fd =
		accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);

	char buffer[BUFFER_SIZE];
	read(client_socket_fd, &buffer, BUFFER_SIZE);
	printf("Server: %s\n", buffer);

	close(client_socket_fd);

	close(sockfd);
	unlink(SOCKET_NAME);
	return 0;
}
