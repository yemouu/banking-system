#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

pthread_mutex_t lock;
// The process that is currently holding onto the mutex lock
pthread_t activeLockerId;

// Function that each thread will run
void *make_transaction(void *arguments) {
	// Create a timeout of 10 seconds before we consider ourself in a deadlock
	// state
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += 10;

	// Setup thread cancelation
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)&lock);

	// Try and get a hold of the lock
	// If we can't get a hold of it before the timeout occurs, we cancel the
	// offending thread
	while (1) {
		int ret = pthread_mutex_timedlock(&lock, &timeout);

		switch (ret) {
		case 0:
			break;
		case EAGAIN:
		case ETIMEDOUT:
			printf("client: thread (%lu) took too long, canceling it\n",
			       activeLockerId);
			pthread_cancel(activeLockerId);
			continue;
		default:
			perror("client: pthread_mutex_timedlock");
			pthread_exit(NULL);
		}

		break;
	}

	// Once we have the mutex lock, let others know
	activeLockerId = pthread_self();

	int r = rand() % 1001 + (-500); // Inclusive range -500 to 500

	// Let's cause a deadlock
	if (r < -250)
		pthread_mutex_lock(&lock);

	// Setup a unix socket
	// We will use this to connect to the bank-server
	int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		perror("client: socket");
		pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	struct sockaddr_un socket_addr;
	memset(&socket_addr, '\0', sizeof(socket_addr));
	socket_addr.sun_family = AF_UNIX;
	strcpy(socket_addr.sun_path, SOCKET_NAME);

	// Connect to the bank-server
	int err = connect(socket_fd, (struct sockaddr *)&socket_addr,
	                  sizeof(socket_addr));
	if (err < 0) {
		perror("client: connect");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	printf("client: transaction of %d\n", r);

	// Write our random number into a buffer and send it to the server
	char buffer[BUFFER_SIZE];
	snprintf(buffer, BUFFER_SIZE, "%d", r);

	err = send(socket_fd, &buffer, sizeof(buffer), 0);
	if (err < 0) {
		perror("client: send");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	err = close(socket_fd);
	if (err < 0) {
		perror("client: close");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	// Unlock the mutex
	activeLockerId = -1;
	err = pthread_mutex_unlock(&lock);
	if (err < 0) {
		perror("client: pthread_mutex_unlock");
		pthread_exit(NULL);
	}
	pthread_cleanup_pop(0);
	return NULL;
}

int main(int argc, char *argv[]) {
	srand(time(NULL));

	pthread_t threads[NUMBER_OF_THREADS];

	// Initialize the mutex lock
	int err = pthread_mutex_init(&lock, NULL);
	if (err < 0) {
		perror("client: pthread_mutex_init");
		return 1;
	}

	activeLockerId = -1;
	for (int i = 0; i < NUMBER_OF_THREADS; i++) {
		// Create threads
		err = pthread_create(&threads[i], NULL, make_transaction, NULL);
		if (err < 0) {
			perror("client: pthread_create");
		}
	}

	for (int i = 0; i < NUMBER_OF_THREADS; i++) {
		// Make sure every thread is collected before we exit
		err = pthread_join(threads[i], NULL);
		if (err < 0) {
			perror("client: pthread_join");
		}
	}

	// Destroy the mutex lock
	err = pthread_mutex_destroy(&lock);
	if (err < 0) {
		perror("client: pthread_mutex_destroy");
		return 1;
	}

	return 0;
}
