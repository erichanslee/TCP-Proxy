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

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define MAX_LISTEN_BACKLOG 5
#define MAX_ADDR_NAME	32
#define ONE_K	1024

/* Data that can pile up to be sent before reading must stop temporarily */
#define MAX_CONN_BACKLOG	(8*ONE_K)
#define GRACE_CONN_BACKLOG	(MAX_CONN_BACKLOG / 2)

/* Watermarks for number of active connections. Lower to 2 for testing */
#define MAX_CONN_HIGH_WATERMARK	(256)
#define MAX_CONN_LOW_WATERMARK	(MAX_CONN_HIGH_WATERMARK - 1)

#define NUM_THREADS	4

#define BUF_SIZE 4096

struct connection{
	int client_fd;
	int server_fd;
};

struct thread_data{
	int threadidx;
	struct connection *fdarray;
};


struct sockaddr_in remote_addr; /* The address of the target server */
struct thread_data thread_data_array[NUM_THREADS];

 //function declarations
int findval(struct connection * array, int size, int val);
int sendall(int destination_fd, char *buf, int len);
int forward(int origin_fd, int destination_fd, void *);
int start_proxy(struct thread_data *mydata);
void *ThreadTask(void *thread_arg);

void * ThreadTask(void *thread_arg){
	struct thread_data *mydata;
	mydata = (struct thread_data *) thread_arg;
	start_proxy(mydata);
}


// Searches array for first instance of val and returns its idx. Otherwise,  returns size + 1
int findval(struct connection * array, int size, int val){
	int i;
	for(i = 0; i < size; i++){
		if(array[i].client_fd == val && array[i].server_fd == val){
			return i;
		}
	}
	return size + 1;
}


/* sendall partially taken from Beej's Guide to Network Programming to handle partial sends
	... Changed for syntatic clarity and functionality*/
int sendall(int destination_fd, char *buf, int len){
	int total = 0; 
	int bytesleft = len; 
	int n;
	while(total < len) {
		n = send(destination_fd, buf+total, bytesleft, 0);
		if (n == -1) { break; }
		total += n;
		bytesleft -= n;
	}
	return n==-1?-1:0;
}



int forward(int origin_fd, int destination_fd, void *buf){
	int size;
	if(size = recv(origin_fd, buf, BUF_SIZE, 0)){
		if(size < -1){
			fprintf(stderr, "Receiving error %s\n", strerror(errno));
			close(origin_fd);
			return 0;
		}
		if(size == -1){
			// do nothing
		}
		else{
			if(sendall(destination_fd, buf, size) < 0){
				fprintf(stderr, "Sending error %s\n", strerror(errno));
				return 0;
			}
		}
	}
}

int start_proxy(struct thread_data *mydata){
	printf("Staring Proxy Forwarding...");
	int exitflag = 0; 
	void *client_buf = malloc(BUF_SIZE);
	void *server_buf = malloc(BUF_SIZE);
	int size, maxfd, i;



	fd_set readfds;
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	maxfd = 0;

	while(1){
		if(exitflag == 1)
			break;
			//todo: need to buffer
		FD_ZERO(&readfds);

		for(i = mydata->threadidx; i < MAX_CONN_HIGH_WATERMARK; i+=NUM_THREADS){
			int client_fd = mydata->fdarray[i].client_fd;
			int server_fd = mydata->fdarray[i].server_fd;
			if(client_fd != -1 && server_fd != -1){
				maxfd = max(maxfd, client_fd);
				maxfd = max(maxfd, server_fd);
				FD_SET(client_fd, &readfds);
				FD_SET(server_fd, &readfds);
			}
		}

		select(maxfd, &readfds, NULL, NULL, &tv);
		
		for(i = mydata->threadidx; i < MAX_CONN_HIGH_WATERMARK; i+=NUM_THREADS){
			int client_fd = mydata->fdarray[i].client_fd;
			int server_fd = mydata->fdarray[i].server_fd;
			if(FD_ISSET(client_fd, &readfds)){
				printf("fowarding from thread %d and file descriptor %d...\n", mydata->threadidx, client_fd);
				forward(client_fd, server_fd, client_buf);
			}
			
			if(FD_ISSET(server_fd, &readfds)){
				printf("fowarding from thread %d and file descriptor %d...\n", mydata->threadidx, server_fd);
				forward(server_fd, client_fd, server_buf);
			}
		}


		
		
	}
}