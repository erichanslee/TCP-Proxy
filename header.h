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
#define MAX_CONN_HIGH_WATERMARK	(4)
#define MAX_CONN_LOW_WATERMARK	(MAX_CONN_HIGH_WATERMARK - 1)

#define MAX_THREAD_NUM	2
#define BUF_SIZE 4096

struct connection{
	int client_fd;
	int server_fd;
};

int sendall(int destination_fd, char *buf, int len);
int forward(int origin_fd, int destination_fd, void *buf);
//int start_proxy(int client_fd, int server_fd);
void * ThreadTask(void *thread_arg);
void __loop(int proxy_fd);
int findval(struct connection * array, int size, int val);
