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

#define MAX_THREAD_NUM	1
#define BUF_SIZE 4096

struct connection{
	int client_fd;
	int server_fd;
};

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

