#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"

struct bankAccount {
	int total;
};

struct threadArgs {
	int transaction_amount;
	struct bankAccount *account;
};

void *complete_transaction(void *ptr) {
	struct threadArgs *args = ptr;

	int transaction_amount = args->transaction_amount;
	struct bankAccount *account = args->account;

	printf("server: new thread for transaction for amount %d\n",
	       transaction_amount);

	int previous_total = account->total;
	account->total += transaction_amount;

	printf("server: total is now %d (%d + %d)\n", account->total,
	       transaction_amount, previous_total);

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

	struct bankAccount account;
	account.total = 0;

	listen(sockfd, 128);
	printf("server: Ready\n");
	while (1) {
		struct sockaddr_un client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		int client_socket_fd =
			accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);

		char buffer[BUFFER_SIZE];
		read(client_socket_fd, &buffer, BUFFER_SIZE);

		if (buffer[0] == 'e' && buffer[1] == '\0') {
			printf("server: Closing\n");
			break;
		}

		printf("server: recieved transaction for %s\n", buffer);
		size_t transaction_amount = strtol(buffer, NULL, 10);

		pthread_t thread;
		struct threadArgs args;

		args.transaction_amount = transaction_amount;
		args.account = &account;

		pthread_create(&thread, NULL, *complete_transaction, &args);

		close(client_socket_fd);
	}

	printf("server: Account Total: %d\n", account.total);

	close(sockfd);
	unlink(SOCKET_NAME);
	return 0;
}
