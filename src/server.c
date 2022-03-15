#include <stdlib.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

/* Constants. */
#define PORTNO 6969
#define MAX_CLIENTS 2

/* User-defined types. */
typedef struct {
	char name[16];
	unsigned int id;
	int fd;
} client_t;

/* Mutexes. */
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Globals. */
static _Atomic unsigned int g_clients_connected = 0;
static client_t *g_clients[MAX_CLIENTS];
static _Atomic unsigned int g_client_id = 1;

/* Functions. */
client_t *create_client(char *, unsigned int, int);
void add_client(client_t *);
void remove_client(unsigned int);
void *manage_client(void *);
int client_exists(const char*);

int
main(void)
{
	int fd = 0;

	if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
		perror("Error creating the socket(): ");
		return EXIT_FAILURE;
	}

	struct sockaddr_in6 sa6; /* Server address IPv6. */
	memset(&sa6, 0, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
	sa6.sin6_port = htons(PORTNO);
	sa6.sin6_addr = in6addr_any;

	if ((bind(fd, (struct sockaddr*) &sa6,
		  sizeof(sa6))) == -1) {
		perror("Error binding: ");
		return EXIT_FAILURE;
	}

	if ((listen(fd, MAX_CLIENTS)) == -1) {
		perror("Error listening: ");
		return EXIT_FAILURE;
	}

	/* Server ready and running. */
	puts("Server started.");

	while (1) {
		pthread_t tid;
		struct sockaddr_in6 ca6; /* Client address. */
		socklen_t ca6_len = sizeof(ca6);
		int cfd = accept(fd, (struct sockaddr*)&ca6,
			(socklen_t *)&ca6_len);

		char buff[BUFF_SIZE];

		/* Check if server is full. */
		if ((g_clients_connected + 1) > MAX_CLIENTS) {
			strcpy(buff, ERR_STATUS);
			if ((send(cfd, buff, sizeof(buff), 0)) == -1) {
				perror("Error sending msg server full: ");
			}
			close(cfd);
			continue;
		}

		/* Send OK status to client. */
		strcpy(buff, OK_STATUS);
		if ((send(cfd, buff, strlen(buff), 0)) == -1) {
			perror("Error sending ok msg: ");
		}

		char name[NAME_LEN];
		memset(name, '\0', sizeof(name));
		if (recv(cfd, &name, sizeof(name), 0) == -1) {
			perror("Error recv'ing client name: ");
			close(cfd);
			continue;
		}

		if (client_exists(name)) {
			strcpy(buff, ERR_STATUS);
			if ((send(cfd, buff, strlen(buff), 0)) == -1) {
				perror("Error sending msg client exists: ");
			}
			close(cfd);
			continue;
		}

		client_t *c = create_client(name, g_client_id, cfd);
		add_client(c);

		++g_clients_connected;
		++g_client_id;

		pthread_create(&tid, NULL, manage_client,
			       (void *) c);
	}

	/* Cleanup. */
	close(fd);

	return EXIT_SUCCESS;
}

/*
 * @brief Find the first NULL slot in the array of current clients
 * connected and assign it to the new client.
 *
 * @param[in] c New client connected.
 */
void
add_client(client_t *c)
{
	pthread_mutex_lock(&client_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (g_clients[i] == NULL) {
			g_clients[i] = c;
			break;
		}
	}

	pthread_mutex_unlock(&client_mutex);
}

/*
 * @brief Find the client that has the id passed as parameter
 * in the array of clients connected.
 * Then, close its fd and free everything.
 *
 * @param[in] id Id of the client to remove.
 */
void
remove_client(unsigned int id)
{
	pthread_mutex_lock(&client_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (g_clients[i]->id == id) {
			close(g_clients[i]->fd);
			free(g_clients[i]->name);
			free(g_clients[i]);
			g_clients[i] = NULL;
			break;
		}
	}

	pthread_mutex_unlock(&client_mutex);
}

/*
 * @brief malloc a new client with the given parameters and return it.
 *
 * @param[in] name Client name in the chatroom.
 * @param[in] id Client id.
 * @param[in] fd Client file descriptor.
 */
client_t *
create_client(char *name, unsigned int id, int fd)
{
	client_t *c = (client_t *) malloc(sizeof(client_t));
	strcpy(c->name, name);
	c->id = id;
	c->fd = fd;

	puts("...Client created...");
	printf("%s\n", c->name);
	printf("%d\n", c->id);
	printf("%d\n", c->fd);

	return c;
}

/*
 * @brief Each client connected will be managed by this function. It
 * basically handles incoming messages from the client and broadcasts
 * them to everyone.
 *
 * @param[in] c New client connected to the chatroom.
 */
void *
manage_client(void *c)
{
	client_t *client = (client_t *) c;

	printf("Client %s has connected.\n", client->name);

	while (1) {
		sleep(10);
	}

	remove_client(client->id);
	--g_clients_connected;

	return NULL;
}

/*
 * @brief Checks if NAME is already in G_CLIENTS.
 *
 * @param[in] name Client's name.
 *
 * @return 1 if the NAME already exists; 0 otherwise.
 */
int
client_exists(const char* name)
{
	pthread_mutex_lock(&client_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (g_clients[i] &&
		    strcmp(g_clients[i]->name, name) == 0) {
			pthread_mutex_unlock(&client_mutex);
			return 1;
		}
	}

	pthread_mutex_unlock(&client_mutex);

	return 0;
}
