#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <errno.h>
#include "socket_utils.h"

int getSSocket(unsigned short port) {
	fprintf(stdout, "Generating new server socket for port %hu\n", port);
	int sockId;
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port);
	if ((sockId = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Error: Socket creation failed\n");
		return -1;
	} else {
		fprintf(stdout, "Info: Socket created\n");
	}
	int opt = 1;
	if (setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
		fprintf(stderr, "Error: Socket options couldn't be set\n");
		return -2;
	} else {
		fprintf(stdout, "Info: Socket options set\n");
	}
	if (bind(sockId, (struct sockaddr*)&server, sizeof(server))) {
		fprintf(stderr, "Error: Could not bind socket\n");
		return -3;
	} else {
		fprintf(stdout, "Info: Socket binded\n");
	}
	if (listen(sockId, 5)) { // 5 pending connections
		fprintf(stderr, "Error: Could not listen to socket\n");
		return -4;
	} else {
		fprintf(stdout, "Info: Socket listening\n");
	}
	char* ipStr = malloc(sizeof(INET_ADDRSTRLEN));
	inet_ntop(AF_INET, &(server.sin_addr), ipStr, INET_ADDRSTRLEN);
	fprintf(stdout, "Info: Server socket created successfully on <%s, %hu>\n", ipStr, port);
	free(ipStr);
	return sockId;
}

int getCSocket() {
	int sockId;
	if ((sockId = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Error: Socket creation failed\n");
		return -1;
	} else {
		fprintf(stdout, "Info: Socket created\n");
	}
	int opt = 1;
	if (setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
		fprintf(stderr, "Error: Socket options couldn't be set\n");
		return -2;
	} else {
		fprintf(stdout, "Info: Socket options set\n");
	}
	return sockId;
}

struct sockaddr_in* conCToS(int sockId, struct sockaddr* server) {
	int retries = 0;
	while (retries < 5) {
	int conRes = connect(sockId, server, sizeof(*server));
	if (conRes) {
		fprintf(stderr, "Error: Connection failed\n");
		if (errno == EADDRINUSE) {
			fprintf(stderr, "Error: Address already in use\n");
			if (retries < 5) {
				retries++;
				continue;
			} else {
				return NULL;
			}
		}
		else if (errno == EBADF) {
			fprintf(stderr, "Error: Not an open fd\n");
			return NULL;
		}
		else if (errno == EISCONN) {
			fprintf(stderr, "Error: Socket already connected\n");
			return NULL;
		} else {
			fprintf(stderr, "Unknown error\n");
			return NULL;
		}
	}
	fprintf(stdout, "Info: Connected to socket\n");
	struct sockaddr_in* client = malloc(sizeof(struct sockaddr_in));
	int clientLen = sizeof(struct sockaddr_in);
	int sockData = getsockname(sockId, (struct sockaddr*) client, &clientLen);
	if (sockData != 0) {
		fprintf(stderr, "Error: Error retrieving client socket data\n");
		return NULL;
	}
	return client;
	}
}
