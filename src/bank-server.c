#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

int accountTotal;
int stop;
pthread_mutex_t lock;
pthread_t activeLockerId;

void *complete_transaction(void *arguments) {
	// Make a copy of the socket file descriptor
	int socket = *(int *)arguments;
	free(arguments);

	// Setup cancelation
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)&lock);

	// Setup timeout for mutex
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += 10;

	// Try to acquire the mutex and cancel the thread that is holding the mutex
	// if the timeout expires
	while (1) {
		int ret = pthread_mutex_timedlock(&lock, &timeout);

		switch (ret) {
		case 0:
			break;
		case EAGAIN:
		case ETIMEDOUT:
			printf("server: thread (%lu) took too long, canceling it\n",
			       activeLockerId);
			pthread_cancel(activeLockerId);
			continue;
		default:
			perror("server: pthread_mutex_timedlock");
			pthread_exit(NULL);
		}

		break;
	}

	// Set ourselves as the active locking thread
	activeLockerId = pthread_self();

	// Read data from the socket
	char buffer[BUFFER_SIZE];
	int err = recv(socket, &buffer, BUFFER_SIZE, 0);
	if (err < 0) {
		perror("server: recv");
	}

	// If we recieve 'e\0' to our socket begin shutdown
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
		pthread_mutex_lock(&lock);

	// Add the transaction to our account
	int previous_total = accountTotal;
	accountTotal += transaction_amount;

	printf("server: total is now %d (%d + %d)\n", accountTotal,
	       transaction_amount, previous_total);

	// Release the lock and remove ourselves as the active locking thread
	activeLockerId = -1;
	pthread_mutex_unlock(&lock);
	pthread_cleanup_pop(0);
	return NULL;
}

int main(void) {
	// Create socket
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("server: socket");
		return 1;
	}

	struct sockaddr_un sock_addr;
	memset(&sock_addr, '\0', sizeof(sock_addr));
	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, SOCKET_NAME);

	// Bind to socket
	int err = bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
	if (err < 0) {
		perror("server: bind");
		return 1;
	}

	accountTotal = 0;

	// Initialize the mutex
	err = pthread_mutex_init(&lock, NULL);
	if (err < 0) {
		perror("server: pthread_mutex_init");
		return 1;
	}

	// Start listening on our socket
	err = listen(sockfd, 128);
	if (err < 0) {
		perror("server: listen");
		return 1;
	}

	socklen_t server_addr_size = sizeof(sock_addr);

	// Setup polling on our socket. Without this if we accept connections in the
	// loop bellow we won't ever be able to exit normally since the accept call
	// is blocking
	struct pollfd socket_poll[1];
	socket_poll[0].fd = sockfd;
	socket_poll[0].events = POLLIN;

	stop = 0;
	activeLockerId = -1;

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

		// Accept the connection and create a thread while giving that thread a
		// copy of the socket file descriptor.
		// We allocate space here so we can have our own copy of the file
		// descriptor so it doesn't get overwritten by the next iteration
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

	// Destory the mutex
	err = pthread_mutex_destroy(&lock);
	if (err < 0) {
		perror("server: pthread_mutex_destroy");
		return 1;
	}

	// Close the socket and clean up the created file
	close(sockfd);
	unlink(SOCKET_NAME);
	return 0;
}
