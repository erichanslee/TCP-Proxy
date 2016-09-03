 // TCP Proxy header.
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
#define BUF_SIZE 4096

int start_proxy(int client_fd, int server_fd){
	printf("Staring Proxy Forwarding...");
	int exitflag = 0; 
	void *buf = malloc(BUF_SIZE);
	int size;
	while(1){
		if(exitflag == 1)
			break;
		//todo: need to buffer
		forward(client_fd, server_fd, buf);
		forward(server_fd, client_fd, buf);

		
	}
}

int forward(int origin_fd, int destination_fd, void *buf){
	int size;
		if(size = recv(origin_fd, buf, BUF_SIZE, 0)){
			if(size < -1){
				fprintf(stderr, "Receiving error from Client-Side%s\n", strerror(errno));
				close(origin_fd);
				return 0;
			}
			if(size == -1){
				// do nothing
			}
			else{
				if(send(destination_fd, buf, size, 0) < 0){
					fprintf(stderr, "Sending error from Client-Side%s\n", strerror(errno));
					return 0;
				}
			}
		}
}