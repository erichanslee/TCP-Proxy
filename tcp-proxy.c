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
#include <fcntl.h>
#include <unistd.h>
#include "list.h"
#include "tcp-proxy.h"





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
	struct connection fdarray[MAX_CONN_HIGH_WATERMARK];
	int idx=0;
	int printflag = 1;
	int i;

	// set connection fields = -1 to represent non-assignment
	for(i = 0; i < MAX_CONN_HIGH_WATERMARK; i++){
		fdarray[i].client_fd = -1;
		fdarray[i].server_fd = -1;
	}

	//create threads
	pthread_t threads[NUM_THREADS];
	for(i = 0; i < NUM_THREADS; i++){
		printf("Creating thread %d\n", i);
		thread_data_array[i].threadidx = i;
		thread_data_array[i].fdarray = fdarray;
		pthread_create(&threads[i], NULL, ThreadTask, (void *)&thread_data_array[i]);
	}

	while(1) {
		// Notify client that no new connections are being accepted
		int idx = findval(fdarray, MAX_CONN_HIGH_WATERMARK, -1);


		//Accept Client-Side 
		memset(&client_addr, 0, sizeof(struct sockaddr_in));
		addr_size = sizeof(client_addr);
		if(idx < MAX_CONN_HIGH_WATERMARK){

			// Connect to client
			client_fd = accept(proxy_fd, (struct sockaddr *)&client_addr, &addr_size);
			if(client_fd == -1) {
				fprintf(stderr, "accept error %s\n", strerror(errno));
				continue;
			}

			// For debugging purpose
			if (getpeername(client_fd, (struct sockaddr *) &client_addr, &addr_size) < 0) {
				fprintf(stderr, "getpeername error %s\n", strerror(errno));
				break;
			}

			strncpy(client_hname, inet_ntoa(client_addr.sin_addr), MAX_ADDR_NAME);
			strncpy(server_hname, inet_ntoa(remote_addr.sin_addr), MAX_ADDR_NAME);

			// TODO: Disable following printf before submission
			printf("Connection proxied: %s:%d --> %s:%d\n",
					client_hname, ntohs(client_addr.sin_port),
					server_hname, ntohs(remote_addr.sin_port));

			// Connect to server
			if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
				fprintf(stderr, "socket error %s\n", strerror(errno));
				close(server_fd);
				continue;
			}

			// Set struct data
			fdarray[idx].client_fd = client_fd;
			fdarray[idx].server_fd = server_fd;
			printf("New Connection Set up from %d to %d\n", fdarray[idx].client_fd, fdarray[idx].server_fd);
			idx++;
			


			if (connect(server_fd, (struct sockaddr *) &remote_addr, 
				sizeof(struct sockaddr_in)) <0) {
				if (errno != EINPROGRESS) {
					fprintf(stderr, "connect error %s\n", strerror(errno));
					close(client_fd);
					close(server_fd);
					continue;
				}		
			}

		}


		/*
		fcntl(client_fd, F_SETFL, O_NONBLOCK);
		fcntl(server_fd, F_SETFL, O_NONBLOCK);
		*/
		
		// see header tcp-proxy.h
		
		/*
		start_proxy(client_fd, server_fd);
		close(client_fd);
		close(server_fd);
		*/
	}

	pthread_exit(NULL);
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

	printf("Waiting for Connection...\n");

	__loop(proxy_fd);

	return 0;
}
