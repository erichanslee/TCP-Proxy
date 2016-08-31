/*
 * Copyright (C) 2014 Ki Suh Lee (kslee@cs.cornell.edu)
 * 
 * TCP Proxy skeleton.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include "list.h"

#define MAX_LISTEN_BACKLOG 5
#define MAX_ADDR_NAME	32
#define ONE_K	1024

/* Data that can pile up to be sent before reading must stop temporarily */
#define MAX_CONN_BACKLOG	(8*ONE_K)
#define GRACE_CONN_BACKLOG	(MAX_CONN_BACKLOG / 2)

/* Watermarks for number of active connections. Lower to 2 for testing */
#define MAX_CONN_HIGH_WATERMARK	(256)
#define MAX_CONN_LOW_WATERMARK	(MAX_CONN_HIGH_WATERMARK - 1)

#define MAX_THREAD_NUM	4

struct sockaddr_in remote_addr; /* The address of the target server */

#define BUF_SIZE 4096

void __loop(int proxy_fd)
{
	struct sockaddr_in client_addr;
	socklen_t addr_size;
	int client_fd, server_fd;
	struct client *client;
	int cur_thread=0;
	struct worker_thread *thread;
	char client_hname[MAX_ADDR_NAME+1];
	char server_hname[MAX_ADDR_NAME+1];

	while(1) {
		memset(&client_addr, 0, sizeof(struct sockaddr_in));
		addr_size = sizeof(client_addr);
		client_fd = accept(proxy_fd, (struct sockaddr *)&client_addr,
					&addr_size);
		if(client_fd == -1) {
			fprintf(stderr, "accept error %s\n", strerror(errno));
			continue;
		}

		// For debugging purpose
		if (getpeername(client_fd, (struct sockaddr *) &client_addr, &addr_size) < 0) {
			fprintf(stderr, "getpeername error %s\n", strerror(errno));
		}

		strncpy(client_hname, inet_ntoa(client_addr.sin_addr), MAX_ADDR_NAME);
		strncpy(server_hname, inet_ntoa(remote_addr.sin_addr), MAX_ADDR_NAME);

		// TODO: Disable following printf before submission
		printf("Connection proxied: %s:%d --> %s:%d\n",
				client_hname, ntohs(client_addr.sin_port),
				server_hname, ntohs(remote_addr.sin_port));

		// Connect to the server
		if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			fprintf(stderr, "socket error %s\n", strerror(errno));
			close(client_fd);
			continue;
		}

		if (connect(server_fd, (struct sockaddr *) &remote_addr, 
			sizeof(struct sockaddr_in)) <0) {
			if (errno != EINPROGRESS) {
				fprintf(stderr, "connect error %s\n", strerror(errno));
				close(client_fd);
				close(server_fd);
				continue;
			}		
		}

		close(client_fd);
		close(server_fd);
	}
}

int main(int argc, char **argv)
{
	char *remote_name;
	struct sockaddr_in proxy_addr;
	unsigned short local_port, remote_port;
	struct hostent *h;
	int arg_idx = 1, proxy_fd;
	
	if (argc != 4)
	{
		fprintf(stderr, "Usage %s <remote-target> <remote-target-port> "
			"<local-port>\n", argv[0]);
		exit(1);
	}

	remote_name = argv[arg_idx++];
	remote_port = atoi(argv[arg_idx++]);
	local_port = atoi(argv[arg_idx++]);

	/* Lookup server name and establish control connection */
	if ((h = gethostbyname(remote_name)) == NULL) {
		fprintf(stderr, "gethostbyname(%s) failed %s\n", remote_name, 
			strerror(errno));
		exit(1);
	}
	memset(&remote_addr, 0, sizeof(struct sockaddr_in));
	remote_addr.sin_family = AF_INET;
	memcpy(&remote_addr.sin_addr.s_addr, h->h_addr_list[0], sizeof(in_addr_t));
	remote_addr.sin_port = htons(remote_port);
	
	/* open up the TCP socket the proxy listens on */
	if ((proxy_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		fprintf(stderr, "socket error %s\n", strerror(errno));
		exit(1);
	}
	/* bind the socket to all local addresses */
	memset(&proxy_addr, 0, sizeof(struct sockaddr_in));
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = INADDR_ANY; /* bind to all local addresses */
	proxy_addr.sin_port = htons(local_port);
	if (bind(proxy_fd, (struct sockaddr *) &proxy_addr, 
			sizeof(proxy_addr)) < 0) {
		fprintf(stderr, "bind error %s\n", strerror(errno));
		exit(1);
	}

	listen(proxy_fd, MAX_LISTEN_BACKLOG);

	__loop(proxy_fd);

	return 0;
}
