#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"
#include "sys/poll.h"

int accountTotal;
int stop;
pthread_mutex_t lock;

void *complete_transaction(void *arguments) {
	int socket = *(int *)arguments;
	free(arguments);

	pthread_mutex_lock(&lock);

	char buffer[BUFFER_SIZE];
	int err = recv(socket, &buffer, BUFFER_SIZE, 0);
	if (err < 0) {
		perror("server: recv");
	}

	if (buffer[0] == 'e' && buffer[1] == '\0') {
		stop = 1;
		printf("server: Closing\n");
		close(socket);
		pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	int transaction_amount = strtol(buffer, NULL, 10);

	printf("server: new thread for transaction for amount %d\n",
	       transaction_amount);

	// Let's cause a deadlock
	if (transaction_amount > 250)
		while (1)
			;

	int previous_total = accountTotal;
	accountTotal += transaction_amount;

	printf("server: total is now %d (%d + %d)\n", accountTotal,
	       transaction_amount, previous_total);

	pthread_mutex_unlock(&lock);
	return NULL;
}

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

	accountTotal = 0;

	err = pthread_mutex_init(&lock, NULL);
	if (err < 0) {
		perror("server: pthread_mutex_init");
		return 1;
	}

	err = listen(sockfd, 128);
	if (err < 0) {
		perror("server: listen");
		return 1;
	}

	socklen_t server_addr_size = sizeof(sock_addr);

	struct pollfd socket_poll[1];
	socket_poll[0].fd = sockfd;
	socket_poll[0].events = POLLIN;

	stop = 0;

	printf("server: Ready\n");
	while (1) {
		if (stop)
			break;

		int ret = poll(socket_poll, 1, 0);
		if (ret < 0) {
			perror("server: poll");
			return 1;
		}

		if (!ret)
			continue;

		int *client_socket = (int *)malloc(sizeof(int));
		*client_socket =
			accept(sockfd, (struct sockaddr *)&sock_addr, &server_addr_size);
		if (*client_socket < 0) {
			perror("server: accept");
			free(client_socket);
			continue;
		}

		printf("server: accepted new connection (fd: %d)\n", *client_socket);

		pthread_t thread;
		pthread_create(&thread, NULL, complete_transaction, client_socket);
	}

	printf("server: Account Total: %d\n", accountTotal);

	err = pthread_mutex_destroy(&lock);
	if (err < 0) {
		perror("server: pthread_mutex_destroy");
		return 1;
	}

	close(sockfd);
	unlink(SOCKET_NAME);
	return 0;
}
