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

void *make_transaction(void *arguments) {
	int err = pthread_mutex_lock(&lock);
	if (err < 0) {
		perror("client: pthread_mutex_lock");
		pthread_exit(NULL);
	}

	int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		perror("client: socket");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	struct sockaddr_un socket_addr;
	memset(&socket_addr, '\0', sizeof(socket_addr));
	socket_addr.sun_family = AF_UNIX;
	strcpy(socket_addr.sun_path, SOCKET_NAME);

	err = connect(socket_fd, (struct sockaddr *)&socket_addr,
	              sizeof(socket_addr));
	if (err < 0) {
		perror("client: connect");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	int r = rand() % 1001 + (-500); // Inclusive range -500 to 500
	printf("client: transaction of %d\n", r);

	char buffer[BUFFER_SIZE];
	snprintf(buffer, BUFFER_SIZE, "%d", r);

	err = send(socket_fd, &buffer, sizeof(buffer), 0);
	if (err < 0) {
		perror("client: send");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	err = pthread_mutex_unlock(&lock);
	if (err < 0) {
		perror("client: pthread_mutex_unlock");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	err = close(socket_fd);
	if (err < 0) {
		perror("client: close");
		err = pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

	err = pthread_mutex_unlock(&lock);
	if (err < 0) {
		perror("client: pthread_mutex_unlock");
		pthread_exit(NULL);
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	srand(time(NULL));

	pthread_t threads[NUMBER_OF_THREADS];
	int err = pthread_mutex_init(&lock, NULL);
	if (err < 0) {
		perror("client: pthread_mutex_init");
		return 1;
	}

	for (int i = 0; i < NUMBER_OF_THREADS; i++) {
		err = pthread_create(&threads[i], NULL, make_transaction, NULL);
		if (err < 0) {
			perror("client: pthread_create");
		}
	}

	for (int i = 0; i < NUMBER_OF_THREADS; i++) {
		err = pthread_join(threads[i], NULL);
		if (err < 0) {
			perror("client: pthread_join");
		}
	}

	err = pthread_mutex_destroy(&lock);
	if (err < 0) {
		perror("client: pthread_mutex_destroy");
		return 1;
	}

	return 0;
}
